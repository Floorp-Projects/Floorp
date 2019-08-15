/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { PROMISE } from "../utils/middleware/promise";
import {
  getSource,
  getSourceFromId,
  getSourceWithContent,
  getSourceContent,
  getGeneratedSource,
  getSourcesEpoch,
  getBreakpointsForSource,
  getSourceActorsForSource,
} from "../../selectors";
import { addBreakpoint } from "../breakpoints";

import { prettyPrintSource } from "./prettyPrint";
import { setBreakableLines } from "./breakableLines";
import { isFulfilled } from "../../utils/async-value";

import { isOriginal, isPretty } from "../../utils/source";
import {
  memoizeableAction,
  type MemoizedAction,
} from "../../utils/memoizableAction";

import { Telemetry } from "devtools-modules";

import type { ThunkArgs } from "../types";
import type { Source, Context } from "../../types";

// Measures the time it takes for a source to load
const loadSourceHistogram = "DEVTOOLS_DEBUGGER_LOAD_SOURCE_MS";
const telemetry = new Telemetry();

async function loadSource(
  state,
  source: Source,
  { sourceMaps, client, getState }
): Promise<?{
  text: string,
  contentType: string,
}> {
  if (isPretty(source) && isOriginal(source)) {
    const generatedSource = getGeneratedSource(state, source);
    if (!generatedSource) {
      throw new Error("Unable to find minified original.");
    }
    const content = getSourceContent(state, generatedSource.id);
    if (!content || !isFulfilled(content)) {
      throw new Error("Cannot pretty-print a file that has not loaded");
    }

    return prettyPrintSource(
      sourceMaps,
      generatedSource,
      content.value,
      getSourceActorsForSource(state, generatedSource.id)
    );
  }

  if (isOriginal(source)) {
    const result = await sourceMaps.getOriginalSourceText(source);
    if (!result) {
      // The way we currently try to load and select a pending
      // selected location, it is possible that we will try to fetch the
      // original source text right after the source map has been cleared
      // after a navigation event.
      throw new Error("Original source text unavailable");
    }
    return result;
  }

  const actors = getSourceActorsForSource(state, source.id);
  if (!actors.length) {
    throw new Error("No source actor for loadSource");
  }

  telemetry.start(loadSourceHistogram, source);
  const response = await client.sourceContents(actors[0]);
  telemetry.finish(loadSourceHistogram, source);

  return {
    text: response.source,
    contentType: response.contentType || "text/javascript",
  };
}

async function loadSourceTextPromise(
  cx: Context,
  source: Source,
  { dispatch, getState, client, sourceMaps, parser }: ThunkArgs
): Promise<?Source> {
  const epoch = getSourcesEpoch(getState());
  await dispatch({
    type: "LOAD_SOURCE_TEXT",
    sourceId: source.id,
    epoch,
    [PROMISE]: loadSource(getState(), source, { sourceMaps, client, getState }),
  });

  const newSource = getSource(getState(), source.id);

  if (!newSource) {
    return;
  }
  const content = getSourceContent(getState(), newSource.id);

  if (!newSource.isWasm && content) {
    parser.setSource(
      newSource.id,
      isFulfilled(content)
        ? content.value
        : { type: "text", value: "", contentType: undefined }
    );

    await dispatch(setBreakableLines(cx, source.id));
    // Update the text in any breakpoints for this source by re-adding them.
    const breakpoints = getBreakpointsForSource(getState(), source.id);
    for (const { location, options, disabled } of breakpoints) {
      await dispatch(addBreakpoint(cx, location, options, disabled));
    }
  }
}

export function loadSourceById(cx: Context, sourceId: string) {
  return ({ getState, dispatch }: ThunkArgs) => {
    const source = getSourceFromId(getState(), sourceId);
    return dispatch(loadSourceText({ cx, source }));
  };
}

export const loadSourceText: MemoizedAction<
  { cx: Context, source: Source },
  ?Source
> = memoizeableAction("loadSourceText", {
  hasValue: ({ source }, { getState }) => {
    return (
      !source ||
      !!(
        getSource(getState(), source.id) &&
        getSourceWithContent(getState(), source.id).content
      )
    );
  },
  getValue: ({ source }, { getState }) =>
    source ? getSource(getState(), source.id) : null,
  createKey: ({ source }, { getState }) => {
    const epoch = getSourcesEpoch(getState());
    return `${epoch}:${source.id}`;
  },
  action: ({ cx, source }, thunkArgs) =>
    loadSourceTextPromise(cx, source, thunkArgs),
});

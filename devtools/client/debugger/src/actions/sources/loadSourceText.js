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
import { isFulfilled, fulfilled } from "../../utils/async-value";

import { isOriginal, isPretty } from "../../utils/source";
import {
  memoizeableAction,
  type MemoizedAction,
} from "../../utils/memoizableAction";

import { Telemetry } from "devtools-modules";

import type { ThunkArgs } from "../types";
import type { Source, Context, SourceId } from "../../types";
import type { State } from "../../reducers/types";

// Measures the time it takes for a source to load
const loadSourceHistogram = "DEVTOOLS_DEBUGGER_LOAD_SOURCE_MS";
const telemetry = new Telemetry();

async function loadSource(
  state: State,
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
    const result = await sourceMaps.getOriginalSourceText(source.id);
    if (!result) {
      // The way we currently try to load and select a pending
      // selected location, it is possible that we will try to fetch the
      // original source text right after the source map has been cleared
      // after a navigation event.
      throw new Error("Original source text unavailable");
    }
    return result;
  }

  // We only need the source text from one actor, but messages sent to retrieve
  // the source might fail if the actor has or is about to shut down. Keep
  // trying with different actors until one request succeeds.
  let response;
  const handledActors = new Set();
  while (true) {
    const actors = getSourceActorsForSource(state, source.id);
    const actor = actors.find(({ actor: a }) => !handledActors.has(a));
    if (!actor) {
      throw new Error("Unknown source");
    }
    handledActors.add(actor.actor);

    try {
      telemetry.start(loadSourceHistogram, source);
      response = await client.sourceContents(actor);
      telemetry.finish(loadSourceHistogram, source);
      break;
    } catch (e) {
      console.warn(`sourceContents failed: ${e}`);
    }
  }

  return {
    text: (response: any).source,
    contentType: (response: any).contentType || "text/javascript",
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

    // Update the text in any breakpoints for this source by re-adding them.
    const breakpoints = getBreakpointsForSource(getState(), source.id);
    for (const { location, options, disabled } of breakpoints) {
      await dispatch(addBreakpoint(cx, location, options, disabled));
    }
  }
}

export function loadSourceById(cx: Context, sourceId: SourceId) {
  return ({ getState, dispatch }: ThunkArgs) => {
    const source = getSourceFromId(getState(), sourceId);
    return dispatch(loadSourceText({ cx, source }));
  };
}

export const loadSourceText: MemoizedAction<
  {| cx: Context, source: Source |},
  ?Source
> = memoizeableAction("loadSourceText", {
  getValue: ({ source }, { getState }) => {
    source = source ? getSource(getState(), source.id) : null;
    if (!source) {
      return null;
    }

    const { content } = getSourceWithContent(getState(), source.id);
    if (!content || content.state === "pending") {
      return content;
    }

    // This currently swallows source-load-failure since we return fulfilled
    // here when content.state === "rejected". In an ideal world we should
    // propagate that error upward.
    return fulfilled(source);
  },
  createKey: ({ source }, { getState }) => {
    const epoch = getSourcesEpoch(getState());
    return `${epoch}:${source.id}`;
  },
  action: ({ cx, source }, thunkArgs) =>
    loadSourceTextPromise(cx, source, thunkArgs),
});

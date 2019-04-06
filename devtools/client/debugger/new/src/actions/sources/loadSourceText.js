/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { PROMISE } from "../utils/middleware/promise";
import {
  getSource,
  getSourceFromId,
  getGeneratedSource,
  getSourcesEpoch,
  getBreakpointsForSource
} from "../../selectors";
import { setBreakpointPositions, addBreakpoint } from "../breakpoints";

import { prettyPrintSource } from "./prettyPrint";

import * as parser from "../../workers/parser";
import { isLoaded, isOriginal, isPretty } from "../../utils/source";
import {
  memoizeableAction,
  type MemoizedAction
} from "../../utils/memoizableAction";

import { Telemetry } from "devtools-modules";

import type { ThunkArgs } from "../types";

import type { Source } from "../../types";

// Measures the time it takes for a source to load
const loadSourceHistogram = "DEVTOOLS_DEBUGGER_LOAD_SOURCE_MS";
const telemetry = new Telemetry();

async function loadSource(
  state,
  source: Source,
  { sourceMaps, client }
): Promise<?{
  text: string,
  contentType: string
}> {
  if (isPretty(source) && isOriginal(source)) {
    const generatedSource = getGeneratedSource(state, source);
    return prettyPrintSource(sourceMaps, source, generatedSource);
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

  if (!source.actors.length) {
    throw new Error("No source actor for loadSource");
  }

  telemetry.start(loadSourceHistogram, source);
  const response = await client.sourceContents(source.actors[0]);
  telemetry.finish(loadSourceHistogram, source);

  return {
    text: response.source,
    contentType: response.contentType || "text/javascript"
  };
}

async function loadSourceTextPromise(
  source: Source,
  { dispatch, getState, client, sourceMaps }: ThunkArgs
): Promise<?Source> {
  const epoch = getSourcesEpoch(getState());
  await dispatch({
    type: "LOAD_SOURCE_TEXT",
    sourceId: source.id,
    epoch,
    [PROMISE]: loadSource(getState(), source, { sourceMaps, client })
  });

  const newSource = getSource(getState(), source.id);

  if (!newSource) {
    return;
  }

  if (!newSource.isWasm && isLoaded(newSource)) {
    parser.setSource(newSource);
    dispatch(setBreakpointPositions({ sourceId: newSource.id }));

    // Update the text in any breakpoints for this source by re-adding them.
    const breakpoints = getBreakpointsForSource(getState(), source.id);
    for (const { location, options, disabled } of breakpoints) {
      await dispatch(addBreakpoint(location, options, disabled));
    }
  }

  return newSource;
}

export function loadSourceById(sourceId: string) {
  return ({ getState, dispatch }: ThunkArgs) => {
    const source = getSourceFromId(getState(), sourceId);
    return dispatch(loadSourceText({ source }));
  };
}

export const loadSourceText: MemoizedAction<
  { source: Source },
  ?Source
> = memoizeableAction("loadSourceText", {
  exitEarly: ({ source }) => !source,
  hasValue: ({ source }, { getState }) => isLoaded(source),
  getValue: ({ source }, { getState }) => getSource(getState(), source.id),
  createKey: ({ source }, { getState }) => {
    const epoch = getSourcesEpoch(getState());
    return `${epoch}:${source.id}`;
  },
  action: ({ source }, thunkArgs) => loadSourceTextPromise(source, thunkArgs)
});

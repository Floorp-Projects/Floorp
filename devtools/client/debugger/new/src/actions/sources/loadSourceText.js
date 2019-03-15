/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { PROMISE } from "../utils/middleware/promise";
import { getSource, getSourcesEpoch } from "../../selectors";
import { setBreakpointPositions } from "../breakpoints";

import * as parser from "../../workers/parser";
import { isLoaded, isOriginal } from "../../utils/source";
import { Telemetry } from "devtools-modules";

import type { ThunkArgs } from "../types";

import type { Source } from "../../types";

const requests = new Map();

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
  if (isOriginal(source)) {
    const result = await sourceMaps.getOriginalSourceText(source);
    if (!result) {
      // TODO: This allows pretty files to continue working the way they have
      // been, but is very ugly. Remove this when we centralize pretty-printing
      // in loadSource. https://github.com/firefox-devtools/debugger/issues/8071
      if (source.isPrettyPrinted) {
        return null;
      }

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
  epoch: number,
  { dispatch, getState, client, sourceMaps }: ThunkArgs
): Promise<?Source> {
  if (isLoaded(source)) {
    return source;
  }

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
    await dispatch(setBreakpointPositions(newSource.id));
  }

  return newSource;
}

/**
 * @memberof actions/sources
 * @static
 */
export function loadSourceText(inputSource: ?Source) {
  return async (thunkArgs: ThunkArgs) => {
    if (!inputSource) {
      return;
    }
    // This ensures that the falsy check above is preserved into the IIFE
    // below in a way that Flow is happy with.
    const source = inputSource;

    const epoch = getSourcesEpoch(thunkArgs.getState());

    const id = `${epoch}:${source.id}`;
    let promise = requests.get(id);
    if (!promise) {
      promise = (async () => {
        try {
          return await loadSourceTextPromise(source, epoch, thunkArgs);
        } catch (e) {
          // TODO: This swallows errors for now. Ideally we would get rid of
          // this once we have a better handle on our async state management.
        } finally {
          requests.delete(id);
        }
      })();
      requests.set(id, promise);
    }

    return promise;
  };
}

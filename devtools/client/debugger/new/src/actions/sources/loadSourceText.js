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

async function loadSource(state, source: Source, { sourceMaps, client }) {
  if (isOriginal(source)) {
    return sourceMaps.getOriginalSourceText(source);
  }

  if (!source.actors.length) {
    throw new Error("No source actor for loadSource");
  }

  telemetry.start(loadSourceHistogram, source);
  const response = await client.sourceContents(source.actors[0]);
  telemetry.finish(loadSourceHistogram, source);

  return {
    id: source.id,
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

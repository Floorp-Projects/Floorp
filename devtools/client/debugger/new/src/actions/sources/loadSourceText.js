/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { PROMISE } from "../utils/middleware/promise";
import { getSource } from "../../selectors";
import { setBreakpointPositions } from "../breakpoints";

import * as parser from "../../workers/parser";
import { isLoaded, isOriginal } from "../../utils/source";
import { Telemetry } from "devtools-modules";

import defer from "../../utils/defer";
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

  const response = await client.sourceContents(source.actors[0]);
  telemetry.finish(loadSourceHistogram, source);

  return {
    id: source.id,
    text: response.source,
    contentType: response.contentType || "text/javascript"
  };
}

/**
 * @memberof actions/sources
 * @static
 */
export function loadSourceText(source: ?Source) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    if (!source) {
      return;
    }

    const id = source.id;
    // Fetch the source text only once.
    if (requests.has(id)) {
      return requests.get(id);
    }

    if (isLoaded(source)) {
      return Promise.resolve();
    }

    const deferred = defer();
    requests.set(id, deferred.promise);

    telemetry.start(loadSourceHistogram, source);
    try {
      await dispatch({
        type: "LOAD_SOURCE_TEXT",
        sourceId: source.id,
        [PROMISE]: loadSource(getState(), source, { sourceMaps, client })
      });
    } catch (e) {
      deferred.resolve();
      requests.delete(id);
      return;
    }

    const newSource = getSource(getState(), source.id);
    if (!newSource) {
      return;
    }

    if (!newSource.isWasm && isLoaded(newSource)) {
      parser.setSource(newSource);
      await dispatch(setBreakpointPositions(newSource.id));
    }

    // signal that the action is finished
    deferred.resolve();
    requests.delete(id);

    return source;
  };
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { PROMISE } from "../utils/middleware/promise";
import { getGeneratedSource, getSource } from "../../selectors";
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

async function loadSource(source: Source, { sourceMaps, client }) {
  const { id } = source;
  if (isOriginal(source)) {
    return sourceMaps.getOriginalSourceText(source);
  }

  const response = await client.sourceContents(id);
  telemetry.finish(loadSourceHistogram, source);

  return {
    id,
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
        [PROMISE]: loadSource(source, { sourceMaps, client })
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

    if (isOriginal(newSource) && !newSource.isWasm) {
      const generatedSource = getGeneratedSource(getState(), source);
      await dispatch(loadSourceText(generatedSource));
    }

    if (!newSource.isWasm) {
      await parser.setSource(newSource);
    }

    // signal that the action is finished
    deferred.resolve();
    requests.delete(id);

    return source;
  };
}

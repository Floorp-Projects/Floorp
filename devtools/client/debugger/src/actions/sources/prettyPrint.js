/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import SourceMaps, { generatedToOriginalId } from "devtools-source-map";

import assert from "../../utils/assert";
import { recordEvent } from "../../utils/telemetry";
import { remapBreakpoints } from "../breakpoints";

import { setSymbols } from "./symbols";
import { prettyPrint } from "../../workers/pretty-print";
import {
  getPrettySourceURL,
  isGenerated,
  isJavaScript,
} from "../../utils/source";
import { loadSourceText } from "./loadSourceText";
import { mapFrames } from "../pause";
import { selectSpecificLocation } from "../sources";

import {
  getSource,
  getSourceFromId,
  getSourceByURL,
  getSelectedLocation,
  getThreadContext,
} from "../../selectors";

import type { Action, ThunkArgs } from "../types";
import { selectSource } from "./select";
import type {
  Source,
  SourceId,
  SourceContent,
  SourceActor,
  Context,
  SourceLocation,
} from "../../types";

export async function prettyPrintSource(
  sourceMaps: typeof SourceMaps,
  generatedSource: Source,
  content: SourceContent,
  actors: Array<SourceActor>
) {
  if (!isJavaScript(generatedSource, content) || content.type !== "text") {
    throw new Error("Can't prettify non-javascript files.");
  }

  const url = getPrettySourceURL(generatedSource.url);
  const { code, mappings } = await prettyPrint({
    text: content.value,
    url,
  });
  await sourceMaps.applySourceMap(generatedSource.id, url, code, mappings);

  // The source map URL service used by other devtools listens to changes to
  // sources based on their actor IDs, so apply the mapping there too.
  for (const { actor } of actors) {
    await sourceMaps.applySourceMap(actor, url, code, mappings);
  }
  return {
    text: code,
    contentType: "text/javascript",
  };
}

export function createPrettySource(cx: Context, sourceId: SourceId) {
  return async ({ dispatch, getState, sourceMaps }: ThunkArgs) => {
    const source = getSourceFromId(getState(), sourceId);
    const url = getPrettySourceURL(source.url || source.id);
    const id = generatedToOriginalId(sourceId, url);

    const prettySource = {
      id,
      url,
      relativeUrl: url,
      isBlackBoxed: false,
      isPrettyPrinted: true,
      isWasm: false,
      isExtension: false,
      extensionName: null,
      isOriginal: true,
    };

    dispatch(({ type: "ADD_SOURCE", cx, source: prettySource }: Action));

    await dispatch(selectSource(cx, id));

    return prettySource;
  };
}

function selectPrettyLocation(
  cx: Context,
  prettySource: Source,
  generatedLocation: ?SourceLocation
) {
  return async ({ dispatch, sourceMaps, getState }: ThunkArgs) => {
    let location = generatedLocation
      ? generatedLocation
      : getSelectedLocation(getState());

    if (location && location.line >= 1) {
      location = await sourceMaps.getOriginalLocation(location);

      return dispatch(
        selectSpecificLocation(cx, { ...location, sourceId: prettySource.id })
      );
    }

    return dispatch(selectSource(cx, prettySource.id));
  };
}

/**
 * Toggle the pretty printing of a source's text. All subsequent calls to
 * |getText| will return the pretty-toggled text. Nothing will happen for
 * non-javascript files.
 *
 * @memberof actions/sources
 * @static
 * @param string id The source form from the RDP.
 * @returns Promise
 *          A promise that resolves to [aSource, prettyText] or rejects to
 *          [aSource, error].
 */
export function togglePrettyPrint(cx: Context, sourceId: SourceId) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const source = getSource(getState(), sourceId);
    if (!source) {
      return {};
    }

    if (!source.isPrettyPrinted) {
      recordEvent("pretty_print");
    }

    await dispatch(loadSourceText({ cx, source }));
    assert(
      isGenerated(source),
      "Pretty-printing only allowed on generated sources"
    );

    const url = getPrettySourceURL(source.url);
    const prettySource = getSourceByURL(getState(), url);

    if (prettySource) {
      return dispatch(selectPrettyLocation(cx, prettySource));
    }

    const selectedLocation = getSelectedLocation(getState());
    const newPrettySource = await dispatch(createPrettySource(cx, sourceId));
    dispatch(selectPrettyLocation(cx, newPrettySource, selectedLocation));

    const threadcx = getThreadContext(getState());
    await dispatch(mapFrames(threadcx));

    await dispatch(setSymbols({ cx, source: newPrettySource }));

    await dispatch(remapBreakpoints(cx, sourceId));

    return newPrettySource;
  };
}

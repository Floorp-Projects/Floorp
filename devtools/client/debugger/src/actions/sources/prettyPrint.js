/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { generatedToOriginalId } from "devtools/client/shared/source-map-loader/index";

import assert from "../../utils/assert";
import { recordEvent } from "../../utils/telemetry";
import { updateBreakpointsForNewPrettyPrintedSource } from "../breakpoints";

import { setSymbols } from "./symbols";
import {
  getPrettySourceURL,
  isGenerated,
  isJavaScript,
} from "../../utils/source";
import { isFulfilled } from "../../utils/async-value";
import { loadGeneratedSourceText } from "./loadSourceText";
import { mapFrames } from "../pause";
import { selectSpecificLocation } from "../sources";
import { createPrettyPrintOriginalSource } from "../../client/firefox/create";

import {
  getSource,
  getFirstSourceActorForGeneratedSource,
  getSourceFromId,
  getSourceByURL,
  getSelectedLocation,
  getThreadContext,
} from "../../selectors";

import { selectSource } from "./select";

function getPrettyOriginalSourceURL(generatedSource) {
  return getPrettySourceURL(generatedSource.url || generatedSource.id);
}

export async function prettyPrintSource(
  sourceMapLoader,
  prettyPrintWorker,
  generatedSource,
  content,
  actors
) {
  if (!content || !isFulfilled(content)) {
    throw new Error("Cannot pretty-print a file that has not loaded");
  }

  const contentValue = content.value;
  if (
    !isJavaScript(generatedSource, contentValue) ||
    contentValue.type !== "text"
  ) {
    throw new Error("Can't prettify non-javascript files.");
  }

  const url = getPrettyOriginalSourceURL(generatedSource);
  const { code, mappings } = await prettyPrintWorker.prettyPrint({
    text: contentValue.value,
    url,
  });
  await sourceMapLoader.applySourceMap(generatedSource.id, url, code, mappings);

  // The source map URL service used by other devtools listens to changes to
  // sources based on their actor IDs, so apply the mapping there too.
  for (const { actor } of actors) {
    await sourceMapLoader.applySourceMap(actor, url, code, mappings);
  }
  return {
    text: code,
    contentType: "text/javascript",
  };
}

export function createPrettySource(cx, sourceId) {
  return async ({ dispatch, getState }) => {
    const source = getSourceFromId(getState(), sourceId);
    const url = getPrettyOriginalSourceURL(source);
    const id = generatedToOriginalId(sourceId, url);
    const prettySource = createPrettyPrintOriginalSource(id, url);

    dispatch({
      type: "ADD_ORIGINAL_SOURCES",
      cx,
      originalSources: [prettySource],
    });

    await dispatch(selectSource(cx, id));

    return prettySource;
  };
}

function selectPrettyLocation(cx, prettySource, generatedLocation) {
  return async ({ dispatch, sourceMapLoader, getState }) => {
    let location = generatedLocation
      ? generatedLocation
      : getSelectedLocation(getState());

    if (location && location.line >= 1) {
      location = await sourceMapLoader.getOriginalLocation(location);

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
export function togglePrettyPrint(cx, sourceId) {
  return async ({ dispatch, getState }) => {
    const source = getSource(getState(), sourceId);
    if (!source) {
      return {};
    }

    if (!source.isPrettyPrinted) {
      recordEvent("pretty_print");
    }

    assert(
      isGenerated(source),
      "Pretty-printing only allowed on generated sources"
    );

    const sourceActor = getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );

    await dispatch(loadGeneratedSourceText({ cx, sourceActor }));

    const url = getPrettySourceURL(source.url);
    const prettySource = getSourceByURL(getState(), url);

    if (prettySource) {
      return dispatch(selectPrettyLocation(cx, prettySource));
    }

    const selectedLocation = getSelectedLocation(getState());
    const newPrettySource = await dispatch(createPrettySource(cx, sourceId));
    await dispatch(selectPrettyLocation(cx, newPrettySource, selectedLocation));

    const threadcx = getThreadContext(getState());
    await dispatch(mapFrames(threadcx));

    await dispatch(setSymbols({ cx, source: newPrettySource, sourceActor }));

    await dispatch(updateBreakpointsForNewPrettyPrintedSource(cx, sourceId));

    return newPrettySource;
  };
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  generatedToOriginalId,
  originalToGeneratedId,
} from "devtools/client/shared/source-map-loader/index";

import assert from "../../utils/assert";
import { recordEvent } from "../../utils/telemetry";
import { updateBreakpointsForNewPrettyPrintedSource } from "../breakpoints";

import {
  getPrettySourceURL,
  isGenerated,
  isJavaScript,
} from "../../utils/source";
import { isFulfilled } from "../../utils/async-value";
import { getOriginalLocation } from "../../utils/source-maps";
import {
  loadGeneratedSourceText,
  loadOriginalSourceText,
} from "./loadSourceText";
import { mapFrames } from "../pause";
import { selectSpecificLocation } from "../sources";
import { createPrettyPrintOriginalSource } from "../../client/firefox/create";

import {
  getSource,
  getFirstSourceActorForGeneratedSource,
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
  const { code, sourceMap } = await prettyPrintWorker.prettyPrint({
    text: contentValue.value,
    url,
  });

  // The source map URL service used by other devtools listens to changes to
  // sources based on their actor IDs, so apply the sourceMap there too.
  const generatedSourceIds = [
    generatedSource.id,
    ...actors.map(item => item.actor),
  ];
  await sourceMapLoader.setSourceMapForGeneratedSources(
    generatedSourceIds,
    sourceMap
  );

  return {
    text: code,
    contentType: "text/javascript",
  };
}

function createPrettySource(cx, source) {
  return async ({ dispatch, sourceMapLoader, getState }) => {
    const url = getPrettyOriginalSourceURL(source);
    const id = generatedToOriginalId(source.id, url);
    const prettySource = createPrettyPrintOriginalSource(id, url);

    dispatch({
      type: "ADD_ORIGINAL_SOURCES",
      cx,
      originalSources: [prettySource],
    });
    return prettySource;
  };
}

function selectPrettyLocation(cx, prettySource) {
  return async thunkArgs => {
    const { dispatch, getState } = thunkArgs;
    let location = getSelectedLocation(getState());

    // If we were selecting a particular line in the minified/generated source,
    // try to select the matching line in the prettified/original source.
    if (
      location &&
      location.line >= 1 &&
      location.sourceId == originalToGeneratedId(prettySource.id)
    ) {
      location = await getOriginalLocation(location, thunkArgs);

      return dispatch(
        selectSpecificLocation(cx, { ...location, sourceId: prettySource.id })
      );
    }

    return dispatch(selectSource(cx, prettySource.id));
  };
}

/**
 * Toggle the pretty printing of a source's text.
 * Nothing will happen for non-javascript files.
 *
 * @param Object cx
 * @param String sourceId
 *        The source ID for the minified/generated source object.
 * @returns Promise
 *          A promise that resolves to the Pretty print/original source object.
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

    const newPrettySource = await dispatch(createPrettySource(cx, source));

    // Force loading the pretty source/original text.
    // This will end up calling prettyPrintSource() of this module, and
    // more importantly, will populate the sourceMapLoader, which is used by selectPrettyLocation.
    await dispatch(loadOriginalSourceText({ cx, source: newPrettySource }));
    // Select the pretty/original source based on the location we may
    // have had against the minified/generated source.
    // This uses source map to map locations.
    // Also note that selecting a location force many things:
    // * opening tabs
    // * fetching symbols/inline scope
    // * fetching breakable lines
    await dispatch(selectPrettyLocation(cx, newPrettySource));

    const threadcx = getThreadContext(getState());
    // Update frames to the new pretty/original source (in case we were paused)
    await dispatch(mapFrames(threadcx));
    // Update breakpoints locations to the new pretty/original source
    await dispatch(updateBreakpointsForNewPrettyPrintedSource(cx, sourceId));

    return newPrettySource;
  };
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { generatedToOriginalId } from "devtools/client/shared/source-map-loader/index";

import assert from "../../utils/assert";
import { recordEvent } from "../../utils/telemetry";
import { updateBreakpointsForNewPrettyPrintedSource } from "../breakpoints/index";

import { getPrettySourceURL, isJavaScript } from "../../utils/source";
import { isFulfilled, fulfilled } from "../../utils/async-value";
import { getOriginalLocation } from "../../utils/source-maps";
import { prefs } from "../../utils/prefs";
import {
  loadGeneratedSourceText,
  loadOriginalSourceText,
} from "./loadSourceText";
import { mapFrames } from "../pause/index";
import { selectSpecificLocation } from "../sources/index";
import { createPrettyPrintOriginalSource } from "../../client/firefox/create";

import {
  getFirstSourceActorForGeneratedSource,
  getSourceFromId,
  getSelectedLocation,
} from "../../selectors/index";

import { selectSource } from "./select";
import { memoizeableAction } from "../../utils/memoizableAction";

import DevToolsUtils from "devtools/shared/DevToolsUtils";

const LINE_BREAK_REGEX = /\r\n?|\n|\u2028|\u2029/g;
function matchAllLineBreaks(str) {
  return Array.from(str.matchAll(LINE_BREAK_REGEX));
}

function getPrettyOriginalSourceURL(generatedSource) {
  return getPrettySourceURL(generatedSource.url || generatedSource.id);
}

export async function prettyPrintSourceTextContent(
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
    (!isJavaScript(generatedSource, contentValue) && !generatedSource.isHTML) ||
    contentValue.type !== "text"
  ) {
    throw new Error(
      `Can't prettify ${contentValue.contentType} files, only HTML and Javascript.`
    );
  }

  const url = getPrettyOriginalSourceURL(generatedSource);

  let prettyPrintWorkerResult;
  if (generatedSource.isHTML) {
    prettyPrintWorkerResult = await prettyPrintHtmlFile({
      prettyPrintWorker,
      generatedSource,
      content,
      actors,
    });
  } else {
    prettyPrintWorkerResult = await prettyPrintWorker.prettyPrint({
      sourceText: contentValue.value,
      indent: " ".repeat(prefs.indentSize),
      url,
    });
  }

  // The source map URL service used by other devtools listens to changes to
  // sources based on their actor IDs, so apply the sourceMap there too.
  const generatedSourceIds = [
    generatedSource.id,
    ...actors.map(item => item.actor),
  ];
  await sourceMapLoader.setSourceMapForGeneratedSources(
    generatedSourceIds,
    prettyPrintWorkerResult.sourceMap
  );

  return {
    text: prettyPrintWorkerResult.code,
    contentType: contentValue.contentType,
  };
}

/**
 * Pretty print inline script inside an HTML file
 *
 * @param {Object} options
 * @param {PrettyPrintDispatcher} options.prettyPrintWorker: The prettyPrint worker
 * @param {Object} options.generatedSource: The HTML source we want to pretty print
 * @param {Object} options.content
 * @param {Array} options.actors: An array of the HTML file inline script sources data
 *
 * @returns Promise<Object> A promise that resolves with an object of the following shape:
 *                          - {String} code: The prettified HTML text
 *                          - {Object} sourceMap: The sourceMap object
 */
async function prettyPrintHtmlFile({
  prettyPrintWorker,
  generatedSource,
  content,
  actors,
}) {
  const url = getPrettyOriginalSourceURL(generatedSource);
  const contentValue = content.value;
  const htmlFileText = contentValue.value;
  const prettyPrintWorkerResult = { code: htmlFileText };

  const allLineBreaks = matchAllLineBreaks(htmlFileText);
  let lineCountDelta = 0;

  // Sort inline script actors so they are in the same order as in the html document.
  actors.sort((a, b) => {
    if (a.sourceStartLine === b.sourceStartLine) {
      return a.sourceStartColumn > b.sourceStartColumn;
    }
    return a.sourceStartLine > b.sourceStartLine;
  });

  const prettyPrintTaskId = generatedSource.id;

  // We don't want to replace part of the HTML document in the loop since it would require
  // to account for modified lines for each iteration.
  // Instead, we'll put each sections to replace in this array, where elements will be
  // objects of the following shape:
  // {Integer} startIndex: The start index in htmlFileText of the section we want to replace
  // {Integer} endIndex: The end index in htmlFileText of the section we want to replace
  // {String} prettyText: The pretty text we'll replace the original section with
  // Once we iterated over all the inline scripts, we'll do the replacements (on the html
  // file text) in reverse order, so we don't need have to care about the modified lines
  // for each iteration.
  const replacements = [];

  const seenLocations = new Set();

  for (const sourceInfo of actors) {
    // We can get duplicate source actors representing the same inline script which will
    // cause trouble in the pretty printing here. This should be fixed on the server (see
    // Bug 1824979), but in the meantime let's not handle the same location twice so the
    // pretty printing is not impacted.
    const location = `${sourceInfo.sourceStartLine}:${sourceInfo.sourceStartColumn}`;
    if (!sourceInfo.sourceLength || seenLocations.has(location)) {
      continue;
    }
    seenLocations.add(location);
    // Here we want to get the index of the last line break before the script tag.
    // In allLineBreaks, this would be the item at (script tag line - 1)
    // Since sourceInfo.sourceStartLine is 1-based, we need to get the item at (sourceStartLine - 2)
    const indexAfterPreviousLineBreakInHtml =
      sourceInfo.sourceStartLine > 1
        ? allLineBreaks[sourceInfo.sourceStartLine - 2].index + 1
        : 0;
    const startIndex =
      indexAfterPreviousLineBreakInHtml + sourceInfo.sourceStartColumn;
    const endIndex = startIndex + sourceInfo.sourceLength;
    const scriptText = htmlFileText.substring(startIndex, endIndex);
    DevToolsUtils.assert(
      scriptText.length == sourceInfo.sourceLength,
      "script text has expected length"
    );

    // Here we're going to pretty print each inline script content.
    // Since we want to have a sourceMap that we'll apply to the whole HTML file,
    // we'll only collect the sourceMap once we handled all inline scripts.
    // `taskId` allows us to signal to the worker that all those calls are part of the
    // same bigger file, and we'll use it later to get the sourceMap.
    const prettyText = await prettyPrintWorker.prettyPrintInlineScript({
      taskId: prettyPrintTaskId,
      sourceText: scriptText,
      indent: " ".repeat(prefs.indentSize),
      url,
      originalStartLine: sourceInfo.sourceStartLine,
      originalStartColumn: sourceInfo.sourceStartColumn,
      // The generated line will be impacted by the previous inline scripts that were
      // pretty printed, which is why we offset with lineCountDelta
      generatedStartLine: sourceInfo.sourceStartLine + lineCountDelta,
      generatedStartColumn: sourceInfo.sourceStartColumn,
      lineCountDelta,
    });

    // We need to keep track of the line added/removed in order to properly offset
    // the mapping of the pretty-print text
    lineCountDelta +=
      matchAllLineBreaks(prettyText).length -
      matchAllLineBreaks(scriptText).length;

    replacements.push({
      startIndex,
      endIndex,
      prettyText,
    });
  }

  // `getSourceMap` allow us to collect the computed source map resulting of the calls
  // to `prettyPrint` with the same taskId.
  prettyPrintWorkerResult.sourceMap = await prettyPrintWorker.getSourceMap(
    prettyPrintTaskId
  );

  // Sort replacement in reverse order so we can replace code in the HTML file more easily
  replacements.sort((a, b) => a.startIndex < b.startIndex);
  for (const { startIndex, endIndex, prettyText } of replacements) {
    prettyPrintWorkerResult.code =
      prettyPrintWorkerResult.code.substring(0, startIndex) +
      prettyText +
      prettyPrintWorkerResult.code.substring(endIndex);
  }

  return prettyPrintWorkerResult;
}

function createPrettySource(source, sourceActor) {
  return async ({ dispatch, sourceMapLoader, getState }) => {
    const url = getPrettyOriginalSourceURL(source);
    const id = generatedToOriginalId(source.id, url);
    const prettySource = createPrettyPrintOriginalSource(id, url);

    dispatch({
      type: "ADD_ORIGINAL_SOURCES",
      originalSources: [prettySource],
      generatedSourceActor: sourceActor,
    });
    return prettySource;
  };
}

function selectPrettyLocation(prettySource) {
  return async thunkArgs => {
    const { dispatch, getState } = thunkArgs;
    let location = getSelectedLocation(getState());

    // If we were selecting a particular line in the minified/generated source,
    // try to select the matching line in the prettified/original source.
    if (
      location &&
      location.line >= 1 &&
      getPrettySourceURL(location.source.url) == prettySource.url
    ) {
      // Note that it requires to have called `prettyPrintSourceTextContent` and `sourceMapLoader.setSourceMapForGeneratedSources`
      // to be functional and so to be called after `loadOriginalSourceText` completed.
      location = await getOriginalLocation(location, thunkArgs);

      // If the precise line/column correctly mapped to the pretty printed source, select that precise location.
      // Otherwise fallback to selectSource in order to select the first line instead of the current line within the bundle.
      if (location.source == prettySource) {
        return dispatch(selectSpecificLocation(location));
      }
    }

    return dispatch(selectSource(prettySource));
  };
}

/**
 * Toggle the pretty printing of a source's text.
 * Nothing will happen for non-javascript files.
 *
 * @param Object source
 *        The source object for the minified/generated source.
 * @returns Promise
 *          A promise that resolves to the Pretty print/original source object.
 */
export async function prettyPrintSource(source, thunkArgs) {
  const { dispatch, getState } = thunkArgs;
  recordEvent("pretty_print");

  assert(
    !source.isOriginal,
    "Pretty-printing only allowed on generated sources"
  );

  const sourceActor = getFirstSourceActorForGeneratedSource(
    getState(),
    source.id
  );

  await dispatch(loadGeneratedSourceText(sourceActor));

  const newPrettySource = await dispatch(
    createPrettySource(source, sourceActor)
  );

  // Force loading the pretty source/original text.
  // This will end up calling prettyPrintSourceTextContent() of this module, and
  // more importantly, will populate the sourceMapLoader, which is used by selectPrettyLocation.
  await dispatch(loadOriginalSourceText(newPrettySource));

  // Update frames to the new pretty/original source (in case we were paused).
  // Map the frames before selecting the pretty source in order to avoid
  // having bundle/generated source for frames (we may compute scope things for the bundle).
  await dispatch(mapFrames(sourceActor.thread));

  // Update breakpoints locations to the new pretty/original source
  await dispatch(updateBreakpointsForNewPrettyPrintedSource(source));

  // A mutated flag, only meant to be used within this module
  // to know when we are done loading the pretty printed source.
  // This is important for the callsite in `selectLocation`
  // in order to ensure all action are completed and especially `mapFrames`.
  // Otherwise we may use generated frames there.
  newPrettySource._loaded = true;

  return newPrettySource;
}

// Use memoization in order to allow calling this actions many times
// while ensuring creating the pretty source only once.
const memoizedPrettyPrintSource = memoizeableAction("setSymbols", {
  getValue: (source, { getState }) => {
    // Lookup for an already existing pretty source
    const url = getPrettyOriginalSourceURL(source);
    const id = generatedToOriginalId(source.id, url);
    const s = getSourceFromId(getState(), id);
    // Avoid returning it if doTogglePrettyPrint isn't completed.
    if (!s || !s._loaded) {
      return undefined;
    }
    return fulfilled(s);
  },
  createKey: source => source.id,
  action: (source, thunkArgs) => prettyPrintSource(source, thunkArgs),
});

export function prettyPrintAndSelectSource(source) {
  return async ({ dispatch, sourceMapLoader, getState }) => {
    const prettySource = await dispatch(memoizedPrettyPrintSource(source));

    // Select the pretty/original source based on the location we may
    // have had against the minified/generated source.
    // This uses source map to map locations.
    // Also note that selecting a location force many things:
    // * opening tabs
    // * fetching symbols/inline scope
    // * fetching breakable lines
    //
    // This isn't part of memoizedTogglePrettyPrint/doTogglePrettyPrint
    // because if the source is already pretty printed, the memoization
    // would avoid trying to update to the mapped location based
    // on current location on the minified source.
    await dispatch(selectPrettyLocation(prettySource));

    return prettySource;
  };
}

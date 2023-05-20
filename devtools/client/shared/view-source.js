/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Tries to open a Stylesheet file in the Style Editor. If the file is not
 * found, it is opened in source view instead.
 * Returns a promise resolving to a boolean indicating whether or not
 * the source was able to be displayed in the StyleEditor, as the built-in
 * Firefox View Source is the fallback.
 *
 * @param {Toolbox} toolbox
 * @param {string|Object} stylesheetResourceOrGeneratedURL
 * @param {number} generatedLine
 * @param {number} generatedColumn
 *
 * @return {Promise<boolean>}
 */
exports.viewSourceInStyleEditor = async function (
  toolbox,
  stylesheetResourceOrGeneratedURL,
  generatedLine,
  generatedColumn
) {
  const originalPanelId = toolbox.currentToolId;

  try {
    const panel = await toolbox.selectTool("styleeditor", "view-source", {
      // This will be only used in case the styleeditor wasn't loaded yet, to make the
      // initialization faster in case we already have a stylesheet resource. We still
      // need the rest of this function to handle subsequent calls and sourcemapped stylesheets.
      stylesheetToSelect: {
        stylesheet: stylesheetResourceOrGeneratedURL,
        line: generatedLine,
        column: generatedColumn,
      },
    });

    let stylesheetResource;
    if (typeof stylesheetResourceOrGeneratedURL === "string") {
      stylesheetResource = panel.getStylesheetResourceForGeneratedURL(
        stylesheetResourceOrGeneratedURL
      );
    } else {
      stylesheetResource = stylesheetResourceOrGeneratedURL;
    }

    const originalLocation = stylesheetResource
      ? await getOriginalLocation(
          toolbox,
          stylesheetResource.resourceId,
          generatedLine,
          generatedColumn
        )
      : null;

    if (originalLocation) {
      await panel.selectOriginalSheet(
        originalLocation.sourceId,
        originalLocation.line,
        originalLocation.column
      );
      return true;
    }

    if (stylesheetResource) {
      await panel.selectStyleSheet(
        stylesheetResource,
        generatedLine,
        generatedColumn
      );
      return true;
    }
  } catch (e) {
    console.error("Failed to view source in style editor", e);
  }

  // If we weren't able to select the stylesheet in the style editor, display it in a
  // view-source tab
  exports.viewSource(
    toolbox,
    typeof stylesheetResourceOrGeneratedURL === "string"
      ? stylesheetResourceOrGeneratedURL
      : stylesheetResourceOrGeneratedURL.href ||
          stylesheetResourceOrGeneratedURL.nodeHref,
    generatedLine
  );

  // As we might have moved to the styleeditor, switch back to the original panel
  await toolbox.selectTool(originalPanelId);

  return false;
};

/**
 * Tries to open a JavaScript file in the Debugger. If the file is not found,
 * it is opened in source view instead. Either the source URL or source actor ID
 * can be specified. If both are specified, the source actor ID is used.
 *
 * Returns a promise resolving to a boolean indicating whether or not
 * the source was able to be displayed in the Debugger, as the built-in Firefox
 * View Source is the fallback.
 *
 * @param {Toolbox} toolbox
 * @param {string} sourceURL
 * @param {number} sourceLine
 * @param {number} sourceColumn
 * @param {string} sourceID
 * @param {(string|object)} [reason=unknown]
 *
 * @return {Promise<boolean>}
 */
exports.viewSourceInDebugger = async function (
  toolbox,
  generatedURL,
  generatedLine,
  generatedColumn,
  sourceActorId,
  reason = "unknown"
) {
  // Load the debugger in the background
  const dbg = await toolbox.loadTool("jsdebugger");

  const openedSourceInDebugger = await dbg.openSourceInDebugger({
    generatedURL,
    generatedLine,
    generatedColumn,
    sourceActorId,
    reason,
  });

  if (openedSourceInDebugger) {
    return true;
  }

  // Fallback to built-in firefox view-source:
  exports.viewSource(toolbox, generatedURL, generatedLine, generatedColumn);
  return false;
};

async function getOriginalLocation(
  toolbox,
  generatedID,
  generatedLine,
  generatedColumn
) {
  // If there is no line number, then there's no chance that we'll get back
  // a useful original location.
  if (typeof generatedLine !== "number") {
    return null;
  }

  let originalLocation = null;
  try {
    originalLocation = await toolbox.sourceMapLoader.getOriginalLocation({
      sourceId: generatedID,
      line: generatedLine,
      column: generatedColumn,
    });
    if (originalLocation && originalLocation.sourceId === generatedID) {
      originalLocation = null;
    }
  } catch (err) {
    console.error(
      "Failed to resolve sourcemapped location for the given source location",
      { generatedID, generatedLine, generatedColumn },
      err
    );
  }
  return originalLocation;
}

/**
 * Open a link in Firefox's View Source.
 *
 * @param {Toolbox} toolbox
 * @param {string} sourceURL
 * @param {number} sourceLine
 * @param {number} sourceColumn
 *
 * @return {Promise}
 */
exports.viewSource = async function (
  toolbox,
  sourceURL,
  sourceLine,
  sourceColumn
) {
  const utils = toolbox.gViewSourceUtils;
  utils.viewSource({
    URL: sourceURL,
    lineNumber: sourceLine || -1,
    columnNumber: sourceColumn || -1,
  });
};

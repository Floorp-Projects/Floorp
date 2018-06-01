/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
var { gDevTools } = require("devtools/client/framework/devtools");
var { getSourceText } = require("devtools/client/debugger/content/queries");

/**
 * Tries to open a Stylesheet file in the Style Editor. If the file is not
 * found, it is opened in source view instead.
 * Returns a promise resolving to a boolean indicating whether or not
 * the source was able to be displayed in the StyleEditor, as the built-in
 * Firefox View Source is the fallback.
 *
 * @param {Toolbox} toolbox
 * @param {string} sourceURL
 * @param {number} sourceLine
 *
 * @return {Promise<boolean>}
 */
exports.viewSourceInStyleEditor = async function(toolbox, sourceURL,
                                                        sourceLine) {
  const panel = await toolbox.loadTool("styleeditor");

  try {
    await panel.selectStyleSheet(sourceURL, sourceLine);
    await toolbox.selectTool("styleeditor");
    return true;
  } catch (e) {
    exports.viewSource(toolbox, sourceURL, sourceLine);
    return false;
  }
};

/**
 * Tries to open a JavaScript file in the Debugger. If the file is not found,
 * it is opened in source view instead.
 * Returns a promise resolving to a boolean indicating whether or not
 * the source was able to be displayed in the Debugger, as the built-in Firefox
 * View Source is the fallback.
 *
 * @param {Toolbox} toolbox
 * @param {string} sourceURL
 * @param {number} sourceLine
 *
 * @return {Promise<boolean>}
 */
exports.viewSourceInDebugger = async function(toolbox, sourceURL, sourceLine) {
  // If the Debugger was already open, switch to it and try to show the
  // source immediately. Otherwise, initialize it and wait for the sources
  // to be added first.
  const debuggerAlreadyOpen = toolbox.getPanel("jsdebugger");
  const dbg = await toolbox.loadTool("jsdebugger");

  // New debugger frontend
  if (Services.prefs.getBoolPref("devtools.debugger.new-debugger-frontend")) {
    const source = dbg.getSource(sourceURL);
    if (source) {
      await toolbox.selectTool("jsdebugger");
      dbg.selectSource(sourceURL, sourceLine);
      return true;
    }

    exports.viewSource(toolbox, sourceURL, sourceLine);
    return false;
  }

  const win = dbg.panelWin;

  // Old debugger frontend
  if (!debuggerAlreadyOpen) {
    await win.DebuggerController.waitForSourcesLoaded();
  }

  const { DebuggerView } = win;
  const { Sources } = DebuggerView;

  const item = Sources.getItemForAttachment(a => a.source.url === sourceURL);
  if (item) {
    await toolbox.selectTool("jsdebugger");

    // Determine if the source has already finished loading. There's two cases
    // in which we need to wait for the source to be shown:
    // 1) The requested source is not yet selected and will be shown once it is
    //    selected and loaded
    // 2) The requested source is selected BUT the source text is still loading.
    const { actor } = item.attachment.source;
    const state = win.DebuggerController.getState();

    // (1) Is the source selected?
    const selected = state.sources.selectedSource;
    const isSelected = selected === actor;

    // (2) Has the source text finished loading?
    let isLoading = false;

    // Only check if the source is loading when the source is already selected.
    // If the source is not selected, we will select it below and the already
    // pending load will be cancelled and this check is useless.
    if (isSelected) {
      const sourceTextInfo = getSourceText(state, selected);
      isLoading = sourceTextInfo && sourceTextInfo.loading;
    }

    // Select the requested source
    DebuggerView.setEditorLocation(actor, sourceLine, { noDebug: true });

    // Wait for it to load
    if (!isSelected || isLoading) {
      await win.DebuggerController.waitForSourceShown(sourceURL);
    }
    return true;
  }

  // If not found, still attempt to open in View Source
  exports.viewSource(toolbox, sourceURL, sourceLine);
  return false;
};

/**
 * Tries to open a JavaScript file in the corresponding Scratchpad.
 *
 * @param {string} sourceURL
 * @param {number} sourceLine
 *
 * @return {Promise}
 */
exports.viewSourceInScratchpad = async function(sourceURL, sourceLine) {
  // Check for matching top level scratchpad window.
  const wins = Services.wm.getEnumerator("devtools:scratchpad");

  while (wins.hasMoreElements()) {
    const win = wins.getNext();

    if (!win.closed && win.Scratchpad.uniqueName === sourceURL) {
      win.focus();
      win.Scratchpad.editor.setCursor({ line: sourceLine, ch: 0 });
      return;
    }
  }

  // For scratchpads within toolbox
  for (const toolbox of gDevTools.getToolboxes()) {
    const scratchpadPanel = toolbox.getPanel("scratchpad");
    if (scratchpadPanel) {
      const { scratchpad } = scratchpadPanel;
      if (scratchpad.uniqueName === sourceURL) {
        toolbox.selectTool("scratchpad");
        toolbox.raise();
        scratchpad.editor.focus();
        scratchpad.editor.setCursor({ line: sourceLine, ch: 0 });
        return;
      }
    }
  }
};

/**
 * Open a link in Firefox's View Source.
 *
 * @param {Toolbox} toolbox
 * @param {string} sourceURL
 * @param {number} sourceLine
 *
 * @return {Promise}
 */
exports.viewSource = async function(toolbox, sourceURL, sourceLine) {
  const utils = toolbox.gViewSourceUtils;
  utils.viewSource({
    URL: sourceURL,
    lineNumber: sourceLine || 0,
  });
};

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
var { gDevTools } = require("devtools/client/framework/devtools");

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
 * @param {string} [reason=unknown]
 *
 * @return {Promise<boolean>}
 */
exports.viewSourceInDebugger = async function(toolbox, sourceURL, sourceLine,
                                              reason = "unknown") {
  const dbg = await toolbox.loadTool("jsdebugger");
  const source = dbg.getSource(sourceURL);
  if (source) {
    await toolbox.selectTool("jsdebugger", reason);
    dbg.selectSource(sourceURL, sourceLine);
    return true;
  }

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
  for (const win of Services.wm.getEnumerator("devtools:scratchpad")) {
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

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "Services");
loader.lazyImporter(this, "gDevTools", "resource://devtools/client/framework/gDevTools.jsm");
loader.lazyImporter(this, "Task", "resource://gre/modules/Task.jsm");

/**
 * Tries to open a Stylesheet file in the Style Editor. If the file is not found,
 * it is opened in source view instead.
 * Returns a promise resolving to a boolean indicating whether or not
 * the source was able to be displayed in the StyleEditor, as the built-in Firefox
 * View Source is the fallback.
 *
 * @param {Toolbox} toolbox
 * @param {string} sourceURL
 * @param {number} sourceLine
 *
 * @return {Promise<boolean>}
 */
exports.viewSourceInStyleEditor = Task.async(function *(toolbox, sourceURL, sourceLine) {
  let panel = yield toolbox.loadTool("styleeditor");

  try {
    yield panel.selectStyleSheet(sourceURL, sourceLine);
    yield toolbox.selectTool("styleeditor");
    return true;
  } catch (e) {
    exports.viewSource(toolbox, sourceURL, sourceLine);
    return false;
  }
});

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
exports.viewSourceInDebugger = Task.async(function *(toolbox, sourceURL, sourceLine) {
  // If the Debugger was already open, switch to it and try to show the
  // source immediately. Otherwise, initialize it and wait for the sources
  // to be added first.
  let debuggerAlreadyOpen = toolbox.getPanel("jsdebugger");
  let { panelWin: dbg } = yield toolbox.loadTool("jsdebugger");

  if (!debuggerAlreadyOpen) {
    yield dbg.once(dbg.EVENTS.SOURCES_ADDED);
  }

  let { DebuggerView } = dbg;
  let { Sources } = DebuggerView;

  let item = Sources.getItemForAttachment(a => a.source.url === sourceURL);
  if (item) {
    yield toolbox.selectTool("jsdebugger");
    yield DebuggerView.setEditorLocation(item.attachment.source.actor, sourceLine, { noDebug: true });
    return true;
  }

  // If not found, still attempt to open in View Source
  exports.viewSource(toolbox, sourceURL, sourceLine);
  return false;
});

/**
 * Tries to open a JavaScript file in the corresponding Scratchpad.
 *
 * @param {string} sourceURL
 * @param {number} sourceLine
 *
 * @return {Promise}
 */
exports.viewSourceInScratchpad = Task.async(function *(sourceURL, sourceLine) {
  // Check for matching top level scratchpad window.
  let wins = Services.wm.getEnumerator("devtools:scratchpad");

  while (wins.hasMoreElements()) {
    let win = wins.getNext();

    if (!win.closed && win.Scratchpad.uniqueName === sourceURL) {
      win.focus();
      win.Scratchpad.editor.setCursor({ line: sourceLine, ch: 0 });
      return;
    }
  }

  // For scratchpads within toolbox
  for (let [, toolbox] of gDevTools) {
    let scratchpadPanel = toolbox.getPanel("scratchpad");
    if (scratchpadPanel) {
      let { scratchpad } = scratchpadPanel;
      if (scratchpad.uniqueName === sourceURL) {
        toolbox.selectTool("scratchpad");
        toolbox.raise();
        scratchpad.editor.focus();
        scratchpad.editor.setCursor({ line: sourceLine, ch: 0 });
        return;
      }
    }
  }
});

/**
 * Open a link in Firefox's View Source.
 *
 * @param {Toolbox} toolbox
 * @param {string} sourceURL
 * @param {number} sourceLine
 *
 * @return {Promise}
 */
exports.viewSource = Task.async(function *(toolbox, sourceURL, sourceLine) {
  // Attempt to access view source via a browser first, which may display it in
  // a tab, if enabled.
  let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
  if (browserWin) {
    return browserWin.BrowserViewSourceOfDocument({
      URL: sourceURL,
      lineNumber: sourceLine
    });
  }
  let utils = toolbox.gViewSourceUtils;
  utils.viewSource(sourceURL, null, toolbox.doc, sourceLine || 0);
});

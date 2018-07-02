/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
loader.lazyRequireGetter(this, "Utils", "devtools/client/webconsole/utils", true);
loader.lazyRequireGetter(this, "WebConsoleFrame", "devtools/client/webconsole/webconsole-frame", true);
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "viewSource", "devtools/client/shared/view-source");
loader.lazyRequireGetter(this, "openDocLink", "devtools/client/shared/link", true);

var gHudId = 0;

/**
 * A WebConsole instance is an interactive console initialized *per target*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate the target's document content.
 *
 * This object only wraps the iframe that holds the Web Console UI. This is
 * meant to be an integration point between the Firefox UI and the Web Console
 * UI and features.
 *
 * @constructor
 * @param object target
 *        The target that the web console will connect to.
 * @param nsIDOMWindow iframeWindow
 *        The window where the web console UI is already loaded.
 * @param nsIDOMWindow chromeWindow
 *        The window of the web console owner.
 * @param object hudService
 *        The parent HUD Service
 */
function WebConsole(target, iframeWindow, chromeWindow, hudService) {
  this.iframeWindow = iframeWindow;
  this.chromeWindow = chromeWindow;
  this.hudId = "hud_" + ++gHudId;
  this.target = target;
  this.browserWindow = this.chromeWindow.top;
  this.hudService = hudService;

  const element = this.browserWindow.document.documentElement;
  if (element.getAttribute("windowtype") != gDevTools.chromeWindowType) {
    this.browserWindow = this.hudService.currentContext();
  }
  this.ui = new WebConsoleFrame(this);
}

WebConsole.prototype = {
  iframeWindow: null,
  chromeWindow: null,
  browserWindow: null,
  hudId: null,
  target: null,
  ui: null,
  _browserConsole: false,
  _destroyer: null,

  /**
   * Getter for a function to to listen for every request that completes. Used
   * by unit tests. The callback takes one argument: the HTTP activity object as
   * received from the remote Web Console.
   *
   * @type function
   */
  get lastFinishedRequestCallback() {
    return this.hudService.lastFinishedRequest.callback;
  },

  /**
   * Getter for the window that can provide various utilities that the web
   * console makes use of, like opening links, managing popups, etc.  In
   * most cases, this will be |this.browserWindow|, but in some uses (such as
   * the Browser Toolbox), there is no browser window, so an alternative window
   * hosts the utilities there.
   * @type nsIDOMWindow
   */
  get chromeUtilsWindow() {
    if (this.browserWindow) {
      return this.browserWindow;
    }
    return this.chromeWindow.top;
  },

  /**
   * Getter for the xul:popupset that holds any popups we open.
   * @type Element
   */
  get mainPopupSet() {
    return this.chromeUtilsWindow.document.getElementById("mainPopupSet");
  },

  /**
   * Getter for the output element that holds messages we display.
   * @type Element
   */
  get outputNode() {
    return this.ui ? this.ui.outputNode : null;
  },

  get gViewSourceUtils() {
    return this.chromeUtilsWindow.gViewSourceUtils;
  },

  /**
   * Initialize the Web Console instance.
   *
   * @return object
   *         A promise for the initialization.
   */
  init() {
    return this.ui.init().then(() => this);
  },

  /**
   * The JSTerm object that manages the console's input.
   * @see webconsole.js::JSTerm
   * @type object
   */
  get jsterm() {
    return this.ui ? this.ui.jsterm : null;
  },

  /**
   * Alias for the WebConsoleFrame.setFilterState() method.
   * @see webconsole.js::WebConsoleFrame.setFilterState()
   */
  setFilterState() {
    this.ui && this.ui.setFilterState.apply(this.ui, arguments);
  },

  /**
   * Open a link in a new tab.
   *
   * @param string link
   *        The URL you want to open in a new tab.
   */
  openLink(link, e) {
    openDocLink(link);
  },

  /**
   * Open a link in Firefox's view source.
   *
   * @param string sourceURL
   *        The URL of the file.
   * @param integer sourceLine
   *        The line number which should be highlighted.
   */
  viewSource(sourceURL, sourceLine) {
    this.gViewSourceUtils.viewSource({ URL: sourceURL, lineNumber: sourceLine || 0 });
  },

  /**
   * Tries to open a Stylesheet file related to the web page for the web console
   * instance in the Style Editor. If the file is not found, it is opened in
   * source view instead.
   *
   * Manually handle the case where toolbox does not exist (Browser Console).
   *
   * @param string sourceURL
   *        The URL of the file.
   * @param integer sourceLine
   *        The line number which you want to place the caret.
   */
  viewSourceInStyleEditor(sourceURL, sourceLine) {
    const toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      this.viewSource(sourceURL, sourceLine);
      return;
    }
    toolbox.viewSourceInStyleEditor(sourceURL, sourceLine);
  },

  /**
   * Tries to open a JavaScript file related to the web page for the web console
   * instance in the Script Debugger. If the file is not found, it is opened in
   * source view instead.
   *
   * Manually handle the case where toolbox does not exist (Browser Console).
   *
   * @param string sourceURL
   *        The URL of the file.
   * @param integer sourceLine
   *        The line number which you want to place the caret.
   */
  viewSourceInDebugger(sourceURL, sourceLine) {
    const toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      this.viewSource(sourceURL, sourceLine);
      return;
    }
    toolbox.viewSourceInDebugger(sourceURL, sourceLine).then(() => {
      this.ui.emit("source-in-debugger-opened");
    });
  },

  /**
   * Tries to open a JavaScript file related to the web page for the web console
   * instance in the corresponding Scratchpad.
   *
   * @param string sourceURL
   *        The URL of the file which corresponds to a Scratchpad id.
   */
  viewSourceInScratchpad(sourceURL, sourceLine) {
    viewSource.viewSourceInScratchpad(sourceURL, sourceLine);
  },

  /**
   * Retrieve information about the JavaScript debugger's stackframes list. This
   * is used to allow the Web Console to evaluate code in the selected
   * stackframe.
   *
   * @return object|null
   *         An object which holds:
   *         - frames: the active ThreadClient.cachedFrames array.
   *         - selected: depth/index of the selected stackframe in the debugger
   *         UI.
   *         If the debugger is not open or if it's not paused, then |null| is
   *         returned.
   */
  getDebuggerFrames() {
    const toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      return null;
    }
    const panel = toolbox.getPanel("jsdebugger");

    if (!panel) {
      return null;
    }

    return panel.getFrames();
  },

  async getMappedExpression(expression) {
    const toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      return expression;
    }
    const panel = toolbox.getPanel("jsdebugger");

    if (!panel) {
      return expression;
    }

    return panel.getMappedExpression(expression);
  },

  /**
   * Retrieves the current selection from the Inspector, if such a selection
   * exists. This is used to pass the ID of the selected actor to the Web
   * Console server for the $0 helper.
   *
   * @return object|null
   *         A Selection referring to the currently selected node in the
   *         Inspector.
   *         If the inspector was never opened, or no node was ever selected,
   *         then |null| is returned.
   */
  getInspectorSelection() {
    const toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      return null;
    }
    const panel = toolbox.getPanel("inspector");
    if (!panel || !panel.selection) {
      return null;
    }
    return panel.selection;
  },

  /**
   * Destroy the object. Call this method to avoid memory leaks when the Web
   * Console is closed.
   *
   * @return object
   *         A promise object that is resolved once the Web Console is closed.
   */
  async destroy() {
    if (this._destroyer) {
      return this._destroyer;
    }

    this._destroyer = (async () => {
      this.hudService.consoles.delete(this.hudId);

      // The document may already be removed
      if (this.chromeUtilsWindow && this.mainPopupSet) {
        const popupset = this.mainPopupSet;
        const panels = popupset.querySelectorAll("panel[hudId=" + this.hudId + "]");
        for (const panel of panels) {
          panel.hidePopup();
        }
      }

      if (this.ui) {
        await this.ui.destroy();
      }

      if (!this._browserConsole) {
        try {
          await this.target.activeTab.focus();
        } catch (ex) {
          // Tab focus can fail if the tab or target is closed.
        }
      }

      const id = Utils.supportsString(this.hudId);
      Services.obs.notifyObservers(id, "web-console-destroyed");
    })();

    return this._destroyer;
  },
};

module.exports = WebConsole;

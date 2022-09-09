/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "Utils",
  "devtools/client/webconsole/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "WebConsoleUI",
  "devtools/client/webconsole/webconsole-ui",
  true
);
loader.lazyRequireGetter(
  this,
  "gDevTools",
  "devtools/client/framework/devtools",
  true
);
loader.lazyRequireGetter(
  this,
  "openDocLink",
  "devtools/client/shared/link",
  true
);
loader.lazyRequireGetter(
  this,
  "DevToolsUtils",
  "devtools/shared/DevToolsUtils"
);
const EventEmitter = require("devtools/shared/event-emitter");
const Telemetry = require("devtools/client/shared/telemetry");

var gHudId = 0;
const isMacOS = Services.appinfo.OS === "Darwin";

/**
 * A WebConsole instance is an interactive console initialized *per target*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate the target's document content.
 *
 * This object only wraps the iframe that holds the Web Console UI. This is
 * meant to be an integration point between the Firefox UI and the Web Console
 * UI and features.
 */
class WebConsole {
  /*
   * @constructor
   * @param object toolbox
   *        The toolbox where the web console is displayed.
   * @param object commands
   *        The commands object with all interfaces defined from devtools/shared/commands/
   * @param nsIDOMWindow iframeWindow
   *        The window where the web console UI is already loaded.
   * @param nsIDOMWindow chromeWindow
   *        The window of the web console owner.
   * @param bool isBrowserConsole
   */
  constructor(
    toolbox,
    commands,
    iframeWindow,
    chromeWindow,
    isBrowserConsole = false
  ) {
    this.toolbox = toolbox;
    this.commands = commands;
    this.iframeWindow = iframeWindow;
    this.chromeWindow = chromeWindow;
    this.hudId = "hud_" + ++gHudId;
    this.browserWindow = DevToolsUtils.getTopWindow(this.chromeWindow);
    this.isBrowserConsole = isBrowserConsole;
    this.telemetry = new Telemetry();

    const element = this.browserWindow.document.documentElement;
    if (element.getAttribute("windowtype") != gDevTools.chromeWindowType) {
      this.browserWindow = Services.wm.getMostRecentWindow(
        gDevTools.chromeWindowType
      );
    }
    this.ui = new WebConsoleUI(this);
    this._destroyer = null;

    EventEmitter.decorate(this);
  }

  recordEvent(event, extra = {}) {
    this.telemetry.recordEvent(event, "webconsole", null, {
      session_id: (this.toolbox && this.toolbox.sessionId) || -1,
      ...extra,
    });
  }

  get currentTarget() {
    return this.commands.targetCommand.targetFront;
  }

  get resourceCommand() {
    return this.commands.resourceCommand;
  }

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
    return DevToolsUtils.getTopWindow(this.chromeWindow);
  }

  get gViewSourceUtils() {
    return this.chromeUtilsWindow.gViewSourceUtils;
  }

  getFrontByID(id) {
    return this.commands.client.getFrontByID(id);
  }

  /**
   * Initialize the Web Console instance.
   *
   * @param {Boolean} emitCreatedEvent: Defaults to true. If false is passed,
   *        We won't be sending the 'web-console-created' event.
   *
   * @return object
   *         A promise for the initialization.
   */
  async init(emitCreatedEvent = true) {
    await this.ui.init();

    // This event needs to be fired later in the case of the BrowserConsole
    if (emitCreatedEvent) {
      const id = Utils.supportsString(this.hudId);
      Services.obs.notifyObservers(id, "web-console-created");
    }
  }

  /**
   * The JSTerm object that manages the console's input.
   * @see webconsole.js::JSTerm
   * @type object
   */
  get jsterm() {
    return this.ui ? this.ui.jsterm : null;
  }

  /**
   * Get the value from the input field.
   * @returns {String|null} returns null if there's no input.
   */
  getInputValue() {
    if (!this.jsterm) {
      return null;
    }

    return this.jsterm._getValue();
  }

  inputHasSelection() {
    const { editor } = this.jsterm || {};
    return editor && !!editor.getSelection();
  }

  getInputSelection() {
    if (!this.jsterm || !this.jsterm.editor) {
      return null;
    }
    return this.jsterm.editor.getSelection();
  }

  /**
   * Sets the value of the input field (command line)
   *
   * @param {String} newValue: The new value to set.
   */
  setInputValue(newValue) {
    if (!this.jsterm) {
      return;
    }

    this.jsterm._setValue(newValue);
  }

  focusInput() {
    return this.jsterm && this.jsterm.focus();
  }

  /**
   * Open a link in a new tab.
   *
   * @param string link
   *        The URL you want to open in a new tab.
   */
  openLink(link, e = {}) {
    openDocLink(link, {
      relatedToCurrent: true,
      inBackground: isMacOS ? e.metaKey : e.ctrlKey,
    });
    if (e && typeof e.stopPropagation === "function") {
      e.stopPropagation();
    }
  }

  /**
   * Open a link in Firefox's view source.
   *
   * @param string sourceURL
   *        The URL of the file.
   * @param integer sourceLine
   *        The line number which should be highlighted.
   */
  viewSource(sourceURL, sourceLine) {
    this.gViewSourceUtils.viewSource({
      URL: sourceURL,
      lineNumber: sourceLine || 0,
    });
  }

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
   * @param integer sourceColumn
   *        The column number which you want to place the caret.
   */
  async viewSourceInDebugger(sourceURL, sourceLine, sourceColumn) {
    const { toolbox } = this;
    if (!toolbox) {
      this.viewSource(sourceURL, sourceLine, sourceColumn);
      return;
    }

    await toolbox.viewSourceInDebugger(sourceURL, sourceLine, sourceColumn);
    this.ui.emitForTests("source-in-debugger-opened");
  }

  /**
   * Retrieve information about the JavaScript debugger's currently selected stackframe.
   * is used to allow the Web Console to evaluate code in the selected stackframe.
   *
   * @return {String}
   *         The Frame Actor ID.
   *         If the debugger is not open or if it's not paused, then |null| is
   *         returned.
   */
  getSelectedFrameActorID() {
    const { toolbox } = this;
    if (!toolbox) {
      return null;
    }
    const panel = toolbox.getPanel("jsdebugger");

    if (!panel) {
      return null;
    }

    return panel.getSelectedFrameActorID();
  }

  /**
   * Given an expression, returns an object containing a new expression, mapped by the
   * parser worker to provide additional feature for the user (top-level await,
   * original languages mapping, …).
   *
   * @param {String} expression: The input to maybe map.
   * @returns {Object|null}
   *          Returns null if the input can't be mapped.
   *          If it can, returns an object containing the following:
   *            - {String} expression: The mapped expression
   *            - {Object} mapped: An object containing the different mapping that could
   *                               be done and if they were applied on the input.
   *                               At the moment, contains `await`, `bindings` and
   *                               `originalExpression`.
   */
  getMappedExpression(expression) {
    const { toolbox } = this;

    // We need to check if the debugger is open, since it may perform a variable name
    // substitution for sourcemapped script (i.e. evaluated `myVar.trim()` might need to
    // be transformed into `a.trim()`).
    const panel = toolbox && toolbox.getPanel("jsdebugger");
    if (panel) {
      return panel.getMappedExpression(expression);
    }

    if (expression.includes("await ")) {
      const shouldMapBindings = false;
      const shouldMapAwait = true;
      const res = this.parserService.mapExpression(
        expression,
        null,
        null,
        shouldMapBindings,
        shouldMapAwait
      );
      return res;
    }

    return null;
  }

  getMappedVariables() {
    const { toolbox } = this;
    return toolbox?.getPanel("jsdebugger")?.getMappedVariables();
  }

  get parserService() {
    if (this._parserService) {
      return this._parserService;
    }

    const {
      ParserDispatcher,
    } = require("devtools/client/debugger/src/workers/parser/index");

    this._parserService = new ParserDispatcher();
    this._parserService.start(
      "resource://devtools/client/debugger/dist/parser-worker.js",
      this.chromeUtilsWindow
    );
    return this._parserService;
  }

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
    const { toolbox } = this;
    if (!toolbox) {
      return null;
    }
    const panel = toolbox.getPanel("inspector");
    if (!panel || !panel.selection) {
      return null;
    }
    return panel.selection;
  }

  async onViewSourceInDebugger({ id, url, line, column }) {
    if (this.toolbox) {
      await this.toolbox.viewSourceInDebugger(url, line, column, id);

      this.recordEvent("jump_to_source");
      this.emitForTests("source-in-debugger-opened");
    }
  }

  async onViewSourceInStyleEditor({ url, line, column }) {
    if (!this.toolbox) {
      return;
    }
    await this.toolbox.viewSourceInStyleEditorByURL(url, line, column);
    this.recordEvent("jump_to_source");
  }

  async openNetworkPanel(requestId) {
    if (!this.toolbox) {
      return;
    }
    const netmonitor = await this.toolbox.selectTool("netmonitor");
    await netmonitor.panelWin.Netmonitor.inspectRequest(requestId);
  }

  getHighlighter() {
    if (!this.toolbox) {
      return null;
    }

    if (this._highlighter) {
      return this._highlighter;
    }

    this._highlighter = this.toolbox.getHighlighter();
    return this._highlighter;
  }

  async resendNetworkRequest(requestId) {
    if (!this.toolbox) {
      return;
    }

    const api = await this.toolbox.getNetMonitorAPI();
    await api.resendRequest(requestId);
  }

  async openNodeInInspector(grip) {
    if (!this.toolbox) {
      return;
    }

    const onSelectInspector = this.toolbox.selectTool(
      "inspector",
      "inspect_dom"
    );

    const onNodeFront = this.toolbox.target
      .getFront("inspector")
      .then(inspectorFront => inspectorFront.getNodeFrontFromNodeGrip(grip));

    const [nodeFront, inspectorPanel] = await Promise.all([
      onNodeFront,
      onSelectInspector,
    ]);

    const onInspectorUpdated = inspectorPanel.once("inspector-updated");
    const onNodeFrontSet = this.toolbox.selection.setNodeFront(nodeFront, {
      reason: "console",
    });

    await Promise.all([onNodeFrontSet, onInspectorUpdated]);
  }

  /**
   * Destroy the object. Call this method to avoid memory leaks when the Web
   * Console is closed.
   *
   * @return object
   *         A promise object that is resolved once the Web Console is closed.
   */
  destroy() {
    if (!this.hudId) {
      return;
    }

    if (this.ui) {
      this.ui.destroy();
    }

    if (this._parserService) {
      this._parserService.stop();
      this._parserService = null;
    }

    const id = Utils.supportsString(this.hudId);
    Services.obs.notifyObservers(id, "web-console-destroyed");
    this.hudId = null;

    this.emit("destroyed");
  }
}

module.exports = WebConsole;

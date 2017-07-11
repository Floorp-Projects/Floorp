/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Utils: WebConsoleUtils} =
  require("devtools/client/webconsole/utils");
const promise = require("promise");
const Debugger = require("Debugger");
const Services = require("Services");
const {KeyCodes} = require("devtools/client/shared/keycodes");

loader.lazyServiceGetter(this, "clipboardHelper",
                         "@mozilla.org/widget/clipboardhelper;1",
                         "nsIClipboardHelper");
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "AutocompletePopup", "devtools/client/shared/autocomplete-popup");
loader.lazyRequireGetter(this, "ToolSidebar", "devtools/client/framework/sidebar", true);
loader.lazyRequireGetter(this, "Messages", "devtools/client/webconsole/console-output", true);
loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");
loader.lazyRequireGetter(this, "EnvironmentClient", "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "ObjectClient", "devtools/shared/client/main", true);
loader.lazyImporter(this, "VariablesView", "resource://devtools/client/shared/widgets/VariablesView.jsm");
loader.lazyImporter(this, "VariablesViewController", "resource://devtools/client/shared/widgets/VariablesViewController.jsm");
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);

const l10n = require("devtools/client/webconsole/webconsole-l10n");

// Constants used for defining the direction of JSTerm input history navigation.
const HISTORY_BACK = -1;
const HISTORY_FORWARD = 1;

const XHTML_NS = "http://www.w3.org/1999/xhtml";

const HELP_URL = "https://developer.mozilla.org/docs/Tools/Web_Console/Helpers";

const VARIABLES_VIEW_URL = "chrome://devtools/content/shared/widgets/VariablesView.xul";

const PREF_INPUT_HISTORY_COUNT = "devtools.webconsole.inputHistoryCount";
const PREF_AUTO_MULTILINE = "devtools.webconsole.autoMultiline";

/**
 * Create a JSTerminal (a JavaScript command line). This is attached to an
 * existing HeadsUpDisplay (a Web Console instance). This code is responsible
 * with handling command line input, code evaluation and result output.
 *
 * @constructor
 * @param object webConsoleFrame
 *        The WebConsoleFrame object that owns this JSTerm instance.
 */
function JSTerm(webConsoleFrame) {
  this.hud = webConsoleFrame;
  this.hudId = this.hud.hudId;
  this.inputHistoryCount = Services.prefs.getIntPref(PREF_INPUT_HISTORY_COUNT);

  this.lastCompletion = { value: null };
  this._loadHistory();

  this._objectActorsInVariablesViews = new Map();

  this._keyPress = this._keyPress.bind(this);
  this._inputEventHandler = this._inputEventHandler.bind(this);
  this._focusEventHandler = this._focusEventHandler.bind(this);
  this._onKeypressInVariablesView = this._onKeypressInVariablesView.bind(this);
  this._blurEventHandler = this._blurEventHandler.bind(this);

  EventEmitter.decorate(this);
}

JSTerm.prototype = {
  SELECTED_FRAME: -1,

  /**
   * Load the console history from previous sessions.
   * @private
   */
  _loadHistory: function () {
    this.history = [];
    this.historyIndex = this.historyPlaceHolder = 0;

    this.historyLoaded = asyncStorage.getItem("webConsoleHistory")
      .then(value => {
        if (Array.isArray(value)) {
          // Since it was gotten asynchronously, there could be items already in
          // the history.  It's not likely but stick them onto the end anyway.
          this.history = value.concat(this.history);

          // Holds the number of entries in history. This value is incremented
          // in this.execute().
          this.historyIndex = this.history.length;

          // Holds the index of the history entry that the user is currently
          // viewing. This is reset to this.history.length when this.execute()
          // is invoked.
          this.historyPlaceHolder = this.history.length;
        }
      }, console.error);
  },

  /**
   * Clear the console history altogether.  Note that this will not affect
   * other consoles that are already opened (since they have their own copy),
   * but it will reset the array for all newly-opened consoles.
   * @returns Promise
   *          Resolves once the changes have been persisted.
   */
  clearHistory: function () {
    this.history = [];
    this.historyIndex = this.historyPlaceHolder = 0;
    return this.storeHistory();
  },

  /**
   * Stores the console history for future console instances.
   * @returns Promise
   *          Resolves once the changes have been persisted.
   */
  storeHistory: function () {
    return asyncStorage.setItem("webConsoleHistory", this.history);
  },

  /**
   * Stores the data for the last completion.
   * @type object
   */
  lastCompletion: null,

  /**
   * Array that caches the user input suggestions received from the server.
   * @private
   * @type array
   */
  _autocompleteCache: null,

  /**
   * The input that caused the last request to the server, whose response is
   * cached in the _autocompleteCache array.
   * @private
   * @type string
   */
  _autocompleteQuery: null,

  /**
   * The frameActorId used in the last autocomplete query. Whenever this changes
   * the autocomplete cache must be invalidated.
   * @private
   * @type string
   */
  _lastFrameActorId: null,

  /**
   * The Web Console sidebar.
   * @see this._createSidebar()
   * @see Sidebar.jsm
   */
  sidebar: null,

  /**
   * The Variables View instance shown in the sidebar.
   * @private
   * @type object
   */
  _variablesView: null,

  /**
   * Tells if you want the variables view UI updates to be lazy or not. Tests
   * disable lazy updates.
   *
   * @private
   * @type boolean
   */
  _lazyVariablesView: true,

  /**
   * Holds a map between VariablesView instances and sets of ObjectActor IDs
   * that have been retrieved from the server. This allows us to release the
   * objects when needed.
   *
   * @private
   * @type Map
   */
  _objectActorsInVariablesViews: null,

  /**
   * Last input value.
   * @type string
   */
  lastInputValue: "",

  /**
   * Tells if the input node changed since the last focus.
   *
   * @private
   * @type boolean
   */
  _inputChanged: false,

  /**
   * Tells if the autocomplete popup was navigated since the last open.
   *
   * @private
   * @type boolean
   */
  _autocompletePopupNavigated: false,

  /**
   * History of code that was executed.
   * @type array
   */
  history: null,
  autocompletePopup: null,
  inputNode: null,
  completeNode: null,

  /**
   * Getter for the element that holds the messages we display.
   * @type nsIDOMElement
   */
  get outputNode() {
    return this.hud.outputNode;
  },

  /**
   * Getter for the debugger WebConsoleClient.
   * @type object
   */
  get webConsoleClient() {
    return this.hud.webConsoleClient;
  },

  COMPLETE_FORWARD: 0,
  COMPLETE_BACKWARD: 1,
  COMPLETE_HINT_ONLY: 2,
  COMPLETE_PAGEUP: 3,
  COMPLETE_PAGEDOWN: 4,

  /**
   * Initialize the JSTerminal UI.
   */
  init: function () {
    let autocompleteOptions = {
      onSelect: this.onAutocompleteSelect.bind(this),
      onClick: this.acceptProposedCompletion.bind(this),
      listId: "webConsole_autocompletePopupListBox",
      position: "top",
      theme: "auto",
      autoSelect: true
    };

    let doc = this.hud.document;
    let toolbox = gDevTools.getToolbox(this.hud.owner.target);
    let tooltipDoc = toolbox ? toolbox.doc : doc;
    // The popup will be attached to the toolbox document or HUD document in the case
    // such as the browser console which doesn't have a toolbox.
    this.autocompletePopup = new AutocompletePopup(tooltipDoc, autocompleteOptions);

    let inputContainer = doc.querySelector(".jsterm-input-container");
    this.completeNode = doc.querySelector(".jsterm-complete-node");
    this.inputNode = doc.querySelector(".jsterm-input-node");

    if (this.hud.isBrowserConsole &&
        !Services.prefs.getBoolPref("devtools.chrome.enabled")) {
      inputContainer.style.display = "none";
    } else {
      let okstring = l10n.getStr("selfxss.okstring");
      let msg = l10n.getFormatStr("selfxss.msg", [okstring]);
      this._onPaste = WebConsoleUtils.pasteHandlerGen(
        this.inputNode, doc.getElementById("webconsole-notificationbox"),
        msg, okstring);
      this.inputNode.addEventListener("keypress", this._keyPress);
      this.inputNode.addEventListener("paste", this._onPaste);
      this.inputNode.addEventListener("drop", this._onPaste);
      this.inputNode.addEventListener("input", this._inputEventHandler);
      this.inputNode.addEventListener("keyup", this._inputEventHandler);
      this.inputNode.addEventListener("focus", this._focusEventHandler);
    }

    this.hud.window.addEventListener("blur", this._blurEventHandler);
    this.lastInputValue && this.setInputValue(this.lastInputValue);
  },

  focus: function () {
    if (!this.inputNode.getAttribute("focused")) {
      this.inputNode.focus();
    }
  },

  /**
   * The JavaScript evaluation response handler.
   *
   * @private
   * @param function [callback]
   *        Optional function to invoke when the evaluation result is added to
   *        the output.
   * @param object response
   *        The message received from the server.
   */
  _executeResultCallback: function (callback, response) {
    if (!this.hud) {
      return;
    }
    if (response.error) {
      console.error("Evaluation error " + response.error + ": " +
                    response.message);
      return;
    }
    let errorMessage = response.exceptionMessage;
    let errorDocURL = response.exceptionDocURL;

    let errorDocLink;
    if (errorDocURL) {
      errorMessage += " ";
      errorDocLink = this.hud.document.createElementNS(XHTML_NS, "a");
      errorDocLink.className = "learn-more-link webconsole-learn-more-link";
      errorDocLink.textContent = `[${l10n.getStr("webConsoleMoreInfoLabel")}]`;
      errorDocLink.title = errorDocURL.split("?")[0];
      errorDocLink.href = "#";
      errorDocLink.draggable = false;
      errorDocLink.addEventListener("click", () => {
        this.hud.owner.openLink(errorDocURL);
      });
    }

    // Wrap thrown strings in Error objects, so `throw "foo"` outputs
    // "Error: foo"
    if (typeof response.exception === "string") {
      errorMessage = new Error(errorMessage).toString();
    }
    let result = response.result;
    let helperResult = response.helperResult;
    let helperHasRawOutput = !!(helperResult || {}).rawOutput;

    if (helperResult && helperResult.type) {
      switch (helperResult.type) {
        case "clearOutput":
          this.clearOutput();
          break;
        case "clearHistory":
          this.clearHistory();
          break;
        case "inspectObject":
          if (!this.hud.NEW_CONSOLE_OUTPUT_ENABLED) {
            this.inspectObjectActor(helperResult.object);
          }
          break;
        case "error":
          try {
            errorMessage = l10n.getStr(helperResult.message);
          } catch (ex) {
            errorMessage = helperResult.message;
          }
          break;
        case "help":
          this.hud.owner.openLink(HELP_URL);
          break;
        case "copyValueToClipboard":
          clipboardHelper.copyString(helperResult.value);
          break;
      }
    }

    // Hide undefined results coming from JSTerm helper functions.
    if (!errorMessage
        && result
        && typeof result == "object"
        && result.type == "undefined"
        && helperResult
        && !helperHasRawOutput
        && !(this.hud.NEW_CONSOLE_OUTPUT_ENABLED && helperResult.type === "inspectObject")
    ) {
      callback && callback();
      return;
    }

    if (this.hud.NEW_CONSOLE_OUTPUT_ENABLED) {
      this.hud.newConsoleOutput.dispatchMessageAdd(response, true).then(callback);
      return;
    }
    let msg = new Messages.JavaScriptEvalOutput(response,
                                                errorMessage, errorDocLink);
    this.hud.output.addMessage(msg);

    if (callback) {
      let oldFlushCallback = this.hud._flushCallback;
      this.hud._flushCallback = () => {
        callback(msg.element);
        if (oldFlushCallback) {
          oldFlushCallback();
          this.hud._flushCallback = oldFlushCallback;
          return true;
        }

        return false;
      };
    }

    msg._objectActors = new Set();

    if (WebConsoleUtils.isActorGrip(response.exception)) {
      msg._objectActors.add(response.exception.actor);
    }

    if (WebConsoleUtils.isActorGrip(result)) {
      msg._objectActors.add(result.actor);
    }
  },

  inspectObjectActor: function (objectActor) {
    return this.openVariablesView({
      objectActor,
      label: VariablesView.getString(objectActor, {concise: true}),
    });
  },

  /**
   * Execute a string. Execution happens asynchronously in the content process.
   *
   * @param string [executeString]
   *        The string you want to execute. If this is not provided, the current
   *        user input is used - taken from |this.getInputValue()|.
   * @param function [callback]
   *        Optional function to invoke when the result is displayed.
   *        This is deprecated - please use the promise return value instead.
   * @returns Promise
   *          Resolves with the message once the result is displayed.
   */
  execute: function (executeString, callback) {
    let deferred = promise.defer();
    let resultCallback;
    if (this.hud.NEW_CONSOLE_OUTPUT_ENABLED) {
      resultCallback = (msg) => deferred.resolve(msg);
    } else {
      resultCallback = (msg) => {
        deferred.resolve(msg);
        if (callback) {
          callback(msg);
        }
      };
    }

    // attempt to execute the content of the inputNode
    executeString = executeString || this.getInputValue();
    if (!executeString) {
      return null;
    }

    let selectedNodeActor = null;
    let inspectorSelection = this.hud.owner.getInspectorSelection();
    if (inspectorSelection && inspectorSelection.nodeFront) {
      selectedNodeActor = inspectorSelection.nodeFront.actorID;
    }

    if (this.hud.NEW_CONSOLE_OUTPUT_ENABLED) {
      const { ConsoleCommand } = require("devtools/client/webconsole/new-console-output/types");
      let message = new ConsoleCommand({
        messageText: executeString,
      });
      this.hud.proxy.dispatchMessageAdd(message);
    } else {
      let message = new Messages.Simple(executeString, {
        category: "input",
        severity: "log",
      });
      this.hud.output.addMessage(message);
    }
    let onResult = this._executeResultCallback.bind(this, resultCallback);

    let options = {
      frame: this.SELECTED_FRAME,
      selectedNodeActor: selectedNodeActor,
    };

    this.requestEvaluation(executeString, options).then(onResult, onResult);

    // Append a new value in the history of executed code, or overwrite the most
    // recent entry. The most recent entry may contain the last edited input
    // value that was not evaluated yet.
    this.history[this.historyIndex++] = executeString;
    this.historyPlaceHolder = this.history.length;

    if (this.history.length > this.inputHistoryCount) {
      this.history.splice(0, this.history.length - this.inputHistoryCount);
      this.historyIndex = this.historyPlaceHolder = this.history.length;
    }
    this.storeHistory();
    WebConsoleUtils.usageCount++;
    this.setInputValue("");
    this.clearCompletion();
    return deferred.promise;
  },

  /**
   * Request a JavaScript string evaluation from the server.
   *
   * @param string str
   *        String to execute.
   * @param object [options]
   *        Options for evaluation:
   *        - bindObjectActor: tells the ObjectActor ID for which you want to do
   *        the evaluation. The Debugger.Object of the OA will be bound to
   *        |_self| during evaluation, such that it's usable in the string you
   *        execute.
   *        - frame: tells the stackframe depth to evaluate the string in. If
   *        the jsdebugger is paused, you can pick the stackframe to be used for
   *        evaluation. Use |this.SELECTED_FRAME| to always pick the
   *        user-selected stackframe.
   *        If you do not provide a |frame| the string will be evaluated in the
   *        global content window.
   *        - selectedNodeActor: tells the NodeActor ID of the current selection
   *        in the Inspector, if such a selection exists. This is used by
   *        helper functions that can evaluate on the current selection.
   * @return object
   *         A promise object that is resolved when the server response is
   *         received.
   */
  requestEvaluation: function (str, options = {}) {
    let deferred = promise.defer();

    function onResult(response) {
      if (!response.error) {
        deferred.resolve(response);
      } else {
        deferred.reject(response);
      }
    }

    let frameActor = null;
    if ("frame" in options) {
      frameActor = this.getFrameActor(options.frame);
    }

    let evalOptions = {
      bindObjectActor: options.bindObjectActor,
      frameActor: frameActor,
      selectedNodeActor: options.selectedNodeActor,
      selectedObjectActor: options.selectedObjectActor,
    };

    this.webConsoleClient.evaluateJSAsync(str, onResult, evalOptions);
    return deferred.promise;
  },

  /**
   * Retrieve the FrameActor ID given a frame depth.
   *
   * @param number frame
   *        Frame depth.
   * @return string|null
   *         The FrameActor ID for the given frame depth.
   */
  getFrameActor: function (frame) {
    let state = this.hud.owner.getDebuggerFrames();
    if (!state) {
      return null;
    }

    let grip;
    if (frame == this.SELECTED_FRAME) {
      grip = state.frames[state.selected];
    } else {
      grip = state.frames[frame];
    }

    return grip ? grip.actor : null;
  },

  /**
   * Opens a new variables view that allows the inspection of the given object.
   *
   * @param object options
   *        Options for the variables view:
   *        - objectActor: grip of the ObjectActor you want to show in the
   *        variables view.
   *        - rawObject: the raw object you want to show in the variables view.
   *        - label: label to display in the variables view for inspected
   *        object.
   *        - hideFilterInput: optional boolean, |true| if you want to hide the
   *        variables view filter input.
   *        - targetElement: optional nsIDOMElement to append the variables view
   *        to. An iframe element is used as a container for the view. If this
   *        option is not used, then the variables view opens in the sidebar.
   *        - autofocus: optional boolean, |true| if you want to give focus to
   *        the variables view window after open, |false| otherwise.
   * @return object
   *         A promise object that is resolved when the variables view has
   *         opened. The new variables view instance is given to the callbacks.
   */
  openVariablesView: function (options) {
    let onContainerReady = (window) => {
      let container = window.document.querySelector("#variables");
      let view = this._variablesView;
      if (!view || options.targetElement) {
        let viewOptions = {
          container: container,
          hideFilterInput: options.hideFilterInput,
        };
        view = this._createVariablesView(viewOptions);
        if (!options.targetElement) {
          this._variablesView = view;
          window.addEventListener("keypress", this._onKeypressInVariablesView);
        }
      }
      options.view = view;
      this._updateVariablesView(options);

      if (!options.targetElement && options.autofocus) {
        window.focus();
      }

      this.emit("variablesview-open", view, options);
      return view;
    };

    let openPromise;
    if (options.targetElement) {
      let deferred = promise.defer();
      openPromise = deferred.promise;
      let document = options.targetElement.ownerDocument;
      let iframe = document.createElementNS(XHTML_NS, "iframe");

      iframe.addEventListener("load", function () {
        iframe.style.visibility = "visible";
        deferred.resolve(iframe.contentWindow);
      }, {capture: true, once: true});

      iframe.flex = 1;
      iframe.style.visibility = "hidden";
      iframe.setAttribute("src", VARIABLES_VIEW_URL);
      options.targetElement.appendChild(iframe);
    } else {
      if (!this.sidebar) {
        this._createSidebar();
      }
      openPromise = this._addVariablesViewSidebarTab();
    }

    return openPromise.then(onContainerReady);
  },

  /**
   * Create the Web Console sidebar.
   *
   * @see devtools/framework/sidebar.js
   * @private
   */
  _createSidebar: function () {
    let tabbox = this.hud.document.querySelector("#webconsole-sidebar");
    this.sidebar = new ToolSidebar(tabbox, this, "webconsole");
    this.sidebar.show();
    this.emit("sidebar-opened");
  },

  /**
   * Add the variables view tab to the sidebar.
   *
   * @private
   * @return object
   *         A promise object for the adding of the new tab.
   */
  _addVariablesViewSidebarTab: function () {
    let deferred = promise.defer();

    let onTabReady = () => {
      let window = this.sidebar.getWindowForTab("variablesview");
      deferred.resolve(window);
    };

    let tabPanel = this.sidebar.getTabPanel("variablesview");
    if (tabPanel) {
      if (this.sidebar.getCurrentTabID() == "variablesview") {
        onTabReady();
      } else {
        this.sidebar.once("variablesview-selected", onTabReady);
        this.sidebar.select("variablesview");
      }
    } else {
      this.sidebar.once("variablesview-ready", onTabReady);
      this.sidebar.addTab("variablesview", VARIABLES_VIEW_URL, {selected: true});
    }

    return deferred.promise;
  },

  /**
   * The keypress event handler for the Variables View sidebar. Currently this
   * is used for removing the sidebar when Escape is pressed.
   *
   * @private
   * @param nsIDOMEvent event
   *        The keypress DOM event object.
   */
  _onKeypressInVariablesView: function (event) {
    let tag = event.target.nodeName;
    if (event.keyCode != KeyCodes.DOM_VK_ESCAPE || event.shiftKey ||
        event.altKey || event.ctrlKey || event.metaKey ||
        ["input", "textarea", "select", "textbox"].indexOf(tag) > -1) {
      return;
    }

    this._sidebarDestroy();
    this.focus();
    event.stopPropagation();
  },

  /**
   * Create a variables view instance.
   *
   * @private
   * @param object options
   *        Options for the new Variables View instance:
   *        - container: the DOM element where the variables view is inserted.
   *        - hideFilterInput: boolean, if true the variables filter input is
   *        hidden.
   * @return object
   *         The new Variables View instance.
   */
  _createVariablesView: function (options) {
    let view = new VariablesView(options.container);
    view.toolbox = gDevTools.getToolbox(this.hud.owner.target);
    view.searchPlaceholder = l10n.getStr("propertiesFilterPlaceholder");
    view.emptyText = l10n.getStr("emptyPropertiesList");
    view.searchEnabled = !options.hideFilterInput;
    view.lazyEmpty = this._lazyVariablesView;

    VariablesViewController.attach(view, {
      getEnvironmentClient: grip => {
        return new EnvironmentClient(this.hud.proxy.client, grip);
      },
      getObjectClient: grip => {
        return new ObjectClient(this.hud.proxy.client, grip);
      },
      getLongStringClient: grip => {
        return this.webConsoleClient.longString(grip);
      },
      releaseActor: actor => {
        this.hud._releaseObject(actor);
      },
      simpleValueEvalMacro: simpleValueEvalMacro,
      overrideValueEvalMacro: overrideValueEvalMacro,
      getterOrSetterEvalMacro: getterOrSetterEvalMacro,
    });

    // Relay events from the VariablesView.
    view.on("fetched", (event, type, variableObject) => {
      this.emit("variablesview-fetched", variableObject);
    });

    return view;
  },

  /**
   * Update the variables view.
   *
   * @private
   * @param object options
   *        Options for updating the variables view:
   *        - view: the view you want to update.
   *        - objectActor: the grip of the new ObjectActor you want to show in
   *        the view.
   *        - rawObject: the new raw object you want to show.
   *        - label: the new label for the inspected object.
   */
  _updateVariablesView: function (options) {
    let view = options.view;
    view.empty();

    // We need to avoid pruning the object inspection starting point.
    // That one is pruned when the console message is removed.
    view.controller.releaseActors(actor => {
      return view._consoleLastObjectActor != actor;
    });

    if (options.objectActor &&
        (!this.hud.isBrowserConsole ||
         Services.prefs.getBoolPref("devtools.chrome.enabled"))) {
      // Make sure eval works in the correct context.
      view.eval = this._variablesViewEvaluate.bind(this, options);
      view.switch = this._variablesViewSwitch.bind(this, options);
      view.delete = this._variablesViewDelete.bind(this, options);
    } else {
      view.eval = null;
      view.switch = null;
      view.delete = null;
    }

    let { variable, expanded } = view.controller.setSingleVariable(options);
    variable.evaluationMacro = simpleValueEvalMacro;

    if (options.objectActor) {
      view._consoleLastObjectActor = options.objectActor.actor;
    } else if (options.rawObject) {
      view._consoleLastObjectActor = null;
    } else {
      throw new Error(
        "Variables View cannot open without giving it an object display.");
    }

    expanded.then(() => {
      this.emit("variablesview-updated", view, options);
    });
  },

  /**
   * The evaluation function used by the variables view when editing a property
   * value.
   *
   * @private
   * @param object options
   *        The options used for |this._updateVariablesView()|.
   * @param object variableObject
   *        The Variable object instance for the edited property.
   * @param string value
   *        The value the edited property was changed to.
   */
  _variablesViewEvaluate: function (options, variableObject, value) {
    let updater = this._updateVariablesView.bind(this, options);
    let onEval = this._silentEvalCallback.bind(this, updater);
    let string = variableObject.evaluationMacro(variableObject, value);

    let evalOptions = {
      frame: this.SELECTED_FRAME,
      bindObjectActor: options.objectActor.actor,
    };

    this.requestEvaluation(string, evalOptions).then(onEval, onEval);
  },

  /**
   * The property deletion function used by the variables view when a property
   * is deleted.
   *
   * @private
   * @param object options
   *        The options used for |this._updateVariablesView()|.
   * @param object variableObject
   *        The Variable object instance for the deleted property.
   */
  _variablesViewDelete: function (options, variableObject) {
    let onEval = this._silentEvalCallback.bind(this, null);

    let evalOptions = {
      frame: this.SELECTED_FRAME,
      bindObjectActor: options.objectActor.actor,
    };

    this.requestEvaluation("delete _self" +
      variableObject.symbolicName, evalOptions).then(onEval, onEval);
  },

  /**
   * The property rename function used by the variables view when a property
   * is renamed.
   *
   * @private
   * @param object options
   *        The options used for |this._updateVariablesView()|.
   * @param object variableObject
   *        The Variable object instance for the renamed property.
   * @param string newName
   *        The new name for the property.
   */
  _variablesViewSwitch: function (options, variableObject, newName) {
    let updater = this._updateVariablesView.bind(this, options);
    let onEval = this._silentEvalCallback.bind(this, updater);

    let evalOptions = {
      frame: this.SELECTED_FRAME,
      bindObjectActor: options.objectActor.actor,
    };

    let newSymbolicName =
      variableObject.ownerView.symbolicName + '["' + newName + '"]';
    if (newSymbolicName == variableObject.symbolicName) {
      return;
    }

    let code = "_self" + newSymbolicName + " = _self" +
      variableObject.symbolicName + ";" + "delete _self" +
      variableObject.symbolicName;

    this.requestEvaluation(code, evalOptions).then(onEval, onEval);
  },

  /**
   * A noop callback for JavaScript evaluation. This method releases any
   * result ObjectActors that come from the server for evaluation requests. This
   * is used for editing, renaming and deleting properties in the variables
   * view.
   *
   * Exceptions are displayed in the output.
   *
   * @private
   * @param function callback
   *        Function to invoke once the response is received.
   * @param object response
   *        The response packet received from the server.
   */
  _silentEvalCallback: function (callback, response) {
    if (response.error) {
      console.error("Web Console evaluation failed. " + response.error + ":" +
                    response.message);

      callback && callback(response);
      return;
    }

    if (response.exceptionMessage) {
      let message = new Messages.Simple(response.exceptionMessage, {
        category: "output",
        severity: "error",
        timestamp: response.timestamp,
      });
      this.hud.output.addMessage(message);
      message._objectActors = new Set();
      if (WebConsoleUtils.isActorGrip(response.exception)) {
        message._objectActors.add(response.exception.actor);
      }
    }

    let helper = response.helperResult || { type: null };
    let helperGrip = null;
    if (helper.type == "inspectObject") {
      helperGrip = helper.object;
    }

    let grips = [response.result, helperGrip];
    for (let grip of grips) {
      if (WebConsoleUtils.isActorGrip(grip)) {
        this.hud._releaseObject(grip.actor);
      }
    }

    callback && callback(response);
  },

  /**
   * Clear the Web Console output.
   *
   * This method emits the "messages-cleared" notification.
   *
   * @param boolean clearStorage
   *        True if you want to clear the console messages storage associated to
   *        this Web Console.
   */
  clearOutput: function (clearStorage) {
    let hud = this.hud;
    let outputNode = hud.outputNode;
    let node;
    while ((node = outputNode.firstChild)) {
      hud.removeOutputMessage(node);
    }

    hud.groupDepth = 0;
    hud._outputQueue.forEach(hud._destroyItem, hud);
    hud._outputQueue = [];
    this.webConsoleClient.clearNetworkRequests();
    hud._repeatNodes = {};

    if (clearStorage) {
      this.webConsoleClient.clearMessagesCache();
    }

    this._sidebarDestroy();

    if (hud.NEW_CONSOLE_OUTPUT_ENABLED) {
      hud.newConsoleOutput.dispatchMessagesClear();
    }

    this.emit("messages-cleared");
  },

  /**
   * Remove all of the private messages from the Web Console output.
   *
   * This method emits the "private-messages-cleared" notification.
   */
  clearPrivateMessages: function () {
    let nodes = this.hud.outputNode.querySelectorAll(".message[private]");
    for (let node of nodes) {
      this.hud.removeOutputMessage(node);
    }
    this.emit("private-messages-cleared");
  },

  /**
   * Updates the size of the input field (command line) to fit its contents.
   *
   * @returns void
   */
  resizeInput: function () {
    let inputNode = this.inputNode;

    // Reset the height so that scrollHeight will reflect the natural height of
    // the contents of the input field.
    inputNode.style.height = "auto";

    // Now resize the input field to fit its contents.
    let scrollHeight = inputNode.inputField.scrollHeight;
    if (scrollHeight > 0) {
      inputNode.style.height = scrollHeight + "px";
    }
  },

  /**
   * Sets the value of the input field (command line), and resizes the field to
   * fit its contents. This method is preferred over setting "inputNode.value"
   * directly, because it correctly resizes the field.
   *
   * @param string newValue
   *        The new value to set.
   * @returns void
   */
  setInputValue: function (newValue) {
    this.inputNode.value = newValue;
    this.lastInputValue = newValue;
    this.completeNode.value = "";
    this.resizeInput();
    this._inputChanged = true;
    this.emit("set-input-value");
  },

  /**
   * Gets the value from the input field
   * @returns string
   */
  getInputValue: function () {
    return this.inputNode.value || "";
  },

  /**
   * The inputNode "input" and "keyup" event handler.
   * @private
   */
  _inputEventHandler: function () {
    if (this.lastInputValue != this.getInputValue()) {
      this.resizeInput();
      this.complete(this.COMPLETE_HINT_ONLY);
      this.lastInputValue = this.getInputValue();
      this._inputChanged = true;
    }
  },

  /**
   * The window "blur" event handler.
   * @private
   */
  _blurEventHandler: function () {
    if (this.autocompletePopup) {
      this.clearCompletion();
    }
  },

  /* eslint-disable complexity */
  /**
   * The inputNode "keypress" event handler.
   *
   * @private
   * @param nsIDOMEvent event
   */
  _keyPress: function (event) {
    let inputNode = this.inputNode;
    let inputValue = this.getInputValue();
    let inputUpdated = false;

    if (event.ctrlKey) {
      switch (event.charCode) {
        case 101:
          // control-e
          if (Services.appinfo.OS == "WINNT") {
            break;
          }
          let lineEndPos = inputValue.length;
          if (this.hasMultilineInput()) {
            // find index of closest newline >= cursor
            for (let i = inputNode.selectionEnd; i < lineEndPos; i++) {
              if (inputValue.charAt(i) == "\r" ||
                  inputValue.charAt(i) == "\n") {
                lineEndPos = i;
                break;
              }
            }
          }
          inputNode.setSelectionRange(lineEndPos, lineEndPos);
          event.preventDefault();
          this.clearCompletion();
          break;

        case 110:
          // Control-N differs from down arrow: it ignores autocomplete state.
          // Note that we preserve the default 'down' navigation within
          // multiline text.
          if (Services.appinfo.OS == "Darwin" &&
              this.canCaretGoNext() &&
              this.historyPeruse(HISTORY_FORWARD)) {
            event.preventDefault();
            // Ctrl-N is also used to focus the Network category button on
            // MacOSX. The preventDefault() call doesn't prevent the focus
            // from moving away from the input.
            this.focus();
          }
          this.clearCompletion();
          break;

        case 112:
          // Control-P differs from up arrow: it ignores autocomplete state.
          // Note that we preserve the default 'up' navigation within
          // multiline text.
          if (Services.appinfo.OS == "Darwin" &&
              this.canCaretGoPrevious() &&
              this.historyPeruse(HISTORY_BACK)) {
            event.preventDefault();
            // Ctrl-P may also be used to focus some category button on MacOSX.
            // The preventDefault() call doesn't prevent the focus from moving
            // away from the input.
            this.focus();
          }
          this.clearCompletion();
          break;
        default:
          break;
      }
      return;
    } else if (event.keyCode == KeyCodes.DOM_VK_RETURN) {
      let autoMultiline = Services.prefs.getBoolPref(PREF_AUTO_MULTILINE);
      if (event.shiftKey ||
          (!Debugger.isCompilableUnit(inputNode.value) && autoMultiline)) {
        // shift return or incomplete statement
        return;
      }
    }

    switch (event.keyCode) {
      case KeyCodes.DOM_VK_ESCAPE:
        if (this.autocompletePopup.isOpen) {
          this.clearCompletion();
          event.preventDefault();
          event.stopPropagation();
        } else if (this.sidebar) {
          this._sidebarDestroy();
          event.preventDefault();
          event.stopPropagation();
        }
        break;

      case KeyCodes.DOM_VK_RETURN:
        if (this._autocompletePopupNavigated &&
            this.autocompletePopup.isOpen &&
            this.autocompletePopup.selectedIndex > -1) {
          this.acceptProposedCompletion();
        } else {
          this.execute();
          this._inputChanged = false;
        }
        event.preventDefault();
        break;

      case KeyCodes.DOM_VK_UP:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_BACKWARD);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
        } else if (this.canCaretGoPrevious()) {
          inputUpdated = this.historyPeruse(HISTORY_BACK);
        }
        if (inputUpdated) {
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_DOWN:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_FORWARD);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
        } else if (this.canCaretGoNext()) {
          inputUpdated = this.historyPeruse(HISTORY_FORWARD);
        }
        if (inputUpdated) {
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_PAGE_UP:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_PAGEUP);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
        } else {
          this.hud.outputScroller.scrollTop =
            Math.max(0,
              this.hud.outputScroller.scrollTop -
              this.hud.outputScroller.clientHeight
            );
        }
        event.preventDefault();
        break;

      case KeyCodes.DOM_VK_PAGE_DOWN:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_PAGEDOWN);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
        } else {
          this.hud.outputScroller.scrollTop =
            Math.min(this.hud.outputScroller.scrollHeight,
              this.hud.outputScroller.scrollTop +
              this.hud.outputScroller.clientHeight
            );
        }
        event.preventDefault();
        break;

      case KeyCodes.DOM_VK_HOME:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectedIndex = 0;
          event.preventDefault();
        } else if (inputValue.length <= 0) {
          this.hud.outputScroller.scrollTop = 0;
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_END:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectedIndex =
            this.autocompletePopup.itemCount - 1;
          event.preventDefault();
        } else if (inputValue.length <= 0) {
          this.hud.outputScroller.scrollTop =
            this.hud.outputScroller.scrollHeight;
          event.preventDefault();
        }
        break;

      case KeyCodes.DOM_VK_LEFT:
        if (this.autocompletePopup.isOpen || this.lastCompletion.value) {
          this.clearCompletion();
        }
        break;

      case KeyCodes.DOM_VK_RIGHT:
        let cursorAtTheEnd = this.inputNode.selectionStart ==
                             this.inputNode.selectionEnd &&
                             this.inputNode.selectionStart ==
                             inputValue.length;
        let haveSuggestion = this.autocompletePopup.isOpen ||
                             this.lastCompletion.value;
        let useCompletion = cursorAtTheEnd || this._autocompletePopupNavigated;
        if (haveSuggestion && useCompletion &&
            this.complete(this.COMPLETE_HINT_ONLY) &&
            this.lastCompletion.value &&
            this.acceptProposedCompletion()) {
          event.preventDefault();
        }
        if (this.autocompletePopup.isOpen) {
          this.clearCompletion();
        }
        break;

      case KeyCodes.DOM_VK_TAB:
        // Generate a completion and accept the first proposed value.
        if (this.complete(this.COMPLETE_HINT_ONLY) &&
            this.lastCompletion &&
            this.acceptProposedCompletion()) {
          event.preventDefault();
        } else if (this._inputChanged) {
          this.updateCompleteNode(l10n.getStr("Autocomplete.blank"));
          event.preventDefault();
        }
        break;
      default:
        break;
    }
  },
  /* eslint-enable complexity */

  /**
   * The inputNode "focus" event handler.
   * @private
   */
  _focusEventHandler: function () {
    this._inputChanged = false;
  },

  /**
   * Go up/down the history stack of input values.
   *
   * @param number direction
   *        History navigation direction: HISTORY_BACK or HISTORY_FORWARD.
   *
   * @returns boolean
   *          True if the input value changed, false otherwise.
   */
  historyPeruse: function (direction) {
    if (!this.history.length) {
      return false;
    }

    // Up Arrow key
    if (direction == HISTORY_BACK) {
      if (this.historyPlaceHolder <= 0) {
        return false;
      }
      let inputVal = this.history[--this.historyPlaceHolder];

      // Save the current input value as the latest entry in history, only if
      // the user is already at the last entry.
      // Note: this code does not store changes to items that are already in
      // history.
      if (this.historyPlaceHolder + 1 == this.historyIndex) {
        this.history[this.historyIndex] = this.getInputValue() || "";
      }

      this.setInputValue(inputVal);
    } else if (direction == HISTORY_FORWARD) {
      // Down Arrow key
      if (this.historyPlaceHolder >= (this.history.length - 1)) {
        return false;
      }

      let inputVal = this.history[++this.historyPlaceHolder];
      this.setInputValue(inputVal);
    } else {
      throw new Error("Invalid argument 0");
    }

    return true;
  },

  /**
   * Test for multiline input.
   *
   * @return boolean
   *         True if CR or LF found in node value; else false.
   */
  hasMultilineInput: function () {
    return /[\r\n]/.test(this.getInputValue());
  },

  /**
   * Check if the caret is at a location that allows selecting the previous item
   * in history when the user presses the Up arrow key.
   *
   * @return boolean
   *         True if the caret is at a location that allows selecting the
   *         previous item in history when the user presses the Up arrow key,
   *         otherwise false.
   */
  canCaretGoPrevious: function () {
    let node = this.inputNode;
    if (node.selectionStart != node.selectionEnd) {
      return false;
    }

    let multiline = /[\r\n]/.test(node.value);
    return node.selectionStart == 0 ? true :
           node.selectionStart == node.value.length && !multiline;
  },

  /**
   * Check if the caret is at a location that allows selecting the next item in
   * history when the user presses the Down arrow key.
   *
   * @return boolean
   *         True if the caret is at a location that allows selecting the next
   *         item in history when the user presses the Down arrow key, otherwise
   *         false.
   */
  canCaretGoNext: function () {
    let node = this.inputNode;
    if (node.selectionStart != node.selectionEnd) {
      return false;
    }

    let multiline = /[\r\n]/.test(node.value);
    return node.selectionStart == node.value.length ? true :
           node.selectionStart == 0 && !multiline;
  },

  /**
   * Completes the current typed text in the inputNode. Completion is performed
   * only if the selection/cursor is at the end of the string. If no completion
   * is found, the current inputNode value and cursor/selection stay.
   *
   * @param int type possible values are
   *    - this.COMPLETE_FORWARD: If there is more than one possible completion
   *          and the input value stayed the same compared to the last time this
   *          function was called, then the next completion of all possible
   *          completions is used. If the value changed, then the first possible
   *          completion is used and the selection is set from the current
   *          cursor position to the end of the completed text.
   *          If there is only one possible completion, then this completion
   *          value is used and the cursor is put at the end of the completion.
   *    - this.COMPLETE_BACKWARD: Same as this.COMPLETE_FORWARD but if the
   *          value stayed the same as the last time the function was called,
   *          then the previous completion of all possible completions is used.
   *    - this.COMPLETE_PAGEUP: Scroll up one page if available or select the
   *          first item.
   *    - this.COMPLETE_PAGEDOWN: Scroll down one page if available or select
   *          the last item.
   *    - this.COMPLETE_HINT_ONLY: If there is more than one possible
   *          completion and the input value stayed the same compared to the
   *          last time this function was called, then the same completion is
   *          used again. If there is only one possible completion, then
   *          the this.getInputValue() is set to this value and the selection
   *          is set from the current cursor position to the end of the
   *          completed text.
   * @param function callback
   *        Optional function invoked when the autocomplete properties are
   *        updated.
   * @returns boolean true if there existed a completion for the current input,
   *          or false otherwise.
   */
  complete: function (type, callback) {
    let inputNode = this.inputNode;
    let inputValue = this.getInputValue();
    let frameActor = this.getFrameActor(this.SELECTED_FRAME);

    // If the inputNode has no value, then don't try to complete on it.
    if (!inputValue) {
      this.clearCompletion();
      callback && callback(this);
      this.emit("autocomplete-updated");
      return false;
    }

    // Only complete if the selection is empty.
    if (inputNode.selectionStart != inputNode.selectionEnd) {
      this.clearCompletion();
      callback && callback(this);
      this.emit("autocomplete-updated");
      return false;
    }

    // Update the completion results.
    if (this.lastCompletion.value != inputValue ||
        frameActor != this._lastFrameActorId) {
      this._updateCompletionResult(type, callback);
      return false;
    }

    let popup = this.autocompletePopup;
    let accepted = false;

    if (type != this.COMPLETE_HINT_ONLY && popup.itemCount == 1) {
      this.acceptProposedCompletion();
      accepted = true;
    } else if (type == this.COMPLETE_BACKWARD) {
      popup.selectPreviousItem();
    } else if (type == this.COMPLETE_FORWARD) {
      popup.selectNextItem();
    } else if (type == this.COMPLETE_PAGEUP) {
      popup.selectPreviousPageItem();
    } else if (type == this.COMPLETE_PAGEDOWN) {
      popup.selectNextPageItem();
    }

    callback && callback(this);
    this.emit("autocomplete-updated");
    return accepted || popup.itemCount > 0;
  },

  /**
   * Update the completion result. This operation is performed asynchronously by
   * fetching updated results from the content process.
   *
   * @private
   * @param int type
   *        Completion type. See this.complete() for details.
   * @param function [callback]
   *        Optional, function to invoke when completion results are received.
   */
  _updateCompletionResult: function (type, callback) {
    let frameActor = this.getFrameActor(this.SELECTED_FRAME);
    if (this.lastCompletion.value == this.getInputValue() &&
        frameActor == this._lastFrameActorId) {
      return;
    }

    let requestId = gSequenceId();
    let cursor = this.inputNode.selectionStart;
    let input = this.getInputValue().substring(0, cursor);
    let cache = this._autocompleteCache;

    // If the current input starts with the previous input, then we already
    // have a list of suggestions and we just need to filter the cached
    // suggestions. When the current input ends with a non-alphanumeric
    // character we ask the server again for suggestions.

    // Check if last character is non-alphanumeric
    if (!/[a-zA-Z0-9]$/.test(input) || frameActor != this._lastFrameActorId) {
      this._autocompleteQuery = null;
      this._autocompleteCache = null;
    }

    if (this._autocompleteQuery && input.startsWith(this._autocompleteQuery)) {
      let filterBy = input;
      // Find the last non-alphanumeric other than _ or $ if it exists.
      let lastNonAlpha = input.match(/[^a-zA-Z0-9_$][a-zA-Z0-9_$]*$/);
      // If input contains non-alphanumerics, use the part after the last one
      // to filter the cache
      if (lastNonAlpha) {
        filterBy = input.substring(input.lastIndexOf(lastNonAlpha) + 1);
      }

      let newList = cache.sort().filter(function (l) {
        return l.startsWith(filterBy);
      });

      this.lastCompletion = {
        requestId: null,
        completionType: type,
        value: null,
      };

      let response = { matches: newList, matchProp: filterBy };
      this._receiveAutocompleteProperties(null, callback, response);
      return;
    }

    this._lastFrameActorId = frameActor;

    this.lastCompletion = {
      requestId: requestId,
      completionType: type,
      value: null,
    };

    let autocompleteCallback =
      this._receiveAutocompleteProperties.bind(this, requestId, callback);

    this.webConsoleClient.autocomplete(
      input, cursor, autocompleteCallback, frameActor);
  },

  /**
   * Handler for the autocompletion results. This method takes
   * the completion result received from the server and updates the UI
   * accordingly.
   *
   * @param number requestId
   *        Request ID.
   * @param function [callback=null]
   *        Optional, function to invoke when the completion result is received.
   * @param object message
   *        The JSON message which holds the completion results received from
   *        the content process.
   */
  _receiveAutocompleteProperties: function (requestId, callback, message) {
    let inputNode = this.inputNode;
    let inputValue = this.getInputValue();
    if (this.lastCompletion.value == inputValue ||
        requestId != this.lastCompletion.requestId) {
      return;
    }
    // Cache whatever came from the server if the last char is
    // alphanumeric or '.'
    let cursor = inputNode.selectionStart;
    let inputUntilCursor = inputValue.substring(0, cursor);

    if (requestId != null && /[a-zA-Z0-9.]$/.test(inputUntilCursor)) {
      this._autocompleteCache = message.matches;
      this._autocompleteQuery = inputUntilCursor;
    }

    let matches = message.matches;
    let lastPart = message.matchProp;
    if (!matches.length) {
      this.clearCompletion();
      callback && callback(this);
      this.emit("autocomplete-updated");
      return;
    }

    let items = matches.reverse().map(function (match) {
      return { preLabel: lastPart, label: match };
    });

    let popup = this.autocompletePopup;
    popup.setItems(items);

    let completionType = this.lastCompletion.completionType;
    this.lastCompletion = {
      value: inputValue,
      matchProp: lastPart,
    };

    if (items.length > 1 && !popup.isOpen) {
      let str = this.getInputValue().substr(0, this.inputNode.selectionStart);
      let offset = str.length - (str.lastIndexOf("\n") + 1) - lastPart.length;
      let x = offset * this.hud._inputCharWidth;
      popup.openPopup(inputNode, x + this.hud._chevronWidth);
      this._autocompletePopupNavigated = false;
    } else if (items.length < 2 && popup.isOpen) {
      popup.hidePopup();
      this._autocompletePopupNavigated = false;
    }

    if (items.length == 1) {
      popup.selectedIndex = 0;
    }

    this.onAutocompleteSelect();

    if (completionType != this.COMPLETE_HINT_ONLY && popup.itemCount == 1) {
      this.acceptProposedCompletion();
    } else if (completionType == this.COMPLETE_BACKWARD) {
      popup.selectPreviousItem();
    } else if (completionType == this.COMPLETE_FORWARD) {
      popup.selectNextItem();
    }

    callback && callback(this);
    this.emit("autocomplete-updated");
  },

  onAutocompleteSelect: function () {
    // Render the suggestion only if the cursor is at the end of the input.
    if (this.inputNode.selectionStart != this.getInputValue().length) {
      return;
    }

    let currentItem = this.autocompletePopup.selectedItem;
    if (currentItem && this.lastCompletion.value) {
      let suffix =
        currentItem.label.substring(this.lastCompletion.matchProp.length);
      this.updateCompleteNode(suffix);
    } else {
      this.updateCompleteNode("");
    }
  },

  /**
   * Clear the current completion information and close the autocomplete popup,
   * if needed.
   */
  clearCompletion: function () {
    this.autocompletePopup.clearItems();
    this.lastCompletion = { value: null };
    this.updateCompleteNode("");
    if (this.autocompletePopup.isOpen) {
      // Trigger a blur/focus of the JSTerm input to force screen readers to read the
      // value again.
      this.inputNode.blur();
      this.autocompletePopup.once("popup-closed", () => {
        this.inputNode.focus();
      });
      this.autocompletePopup.hidePopup();
      this._autocompletePopupNavigated = false;
    }
  },

  /**
   * Accept the proposed input completion.
   *
   * @return boolean
   *         True if there was a selected completion item and the input value
   *         was updated, false otherwise.
   */
  acceptProposedCompletion: function () {
    let updated = false;

    let currentItem = this.autocompletePopup.selectedItem;
    if (currentItem && this.lastCompletion.value) {
      let suffix =
        currentItem.label.substring(this.lastCompletion.matchProp.length);
      let cursor = this.inputNode.selectionStart;
      let value = this.getInputValue();
      this.setInputValue(value.substr(0, cursor) +
        suffix + value.substr(cursor));
      let newCursor = cursor + suffix.length;
      this.inputNode.selectionStart = this.inputNode.selectionEnd = newCursor;
      updated = true;
    }

    this.clearCompletion();

    return updated;
  },

  /**
   * Update the node that displays the currently selected autocomplete proposal.
   *
   * @param string suffix
   *        The proposed suffix for the inputNode value.
   */
  updateCompleteNode: function (suffix) {
    // completion prefix = input, with non-control chars replaced by spaces
    let prefix = suffix ? this.getInputValue().replace(/[\S]/g, " ") : "";
    this.completeNode.value = prefix + suffix;
  },

  /**
   * Destroy the sidebar.
   * @private
   */
  _sidebarDestroy: function () {
    if (this._variablesView) {
      this._variablesView.controller.releaseActors();
      this._variablesView = null;
    }

    if (this.sidebar) {
      this.sidebar.hide();
      this.sidebar.destroy();
      this.sidebar = null;
    }

    this.emit("sidebar-closed");
  },

  /**
   * Destroy the JSTerm object. Call this method to avoid memory leaks.
   */
  destroy: function () {
    this._sidebarDestroy();

    this.clearCompletion();
    this.clearOutput();

    this.autocompletePopup.destroy();
    this.autocompletePopup = null;

    if (this._onPaste) {
      this.inputNode.removeEventListener("paste", this._onPaste);
      this.inputNode.removeEventListener("drop", this._onPaste);
      this._onPaste = null;
    }

    this.inputNode.removeEventListener("keypress", this._keyPress);
    this.inputNode.removeEventListener("input", this._inputEventHandler);
    this.inputNode.removeEventListener("keyup", this._inputEventHandler);
    this.inputNode.removeEventListener("focus", this._focusEventHandler);
    this.hud.window.removeEventListener("blur", this._blurEventHandler);

    this.hud = null;
  },
};

function gSequenceId() {
  return gSequenceId.n++;
}
gSequenceId.n = 0;
exports.gSequenceId = gSequenceId;

/**
 * @see VariablesView.simpleValueEvalMacro
 */
function simpleValueEvalMacro(item, currentString) {
  return VariablesView.simpleValueEvalMacro(item, currentString, "_self");
}

/**
 * @see VariablesView.overrideValueEvalMacro
 */
function overrideValueEvalMacro(item, currentString) {
  return VariablesView.overrideValueEvalMacro(item, currentString, "_self");
}

/**
 * @see VariablesView.getterOrSetterEvalMacro
 */
function getterOrSetterEvalMacro(item, currentString) {
  return VariablesView.getterOrSetterEvalMacro(item, currentString, "_self");
}

exports.JSTerm = JSTerm;

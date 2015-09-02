/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const SOURCE_SYNTAX_HIGHLIGHT_MAX_FILE_SIZE = 1048576; // 1 MB in bytes
const SOURCE_URL_DEFAULT_MAX_LENGTH = 64; // chars
const STACK_FRAMES_SOURCE_URL_MAX_LENGTH = 15; // chars
const STACK_FRAMES_SOURCE_URL_TRIM_SECTION = "center";
const STACK_FRAMES_SCROLL_DELAY = 100; // ms
const BREAKPOINT_LINE_TOOLTIP_MAX_LENGTH = 1000; // chars
const BREAKPOINT_CONDITIONAL_POPUP_POSITION = "before_start";
const BREAKPOINT_CONDITIONAL_POPUP_OFFSET_X = 7; // px
const BREAKPOINT_CONDITIONAL_POPUP_OFFSET_Y = -3; // px
const BREAKPOINT_SMALL_WINDOW_WIDTH = 850; // px
const RESULTS_PANEL_POPUP_POSITION = "before_end";
const RESULTS_PANEL_MAX_RESULTS = 10;
const FILE_SEARCH_ACTION_MAX_DELAY = 300; // ms
const GLOBAL_SEARCH_EXPAND_MAX_RESULTS = 50;
const GLOBAL_SEARCH_LINE_MAX_LENGTH = 300; // chars
const GLOBAL_SEARCH_ACTION_MAX_DELAY = 1500; // ms
const FUNCTION_SEARCH_ACTION_MAX_DELAY = 400; // ms
const SEARCH_GLOBAL_FLAG = "!";
const SEARCH_FUNCTION_FLAG = "@";
const SEARCH_TOKEN_FLAG = "#";
const SEARCH_LINE_FLAG = ":";
const SEARCH_VARIABLE_FLAG = "*";
const SEARCH_AUTOFILL = [SEARCH_GLOBAL_FLAG, SEARCH_FUNCTION_FLAG, SEARCH_TOKEN_FLAG];
const EDITOR_VARIABLE_HOVER_DELAY = 750; // ms
const EDITOR_VARIABLE_POPUP_POSITION = "topcenter bottomleft";
const TOOLBAR_ORDER_POPUP_POSITION = "topcenter bottomleft";
const RESIZE_REFRESH_RATE = 50; // ms
const PROMISE_DEBUGGER_URL =
  "chrome://browser/content/devtools/promisedebugger/promise-debugger.xhtml";

const createDispatcher = require('devtools/shared/create-dispatcher')();
const stores = require('./content/stores/index');
const dispatcher = createDispatcher(stores);
const waitUntilService = require('devtools/shared/fluxify/waitUntilService');
const services = {
  WAIT_UNTIL: waitUntilService.name
};

const EventListenersView = require('./content/views/event-listeners-view');
const actions = require('./content/stores/event-listeners').actions;

/**
 * Object defining the debugger view components.
 */
let DebuggerView = {
  /**
   * Initializes the debugger view.
   *
   * @return object
   *         A promise that is resolved when the view finishes initializing.
   */
  initialize: function() {
    if (this._startup) {
      return this._startup;
    }

    let deferred = promise.defer();
    this._startup = deferred.promise;

    this._initializePanes();
    this.Toolbar.initialize();
    this.Options.initialize();
    this.Filtering.initialize();
    this.StackFrames.initialize();
    this.StackFramesClassicList.initialize();
    this.Workers.initialize();
    this.Sources.initialize();
    this.VariableBubble.initialize();
    this.WatchExpressions.initialize();
    this.EventListeners.initialize();
    this.GlobalSearch.initialize();
    this._initializeVariablesView();
    this._initializeEditor(deferred.resolve);

    document.title = L10N.getStr("DebuggerWindowTitle");

    return deferred.promise;
  },

  /**
   * Destroys the debugger view.
   *
   * @return object
   *         A promise that is resolved when the view finishes destroying.
   */
  destroy: function() {
    if (this._shutdown) {
      return this._shutdown;
    }

    window.removeEventListener("resize", this._onResize, false);

    let deferred = promise.defer();
    this._shutdown = deferred.promise;

    this.Toolbar.destroy();
    this.Options.destroy();
    this.Filtering.destroy();
    this.StackFrames.destroy();
    this.StackFramesClassicList.destroy();
    this.Sources.destroy();
    this.VariableBubble.destroy();
    this.WatchExpressions.destroy();
    this.EventListeners.destroy();
    this.GlobalSearch.destroy();
    this._destroyPromiseDebugger();
    this._destroyPanes();
    this._destroyEditor(deferred.resolve);

    return deferred.promise;
  },

  /**
   * Initializes the UI for all the displayed panes.
   */
  _initializePanes: function() {
    dumpn("Initializing the DebuggerView panes");

    this._body = document.getElementById("body");
    this._editorDeck = document.getElementById("editor-deck");
    this._workersAndSourcesPane = document.getElementById("workers-and-sources-pane");
    this._instrumentsPane = document.getElementById("instruments-pane");
    this._instrumentsPaneToggleButton = document.getElementById("instruments-pane-toggle");
    this._promisePane = document.getElementById("promise-debugger-pane");

    this.showEditor = this.showEditor.bind(this);
    this.showBlackBoxMessage = this.showBlackBoxMessage.bind(this);
    this.showProgressBar = this.showProgressBar.bind(this);
    this.maybeShowBlackBoxMessage = this.maybeShowBlackBoxMessage.bind(this);

    this._onTabSelect = this._onInstrumentsPaneTabSelect.bind(this);
    this._instrumentsPane.tabpanels.addEventListener("select", this._onTabSelect);

    this._collapsePaneString = L10N.getStr("collapsePanes");
    this._expandPaneString = L10N.getStr("expandPanes");

    this._workersAndSourcesPane.setAttribute("width", Prefs.workersAndSourcesWidth);
    this._instrumentsPane.setAttribute("width", Prefs.instrumentsWidth);
    this.toggleInstrumentsPane({ visible: Prefs.panesVisibleOnStartup });

    this.updateLayoutMode();

    this._onResize = this._onResize.bind(this);
    window.addEventListener("resize", this._onResize, false);
  },

  /**
   * Destroys the UI for all the displayed panes.
   */
  _destroyPanes: function() {
    dumpn("Destroying the DebuggerView panes");

    if (gHostType != "side") {
      Prefs.workersAndSourcesWidth = this._workersAndSourcesPane.getAttribute("width");
      Prefs.instrumentsWidth = this._instrumentsPane.getAttribute("width");
    }

    this._workersAndSourcesPane = null;
    this._instrumentsPane = null;
    this._instrumentsPaneToggleButton = null;
    this._promisePane = null;
  },

  /**
   * Initializes the VariablesView instance and attaches a controller.
   */
  _initializeVariablesView: function() {
    this.Variables = new VariablesView(document.getElementById("variables"), {
      searchPlaceholder: L10N.getStr("emptyVariablesFilterText"),
      emptyText: L10N.getStr("emptyVariablesText"),
      onlyEnumVisible: Prefs.variablesOnlyEnumVisible,
      searchEnabled: Prefs.variablesSearchboxVisible,
      eval: (variable, value) => {
        let string = variable.evaluationMacro(variable, value);
        DebuggerController.StackFrames.evaluate(string);
      },
      lazyEmpty: true
    });

    // Attach the current toolbox to the VView so it can link DOMNodes to
    // the inspector/highlighter
    this.Variables.toolbox = DebuggerController._toolbox;

    // Attach a controller that handles interfacing with the debugger protocol.
    VariablesViewController.attach(this.Variables, {
      getEnvironmentClient: aObject => gThreadClient.environment(aObject),
      getObjectClient: aObject => {
        return gThreadClient.pauseGrip(aObject)
      }
    });

    // Relay events from the VariablesView.
    this.Variables.on("fetched", (aEvent, aType) => {
      switch (aType) {
        case "scopes":
          window.emit(EVENTS.FETCHED_SCOPES);
          break;
        case "variables":
          window.emit(EVENTS.FETCHED_VARIABLES);
          break;
        case "properties":
          window.emit(EVENTS.FETCHED_PROPERTIES);
          break;
      }
    });
  },

  /**
   * Initialie the Promise Debugger instance.
   */
  _initializePromiseDebugger: function() {
    let iframe = this._promiseDebuggerIframe = document.createElement("iframe");
    iframe.setAttribute("flex", 1);

    let onLoad = (event) => {
      iframe.removeEventListener("load", onLoad, true);

      let doc = event.target;
      let win = doc.defaultView;

      win.setPanel(DebuggerController._toolbox);
    };

    iframe.addEventListener("load", onLoad, true);
    iframe.setAttribute("src", PROMISE_DEBUGGER_URL);
    this._promisePane.appendChild(iframe);
  },

  /**
   * Destroy the Promise Debugger instance.
   */
  _destroyPromiseDebugger: function() {
    if (this._promiseDebuggerIframe) {
      this._promiseDebuggerIframe.contentWindow.destroy();

      this._promiseDebuggerIframe.parentNode.removeChild(
        this._promiseDebuggerIframe);

      this._promiseDebuggerIframe = null;
    }
  },

  /**
   * Initializes the Editor instance.
   *
   * @param function aCallback
   *        Called after the editor finishes initializing.
   */
  _initializeEditor: function(aCallback) {
    dumpn("Initializing the DebuggerView editor");

    let extraKeys = {};
    bindKey("_doTokenSearch", "tokenSearchKey");
    bindKey("_doGlobalSearch", "globalSearchKey", { alt: true });
    bindKey("_doFunctionSearch", "functionSearchKey");
    extraKeys[Editor.keyFor("jumpToLine")] = false;
    extraKeys["Esc"] = false;

    function bindKey(func, key, modifiers = {}) {
      key = document.getElementById(key).getAttribute("key");
      let shortcut = Editor.accel(key, modifiers);
      extraKeys[shortcut] = () => DebuggerView.Filtering[func]();
    }

    let gutters = ["breakpoints"];

    this.editor = new Editor({
      mode: Editor.modes.text,
      readOnly: true,
      lineNumbers: true,
      showAnnotationRuler: true,
      gutters: gutters,
      extraKeys: extraKeys,
      contextMenu: "sourceEditorContextMenu",
      enableCodeFolding: false
    });

    this.editor.appendTo(document.getElementById("editor")).then(() => {
      this.editor.extend(DebuggerEditor);
      this._loadingText = L10N.getStr("loadingText");
      this._onEditorLoad(aCallback);
    });

    this.editor.on("gutterClick", (ev, line, button) => {
      // A right-click shouldn't do anything but keep track of where
      // it was clicked.
      if (button == 2) {
        this.clickedLine = line;
      }
      else {
        if (this.editor.hasBreakpoint(line)) {
          this.editor.removeBreakpoint(line);
        } else {
          this.editor.addBreakpoint(line);
        }
      }
    });
  },

  /**
   * The load event handler for the source editor, also executing any necessary
   * post-load operations.
   *
   * @param function aCallback
   *        Called after the editor finishes loading.
   */
  _onEditorLoad: function(aCallback) {
    dumpn("Finished loading the DebuggerView editor");

    DebuggerController.Breakpoints.initialize().then(() => {
      window.emit(EVENTS.EDITOR_LOADED, this.editor);
      aCallback();
    });
  },

  /**
   * Destroys the Editor instance and also executes any necessary
   * post-unload operations.
   *
   * @param function aCallback
   *        Called after the editor finishes destroying.
   */
  _destroyEditor: function(aCallback) {
    dumpn("Destroying the DebuggerView editor");

    DebuggerController.Breakpoints.destroy().then(() => {
      window.emit(EVENTS.EDITOR_UNLOADED, this.editor);
      this.editor.destroy();
      this.editor = null;
      aCallback();
    });
  },

  /**
   * Display the source editor.
   */
  showEditor: function() {
    this._editorDeck.selectedIndex = 0;
  },

  /**
   * Display the black box message.
   */
  showBlackBoxMessage: function() {
    this._editorDeck.selectedIndex = 1;
  },

  /**
   * Display the progress bar.
   */
  showProgressBar: function() {
    this._editorDeck.selectedIndex = 2;
  },

  /**
   * Show or hide the black box message vs. source editor depending on if the
   * selected source is black boxed or not.
   */
  maybeShowBlackBoxMessage: function() {
    let { source } = DebuggerView.Sources.selectedItem.attachment;
    if (gThreadClient.source(source).isBlackBoxed) {
      this.showBlackBoxMessage();
    } else {
      this.showEditor();
    }
  },

  /**
   * Sets the currently displayed text contents in the source editor.
   * This resets the mode and undo stack.
   *
   * @param string aTextContent
   *        The source text content.
   */
  _setEditorText: function(aTextContent = "") {
    this.editor.setMode(Editor.modes.text);
    this.editor.setText(aTextContent);
    this.editor.clearDebugLocation();
    this.editor.clearHistory();
  },

  /**
   * Sets the proper editor mode (JS or HTML) according to the specified
   * content type, or by determining the type from the url or text content.
   *
   * @param string aUrl
   *        The source url.
   * @param string aContentType [optional]
   *        The source content type.
   * @param string aTextContent [optional]
   *        The source text content.
   */
  _setEditorMode: function(aUrl, aContentType = "", aTextContent = "") {
    // Avoid setting the editor mode for very large files.
    // Is this still necessary? See bug 929225.
    if (aTextContent.length >= SOURCE_SYNTAX_HIGHLIGHT_MAX_FILE_SIZE) {
      return void this.editor.setMode(Editor.modes.text);
    }

    // Use JS mode for files with .js and .jsm extensions.
    if (SourceUtils.isJavaScript(aUrl, aContentType)) {
      return void this.editor.setMode(Editor.modes.js);
    }

    // Use HTML mode for files in which the first non whitespace character is
    // &lt;, regardless of extension.
    if (aTextContent.match(/^\s*</)) {
      return void this.editor.setMode(Editor.modes.html);
    }

    // Unknown language, use text.
    this.editor.setMode(Editor.modes.text);
  },

  /**
   * Sets the currently displayed source text in the editor.
   *
   * You should use DebuggerView.updateEditor instead. It updates the current
   * caret and debug location based on a requested url and line.
   *
   * @param object aSource
   *        The source object coming from the active thread.
   * @param object aFlags
   *        Additional options for setting the source. Supported options:
   *          - force: boolean forcing all text to be reshown in the editor
   * @return object
   *         A promise that is resolved after the source text has been set.
   */
  _setEditorSource: function(aSource, aFlags={}) {
    // Avoid setting the same source text in the editor again.
    if (this._editorSource.actor == aSource.actor && !aFlags.force) {
      return this._editorSource.promise;
    }
    let transportType = gClient.localTransport ? "_LOCAL" : "_REMOTE";
    let histogramId = "DEVTOOLS_DEBUGGER_DISPLAY_SOURCE" + transportType + "_MS";
    let histogram = Services.telemetry.getHistogramById(histogramId);
    let startTime = Date.now();

    let deferred = promise.defer();

    this._setEditorText(L10N.getStr("loadingText"));
    this._editorSource = { actor: aSource.actor, promise: deferred.promise };

    DebuggerController.SourceScripts.getText(aSource).then(([, aText, aContentType]) => {
      // Avoid setting an unexpected source. This may happen when switching
      // very fast between sources that haven't been fetched yet.
      if (this._editorSource.actor != aSource.actor) {
        return;
      }

      this._setEditorText(aText);
      this._setEditorMode(aSource.url, aContentType, aText);

      // Synchronize any other components with the currently displayed
      // source.
      DebuggerView.Sources.selectedValue = aSource.actor;
      DebuggerController.Breakpoints.updateEditorBreakpoints();

      histogram.add(Date.now() - startTime);

      // Resolve and notify that a source file was shown.
      window.emit(EVENTS.SOURCE_SHOWN, aSource);
      deferred.resolve([aSource, aText, aContentType]);
    },
    ([, aError]) => {
      let url = aError;
      let msg = L10N.getFormatStr("errorLoadingText2", url);
      this._setEditorText(msg);
      Cu.reportError(msg);
      dumpn(msg);

      // Reject and notify that there was an error showing the source file.
      window.emit(EVENTS.SOURCE_ERROR_SHOWN, aSource);
      deferred.reject([aSource, aError]);
    });

    return deferred.promise;
  },

  /**
   * Update the source editor's current caret and debug location based on
   * a requested url and line.
   *
   * @param string aActor
   *        The target actor id.
   * @param number aLine [optional]
   *        The target line in the source.
   * @param object aFlags [optional]
   *        Additional options for showing the source. Supported options:
   *          - charOffset: character offset for the caret or debug location
   *          - lineOffset: line offset for the caret or debug location
   *          - columnOffset: column offset for the caret or debug location
   *          - noCaret: don't set the caret location at the specified line
   *          - noDebug: don't set the debug location at the specified line
   *          - align: string specifying whether to align the specified line
   *                   at the "top", "center" or "bottom" of the editor
   *          - force: boolean forcing all text to be reshown in the editor
   * @return object
   *         A promise that is resolved after the source text has been set.
   */
  setEditorLocation: function(aActor, aLine = 0, aFlags = {}) {
    // Avoid trying to set a source for a url that isn't known yet.
    if (!this.Sources.containsValue(aActor)) {
      return promise.reject(new Error("Unknown source for the specified URL."));
    }

    // If the line is not specified, default to the current frame's position,
    // if available and the frame's url corresponds to the requested url.
    if (!aLine) {
      let cachedFrames = DebuggerController.activeThread.cachedFrames;
      let currentDepth = DebuggerController.StackFrames.currentFrameDepth;
      let frame = cachedFrames[currentDepth];
      if (frame && frame.source.actor == aActor) {
        aLine = frame.where.line;
      }
    }

    let sourceItem = this.Sources.getItemByValue(aActor);
    let sourceForm = sourceItem.attachment.source;

    this._editorLoc = { actor: sourceForm.actor };

    // Make sure the requested source client is shown in the editor, then
    // update the source editor's caret position and debug location.
    return this._setEditorSource(sourceForm, aFlags).then(([,, aContentType]) => {
      if (this._editorLoc.actor !== sourceForm.actor) {
        return;
      }

      // Record the contentType learned from fetching
      sourceForm.contentType = aContentType;
      // Line numbers in the source editor should start from 1. If invalid
      // or not specified, then don't do anything.
      if (aLine < 1) {
        window.emit(EVENTS.EDITOR_LOCATION_SET);
        return;
      }
      if (aFlags.charOffset) {
        aLine += this.editor.getPosition(aFlags.charOffset).line;
      }
      if (aFlags.lineOffset) {
        aLine += aFlags.lineOffset;
      }
      if (!aFlags.noCaret) {
        let location = { line: aLine -1, ch: aFlags.columnOffset || 0 };
        this.editor.setCursor(location, aFlags.align);
      }
      if (!aFlags.noDebug) {
        this.editor.setDebugLocation(aLine - 1);
      }
      window.emit(EVENTS.EDITOR_LOCATION_SET);
    }).then(null, console.error);
  },

  /**
   * Gets the visibility state of the instruments pane.
   * @return boolean
   */
  get instrumentsPaneHidden() {
    return this._instrumentsPane.hasAttribute("pane-collapsed");
  },

  /**
   * Gets the currently selected tab in the instruments pane.
   * @return string
   */
  get instrumentsPaneTab() {
    return this._instrumentsPane.selectedTab.id;
  },

  /**
   * Sets the instruments pane hidden or visible.
   *
   * @param object aFlags
   *        An object containing some of the following properties:
   *        - visible: true if the pane should be shown, false to hide
   *        - animated: true to display an animation on toggle
   *        - delayed: true to wait a few cycles before toggle
   *        - callback: a function to invoke when the toggle finishes
   * @param number aTabIndex [optional]
   *        The index of the intended selected tab in the details pane.
   */
  toggleInstrumentsPane: function(aFlags, aTabIndex) {
    let pane = this._instrumentsPane;
    let button = this._instrumentsPaneToggleButton;

    ViewHelpers.togglePane(aFlags, pane);

    if (aFlags.visible) {
      button.removeAttribute("pane-collapsed");
      button.setAttribute("tooltiptext", this._collapsePaneString);
    } else {
      button.setAttribute("pane-collapsed", "");
      button.setAttribute("tooltiptext", this._expandPaneString);
    }

    if (aTabIndex !== undefined) {
      pane.selectedIndex = aTabIndex;
    }
  },

  /**
   * Sets the instruments pane visible after a short period of time.
   *
   * @param function aCallback
   *        A function to invoke when the toggle finishes.
   */
  showInstrumentsPane: function(aCallback) {
    DebuggerView.toggleInstrumentsPane({
      visible: true,
      animated: true,
      delayed: true,
      callback: aCallback
    }, 0);
  },

  /**
   * Handles a tab selection event on the instruments pane.
   */
  _onInstrumentsPaneTabSelect: function() {
    if (this._instrumentsPane.selectedTab.id == "events-tab") {
      dispatcher.dispatch(actions.fetchEventListeners());
    }
  },

  /**
   * Handles a host change event issued by the parent toolbox.
   *
   * @param string aType
   *        The host type, either "bottom", "side" or "window".
   */
  handleHostChanged: function(hostType) {
    this._hostType = hostType;
    this.updateLayoutMode();
  },

  /**
   * Resize handler for this container's window.
   */
  _onResize: function (evt) {
    // Allow requests to settle down first.
    setNamedTimeout(
      "resize-events", RESIZE_REFRESH_RATE, () => this.updateLayoutMode());
  },

  /**
   * Set the layout to "vertical" or "horizontal" depending on the host type.
   */
  updateLayoutMode: function() {
    if (this._isSmallWindowHost() || this._hostType == "side") {
      this._setLayoutMode("vertical");
    } else {
      this._setLayoutMode("horizontal");
    }
  },

  /**
   * Check if the current host is in window mode and is
   * too small for horizontal layout
   */
  _isSmallWindowHost: function() {
    if (this._hostType != "window") {
      return false;
    }

    return window.outerWidth <= BREAKPOINT_SMALL_WINDOW_WIDTH;
  },

  /**
   * Enter the provided layoutMode. Do nothing if the layout is the same as the current one.
   * @param {String} layoutMode new layout ("vertical" or "horizontal")
   */
  _setLayoutMode: function(layoutMode) {
    if (this._body.getAttribute("layout") == layoutMode) {
      return;
    }

    if (layoutMode == "vertical") {
      this._enterVerticalLayout();
    } else {
      this._enterHorizontalLayout();
    }

    this._body.setAttribute("layout", layoutMode);
    window.emit(EVENTS.LAYOUT_CHANGED, layoutMode);
  },

  /**
   * Switches the debugger widgets to a vertical layout.
   */
  _enterVerticalLayout: function() {
    let vertContainer = document.getElementById("vertical-layout-panes-container");

    // Move the soruces and instruments panes in a different container.
    let splitter = document.getElementById("sources-and-instruments-splitter");
    vertContainer.insertBefore(this._workersAndSourcesPane, splitter);
    vertContainer.appendChild(this._instrumentsPane);

    // Make sure the vertical layout container's height doesn't repeatedly
    // grow or shrink based on the displayed sources, variables etc.
    vertContainer.setAttribute("height",
      vertContainer.getBoundingClientRect().height);
  },

  /**
   * Switches the debugger widgets to a horizontal layout.
   */
  _enterHorizontalLayout: function() {
    let normContainer = document.getElementById("debugger-widgets");
    let editorPane = document.getElementById("editor-and-instruments-pane");

    // The sources and instruments pane need to be inserted at their
    // previous locations in their normal container.
    let splitter = document.getElementById("sources-and-editor-splitter");
    normContainer.insertBefore(this._workersAndSourcesPane, splitter);
    editorPane.appendChild(this._instrumentsPane);

    // Revert to the preferred sources and instruments widths, because
    // they flexed in the vertical layout.
    this._workersAndSourcesPane.setAttribute("width", Prefs.workersAndSourcesWidth);
    this._instrumentsPane.setAttribute("width", Prefs.instrumentsWidth);
  },

  /**
   * Handles any initialization on a tab navigation event issued by the client.
   */
  handleTabNavigation: function() {
    dumpn("Handling tab navigation in the DebuggerView");

    this.Filtering.clearSearch();
    this.GlobalSearch.clearView();
    this.StackFrames.empty();
    this.Sources.empty();
    this.Variables.empty();
    this.EventListeners.empty();

    if (this.editor) {
      this.editor.setMode(Editor.modes.text);
      this.editor.setText("");
      this.editor.clearHistory();
      this._editorSource = {};
    }

    this.Sources.emptyText = L10N.getStr("loadingSourcesText");
  },

  _startup: null,
  _shutdown: null,
  Toolbar: null,
  Options: null,
  Filtering: null,
  GlobalSearch: null,
  StackFrames: null,
  Sources: null,
  Variables: null,
  VariableBubble: null,
  WatchExpressions: null,
  EventListeners: null,
  editor: null,
  _editorSource: {},
  _loadingText: "",
  _body: null,
  _editorDeck: null,
  _workersAndSourcesPane: null,
  _instrumentsPane: null,
  _instrumentsPaneToggleButton: null,
  _collapsePaneString: "",
  _expandPaneString: ""
};

/**
 * A custom items container, used for displaying views like the
 * FilteredSources, FilteredFunctions etc., inheriting the generic WidgetMethods.
 */
function ResultsPanelContainer() {
}

ResultsPanelContainer.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Sets the anchor node for this container panel.
   * @param nsIDOMNode aNode
   */
  set anchor(aNode) {
    this._anchor = aNode;

    // If the anchor node is not null, create a panel to attach to the anchor
    // when showing the popup.
    if (aNode) {
      if (!this._panel) {
        this._panel = document.createElement("panel");
        this._panel.id = "results-panel";
        this._panel.setAttribute("level", "top");
        this._panel.setAttribute("noautofocus", "true");
        this._panel.setAttribute("consumeoutsideclicks", "false");
        document.documentElement.appendChild(this._panel);
      }
      if (!this.widget) {
        this.widget = new SimpleListWidget(this._panel);
        this.autoFocusOnFirstItem = false;
        this.autoFocusOnSelection = false;
        this.maintainSelectionVisible = false;
      }
    }
    // Cleanup the anchor and remove the previously created panel.
    else {
      this._panel.remove();
      this._panel = null;
      this.widget = null;
    }
  },

  /**
   * Gets the anchor node for this container panel.
   * @return nsIDOMNode
   */
  get anchor() {
    return this._anchor;
  },

  /**
   * Sets the container panel hidden or visible. It's hidden by default.
   * @param boolean aFlag
   */
  set hidden(aFlag) {
    if (aFlag) {
      this._panel.hidden = true;
      this._panel.hidePopup();
    } else {
      this._panel.hidden = false;
      this._panel.openPopup(this._anchor, this.position, this.left, this.top);
    }
  },

  /**
   * Gets this container's visibility state.
   * @return boolean
   */
  get hidden() {
    return this._panel.state == "closed" ||
           this._panel.state == "hiding";
  },

  /**
   * Removes all items from this container and hides it.
   */
  clearView: function() {
    this.hidden = true;
    this.empty();
  },

  /**
   * Selects the next found item in this container.
   * Does not change the currently focused node.
   */
  selectNext: function() {
    let nextIndex = this.selectedIndex + 1;
    if (nextIndex >= this.itemCount) {
      nextIndex = 0;
    }
    this.selectedItem = this.getItemAtIndex(nextIndex);
  },

  /**
   * Selects the previously found item in this container.
   * Does not change the currently focused node.
   */
  selectPrev: function() {
    let prevIndex = this.selectedIndex - 1;
    if (prevIndex < 0) {
      prevIndex = this.itemCount - 1;
    }
    this.selectedItem = this.getItemAtIndex(prevIndex);
  },

  /**
   * Customization function for creating an item's UI.
   *
   * @param string aLabel
   *        The item's label string.
   * @param string aBeforeLabel
   *        An optional string shown before the label.
   * @param string aBelowLabel
   *        An optional string shown underneath the label.
   */
  _createItemView: function(aLabel, aBelowLabel, aBeforeLabel) {
    let container = document.createElement("vbox");
    container.className = "results-panel-item";

    let firstRowLabels = document.createElement("hbox");
    let secondRowLabels = document.createElement("hbox");

    if (aBeforeLabel) {
      let beforeLabelNode = document.createElement("label");
      beforeLabelNode.className = "plain results-panel-item-label-before";
      beforeLabelNode.setAttribute("value", aBeforeLabel);
      firstRowLabels.appendChild(beforeLabelNode);
    }

    let labelNode = document.createElement("label");
    labelNode.className = "plain results-panel-item-label";
    labelNode.setAttribute("value", aLabel);
    firstRowLabels.appendChild(labelNode);

    if (aBelowLabel) {
      let belowLabelNode = document.createElement("label");
      belowLabelNode.className = "plain results-panel-item-label-below";
      belowLabelNode.setAttribute("value", aBelowLabel);
      secondRowLabels.appendChild(belowLabelNode);
    }

    container.appendChild(firstRowLabels);
    container.appendChild(secondRowLabels);

    return container;
  },

  _anchor: null,
  _panel: null,
  position: RESULTS_PANEL_POPUP_POSITION,
  left: 0,
  top: 0
});

DebuggerView.EventListeners = new EventListenersView(dispatcher, DebuggerController);

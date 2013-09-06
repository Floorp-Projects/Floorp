/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const SOURCE_URL_DEFAULT_MAX_LENGTH = 64; // chars
const SOURCE_SYNTAX_HIGHLIGHT_MAX_FILE_SIZE = 1048576; // 1 MB in bytes
const STACK_FRAMES_SOURCE_URL_MAX_LENGTH = 15; // chars
const STACK_FRAMES_SOURCE_URL_TRIM_SECTION = "center";
const STACK_FRAMES_POPUP_SOURCE_URL_MAX_LENGTH = 32; // chars
const STACK_FRAMES_POPUP_SOURCE_URL_TRIM_SECTION = "center";
const STACK_FRAMES_SCROLL_DELAY = 100; // ms
const BREAKPOINT_LINE_TOOLTIP_MAX_LENGTH = 1000; // chars
const BREAKPOINT_CONDITIONAL_POPUP_POSITION = "before_start";
const BREAKPOINT_CONDITIONAL_POPUP_OFFSET_X = 7; // px
const BREAKPOINT_CONDITIONAL_POPUP_OFFSET_Y = -3; // px
const RESULTS_PANEL_POPUP_POSITION = "before_end";
const RESULTS_PANEL_MAX_RESULTS = 10;
const GLOBAL_SEARCH_EXPAND_MAX_RESULTS = 50;
const GLOBAL_SEARCH_LINE_MAX_LENGTH = 300; // chars
const GLOBAL_SEARCH_ACTION_MAX_DELAY = 1500; // ms
const FUNCTION_SEARCH_ACTION_MAX_DELAY = 400; // ms
const SEARCH_GLOBAL_FLAG = "!";
const SEARCH_FUNCTION_FLAG = "@";
const SEARCH_TOKEN_FLAG = "#";
const SEARCH_LINE_FLAG = ":";
const SEARCH_VARIABLE_FLAG = "*";

Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm");

/**
 * Object defining the debugger view components.
 */
let DebuggerView = {
  /**
   * Initializes the debugger view.
   *
   * @param function aCallback
   *        Called after the view finishes initializing.
   */
  initialize: function(aCallback) {
    dumpn("Initializing the DebuggerView");

    this._initializePanes();

    this.Toolbar.initialize();
    this.Options.initialize();
    this.Filtering.initialize();
    this.FilteredSources.initialize();
    this.FilteredFunctions.initialize();
    this.ChromeGlobals.initialize();
    this.StackFrames.initialize();
    this.Sources.initialize();
    this.WatchExpressions.initialize();
    this.GlobalSearch.initialize();

    this._initializeVariablesView();
    this._initializeEditor(aCallback);
  },

  /**
   * Destroys the debugger view.
   *
   * @param function aCallback
   *        Called after the view finishes destroying.
   */
  destroy: function(aCallback) {
    dumpn("Destroying the DebuggerView");

    this.Toolbar.destroy();
    this.Options.destroy();
    this.Filtering.destroy();
    this.FilteredSources.destroy();
    this.FilteredFunctions.destroy();
    this.ChromeGlobals.destroy();
    this.StackFrames.destroy();
    this.Sources.destroy();
    this.WatchExpressions.destroy();
    this.GlobalSearch.destroy();

    this._destroyPanes();
    this._destroyEditor();

    aCallback();
  },

  /**
   * Initializes the UI for all the displayed panes.
   */
  _initializePanes: function() {
    dumpn("Initializing the DebuggerView panes");

    this._sourcesPane = document.getElementById("sources-pane");
    this._instrumentsPane = document.getElementById("instruments-pane");
    this._instrumentsPaneToggleButton = document.getElementById("instruments-pane-toggle");

    this._collapsePaneString = L10N.getStr("collapsePanes");
    this._expandPaneString = L10N.getStr("expandPanes");

    this._sourcesPane.setAttribute("width", Prefs.sourcesWidth);
    this._instrumentsPane.setAttribute("width", Prefs.instrumentsWidth);
    this.toggleInstrumentsPane({ visible: Prefs.panesVisibleOnStartup });
  },

  /**
   * Destroys the UI for all the displayed panes.
   */
  _destroyPanes: function() {
    dumpn("Destroying the DebuggerView panes");

    Prefs.sourcesWidth = this._sourcesPane.getAttribute("width");
    Prefs.instrumentsWidth = this._instrumentsPane.getAttribute("width");

    this._sourcesPane = null;
    this._instrumentsPane = null;
    this._instrumentsPaneToggleButton = null;
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
      eval: DebuggerController.StackFrames.evaluate,
      lazyEmpty: true
    });

    // Attach a controller that handles interfacing with the debugger protocol.
    VariablesViewController.attach(this.Variables, {
      getObjectClient: aObject => gThreadClient.pauseGrip(aObject)
    });

    // Relay events from the VariablesView.
    this.Variables.on("fetched", (aEvent, aType) => {
      switch (aType) {
        case "variables":
          window.dispatchEvent(document, "Debugger:FetchedVariables");
          break;
        case "properties":
          window.dispatchEvent(document, "Debugger:FetchedProperties");
          break;
      }
    });
  },

  /**
   * Initializes the SourceEditor instance.
   *
   * @param function aCallback
   *        Called after the editor finishes initializing.
   */
  _initializeEditor: function(aCallback) {
    dumpn("Initializing the DebuggerView editor");

    let placeholder = document.getElementById("editor");
    let config = {
      mode: SourceEditor.MODES.JAVASCRIPT,
      readOnly: true,
      showLineNumbers: true,
      showAnnotationRuler: true,
      showOverviewRuler: true
    };

    this.editor = new SourceEditor();
    this.editor.init(placeholder, config, () => {
      this._loadingText = L10N.getStr("loadingText");
      this._onEditorLoad();
      aCallback();
    });
  },

  /**
   * The load event handler for the source editor, also executing any necessary
   * post-load operations.
   */
  _onEditorLoad: function() {
    dumpn("Finished loading the DebuggerView editor");

    DebuggerController.Breakpoints.initialize();
    window.dispatchEvent(document, "Debugger:EditorLoaded", this.editor);
    this.editor.focus();
  },

  /**
   * Destroys the SourceEditor instance and also executes any necessary
   * post-unload operations.
   */
  _destroyEditor: function() {
    dumpn("Destroying the DebuggerView editor");

    DebuggerController.Breakpoints.destroy();
    window.dispatchEvent(document, "Debugger:EditorUnloaded", this.editor);
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
  setEditorMode: function(aUrl, aContentType = "", aTextContent = "") {
    // Avoid setting the editor mode for very large files.
    if (aTextContent.length >= SOURCE_SYNTAX_HIGHLIGHT_MAX_FILE_SIZE) {
      this.editor.setMode(SourceEditor.MODES.TEXT);
      return;
    }

    if (aContentType) {
      if (/javascript/.test(aContentType)) {
        this.editor.setMode(SourceEditor.MODES.JAVASCRIPT);
      } else {
        this.editor.setMode(SourceEditor.MODES.HTML);
      }
    } else if (aTextContent.match(/^\s*</)) {
      // Use HTML mode for files in which the first non whitespace character is
      // &lt;, regardless of extension.
      this.editor.setMode(SourceEditor.MODES.HTML);
    } else {
      // Use JS mode for files with .js and .jsm extensions.
      if (/\.jsm?$/.test(SourceUtils.trimUrlQuery(aUrl))) {
        this.editor.setMode(SourceEditor.MODES.JAVASCRIPT);
      } else {
        this.editor.setMode(SourceEditor.MODES.TEXT);
      }
    }
  },

  /**
   * Sets the currently displayed source text in the editor.
   *
   * To update the source editor's current caret and debug location based on
   * a requested url and line, use the DebuggerView.updateEditor method.
   *
   * @param object aSource
   *        The source object coming from the active thread.
   */
  set editorSource(aSource) {
    if (!this._isInitialized || this._isDestroyed || this._editorSource == aSource) {
      return;
    }

    dumpn("Setting the DebuggerView editor source: " + aSource.url +
          ", fetched: " + !!aSource._fetched);

    this.editor.setMode(SourceEditor.MODES.TEXT);
    this.editor.setText(L10N.getStr("loadingText"));
    this.editor.resetUndo();
    this._editorSource = aSource;

    DebuggerController.SourceScripts.getTextForSource(aSource).then(([, aText]) => {
      // Avoid setting an unexpected source. This may happen when fast switching
      // between sources that haven't been fetched yet.
      if (this._editorSource != aSource) {
        return;
      }

      this.editor.setText(aText);
      this.editor.resetUndo();
      this.setEditorMode(aSource.url, aSource.contentType, aText);

      // Update the editor's current caret and debug locations given by the
      // currently active frame in the stack, if there's one available.
      this.updateEditor();

      // Synchronize any other components with the currently displayed source.
      DebuggerView.Sources.selectedValue = aSource.url;
      DebuggerController.Breakpoints.updateEditorBreakpoints();

      // Notify that we've shown a source file.
      window.dispatchEvent(document, "Debugger:SourceShown", aSource);
    },
    ([, aError]) => {
      // Rejected.
      let msg = L10N.getStr("errorLoadingText") + DevToolsUtils.safeErrorString(aError);
      this.editor.setText(msg);
      window.dispatchEvent(document, "Debugger:SourceErrorShown", aError);
      dumpn(msg);
      Cu.reportError(msg);
    });
  },

  /**
   * Gets the currently displayed source text in the editor.
   *
   * @return object
   *         The source object coming from the active thread.
   */
  get editorSource() this._editorSource,

  /**
   * Update the source editor's current caret and debug location based on
   * a requested url and line. If unspecified, they default to the location
   * given by the currently active frame in the stack.
   *
   * @param string aUrl [optional]
   *        The target source url.
   * @param number aLine [optional]
   *        The target line number in the source.
   * @param object aFlags [optional]
   *        Additional options for showing the source. Supported options:
   *          - charOffset: character offset for the caret or debug location
   *          - lineOffset: line offset for the caret or debug location
   *          - columnOffset: column offset for the caret or debug location
   *          - noSwitch: don't switch to the source if not currently selected
   *          - noCaret: don't set the caret location at the specified line
   *          - noDebug: don't set the debug location at the specified line
   */
  updateEditor: function(aUrl, aLine, aFlags = {}) {
    if (!this._isInitialized || this._isDestroyed) {
      return;
    }
    // If the location is not specified, default to the location given by
    // the currently active frame in the stack.
    if (!aUrl && !aLine) {
      let cachedFrames = DebuggerController.activeThread.cachedFrames;
      let currentFrame = DebuggerController.StackFrames.currentFrame;
      let frame = cachedFrames[currentFrame];
      if (frame) {
        let { url, line } = frame.where;
        this.updateEditor(url, line, { noSwitch: true });
      }
      return;
    }

    dumpn("Updating the DebuggerView editor: " + aUrl + " @ " + aLine +
          ", flags: " + aFlags.toSource());

    // If the currently displayed source is the requested one, update.
    if (this.Sources.selectedValue == aUrl) {
      set(aLine);
    }
    // If the requested source exists, display it and update.
    else if (this.Sources.containsValue(aUrl) && !aFlags.noSwitch) {
      this.Sources.selectedValue = aUrl;
      set(aLine);
    }
    // Dumb request, invalidate the caret position and debug location.
    else {
      set(0);
    }

    // Updates the source editor's caret position and debug location.
    // @param number a Line
    function set(aLine) {
      let editor = DebuggerView.editor;

      // Handle any additional options for showing the source.
      if (aFlags.charOffset) {
        aLine += editor.getLineAtOffset(aFlags.charOffset);
      }
      if (aFlags.lineOffset) {
        aLine += aFlags.lineOffset;
      }
      if (!aFlags.noCaret) {
        editor.setCaretPosition(aLine - 1, aFlags.columnOffset);
      }
      if (!aFlags.noDebug) {
        editor.setDebugLocation(aLine - 1, aFlags.columnOffset);
      }
    }
  },

  /**
   * Gets the text in the source editor's specified line.
   *
   * @param number aLine [optional]
   *        The line to get the text from.
   *        If unspecified, it defaults to the current caret position line.
   * @return string
   *         The specified line's text.
   */
  getEditorLineText: function(aLine) {
    let line = aLine || this.editor.getCaretPosition().line;
    let start = this.editor.getLineStart(line);
    let end = this.editor.getLineEnd(line);
    return this.editor.getText(start, end);
  },

  /**
   * Gets the text in the source editor's selection bounds.
   *
   * @return string
   *         The selected text.
   */
  getEditorSelectionText: function() {
    let selection = this.editor.getSelection();
    return this.editor.getText(selection.start, selection.end);
  },

  /**
   * Gets the visibility state of the instruments pane.
   * @return boolean
   */
  get instrumentsPaneHidden()
    this._instrumentsPane.hasAttribute("pane-collapsed"),

  /**
   * Sets the instruments pane hidden or visible.
   *
   * @param object aFlags
   *        An object containing some of the following properties:
   *        - visible: true if the pane should be shown, false to hide
   *        - animated: true to display an animation on toggle
   *        - delayed: true to wait a few cycles before toggle
   *        - callback: a function to invoke when the toggle finishes
   */
  toggleInstrumentsPane: function(aFlags) {
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
    });
  },

  /**
   * Handles any initialization on a tab navigation event issued by the client.
   */
  _handleTabNavigation: function() {
    dumpn("Handling tab navigation in the DebuggerView");

    this.Filtering.clearSearch();
    this.FilteredSources.clearView();
    this.FilteredFunctions.clearView();
    this.GlobalSearch.clearView();
    this.ChromeGlobals.empty();
    this.StackFrames.empty();
    this.Sources.empty();
    this.Variables.empty();

    if (this.editor) {
      this.editor.setText("");
      this.editor.focus();
      this._editorSource = null;
    }
  },

  Toolbar: null,
  Options: null,
  Filtering: null,
  FilteredSources: null,
  FilteredFunctions: null,
  GlobalSearch: null,
  ChromeGlobals: null,
  StackFrames: null,
  Sources: null,
  Variables: null,
  WatchExpressions: null,
  _editor: null,
  _editorSource: null,
  _loadingText: "",
  _sourcesPane: null,
  _instrumentsPane: null,
  _instrumentsPaneToggleButton: null,
  _collapsePaneString: "",
  _expandPaneString: "",
  _isInitialized: false,
  _isDestroyed: false
};

/**
 * A stacked list of items, compatible with WidgetMethods instances, used for
 * displaying views like the watch expressions, filtering or search results etc.
 *
 * You should never need to access these methods directly, use the wrapped
 * WidgetMethods instead.
 *
 * @param nsIDOMNode aNode
 *        The element associated with the widget.
 */
function ListWidget(aNode) {
  this._parent = aNode;

  // Create an internal list container.
  this._list = document.createElement("vbox");
  this._parent.appendChild(this._list);

  // Delegate some of the associated node's methods to satisfy the interface
  // required by WidgetMethods instances.
  ViewHelpers.delegateWidgetAttributeMethods(this, aNode);
  ViewHelpers.delegateWidgetEventMethods(this, aNode);
}

ListWidget.prototype = {
  /**
   * Overrides an item's element type (e.g. "vbox" or "hbox") in this container.
   * @param string aType
   */
  itemType: "hbox",

  /**
   * Customization function for creating an item's UI in this container.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed item.
   * @param string aLabel
   *        The item's label.
   * @param string aValue
   *        The item's value.
   */
  itemFactory: null,

  /**
   * Immediately inserts an item in this container at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @param string aLabel
   *        The label displayed in the container.
   * @param string aValue
   *        The actual internal value of the item.
   * @param string aDescription [optional]
   *        An optional description of the item.
   * @param any aAttachment [optional]
   *        Some attached primitive/object.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  insertItemAt: function(aIndex, aLabel, aValue, aDescription, aAttachment) {
    let list = this._list;
    let childNodes = list.childNodes;

    let element = document.createElement(this.itemType);
    this.itemFactory(element, aAttachment, aLabel, aValue, aDescription);
    this._removeEmptyNotice();

    element.classList.add("list-widget-item");
    return list.insertBefore(element, childNodes[aIndex]);
  },

  /**
   * Returns the child node in this container situated at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @return nsIDOMNode
   *         The element associated with the displayed item.
   */
  getItemAtIndex: function(aIndex) {
    return this._list.childNodes[aIndex];
  },

  /**
   * Immediately removes the specified child node from this container.
   *
   * @param nsIDOMNode aChild
   *        The element associated with the displayed item.
   */
  removeChild: function(aChild) {
    this._list.removeChild(aChild);

    if (this._selectedItem == aChild) {
      this._selectedItem = null;
    }
    if (!this._list.hasChildNodes()) {
      this._appendEmptyNotice();
    }
  },

  /**
   * Immediately removes all of the child nodes from this container.
   */
  removeAllItems: function() {
    let parent = this._parent;
    let list = this._list;

    while (list.hasChildNodes()) {
      list.firstChild.remove();
    }
    parent.scrollTop = 0;
    parent.scrollLeft = 0;

    this._selectedItem = null;
    this._appendEmptyNotice();
  },

  /**
   * Gets the currently selected child node in this container.
   * @return nsIDOMNode
   */
  get selectedItem() this._selectedItem,

  /**
   * Sets the currently selected child node in this container.
   * @param nsIDOMNode aChild
   */
  set selectedItem(aChild) {
    let childNodes = this._list.childNodes;

    if (!aChild) {
      this._selectedItem = null;
    }
    for (let node of childNodes) {
      if (node == aChild) {
        node.classList.add("selected");
        this._selectedItem = node;
      } else {
        node.classList.remove("selected");
      }
    }
  },

  /**
   * Sets the text displayed permanently in this container's header.
   * @param string aValue
   */
  set permaText(aValue) {
    if (this._permaTextNode) {
      this._permaTextNode.setAttribute("value", aValue);
    }
    this._permaTextValue = aValue;
    this._appendPermaNotice();
  },

  /**
   * Sets the text displayed in this container when there are no available items.
   * @param string aValue
   */
  set emptyText(aValue) {
    if (this._emptyTextNode) {
      this._emptyTextNode.setAttribute("value", aValue);
    }
    this._emptyTextValue = aValue;
    this._appendEmptyNotice();
  },

  /**
   * Creates and appends a label displayed permanently in this container's header.
   */
  _appendPermaNotice: function() {
    if (this._permaTextNode || !this._permaTextValue) {
      return;
    }

    let label = document.createElement("label");
    label.className = "empty list-widget-item";
    label.setAttribute("value", this._permaTextValue);

    this._parent.insertBefore(label, this._list);
    this._permaTextNode = label;
  },

  /**
   * Creates and appends a label signaling that this container is empty.
   */
  _appendEmptyNotice: function() {
    if (this._emptyTextNode || !this._emptyTextValue) {
      return;
    }

    let label = document.createElement("label");
    label.className = "empty list-widget-item";
    label.setAttribute("value", this._emptyTextValue);

    this._parent.appendChild(label);
    this._emptyTextNode = label;
  },

  /**
   * Removes the label signaling that this container is empty.
   */
  _removeEmptyNotice: function() {
    if (!this._emptyTextNode) {
      return;
    }

    this._parent.removeChild(this._emptyTextNode);
    this._emptyTextNode = null;
  },

  _parent: null,
  _list: null,
  _selectedItem: null,
  _permaTextNode: null,
  _permaTextValue: "",
  _emptyTextNode: null,
  _emptyTextValue: ""
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
        this._panel.className = "results-panel";
        this._panel.setAttribute("level", "top");
        this._panel.setAttribute("noautofocus", "true");
        document.documentElement.appendChild(this._panel);
      }
      if (!this.widget) {
        this.widget = new ListWidget(this._panel);
        this.widget.itemType = "vbox";
        this.widget.itemFactory = this._createItemView;
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
  get anchor() this._anchor,

  /**
   * Sets the container panel hidden or visible. It's hidden by default.
   * @param boolean aFlag
   */
  set hidden(aFlag) {
    if (aFlag) {
      this._panel.hidePopup();
    } else {
      this._panel.openPopup(this._anchor, this.position, this.left, this.top);
      this.anchor.focus();
    }
  },

  /**
   * Gets this container's visibility state.
   * @return boolean
   */
  get hidden()
    this._panel.state == "closed" ||
    this._panel.state == "hiding",

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
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed item.
   * @param any aAttachment
   *        Some attached primitive/object.
   * @param string aLabel
   *        The item's label.
   * @param string aValue
   *        The item's value.
   * @param string aDescription
   *        An optional description of the item.
   */
  _createItemView: function(aElementNode, aAttachment, aLabel, aValue, aDescription) {
    let labelsGroup = document.createElement("hbox");

    if (aDescription) {
      let preLabelNode = document.createElement("label");
      preLabelNode.className = "plain results-panel-item-pre";
      preLabelNode.setAttribute("value", aDescription);
      labelsGroup.appendChild(preLabelNode);
    }
    if (aLabel) {
      let labelNode = document.createElement("label");
      labelNode.className = "plain results-panel-item-name";
      labelNode.setAttribute("value", aLabel);
      labelsGroup.appendChild(labelNode);
    }

    let valueNode = document.createElement("label");
    valueNode.className = "plain results-panel-item-details";
    valueNode.setAttribute("value", aValue);

    aElementNode.className = "light results-panel-item";
    aElementNode.appendChild(labelsGroup);
    aElementNode.appendChild(valueNode);
  },

  _anchor: null,
  _panel: null,
  position: RESULTS_PANEL_POPUP_POSITION,
  left: 0,
  top: 0
});

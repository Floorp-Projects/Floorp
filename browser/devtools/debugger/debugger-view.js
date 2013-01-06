/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const SOURCE_URL_MAX_LENGTH = 64; // chars
const SOURCE_SYNTAX_HIGHLIGHT_MAX_FILE_SIZE = 1048576; // 1 MB in bytes
const PANES_APPEARANCE_DELAY = 50; // ms
const BREAKPOINT_LINE_TOOLTIP_MAX_LENGTH = 1000; // chars
const BREAKPOINT_CONDITIONAL_POPUP_POSITION = "after_start";
const BREAKPOINT_CONDITIONAL_POPUP_OFFSET = 50; // px
const FILTERED_SOURCES_POPUP_POSITION = "before_start";
const FILTERED_SOURCES_MAX_RESULTS = 10;
const GLOBAL_SEARCH_EXPAND_MAX_RESULTS = 50;
const GLOBAL_SEARCH_LINE_MAX_LENGTH = 300; // chars
const GLOBAL_SEARCH_ACTION_MAX_DELAY = 1500; // ms
const SEARCH_GLOBAL_FLAG = "!";
const SEARCH_TOKEN_FLAG = "#";
const SEARCH_LINE_FLAG = ":";
const SEARCH_VARIABLE_FLAG = "*";

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
  initialize: function DV_initialize(aCallback) {
    dumpn("Initializing the DebuggerView");

    this._initializeWindow();
    this._initializePanes();

    this.Toolbar.initialize();
    this.Options.initialize();
    this.ChromeGlobals.initialize();
    this.Sources.initialize();
    this.Filtering.initialize();
    this.FilteredSources.initialize();
    this.StackFrames.initialize();
    this.Breakpoints.initialize();
    this.WatchExpressions.initialize();
    this.GlobalSearch.initialize();

    this.Variables = new VariablesView(document.getElementById("variables"));
    this.Variables.searchPlaceholder = L10N.getStr("emptyVariablesFilterText");
    this.Variables.emptyText = L10N.getStr("emptyVariablesText");
    this.Variables.onlyEnumVisible = Prefs.variablesOnlyEnumVisible;
    this.Variables.searchEnabled = Prefs.variablesSearchboxVisible;
    this.Variables.eval = DebuggerController.StackFrames.evaluate;
    this.Variables.lazyEmpty = true;

    this._initializeEditor(aCallback);
  },

  /**
   * Destroys the debugger view.
   *
   * @param function aCallback
   *        Called after the view finishes destroying.
   */
  destroy: function DV_destroy(aCallback) {
    dumpn("Destroying the DebuggerView");

    this.Toolbar.destroy();
    this.Options.destroy();
    this.ChromeGlobals.destroy();
    this.Sources.destroy();
    this.Filtering.destroy();
    this.FilteredSources.destroy();
    this.StackFrames.destroy();
    this.Breakpoints.destroy();
    this.WatchExpressions.destroy();
    this.GlobalSearch.destroy();

    this._destroyWindow();
    this._destroyPanes();
    this._destroyEditor();
    aCallback();
  },

  /**
   * Initializes the UI for the window.
   */
  _initializeWindow: function DV__initializeWindow() {
    dumpn("Initializing the DebuggerView window");

    let isRemote = window._isRemoteDebugger;
    let isChrome = window._isChromeDebugger;

    if (isRemote || isChrome) {
      window.moveTo(Prefs.windowX, Prefs.windowY);
      window.resizeTo(Prefs.windowWidth, Prefs.windowHeight);

      if (isRemote) {
        document.title = L10N.getStr("remoteDebuggerWindowTitle");
      } else {
        document.title = L10N.getStr("chromeDebuggerWindowTitle");
      }
    }
  },

  /**
   * Destroys the UI for the window.
   */
  _destroyWindow: function DV__initializeWindow() {
    dumpn("Destroying the DebuggerView window");

    if (window._isRemoteDebugger || window._isChromeDebugger) {
      Prefs.windowX = window.screenX;
      Prefs.windowY = window.screenY;
      Prefs.windowWidth = window.outerWidth;
      Prefs.windowHeight = window.outerHeight;
    }
  },

  /**
   * Initializes the UI for all the displayed panes.
   */
  _initializePanes: function DV__initializePanes() {
    dumpn("Initializing the DebuggerView panes");

    this._togglePanesButton = document.getElementById("toggle-panes");
    this._stackframesAndBreakpoints = document.getElementById("stackframes+breakpoints");
    this._variablesAndExpressions = document.getElementById("variables+expressions");

    this._stackframesAndBreakpoints.setAttribute("width", Prefs.stackframesWidth);
    this._variablesAndExpressions.setAttribute("width", Prefs.variablesWidth);
    this.togglePanes({
      visible: Prefs.panesVisibleOnStartup,
      animated: false
    });
  },

  /**
   * Destroys the UI for all the displayed panes.
   */
  _destroyPanes: function DV__initializePanes() {
    dumpn("Destroying the DebuggerView panes");

    Prefs.stackframesWidth = this._stackframesAndBreakpoints.getAttribute("width");
    Prefs.variablesWidth = this._variablesAndExpressions.getAttribute("width");

    this._togglePanesButton = null;
    this._stackframesAndBreakpoints = null;
    this._variablesAndExpressions = null;
  },

  /**
   * Initializes the SourceEditor instance.
   *
   * @param function aCallback
   *        Called after the editor finishes initializing.
   */
  _initializeEditor: function DV__initializeEditor(aCallback) {
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
    this.editor.init(placeholder, config, function() {
      this._onEditorLoad();
      aCallback();
    }.bind(this));
  },

  /**
   * The load event handler for the source editor, also executing any necessary
   * post-load operations.
   */
  _onEditorLoad: function DV__onEditorLoad() {
    dumpn("Finished loading the DebuggerView editor");

    DebuggerController.Breakpoints.initialize();
    window.dispatchEvent("Debugger:EditorLoaded", this.editor);
    this.editor.focus();
  },

  /**
   * Destroys the SourceEditor instance and also executes any necessary
   * post-unload operations.
   */
  _destroyEditor: function DV__destroyEditor() {
    dumpn("Destroying the DebuggerView editor");

    DebuggerController.Breakpoints.destroy();
    window.dispatchEvent("Debugger:EditorUnloaded", this.editor);
    this.editor = null;
  },

  /**
   * Sets the proper editor mode (JS or HTML) according to the specified
   * content type, or by determining the type from the url.
   *
   * @param string aUrl
   *        The script url.
   * @param string aContentType [optional]
   *        The script content type.
   * @param string aTextContent [optional]
   *        The script text content.
   */
  setEditorMode:
  function DV_setEditorMode(aUrl, aContentType = "", aTextContent = "") {
    if (!this.editor) {
      return;
    }
    dumpn("Setting the DebuggerView editor mode: " + aUrl +
          ", content type: " + aContentType);

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
   * Load the editor with the specified source text.
   *
   * @param object aSource
   *        The source object coming from the active thread.
   * @param object aOptions [optional]
   *        Additional options for showing the source. Supported options:
   *        - caretLine: place the caret position at the given line number
   *        - debugLine: place the debug location at the given line number
   *        - callback: function called when the source is shown
   */
  setEditorSource: function DV_setEditorSource(aSource, aOptions = {}) {
    if (!this.editor) {
      return;
    }
    dumpn("Setting the DebuggerView editor source: " + aSource.url +
          ", loaded: " + aSource.loaded +
          ", options: " + aOptions.toSource());

    // If the source is not loaded, display a placeholder text.
    if (!aSource.loaded) {
      this.editor.setMode(SourceEditor.MODES.TEXT);
      this.editor.setText(L10N.getStr("loadingText"));
      this.editor.resetUndo();

      // Get the source text from the active thread.
      DebuggerController.SourceScripts.getText(aSource, function(aUrl, aText) {
        this.setEditorSource(aSource, aOptions);
      }.bind(this));
    }
    // If the source is already loaded, display it immediately.
    else {
      if (this._editorSource != aSource) {
        // Avoid setting the editor mode for very large files.
        if (aSource.text.length < SOURCE_SYNTAX_HIGHLIGHT_MAX_FILE_SIZE) {
          this.setEditorMode(aSource.url, aSource.contentType, aSource.text);
        } else {
          this.editor.setMode(SourceEditor.MODES.TEXT);
        }
        this.editor.setText(aSource.text);
        this.editor.resetUndo();
      }
      this._editorSource = aSource;
      this.updateEditor();

      DebuggerView.Sources.selectedValue = aSource.url;
      DebuggerController.Breakpoints.updateEditorBreakpoints();

      // Handle any additional options for showing the source.
      if (aOptions.caretLine) {
        editor.setCaretPosition(aOptions.caretLine - 1);
      }
      if (aOptions.debugLine) {
        editor.setDebugLocation(aOptions.debugLine - 1);
      }
      if (aOptions.callback) {
        aOptions.callback(aSource);
      }
      // Notify that we've shown a source file.
      window.dispatchEvent("Debugger:SourceShown", aSource);
    }
  },

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
   *        An object containing some of the following boolean properties:
   *          - noSwitch: don't switch to the source if not currently selected
   *          - noCaret: don't set the caret location at the specified line
   *          - noDebug: don't set the debug location at the specified line
   */
  updateEditor: function DV_updateEditor(aUrl, aLine, aFlags = {}) {
    if (!this.editor) {
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
      updateLine(aLine);
    }
    // If the requested source exists, display it and update.
    else if (this.Sources.containsValue(aUrl) && !aFlags.noSwitch) {
      this.Sources.selectedValue = aUrl;
      updateLine(aLine);
    }
    // Dumb request, invalidate the caret position and debug location.
    else {
      updateLine(0);
    }

    // Updates the source editor's caret position and debug location.
    // @param number a Line
    function updateLine(aLine) {
      if (!aFlags.noCaret) {
        DebuggerView.editor.setCaretPosition(aLine - 1);
      }
      if (!aFlags.noDebug) {
        DebuggerView.editor.setDebugLocation(aLine - 1);
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
  getEditorLine: function SS_getEditorLine(aLine) {
    let line = aLine || this.editor.getCaretPosition().line;
    let start = this.editor.getLineStart(line);
    let end = this.editor.getLineEnd(line);
    return this.editor.getText(start, end);
  },

  /**
   * Gets the visibility state of the panes.
   * @return boolean
   */
  get panesHidden()
    this._togglePanesButton.hasAttribute("panesHidden"),

  /**
   * Sets all the panes hidden or visible.
   *
   * @param object aFlags [optional]
   *        An object containing some of the following boolean properties:
   *        - visible: true if the pane should be shown, false for hidden
   *        - animated: true to display an animation on toggle
   *        - delayed: true to wait a few cycles before toggle
   *        - callback: a function to invoke when the panes toggle finishes
   */
  togglePanes: function DV__togglePanes(aFlags = {}) {
    // Avoid useless toggles.
    if (aFlags.visible == !this.panesHidden) {
      if (aFlags.callback) aFlags.callback();
      return;
    }

    // Computes and sets the panes margins in order to hide or show them.
    function set() {
      if (aFlags.visible) {
        this._stackframesAndBreakpoints.style.marginLeft = "0";
        this._variablesAndExpressions.style.marginRight = "0";
        this._togglePanesButton.removeAttribute("panesHidden");
        this._togglePanesButton.setAttribute("tooltiptext", L10N.getStr("collapsePanes"));
      } else {
        let marginL = ~~(this._stackframesAndBreakpoints.getAttribute("width")) + 1;
        let marginR = ~~(this._variablesAndExpressions.getAttribute("width")) + 1;
        this._stackframesAndBreakpoints.style.marginLeft = -marginL + "px";
        this._variablesAndExpressions.style.marginRight = -marginR + "px";
        this._togglePanesButton.setAttribute("panesHidden", "true");
        this._togglePanesButton.setAttribute("tooltiptext", L10N.getStr("expandPanes"));
      }

      if (aFlags.animated) {
        // Displaying the panes may have the effect of triggering scrollbars to
        // appear in the source editor, which would render the currently
        // highlighted line to appear behind them in some cases.
        window.addEventListener("transitionend", function onEvent() {
          window.removeEventListener("transitionend", onEvent, false);
          DebuggerView.updateEditor();

          // Invoke the callback when the transition ended.
          if (aFlags.callback) aFlags.callback();
        }, false);
      } else {
        // Invoke the callback immediately since there's no transition.
        if (aFlags.callback) aFlags.callback();
      }
    }

    if (aFlags.animated) {
      this._stackframesAndBreakpoints.setAttribute("animated", "");
      this._variablesAndExpressions.setAttribute("animated", "");
    } else {
      this._stackframesAndBreakpoints.removeAttribute("animated");
      this._variablesAndExpressions.removeAttribute("animated");
    }

    if (aFlags.delayed) {
      window.setTimeout(set.bind(this), PANES_APPEARANCE_DELAY);
    } else {
      set.call(this);
    }
  },

  /**
   * Sets all the panes visible after a short period of time.
   *
   * @param function aCallback
   *        A function to invoke when the panes toggle finishes.
   */
  showPanesSoon: function DV__showPanesSoon(aCallback) {
    // Try to keep animations as smooth as possible, so wait a few cycles.
    window.setTimeout(function() {
      DebuggerView.togglePanes({
        visible: true,
        animated: true,
        delayed: true,
        callback: aCallback
      });
    }, PANES_APPEARANCE_DELAY);
  },

  /**
   * Handles any initialization on a tab navigation event issued by the client.
   */
  _handleTabNavigation: function DV__handleTabNavigation() {
    dumpn("Handling tab navigation in the DebuggerView");

    this.ChromeGlobals.empty();
    this.Sources.empty();
    this.Filtering.clearSearch();
    this.GlobalSearch.clearView();
    this.GlobalSearch.clearCache();
    this.StackFrames.empty();
    this.Breakpoints.empty();
    this.Breakpoints.unhighlightBreakpoint();
    this.Variables.empty();
    SourceUtils.clearLabelsCache();

    if (this.editor) {
      this.editor.setText("");
      this.editor.focus();
      this._editorSource = null;
    }
  },

  Toolbar: null,
  Options: null,
  ChromeGlobals: null,
  Sources: null,
  Filtering: null,
  StackFrames: null,
  Breakpoints: null,
  GlobalSearch: null,
  Variables: null,
  _editor: null,
  _editorSource: null,
  _togglePanesButton: null,
  _stackframesAndBreakpoints: null,
  _variablesAndExpressions: null,
  _isInitialized: false,
  _isDestroyed: false
};

/**
 * A generic item used to describe elements present in views like the
 * ChromeGlobals, Sources, Stackframes, Breakpoints etc.
 *
 * @param string aLabel
 *        The label displayed in the container.
 * @param string aValue
 *        The actual internal value of the item.
 * @param string aDescription [optional]
 *        An optional description of the item.
 * @param any aAttachment [optional]
 *        Some attached primitive/object.
 */
function MenuItem(aLabel, aValue, aDescription, aAttachment) {
  this._label = aLabel + "";
  this._value = aValue + "";
  this._description = aDescription + "";
  this.attachment = aAttachment;
}

MenuItem.prototype = {
  /**
   * Gets the label set for this item.
   * @return string
   */
  get label() this._label,

  /**
   * Gets the value set for this item.
   * @return string
   */
  get value() this._value,

  /**
   * Gets the description set for this item.
   * @return string
   */
  get description() this._description,

  /**
   * Gets the element associated with this item.
   * @return nsIDOMNode
   */
  get target() this._target,

  _label: "",
  _value: "",
  _description: "",
  _target: null,
  finalize: null,
  attachment: null
};

/**
 * A generic items container, used for displaying views like the
 * ChromeGlobals, Sources, Stackframes, Breakpoints etc.
 * Iterable via "for (let item in menuContainer) { }".
 *
 * Language:
 *   - An "item" is an instance (or compatible iterface) of a MenuItem.
 *   - An "element" or "node" is a nsIDOMNode.
 *
 * The container node supplied to all instances of this constructor can either
 * be a <menulist> element, or any other object interfacing the following
 * methods:
 *   - function:nsIDOMNode appendItem(aLabel:string, aValue:string)
 *   - function:nsIDOMNode insertItemAt(aIndex:number, aLabel:string, aValue:string)
 *   - function:nsIDOMNode getItemAtIndex(aIndex:number)
 *   - function removeChild(aChild:nsIDOMNode)
 *   - function removeAllItems()
 *   - get:number itemCount()
 *   - get:number selectedIndex()
 *   - set selectedIndex(aIndex:number)
 *   - get:nsIDOMNode selectedItem()
 *   - set selectedItem(aChild:nsIDOMNode)
 *   - function getAttribute(aName:string)
 *   - function setAttribute(aName:string, aValue:string)
 *   - function removeAttribute(aName:string)
 *   - function addEventListener(aName:string, aCallback:function, aBubbleFlag:boolean)
 *   - function removeEventListener(aName:string, aCallback:function, aBubbleFlag:boolean)
 *
 * @param nsIDOMNode aContainerNode [optional]
 *        The element associated with the displayed container. Although required,
 *        derived objects may set this value later, upon debugger initialization.
 */
function MenuContainer(aContainerNode) {
  this._container = aContainerNode;
  this._stagedItems = [];
  this._itemsByLabel = new Map();
  this._itemsByValue = new Map();
  this._itemsByElement = new Map();
}

MenuContainer.prototype = {
  /**
   * Prepares an item to be added to this container. This allows for a large
   * number of items to be batched up before being alphabetically sorted and
   * added in this menu.
   *
   * If the "forced" flag is true, the item will be immediately inserted at the
   * correct position in this container, so that all the items remain sorted.
   * This can (possibly) be much slower than batching up multiple items.
   *
   * By default, this container assumes that all the items should be displayed
   * sorted by their label. This can be overridden with the "unsorted" flag.
   *
   * Furthermore, this container makes sure that all the items are unique
   * (two items with the same label or value are not allowed) and non-degenerate
   * (items with "undefined" or "null" labels/values). This can, as well, be
   * overridden via the "relaxed" flag.
   *
   * @param string aLabel
   *        The label displayed in the container.
   * @param string aValue
   *        The actual internal value of the item.
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - forced: true to force the item to be immediately appended
   *          - unsorted: true if the items should not always remain sorted
   *          - relaxed: true if this container should allow dupes & degenerates
   *          - description: an optional description of the item
   *          - tooltip: an optional tooltip for the item
   *          - attachment: some attached primitive/object
   * @return MenuItem
   *         The item associated with the displayed element if a forced push,
   *         undefined if the item was staged for a later commit.
   */
  push: function DVMC_push(aLabel, aValue, aOptions = {}) {
    let item = new MenuItem(
      aLabel, aValue, aOptions.description, aOptions.attachment);

    // Batch the item to be added later.
    if (!aOptions.forced) {
      this._stagedItems.push({ item: item, options: aOptions });
    }
    // Immediately insert the item at the specified index.
    else if (aOptions.forced && aOptions.forced.atIndex !== undefined) {
      return this._insertItemAt(aOptions.forced.atIndex, item, aOptions);
    }
    // Find the target position in this container and insert the item there.
    else if (!aOptions.unsorted) {
      return this._insertItemAt(this._findExpectedIndex(aLabel), item, aOptions);
    }
    // Just append the item in this container.
    else {
      return this._appendItem(item, aOptions);
    }
  },

  /**
   * Flushes all the prepared items into this container.
   *
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - unsorted: true if the items should not be sorted beforehand
   */
  commit: function DVMC_commit(aOptions = {}) {
    let stagedItems = this._stagedItems;

    // By default, sort the items before adding them to this container.
    if (!aOptions.unsorted) {
      stagedItems.sort(function(a, b) a.item.label.toLowerCase() >
                                      b.item.label.toLowerCase());
    }
    // Append the prepared items to this container.
    for (let { item, options } of stagedItems) {
      this._appendItem(item, options);
    }
    // Recreate the temporary items list for ulterior pushes.
    this._stagedItems = [];
  },

  /**
   * Updates this container to reflect the information provided by the
   * currently selected item.
   *
   * @return boolean
   *         True if a selected item was available, false otherwise.
   */
  refresh: function DVMC_refresh() {
    let selectedValue = this.selectedValue;
    if (!selectedValue) {
      return false;
    }

    let entangledLabel = this.getItemByValue(selectedValue).label;

    this._container.setAttribute("label", entangledLabel);
    this._container.setAttribute("tooltiptext", selectedValue);
    return true;
  },

  /**
   * Immediately removes the specified item from this container.
   *
   * @param MenuItem aItem
   *        The item associated with the element to remove.
   */
  remove: function DVMC__remove(aItem) {
    this._container.removeChild(aItem.target);
    this._untangleItem(aItem);
  },

  /**
   * Removes all items from this container.
   */
  empty: function DVMC_empty() {
    this._preferredValue = this.selectedValue;
    this._container.selectedIndex = -1;
    this._container.setAttribute("label", this._emptyLabel);
    this._container.removeAttribute("tooltiptext");
    this._container.removeAllItems();

    for (let [, item] of this._itemsByElement) {
      this._untangleItem(item);
    }

    this._itemsByLabel = new Map();
    this._itemsByValue = new Map();
    this._itemsByElement = new Map();
    this._stagedItems = [];
  },

  /**
   * Toggles all the items in this container hidden or visible.
   *
   * @param boolean aVisibleFlag
   *        Specifies the intended visibility.
   */
  toggleContents: function DVMC_toggleContents(aVisibleFlag) {
    for (let [, item] of this._itemsByElement) {
      item.target.hidden = !aVisibleFlag;
    }
  },

  /**
   * Does not remove any item in this container. Instead, it overrides the
   * current label to signal that it is unavailable and removes the tooltip.
   */
  setUnavailable: function DVMC_setUnavailable() {
    this._container.setAttribute("label", this._unavailableLabel);
    this._container.removeAttribute("tooltiptext");
  },

  /**
   * Checks whether an item with the specified label is among the elements
   * shown in this container.
   *
   * @param string aLabel
   *        The item's label.
   * @return boolean
   *         True if the label is known, false otherwise.
   */
  containsLabel: function DVMC_containsLabel(aLabel) {
    return this._itemsByLabel.has(aLabel) ||
           this._stagedItems.some(function({item}) item.label == aLabel);
  },

  /**
   * Checks whether an item with the specified value is among the elements
   * shown in this container.
   *
   * @param string aValue
   *        The item's value.
   * @return boolean
   *         True if the value is known, false otherwise.
   */
  containsValue: function DVMC_containsValue(aValue) {
    return this._itemsByValue.has(aValue) ||
           this._stagedItems.some(function({item}) item.value == aValue);
  },

  /**
   * Checks whether an item with the specified trimmed value is among the
   * elements shown in this container.
   *
   * @param string aValue
   *        The item's value.
   * @param function aTrim [optional]
   *        A custom trimming function.
   * @return boolean
   *         True if the trimmed value is known, false otherwise.
   */
  containsTrimmedValue:
  function DVMC_containsTrimmedValue(aValue,
                                     aTrim = SourceUtils.trimUrlQuery) {
    let trimmedValue = aTrim(aValue);

    for (let [value] of this._itemsByValue) {
      if (aTrim(value) == trimmedValue) {
        return true;
      }
    }
    return this._stagedItems.some(function({item}) aTrim(item.value) == trimmedValue);
  },

  /**
   * Gets the preferred selected value to be displayed in this container.
   * @return string
   */
  get preferredValue() this._preferredValue,

  /**
   * Retrieves the selected element's index in this container.
   * @return number
   */
  get selectedIndex() this._container.selectedIndex,

  /**
   * Retrieves the item associated with the selected element.
   * @return MenuItem
   */
  get selectedItem()
    this._container.selectedItem ?
    this._itemsByElement.get(this._container.selectedItem) : null,

  /**
   * Retrieves the label of the selected element.
   * @return string
   */
  get selectedLabel()
    this._container.selectedItem ?
    this._itemsByElement.get(this._container.selectedItem).label : null,

  /**
   * Retrieves the value of the selected element.
   * @return string
   */
  get selectedValue()
    this._container.selectedItem ?
    this._itemsByElement.get(this._container.selectedItem).value : null,

  /**
   * Selects the element at the specified index in this container.
   * @param number aIndex
   */
  set selectedIndex(aIndex) this._container.selectedIndex = aIndex,

  /**
   * Selects the element with the entangled item in this container.
   * @param MenuItem aItem
   */
  set selectedItem(aItem) this._container.selectedItem = aItem.target,

  /**
   * Selects the element with the specified label in this container.
   * @param string aLabel
   */
  set selectedLabel(aLabel) {
    let item = this._itemsByLabel.get(aLabel);
    if (item) {
      this._container.selectedItem = item.target;
    }
  },

  /**
   * Selects the element with the specified value in this container.
   * @param string aValue
   */
  set selectedValue(aValue) {
    let item = this._itemsByValue.get(aValue);
    if (item) {
      this._container.selectedItem = item.target;
    }
  },

  /**
   * Gets the item in the container having the specified index.
   *
   * @param number aIndex
   *        The index used to identify the element.
   * @return MenuItem
   *         The matched item, or null if nothing is found.
   */
  getItemAtIndex: function DVMC_getItemAtIndex(aIndex) {
    return this.getItemForElement(this._container.getItemAtIndex(aIndex));
  },

  /**
   * Gets the item in the container having the specified label.
   *
   * @param string aLabel
   *        The label used to identify the element.
   * @return MenuItem
   *         The matched item, or null if nothing is found.
   */
  getItemByLabel: function DVMC_getItemByLabel(aLabel) {
    return this._itemsByLabel.get(aLabel);
  },

  /**
   * Gets the item in the container having the specified value.
   *
   * @param string aValue
   *        The value used to identify the element.
   * @return MenuItem
   *         The matched item, or null if nothing is found.
   */
  getItemByValue: function DVMC_getItemByValue(aValue) {
    return this._itemsByValue.get(aValue);
  },

  /**
   * Gets the item in the container associated with the specified element.
   *
   * @param nsIDOMNode aElement
   *        The element used to identify the item.
   * @return MenuItem
   *         The matched item, or null if nothing is found.
   */
  getItemForElement: function DVMC_getItemForElement(aElement) {
    while (aElement) {
      let item = this._itemsByElement.get(aElement);
      if (item) {
        return item;
      }
      aElement = aElement.parentNode;
    }
    return null;
  },

  /**
   * Returns the list of labels in this container.
   * @return array
   */
  get labels() {
    let labels = [];
    for (let [label] of this._itemsByLabel) {
      labels.push(label);
    }
    return labels;
  },

  /**
   * Returns the list of values in this container.
   * @return array
   */
  get values() {
    let values = [];
    for (let [value] of this._itemsByValue) {
      values.push(value);
    }
    return values;
  },

  /**
   * Gets the total number of items in this container.
   * @return number
   */
  get totalItems() {
    return this._itemsByElement.size;
  },

  /**
   * Returns a list of all the visible (non-hidden) items in this container.
   * @return array
   */
  get visibleItems() {
    let items = [];
    for (let [element, item] of this._itemsByElement) {
      if (!element.hidden) {
        items.push(item);
      }
    }
    return items;
  },

  /**
   * Specifies the required conditions for an item to be considered unique.
   * Possible values:
   *   - 1: label AND value are different from all other items
   *   - 2: label OR value are different from all other items
   *   - 3: only label is required to be different
   *   - 4: only value is required to be different
   */
  uniquenessQualifier: 1,

  /**
   * Checks if an item is unique in this container.
   *
   * @param MenuItem aItem
   *        An object containing a label and a value property.
   * @return boolean
   *         True if the element is unique, false otherwise.
   */
  isUnique: function DVMC_isUnique(aItem) {
    switch (this.uniquenessQualifier) {
      case 1:
        return !this._itemsByLabel.has(aItem.label) &&
               !this._itemsByValue.has(aItem.value);
      case 2:
        return !this._itemsByLabel.has(aItem.label) ||
               !this._itemsByValue.has(aItem.value);
      case 3:
        return !this._itemsByLabel.has(aItem.label);
      case 4:
        return !this._itemsByValue.has(aItem.value);
    }
    return false;
  },

  /**
   * Checks if an item's label and value are eligible for this container.
   *
   * @param MenuItem aItem
   *        An object containing a label and a value property.
   * @return boolean
   *         True if the element is eligible, false otherwise.
   */
  isEligible: function DVMC_isEligible(aItem) {
    return this.isUnique(aItem) &&
           aItem.label != "undefined" && aItem.label != "null" &&
           aItem.value != "undefined" && aItem.value != "null";
  },

  /**
   * Finds the expected item index in this container based on its label.
   *
   * @param string aLabel
   *        The label used to identify the element.
   * @return number
   *         The expected item index.
   */
  _findExpectedIndex: function DVMC__findExpectedIndex(aLabel) {
    let container = this._container;
    let itemCount = container.itemCount;

    for (let i = 0; i < itemCount; i++) {
      if (this.getItemForElement(container.getItemAtIndex(i)).label > aLabel) {
        return i;
      }
    }
    return itemCount;
  },

  /**
   * Immediately appends an item in this container.
   *
   * @param MenuItem aItem
   *        An object containing a label and a value property.
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - relaxed: true if this container should allow dupes & degenerates
   * @return MenuItem
   *         The item associated with the displayed element, null if rejected.
   */
  _appendItem: function DVMC__appendItem(aItem, aOptions = {}) {
    if (!aOptions.relaxed && !this.isEligible(aItem)) {
      return null;
    }

    this._entangleItem(aItem, this._container.appendItem(
      aItem.label, aItem.value, "", aOptions.attachment));

    // Handle any additional options after entangling the item.
    if (aOptions.tooltip) {
      aItem._target.setAttribute("tooltiptext", aOptions.tooltip);
    }

    return aItem;
  },

  /**
   * Immediately inserts an item in this container at the specified index.
   *
   * @param number aIndex
   *        The position in the container intended for this item.
   * @param MenuItem aItem
   *        An object containing a label and a value property.
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - relaxed: true if this container should allow dupes & degenerates
   * @return MenuItem
   *         The item associated with the displayed element, null if rejected.
   */
  _insertItemAt: function DVMC__insertItemAt(aIndex, aItem, aOptions) {
    if (!aOptions.relaxed && !this.isEligible(aItem)) {
      return null;
    }

    this._entangleItem(aItem, this._container.insertItemAt(
      aIndex, aItem.label, aItem.value, "", aOptions.attachment));

    // Handle any additional options after entangling the item.
    if (aOptions.tooltip) {
      aItem._target.setAttribute("tooltiptext", aOptions.tooltip);
    }

    return aItem;
  },

  /**
   * Entangles an item (model) with a displayed node element (view).
   *
   * @param MenuItem aItem
   *        The item describing the element.
   * @param nsIDOMNode aElement
   *        The element displaying the item.
   * @return MenuItem
   *         The same item.
   */
  _entangleItem: function DVMC__entangleItem(aItem, aElement) {
    this._itemsByLabel.set(aItem.label, aItem);
    this._itemsByValue.set(aItem.value, aItem);
    this._itemsByElement.set(aElement, aItem);

    aItem._target = aElement;
    return aItem;
  },

  /**
   * Untangles an item (model) from a displayed node element (view).
   *
   * @param MenuItem aItem
   *        The item describing the element.
   * @return MenuItem
   *         The same item.
   */
  _untangleItem: function DVMC__untangleItem(aItem) {
    if (aItem.finalize instanceof Function) {
      aItem.finalize(aItem);
    }

    this._itemsByLabel.delete(aItem.label);
    this._itemsByValue.delete(aItem.value);
    this._itemsByElement.delete(aItem.target);

    aItem._target = null;
    return aItem;
  },

  /**
   * A generator-iterator over all the items in this container.
   */
  __iterator__: function DVMC_iterator() {
    for (let [, item] of this._itemsByElement) {
      yield item;
    }
  },

  _container: null,
  _stagedItems: null,
  _itemsByLabel: null,
  _itemsByValue: null,
  _itemsByElement: null,
  _preferredValue: null,
  _emptyLabel: "",
  _unavailableLabel: ""
};

/**
 * A stacked list of items, compatible with MenuContainer instances, used for
 * displaying views like the StackFrames, Breakpoints etc.
 *
 * Custom methods introduced by this view, not necessary for a MenuContainer:
 * set emptyText(aValue:string)
 * set permaText(aValue:string)
 * set itemType(aType:string)
 * set itemFactory(aCallback:function)
 *
 * @param nsIDOMNode aAssociatedNode
 *        The element associated with the displayed container.
 */
function StackList(aAssociatedNode) {
  this._parent = aAssociatedNode;

  // Create an internal list container.
  this._list = document.createElement("vbox");
  this._parent.appendChild(this._list);
}

StackList.prototype = {
  /**
   * Immediately appends an item in this container.
   *
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
  appendItem:
  function DVSL_appendItem(aLabel, aValue, aDescription, aAttachment) {
    return this.insertItemAt(
      Number.MAX_VALUE, aLabel, aValue, aDescription, aAttachment);
  },

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
  insertItemAt:
  function DVSL_insertItemAt(aIndex, aLabel, aValue, aDescription, aAttachment) {
    let list = this._list;
    let childNodes = list.childNodes;

    let element = document.createElement(this.itemType);
    this._createItemView(element, aLabel, aValue, aAttachment);
    this._removeEmptyNotice();

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
  getItemAtIndex: function DVSL_getItemAtIndex(aIndex) {
    return this._list.childNodes[aIndex];
  },

  /**
   * Immediately removes the specified child node from this container.
   *
   * @param nsIDOMNode aChild
   *        The element associated with the displayed item.
   */
  removeChild: function DVSL__removeChild(aChild) {
    this._list.removeChild(aChild);

    if (!this.itemCount) {
      this._appendEmptyNotice();
    }
  },

  /**
   * Immediately removes all of the child nodes from this container.
   */
  removeAllItems: function DVSL_removeAllItems() {
    let parent = this._parent;
    let list = this._list;
    let firstChild;

    while (firstChild = list.firstChild) {
      list.removeChild(firstChild);
    }
    parent.scrollTop = 0;
    parent.scrollLeft = 0;

    this._selectedItem = null;
    this._selectedIndex = -1;
    this._appendEmptyNotice();
  },

  /**
   * Gets the number of child nodes present in this container.
   * @return number
   */
  get itemCount() this._list.childNodes.length,

  /**
   * Gets the index of the selected child node in this container.
   * @return number
   */
  get selectedIndex() this._selectedIndex,

  /**
   * Sets the index of the selected child node in this container.
   * Only one child node may be selected at a time.
   * @param number aIndex
   */
  set selectedIndex(aIndex) this.selectedItem = this._list.childNodes[aIndex],

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
      this._selectedIndex = -1;
    }
    for (let node of childNodes) {
      if (node == aChild) {
        node.classList.add("selected");
        this._selectedIndex = Array.indexOf(childNodes, node);
        this._selectedItem = node;
      } else {
        node.classList.remove("selected");
      }
    }
  },

  /**
   * Applies an attribute to this container.
   *
   * @param string aName
   *        The name of the attribute to set.
   * @return string
   *         The attribute value.
   */
  getAttribute: function DVSL_setAttribute(aName) {
    return this._parent.getAttribute(aName);
  },

  /**
   * Applies an attribute to this container.
   *
   * @param string aName
   *        The name of the attribute to set.
   * @param any aValue
   *        The supplied attribute value.
   */
  setAttribute: function DVSL_setAttribute(aName, aValue) {
    this._parent.setAttribute(aName, aValue);
  },

  /**
   * Removes an attribute applied to this container.
   *
   * @param string aName
   *        The name of the attribute to remove.
   */
  removeAttribute: function DVSL_removeAttribute(aName) {
    this._parent.removeAttribute(aName);
  },

  /**
   * Adds an event listener to this container.
   *
   * @param string aName
   *        The name of the listener to set.
   * @param function aCallback
   *        The function to be called when the event is triggered.
   * @param boolean aBubbleFlag
   *        True if the event should bubble.
   */
  addEventListener:
  function DVSL_addEventListener(aName, aCallback, aBubbleFlag) {
    this._parent.addEventListener(aName, aCallback, aBubbleFlag);
  },

  /**
   * Removes an event listener added to this container.
   *
   * @param string aName
   *        The name of the listener to remove.
   * @param function aCallback
   *        The function called when the event was triggered.
   * @param boolean aBubbleFlag
   *        True if the event was bubbling.
   */
  removeEventListener:
  function DVSL_removeEventListener(aName, aCallback, aBubbleFlag) {
    this._parent.removeEventListener(aName, aCallback, aBubbleFlag);
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
   * Overrides the item's element type (e.g. "vbox" or "hbox").
   * @param string aType
   */
  itemType: "hbox",

  /**
   * Overrides the customization function for creating an item's UI.
   * @param function aCallback
   */
  set itemFactory(aCallback) this._createItemView = aCallback,

  /**
   * Customization function for creating an item's UI for this container.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed item.
   * @param string aLabel
   *        The item's label.
   * @param string aValue
   *        The item's value.
   */
  _createItemView: function DVSL__createItemView(aElementNode, aLabel, aValue) {
    let labelNode = document.createElement("label");
    let valueNode = document.createElement("label");
    let spacer = document.createElement("spacer");

    labelNode.setAttribute("value", aLabel);
    valueNode.setAttribute("value", aValue);
    spacer.setAttribute("flex", "1");

    aElementNode.appendChild(labelNode);
    aElementNode.appendChild(spacer);
    aElementNode.appendChild(valueNode);

    aElementNode.labelNode = labelNode;
    aElementNode.valueNode = valueNode;
  },

  /**
   * Creates and appends a label displayed permanently in this container's header.
   */
  _appendPermaNotice: function DVSL__appendPermaNotice() {
    if (this._permaTextNode || !this._permaTextValue) {
      return;
    }

    let label = document.createElement("label");
    label.className = "empty list-item";
    label.setAttribute("value", this._permaTextValue);

    this._parent.insertBefore(label, this._list);
    this._permaTextNode = label;
  },

  /**
   * Creates and appends a label signaling that this container is empty.
   */
  _appendEmptyNotice: function DVSL__appendEmptyNotice() {
    if (this._emptyTextNode || !this._emptyTextValue) {
      return;
    }

    let label = document.createElement("label");
    label.className = "empty list-item";
    label.setAttribute("value", this._emptyTextValue);

    this._parent.appendChild(label);
    this._emptyTextNode = label;
  },

  /**
   * Removes the label signaling that this container is empty.
   */
  _removeEmptyNotice: function DVSL__removeEmptyNotice() {
    if (!this._emptyTextNode) {
      return;
    }

    this._parent.removeChild(this._emptyTextNode);
    this._emptyTextNode = null;
  },

  _parent: null,
  _list: null,
  _selectedIndex: -1,
  _selectedItem: null,
  _permaTextNode: null,
  _permaTextValue: "",
  _emptyTextNode: null,
  _emptyTextValue: ""
};

/**
 * A simple way of displaying a "Connect to..." prompt.
 */
function RemoteDebuggerPrompt() {
  this.remote = {};
}

RemoteDebuggerPrompt.prototype = {
  /**
   * Shows the prompt and waits for a remote host and port to connect to.
   *
   * @param boolean aIsReconnectingFlag
   *        True to show the reconnect message instead of the connect request.
   */
  show: function RDP_show(aIsReconnectingFlag) {
    let check = { value: Prefs.remoteAutoConnect };
    let input = { value: Prefs.remoteHost + ":" + Prefs.remotePort };
    let parts;

    while (true) {
      let result = Services.prompt.prompt(null,
        L10N.getStr("remoteDebuggerPromptTitle"),
        L10N.getStr(aIsReconnectingFlag
          ? "remoteDebuggerReconnectMessage"
          : "remoteDebuggerPromptMessage"), input,
        L10N.getStr("remoteDebuggerPromptCheck"), check);

      if (!result) {
        return false;
      }
      if ((parts = input.value.split(":")).length == 2) {
        let [host, port] = parts;

        if (host.length && port.length) {
          this.remote = { host: host, port: port, auto: check.value };
          return true;
        }
      }
    }
  }
};

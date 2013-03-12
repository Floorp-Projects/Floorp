/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the toolbar view: close button, expand/collapse button,
 * pause/resume and stepping buttons etc.
 */
function ToolbarView() {
  dumpn("ToolbarView was instantiated");

  this._onTogglePanesPressed = this._onTogglePanesPressed.bind(this);
  this._onResumePressed = this._onResumePressed.bind(this);
  this._onStepOverPressed = this._onStepOverPressed.bind(this);
  this._onStepInPressed = this._onStepInPressed.bind(this);
  this._onStepOutPressed = this._onStepOutPressed.bind(this);
}

ToolbarView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVT_initialize() {
    dumpn("Initializing the ToolbarView");

    this._instrumentsPaneToggleButton = document.getElementById("instruments-pane-toggle");
    this._resumeButton = document.getElementById("resume");
    this._stepOverButton = document.getElementById("step-over");
    this._stepInButton = document.getElementById("step-in");
    this._stepOutButton = document.getElementById("step-out");
    this._chromeGlobals = document.getElementById("chrome-globals");

    let resumeKey = LayoutHelpers.prettyKey(document.getElementById("resumeKey"), true);
    let stepOverKey = LayoutHelpers.prettyKey(document.getElementById("stepOverKey"), true);
    let stepInKey = LayoutHelpers.prettyKey(document.getElementById("stepInKey"), true);
    let stepOutKey = LayoutHelpers.prettyKey(document.getElementById("stepOutKey"), true);
    this._resumeTooltip = L10N.getFormatStr("resumeButtonTooltip", [resumeKey]);
    this._pauseTooltip = L10N.getFormatStr("pauseButtonTooltip", [resumeKey]);
    this._stepOverTooltip = L10N.getFormatStr("stepOverTooltip", [stepOverKey]);
    this._stepInTooltip = L10N.getFormatStr("stepInTooltip", [stepInKey]);
    this._stepOutTooltip = L10N.getFormatStr("stepOutTooltip", [stepOutKey]);

    this._instrumentsPaneToggleButton.addEventListener("mousedown", this._onTogglePanesPressed, false);
    this._resumeButton.addEventListener("mousedown", this._onResumePressed, false);
    this._stepOverButton.addEventListener("mousedown", this._onStepOverPressed, false);
    this._stepInButton.addEventListener("mousedown", this._onStepInPressed, false);
    this._stepOutButton.addEventListener("mousedown", this._onStepOutPressed, false);

    this._stepOverButton.setAttribute("tooltiptext", this._stepOverTooltip);
    this._stepInButton.setAttribute("tooltiptext", this._stepInTooltip);
    this._stepOutButton.setAttribute("tooltiptext", this._stepOutTooltip);

    // TODO: bug 806775
    // this.toggleChromeGlobalsContainer(window._isChromeDebugger);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVT_destroy() {
    dumpn("Destroying the ToolbarView");

    this._instrumentsPaneToggleButton.removeEventListener("mousedown", this._onTogglePanesPressed, false);
    this._resumeButton.removeEventListener("mousedown", this._onResumePressed, false);
    this._stepOverButton.removeEventListener("mousedown", this._onStepOverPressed, false);
    this._stepInButton.removeEventListener("mousedown", this._onStepInPressed, false);
    this._stepOutButton.removeEventListener("mousedown", this._onStepOutPressed, false);
  },

  /**
   * Sets the resume button state based on the debugger active thread.
   *
   * @param string aState
   *        Either "paused" or "attached".
   */
  toggleResumeButtonState: function DVT_toggleResumeButtonState(aState) {
    // If we're paused, check and show a resume label on the button.
    if (aState == "paused") {
      this._resumeButton.setAttribute("checked", "true");
      this._resumeButton.setAttribute("tooltiptext", this._resumeTooltip);
    }
    // If we're attached, do the opposite.
    else if (aState == "attached") {
      this._resumeButton.removeAttribute("checked");
      this._resumeButton.setAttribute("tooltiptext", this._pauseTooltip);
    }
  },

  /**
   * Sets the chrome globals container hidden or visible. It's hidden by default.
   *
   * @param boolean aVisibleFlag
   *        Specifies the intended visibility.
   */
  toggleChromeGlobalsContainer: function DVT_toggleChromeGlobalsContainer(aVisibleFlag) {
    this._chromeGlobals.setAttribute("hidden", !aVisibleFlag);
  },

  /**
   * Listener handling the toggle button click event.
   */
  _onTogglePanesPressed: function DVT__onTogglePanesPressed() {
    DebuggerView.toggleInstrumentsPane({
      visible: DebuggerView.instrumentsPaneHidden,
      animated: true,
      delayed: true
    });
  },

  /**
   * Listener handling the pause/resume button click event.
   */
  _onResumePressed: function DVT__onResumePressed() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.resume();
    } else {
      DebuggerController.activeThread.interrupt();
    }
  },

  /**
   * Listener handling the step over button click event.
   */
  _onStepOverPressed: function DVT__onStepOverPressed() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepOver();
    }
  },

  /**
   * Listener handling the step in button click event.
   */
  _onStepInPressed: function DVT__onStepInPressed() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepIn();
    }
  },

  /**
   * Listener handling the step out button click event.
   */
  _onStepOutPressed: function DVT__onStepOutPressed() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepOut();
    }
  },

  _instrumentsPaneToggleButton: null,
  _resumeButton: null,
  _stepOverButton: null,
  _stepInButton: null,
  _stepOutButton: null,
  _chromeGlobals: null,
  _resumeTooltip: "",
  _pauseTooltip: "",
  _stepOverTooltip: "",
  _stepInTooltip: "",
  _stepOutTooltip: ""
};

/**
 * Functions handling the options UI.
 */
function OptionsView() {
  dumpn("OptionsView was instantiated");

  this._togglePauseOnExceptions = this._togglePauseOnExceptions.bind(this);
  this._toggleShowPanesOnStartup = this._toggleShowPanesOnStartup.bind(this);
  this._toggleShowVariablesOnlyEnum = this._toggleShowVariablesOnlyEnum.bind(this);
  this._toggleShowVariablesFilterBox = this._toggleShowVariablesFilterBox.bind(this);
}

OptionsView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVO_initialize() {
    dumpn("Initializing the OptionsView");

    this._button = document.getElementById("debugger-options");
    this._pauseOnExceptionsItem = document.getElementById("pause-on-exceptions");
    this._showPanesOnStartupItem = document.getElementById("show-panes-on-startup");
    this._showVariablesOnlyEnumItem = document.getElementById("show-vars-only-enum");
    this._showVariablesFilterBoxItem = document.getElementById("show-vars-filter-box");

    this._pauseOnExceptionsItem.setAttribute("checked", Prefs.pauseOnExceptions);
    this._showPanesOnStartupItem.setAttribute("checked", Prefs.panesVisibleOnStartup);
    this._showVariablesOnlyEnumItem.setAttribute("checked", Prefs.variablesOnlyEnumVisible);
    this._showVariablesFilterBoxItem.setAttribute("checked", Prefs.variablesSearchboxVisible);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVO_destroy() {
    dumpn("Destroying the OptionsView");
    // Nothing to do here yet.
  },

  /**
   * Listener handling the 'gear menu' popup showing event.
   */
  _onPopupShowing: function DVO__onPopupShowing() {
    this._button.setAttribute("open", "true");
  },

  /**
   * Listener handling the 'gear menu' popup hiding event.
   */
  _onPopupHiding: function DVO__onPopupHiding() {
    this._button.removeAttribute("open");
  },

  /**
   * Listener handling the 'pause on exceptions' menuitem command.
   */
  _togglePauseOnExceptions: function DVO__togglePauseOnExceptions() {
    DebuggerController.activeThread.pauseOnExceptions(Prefs.pauseOnExceptions =
      this._pauseOnExceptionsItem.getAttribute("checked") == "true");
  },

  /**
   * Listener handling the 'show panes on startup' menuitem command.
   */
  _toggleShowPanesOnStartup: function DVO__toggleShowPanesOnStartup() {
    Prefs.panesVisibleOnStartup =
      this._showPanesOnStartupItem.getAttribute("checked") == "true";
  },

  /**
   * Listener handling the 'show non-enumerables' menuitem command.
   */
  _toggleShowVariablesOnlyEnum: function DVO__toggleShowVariablesOnlyEnum() {
    DebuggerView.Variables.onlyEnumVisible = Prefs.variablesOnlyEnumVisible =
      this._showVariablesOnlyEnumItem.getAttribute("checked") == "true";
  },

  /**
   * Listener handling the 'show variables searchbox' menuitem command.
   */
  _toggleShowVariablesFilterBox: function DVO__toggleShowVariablesFilterBox() {
    DebuggerView.Variables.searchEnabled = Prefs.variablesSearchboxVisible =
      this._showVariablesFilterBoxItem.getAttribute("checked") == "true";
  },

  _button: null,
  _pauseOnExceptionsItem: null,
  _showPanesOnStartupItem: null,
  _showVariablesOnlyEnumItem: null,
  _showVariablesFilterBoxItem: null
};

/**
 * Functions handling the chrome globals UI.
 */
function ChromeGlobalsView() {
  dumpn("ChromeGlobalsView was instantiated");

  this._onSelect = this._onSelect.bind(this);
  this._onClick = this._onClick.bind(this);
}

create({ constructor: ChromeGlobalsView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVCG_initialize() {
    dumpn("Initializing the ChromeGlobalsView");

    this.node = document.getElementById("chrome-globals");
    this.emptyText = L10N.getStr("noGlobalsText");
    this.unavailableText = L10N.getStr("noMatchingGlobalsText");

    this.node.addEventListener("select", this._onSelect, false);
    this.node.addEventListener("click", this._onClick, false);

    // Show an empty label by default.
    this.empty();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVT_destroy() {
    dumpn("Destroying the ChromeGlobalsView");

    this.node.removeEventListener("select", this._onSelect, false);
    this.node.removeEventListener("click", this._onClick, false);
  },

  /**
   * The select listener for the chrome globals container.
   */
  _onSelect: function DVCG__onSelect() {
    if (!this.refresh()) {
      return;
    }
    // TODO: bug 806775, do something useful for chrome debugging.
  },

  /**
   * The click listener for the chrome globals container.
   */
  _onClick: function DVCG__onClick() {
    // Use this container as a filtering target.
    DebuggerView.Filtering.target = this;
  }
});

/**
 * Functions handling the stackframes UI.
 */
function StackFramesView() {
  dumpn("StackFramesView was instantiated");

  this._framesCache = new Map(); // Can't use a WeakMap because keys are numbers.
  this._onStackframeRemoved = this._onStackframeRemoved.bind(this);
  this._onClick = this._onClick.bind(this);
  this._onScroll = this._onScroll.bind(this);
  this._afterScroll = this._afterScroll.bind(this);
  this._selectFrame = this._selectFrame.bind(this);
}

create({ constructor: StackFramesView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVSF_initialize() {
    dumpn("Initializing the StackFramesView");

    let commandset = this._commandset = document.createElement("commandset");
    let menupopup = this._menupopup = document.createElement("menupopup");
    commandset.id = "stackframesCommandset";
    menupopup.id = "stackframesMenupopup";

    document.getElementById("debuggerPopupset").appendChild(menupopup);
    document.getElementById("debuggerCommands").appendChild(commandset);

    this.node = new BreadcrumbsWidget(document.getElementById("stackframes"));
    this.node.addEventListener("mousedown", this._onClick, false);
    this.node.addEventListener("scroll", this._onScroll, true);
    window.addEventListener("resize", this._onScroll, true);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVSF_destroy() {
    dumpn("Destroying the StackFramesView");

    this.node.removeEventListener("mousedown", this._onClick, false);
    this.node.removeEventListener("scroll", this._onScroll, true);
    window.removeEventListener("resize", this._onScroll, true);
  },

  /**
   * Adds a frame in this stackframes container.
   *
   * @param string aFrameTitle
   *        The frame title to be displayed in the list.
   * @param string aSourceLocation
   *        The source location to be displayed in the list.
   * @param string aLineNumber
   *        The line number to be displayed in the list.
   * @param number aDepth
   *        The frame depth specified by the debugger.
   */
  addFrame: function DVSF_addFrame(aFrameTitle, aSourceLocation, aLineNumber, aDepth) {
    // Create the element node and menu entry for the stack frame item.
    let frameView = this._createFrameView.apply(this, arguments);
    let menuEntry = this._createMenuEntry.apply(this, arguments);

    // Append a stack frame item to this container.
    let stackframeItem = this.push(frameView, {
      index: 0, /* specifies on which position should the item be appended */
      relaxed: true, /* this container should allow dupes & degenerates */
      attachment: {
        popup: menuEntry,
        depth: aDepth
      },
      attributes: [
        ["contextmenu", "stackframesMenupopup"],
        ["tooltiptext", aSourceLocation]
      ],
      // Make sure that when the stack frame item is removed, the corresponding
      // menuitem and command are also destroyed.
      finalize: this._onStackframeRemoved
    });

    this._framesCache.set(aDepth, stackframeItem);
  },

  /**
   * Highlights a frame in this stackframes container.
   *
   * @param number aDepth
   *        The frame depth specified by the debugger controller.
   */
  highlightFrame: function DVSF_highlightFrame(aDepth) {
    let selectedItem = this.selectedItem = this._framesCache.get(aDepth);

    for (let item in this) {
      if (item != selectedItem) {
        item.attachment.popup.menuitem.removeAttribute("checked");
      } else {
        item.attachment.popup.menuitem.setAttribute("checked", "");
      }
    }
  },

  /**
   * Specifies if the active thread has more frames that need to be loaded.
   */
  dirty: false,

  /**
   * Customization function for creating an item's UI.
   *
   * @param string aFrameTitle
   *        The frame title to be displayed in the list.
   * @param string aSourceLocation
   *        The source location to be displayed in the list.
   * @param string aLineNumber
   *        The line number to be displayed in the list.
   * @param number aDepth
   *        The frame depth specified by the debugger.
   * @return nsIDOMNode
   *         The stack frame view.
   */
  _createFrameView:
  function DVSF__createFrameView(aFrameTitle, aSourceLocation, aLineNumber, aDepth) {
    let frameDetails = SourceUtils.getSourceLabel(aSourceLocation,
      STACK_FRAMES_SOURCE_URL_MAX_LENGTH,
      STACK_FRAMES_SOURCE_URL_TRIM_SECTION) +
      SEARCH_LINE_FLAG + aLineNumber;

    let frameTitleNode = document.createElement("label");
    frameTitleNode.className = "plain dbg-stackframe-title breadcrumbs-widget-item-tag";
    frameTitleNode.setAttribute("value", aFrameTitle);

    let frameDetailsNode = document.createElement("label");
    frameDetailsNode.className = "plain dbg-stackframe-details breadcrumbs-widget-item-id";
    frameDetailsNode.setAttribute("value", frameDetails);

    let container = document.createElement("hbox");
    container.id = "stackframe-" + aDepth;
    container.className = "dbg-stackframe";

    container.appendChild(frameTitleNode);
    container.appendChild(frameDetailsNode);

    return container;
  },

  /**
   * Customization function for populating an item's context menu.
   *
   * @param string aFrameTitle
   *        The frame title to be displayed in the list.
   * @param string aSourceLocation
   *        The source location to be displayed in the list.
   * @param string aLineNumber
   *        The line number to be displayed in the list.
   * @param number aDepth
   *        The frame depth specified by the debugger.
   * @return object
   *         An object containing the stack frame command and menu item.
   */
  _createMenuEntry:
  function DVSF__createMenuEntry(aFrameTitle, aSourceLocation, aLineNumber, aDepth) {
    let frameDescription = SourceUtils.getSourceLabel(aSourceLocation,
      STACK_FRAMES_POPUP_SOURCE_URL_MAX_LENGTH,
      STACK_FRAMES_POPUP_SOURCE_URL_TRIM_SECTION) +
      SEARCH_LINE_FLAG + aLineNumber;

    let prefix = "sf-cMenu-"; // "stackframes context menu"
    let commandId = prefix + aDepth + "-" + "-command";
    let menuitemId = prefix + aDepth + "-" + "-menuitem";

    let command = document.createElement("command");
    command.id = commandId;
    command.addEventListener("command", this._selectFrame.bind(this, aDepth), false);

    let menuitem = document.createElement("menuitem");
    menuitem.id = menuitemId;
    menuitem.className = "dbg-stackframe-menuitem";
    menuitem.setAttribute("type", "checkbox");
    menuitem.setAttribute("command", commandId);
    menuitem.setAttribute("tooltiptext", aSourceLocation);

    let labelNode = document.createElement("label");
    labelNode.className = "plain dbg-stackframe-menuitem-title";
    labelNode.setAttribute("value", aFrameTitle);
    labelNode.setAttribute("flex", "1");

    let descriptionNode = document.createElement("label");
    descriptionNode.className = "plain dbg-stackframe-menuitem-details";
    descriptionNode.setAttribute("value", frameDescription);

    menuitem.appendChild(labelNode);
    menuitem.appendChild(descriptionNode);

    this._commandset.appendChild(command);
    this._menupopup.appendChild(menuitem);

    return {
      command: command,
      menuitem: menuitem
    };
  },

  /**
   * Destroys a context menu item for a stack frame.
   *
   * @param object aMenuEntry
   *        An object containing the stack frame command and menu item.
   */
  _destroyMenuEntry: function DVSF__destroyMenuEntry(aMenuEntry) {
    dumpn("Destroying context menu: " +
      aMenuEntry.command.id + " & " + aMenuEntry.menuitem.id);

    let command = aMenuEntry.command;
    let menuitem = aMenuEntry.menuitem;
    command.parentNode.removeChild(command);
    menuitem.parentNode.removeChild(menuitem);
  },

  /**
   * Function called each time a stack frame item is removed.
   *
   * @param MenuItem aItem
   *        The corresponding menu item.
   */
  _onStackframeRemoved: function DVSF__onStackframeRemoved(aItem) {
    dumpn("Finalizing stackframe item: " + aItem);

    let { popup, depth } = aItem.attachment;
    this._destroyMenuEntry(popup);
    this._framesCache.delete(depth);
  },

  /**
   * The click listener for the stackframes container.
   */
  _onClick: function DVSF__onClick(e) {
    if (e && e.button != 0) {
      // Only allow left-click to trigger this event.
      return;
    }
    let item = this.getItemForElement(e.target);
    if (item) {
      // The container is not empty and we clicked on an actual item.
      this._selectFrame(item.attachment.depth);
    }
  },

  /**
   * The scroll listener for the stackframes container.
   */
  _onScroll: function DVSF__onScroll() {
    // Update the stackframes container only if we have to.
    if (!this.dirty) {
      return;
    }
    window.clearTimeout(this._scrollTimeout);
    this._scrollTimeout = window.setTimeout(this._afterScroll, STACK_FRAMES_SCROLL_DELAY);
  },

  /**
   * Requests the addition of more frames from the controller.
   */
  _afterScroll: function DVSF__afterScroll() {
    let list = this.node._list;
    let scrollPosition = list.scrollPosition;
    let scrollWidth = list.scrollWidth;

    // If the stackframes container scrolled almost to the end, with only
    // 1/10 of a breadcrumb remaining, load more content.
    if (scrollPosition - scrollWidth / 10 < 1) {
      list.ensureElementIsVisible(this.getItemAtIndex(CALL_STACK_PAGE_SIZE - 1).target);
      this.dirty = false;

      // Loads more stack frames from the debugger server cache.
      DebuggerController.StackFrames.addMoreFrames();
    }
  },

  /**
   * Requests selection of a frame from the controller.
   *
   * @param number aDepth
   *        The depth of the frame in the stack.
   */
  _selectFrame: function DVSF__selectFrame(aDepth) {
    DebuggerController.StackFrames.selectFrame(aDepth);
  },

  _framesCache: null,
  _commandset: null,
  _menupopup: null,
  _scrollTimeout: null
});

/**
 * Utility functions for handling stackframes.
 */
let StackFrameUtils = {
  /**
   * Create a textual representation for the specified stack frame
   * to display in the stackframes container.
   *
   * @param object aFrame
   *        The stack frame to label.
   */
  getFrameTitle: function SFU_getFrameTitle(aFrame) {
    if (aFrame.type == "call") {
      let c = aFrame.callee;
      return (c.name || c.userDisplayName || c.displayName || "(anonymous)");
    }
    return "(" + aFrame.type + ")";
  },

  /**
   * Constructs a scope label based on its environment.
   *
   * @param object aEnv
   *        The scope's environment.
   * @return string
   *         The scope's label.
   */
  getScopeLabel: function SFU_getScopeLabel(aEnv) {
    let name = "";

    // Name the outermost scope Global.
    if (!aEnv.parent) {
      name = L10N.getStr("globalScopeLabel");
    }
    // Otherwise construct the scope name.
    else {
      name = aEnv.type.charAt(0).toUpperCase() + aEnv.type.slice(1);
    }

    let label = L10N.getFormatStr("scopeLabel", [name]);
    switch (aEnv.type) {
      case "with":
      case "object":
        label += " [" + aEnv.object.class + "]";
        break;
      case "function":
        let f = aEnv.function;
        label += " [" +
          (f.name || f.userDisplayName || f.displayName || "(anonymous)") +
        "]";
        break;
    }
    return label;
  }
};

/**
 * Functions handling the filtering UI.
 */
function FilterView() {
  dumpn("FilterView was instantiated");

  this._onClick = this._onClick.bind(this);
  this._onSearch = this._onSearch.bind(this);
  this._onKeyPress = this._onKeyPress.bind(this);
  this._onBlur = this._onBlur.bind(this);
}

FilterView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVF_initialize() {
    dumpn("Initializing the FilterView");

    this._searchbox = document.getElementById("searchbox");
    this._searchboxHelpPanel = document.getElementById("searchbox-help-panel");
    this._globalOperatorButton = document.getElementById("global-operator-button");
    this._globalOperatorLabel = document.getElementById("global-operator-label");
    this._tokenOperatorButton = document.getElementById("token-operator-button");
    this._tokenOperatorLabel = document.getElementById("token-operator-label");
    this._lineOperatorButton = document.getElementById("line-operator-button");
    this._lineOperatorLabel = document.getElementById("line-operator-label");
    this._variableOperatorButton = document.getElementById("variable-operator-button");
    this._variableOperatorLabel = document.getElementById("variable-operator-label");

    this._fileSearchKey = LayoutHelpers.prettyKey(document.getElementById("fileSearchKey"), true);
    this._globalSearchKey = LayoutHelpers.prettyKey(document.getElementById("globalSearchKey"), true);
    this._tokenSearchKey = LayoutHelpers.prettyKey(document.getElementById("tokenSearchKey"), true);
    this._lineSearchKey = LayoutHelpers.prettyKey(document.getElementById("lineSearchKey"), true);
    this._variableSearchKey = LayoutHelpers.prettyKey(document.getElementById("variableSearchKey"), true);

    this._searchbox.addEventListener("click", this._onClick, false);
    this._searchbox.addEventListener("select", this._onSearch, false);
    this._searchbox.addEventListener("input", this._onSearch, false);
    this._searchbox.addEventListener("keypress", this._onKeyPress, false);
    this._searchbox.addEventListener("blur", this._onBlur, false);

    this._globalOperatorButton.setAttribute("label", SEARCH_GLOBAL_FLAG);
    this._tokenOperatorButton.setAttribute("label", SEARCH_TOKEN_FLAG);
    this._lineOperatorButton.setAttribute("label", SEARCH_LINE_FLAG);
    this._variableOperatorButton.setAttribute("label", SEARCH_VARIABLE_FLAG);

    this._globalOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelGlobal", [this._globalSearchKey]));
    this._tokenOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelToken", [this._tokenSearchKey]));
    this._lineOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelLine", [this._lineSearchKey]));
    this._variableOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelVariable", [this._variableSearchKey]));

    // TODO: bug 806775
    // if (window._isChromeDebugger) {
    //   this.target = DebuggerView.ChromeGlobals;
    // } else {
    this.target = DebuggerView.Sources;
    // }
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVF_destroy() {
    dumpn("Destroying the FilterView");

    this._searchbox.removeEventListener("click", this._onClick, false);
    this._searchbox.removeEventListener("select", this._onSearch, false);
    this._searchbox.removeEventListener("input", this._onSearch, false);
    this._searchbox.removeEventListener("keypress", this._onKeyPress, false);
    this._searchbox.removeEventListener("blur", this._onBlur, false);
  },

  /**
   * Sets the target container to be currently filtered.
   * @param object aView
   */
  set target(aView) {
    let placeholder = "";
    switch (aView) {
      case DebuggerView.ChromeGlobals:
        placeholder = L10N.getFormatStr("emptyChromeGlobalsFilterText", [this._fileSearchKey]);
        break;
      case DebuggerView.Sources:
        placeholder = L10N.getFormatStr("emptyFilterText", [this._fileSearchKey]);
        break;
    }
    this._searchbox.setAttribute("placeholder", placeholder);
    this._target = aView;
  },

  /**
   * Gets the entered file, line and token entered in the searchbox.
   * @return array
   */
  get searchboxInfo() {
    let file, line, token, isGlobal, isVariable;

    let rawValue = this._searchbox.value;
    let rawLength = rawValue.length;
    let globalFlagIndex = rawValue.indexOf(SEARCH_GLOBAL_FLAG);
    let variableFlagIndex = rawValue.indexOf(SEARCH_VARIABLE_FLAG);
    let lineFlagIndex = rawValue.lastIndexOf(SEARCH_LINE_FLAG);
    let tokenFlagIndex = rawValue.lastIndexOf(SEARCH_TOKEN_FLAG);

    // This is not a global or variable search, allow file or line flags.
    if (globalFlagIndex != 0 && variableFlagIndex != 0) {
      let fileEnd = lineFlagIndex != -1
        ? lineFlagIndex
        : tokenFlagIndex != -1 ? tokenFlagIndex : rawLength;

      let lineEnd = tokenFlagIndex != -1
        ? tokenFlagIndex
        : rawLength;

      file = rawValue.slice(0, fileEnd);
      line = ~~(rawValue.slice(fileEnd + 1, lineEnd)) || 0;
      token = rawValue.slice(lineEnd + 1);
      isGlobal = false;
      isVariable = false;
    }
    // Global searches dissalow the use of file or line flags.
    else if (globalFlagIndex == 0) {
      file = "";
      line = 0;
      token = rawValue.slice(1);
      isGlobal = true;
      isVariable = false;
    }
    // Variable searches dissalow the use of file or line flags.
    else if (variableFlagIndex == 0) {
      file = "";
      line = 0;
      token = rawValue.slice(1);
      isGlobal = false;
      isVariable = true;
    }

    return [file, line, token, isGlobal, isVariable];
  },

  /**
   * Returns the currently searched file.
   * @return string
   */
  get searchedFile() this.searchboxInfo[0],

  /**
   * Returns the currently searched line.
   * @return number
   */
  get searchedLine() this.searchboxInfo[1],

  /**
   * Returns the currently searched token.
   * @return string
   */
  get searchedToken() this.searchboxInfo[2],

  /**
   * Clears the text from the searchbox and resets any changed view.
   */
  clearSearch: function DVF_clearSearch() {
    this._searchbox.value = "";
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Performs a file search if necessary.
   *
   * @param string aFile
   *        The source location to search for.
   */
  _performFileSearch: function DVF__performFileSearch(aFile) {
    // Don't search for files if the input hasn't changed.
    if (this._prevSearchedFile == aFile) {
      return;
    }

    // This is the target container to be currently filtered. Clicking on a
    // container generally means it should become a target.
    let view = this._target;

    // If we're not searching for a file anymore, unhide all the items.
    if (!aFile) {
      for (let item in view) {
        item.target.hidden = false;
      }
      view.refresh();
    }
    // If the searched file string is valid, hide non-matched items.
    else {
      let found = false;
      let lowerCaseFile = aFile.toLowerCase();

      for (let item in view) {
        let element = item.target;
        let lowerCaseLabel = item.label.toLowerCase();

        // Search is not case sensitive, and is tied to the label not the value.
        if (lowerCaseLabel.match(lowerCaseFile)) {
          element.hidden = false;

          // Automatically select the first match.
          if (!found) {
            found = true;
            view.selectedItem = item;
            view.refresh();
          }
        }
        // Item not matched, hide the corresponding node.
        else {
          element.hidden = true;
        }
      }
      // If no matches were found, display the appropriate info.
      if (!found) {
        view.setUnavailable();
      }
    }
    // Synchronize with the view's filtered sources container.
    DebuggerView.FilteredSources.syncFileSearch();

    // Hide all the groups with no visible children.
    view.node.hideEmptyGroups();

    // Ensure the currently selected item is visible.
    view.node.ensureSelectionIsVisible(true);

    // Remember the previously searched file to avoid redundant filtering.
    this._prevSearchedFile = aFile;
  },

  /**
   * Performs a line search if necessary.
   * (Jump to lines in the currently visible source).
   *
   * @param number aLine
   *        The source line number to jump to.
   */
  _performLineSearch: function DVF__performLineSearch(aLine) {
    // Don't search for lines if the input hasn't changed.
    if (this._prevSearchedLine != aLine && aLine) {
      DebuggerView.editor.setCaretPosition(aLine - 1);
    }
    // Can't search for lines and tokens at the same time.
    if (this._prevSearchedToken && !aLine) {
      this._target.refresh();
    }

    // Remember the previously searched line to avoid redundant filtering.
    this._prevSearchedLine = aLine;
  },

  /**
   * Performs a token search if necessary.
   * (Search for tokens in the currently visible source).
   *
   * @param string aToken
   *        The source token to find.
   */
  _performTokenSearch: function DVF__performTokenSearch(aToken) {
    // Don't search for tokens if the input hasn't changed.
    if (this._prevSearchedToken != aToken && aToken) {
      let editor = DebuggerView.editor;
      let offset = editor.find(aToken, { ignoreCase: true });
      if (offset > -1) {
        editor.setSelection(offset, offset + aToken.length)
      }
    }
    // Can't search for tokens and lines at the same time.
    if (this._prevSearchedLine && !aToken) {
      this._target.refresh();
    }

    // Remember the previously searched token to avoid redundant filtering.
    this._prevSearchedToken = aToken;
  },

  /**
   * The click listener for the search container.
   */
  _onClick: function DVF__onClick() {
    this._searchboxHelpPanel.openPopup(this._searchbox);
  },

  /**
   * The search listener for the search container.
   */
  _onSearch: function DVF__onScriptsSearch() {
    this._searchboxHelpPanel.hidePopup();
    let [file, line, token, isGlobal, isVariable] = this.searchboxInfo;

    // If this is a global search, schedule it for when the user stops typing,
    // or hide the corresponding pane otherwise.
    if (isGlobal) {
      DebuggerView.GlobalSearch.scheduleSearch(token);
      this._prevSearchedToken = token;
      return;
    }

    // If this is a variable search, defer the action to the corresponding
    // variables view instance.
    if (isVariable) {
      DebuggerView.Variables.scheduleSearch(token);
      this._prevSearchedToken = token;
      return;
    }

    DebuggerView.GlobalSearch.clearView();
    this._performFileSearch(file);
    this._performLineSearch(line);
    this._performTokenSearch(token);
  },

  /**
   * The key press listener for the search container.
   */
  _onKeyPress: function DVF__onScriptsKeyPress(e) {
    // This attribute is not implemented in Gecko at this time, see bug 680830.
    e.char = String.fromCharCode(e.charCode);

    let [file, line, token, isGlobal, isVariable] = this.searchboxInfo;
    let isFileSearch, isLineSearch, isDifferentToken, isReturnKey;
    let action = -1;

    if (file && !line && !token) {
      isFileSearch = true;
    }
    if (line && !token) {
      isLineSearch = true;
    }
    if (this._prevSearchedToken != token) {
      isDifferentToken = true;
    }

    // Meta+G and Ctrl+N focus next matches.
    if ((e.char == "g" && e.metaKey) || e.char == "n" && e.ctrlKey) {
      action = 0;
    }
    // Meta+Shift+G and Ctrl+P focus previous matches.
    else if ((e.char == "G" && e.metaKey) || e.char == "p" && e.ctrlKey) {
      action = 1;
    }
    // Return, enter, down and up keys focus next or previous matches, while
    // the escape key switches focus from the search container.
    else switch (e.keyCode) {
      case e.DOM_VK_RETURN:
      case e.DOM_VK_ENTER:
        isReturnKey = true;
        // fall through
      case e.DOM_VK_DOWN:
        action = 0;
        break;
      case e.DOM_VK_UP:
        action = 1;
        break;
      case e.DOM_VK_ESCAPE:
        action = 2;
        break;
    }

    if (action == 2) {
      DebuggerView.editor.focus();
      return;
    }
    if (action == -1 || (!file && !line && !token)) {
      DebuggerView.FilteredSources.hidden = true;
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    // Select the next or previous file search entry.
    if (isFileSearch) {
      if (isReturnKey) {
        DebuggerView.FilteredSources.hidden = true;
        DebuggerView.editor.focus();
        this.clearSearch();
      } else {
        DebuggerView.FilteredSources[["focusNext", "focusPrev"][action]]();
      }
      this._prevSearchedFile = file;
      return;
    }

    // Perform a global search based on the specified operator.
    if (isGlobal) {
      if (isReturnKey && (isDifferentToken || DebuggerView.GlobalSearch.hidden)) {
        DebuggerView.GlobalSearch.performSearch(token);
      } else {
        DebuggerView.GlobalSearch[["focusNextMatch", "focusPrevMatch"][action]]();
      }
      this._prevSearchedToken = token;
      return;
    }

    // Perform a variable search based on the specified operator.
    if (isVariable) {
      if (isReturnKey && isDifferentToken) {
        DebuggerView.Variables.performSearch(token);
      } else {
        DebuggerView.Variables.expandFirstSearchResults();
      }
      this._prevSearchedToken = token;
      return;
    }

    // Increment or decrement the specified line.
    if (isLineSearch && !isReturnKey) {
      line += action == 0 ? 1 : -1;
      let lineCount = DebuggerView.editor.getLineCount();
      let lineTarget = line < 1 ? 1 : line > lineCount ? lineCount : line;

      DebuggerView.editor.setCaretPosition(lineTarget - 1);
      this._searchbox.value = file + SEARCH_LINE_FLAG + lineTarget;
      this._prevSearchedLine = lineTarget;
      return;
    }

    let editor = DebuggerView.editor;
    let offset = editor[["findNext", "findPrevious"][action]](true);
    if (offset > -1) {
      editor.setSelection(offset, offset + token.length)
    }
  },

  /**
   * The blur listener for the search container.
   */
  _onBlur: function DVF__onBlur() {
    DebuggerView.GlobalSearch.clearView();
    DebuggerView.Variables.performSearch(null);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when a filtering key sequence was pressed.
   *
   * @param string aOperator
   *        The operator to use for filtering.
   */
  _doSearch: function DVF__doSearch(aOperator = "") {
    this._searchbox.focus();
    this._searchbox.value = aOperator;
  },

  /**
   * Called when the source location filter key sequence was pressed.
   */
  _doFileSearch: function DVF__doFileSearch() {
    this._doSearch();
    this._searchboxHelpPanel.openPopup(this._searchbox);
  },

  /**
   * Called when the global search filter key sequence was pressed.
   */
  _doGlobalSearch: function DVF__doGlobalSearch() {
    this._doSearch(SEARCH_GLOBAL_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the source token filter key sequence was pressed.
   */
  _doTokenSearch: function DVF__doTokenSearch() {
    this._doSearch(SEARCH_TOKEN_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the source line filter key sequence was pressed.
   */
  _doLineSearch: function DVF__doLineSearch() {
    this._doSearch(SEARCH_LINE_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the variable search filter key sequence was pressed.
   */
  _doVariableSearch: function DVF__doVariableSearch() {
    DebuggerView.Variables.performSearch("");
    this._doSearch(SEARCH_VARIABLE_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the variables focus key sequence was pressed.
   */
  _doVariablesFocus: function DVG__doVariablesFocus() {
    DebuggerView.showInstrumentsPane();
    DebuggerView.Variables.focusFirstVisibleNode();
  },

  _searchbox: null,
  _searchboxHelpPanel: null,
  _globalOperatorButton: null,
  _globalOperatorLabel: null,
  _tokenOperatorButton: null,
  _tokenOperatorLabel: null,
  _lineOperatorButton: null,
  _lineOperatorLabel: null,
  _variableOperatorButton: null,
  _variableOperatorLabel: null,
  _fileSearchKey: "",
  _globalSearchKey: "",
  _tokenSearchKey: "",
  _lineSearchKey: "",
  _variableSearchKey: "",
  _target: null,
  _prevSearchedFile: "",
  _prevSearchedLine: 0,
  _prevSearchedToken: ""
};

/**
 * Functions handling the filtered sources UI.
 */
function FilteredSourcesView() {
  dumpn("FilteredSourcesView was instantiated");

  this._onClick = this._onClick.bind(this);
}

create({ constructor: FilteredSourcesView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVFS_initialize() {
    dumpn("Initializing the FilteredSourcesView");

    this.node = new ListWidget(document.getElementById("filtered-sources-panel"));
    this._searchbox = document.getElementById("searchbox");

    this.node.itemFactory = this._createItemView;
    this.node.itemType = "vbox";
    this.node.addEventListener("click", this._onClick, false);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVFS_destroy() {
    dumpn("Destroying the FilteredSourcesView");

    this.node.removeEventListener("click", this._onClick, false);
  },

  /**
   * Sets the files container hidden or visible. It's hidden by default.
   * @param boolean aFlag
   */
  set hidden(aFlag) {
    if (aFlag) {
      this.node._parent.hidePopup();
    } else {
      this.node._parent.openPopup(this._searchbox);
    }
  },

  /**
   * Updates the list of sources displayed in this container.
   */
  syncFileSearch: function DVFS_syncFileSearch() {
    this.empty();

    // If there's no currently searched file, or there are no matches found,
    // hide the popup.
    if (!DebuggerView.Filtering.searchedFile ||
        !DebuggerView.Sources.visibleItems.length) {
      this.hidden = true;
      return;
    }

    // Get the currently visible items in the sources container.
    let visibleItems = DebuggerView.Sources.visibleItems;
    let displayedItems = visibleItems.slice(0, FILTERED_SOURCES_MAX_RESULTS);

    for (let item of displayedItems) {
      // Append a location item item to this container.
      let trimmedLabel = SourceUtils.trimUrlLength(item.label);
      let trimmedValue = SourceUtils.trimUrlLength(item.value);
      let locationItem = this.push([trimmedLabel, trimmedValue], {
        relaxed: true, /* this container should allow dupes & degenerates */
        attachment: {
          fullLabel: item.label,
          fullValue: item.value
        }
      });
    }

    this._updateSelection(this.getItemAtIndex(0));
    this.hidden = false;
  },

  /**
   * Focuses the next found match in this container.
   */
  focusNext: function DVFS_focusNext() {
    let nextIndex = this.selectedIndex + 1;
    if (nextIndex >= this.itemCount) {
      nextIndex = 0;
    }
    this._updateSelection(this.getItemAtIndex(nextIndex));
  },

  /**
   * Focuses the previously found match in this container.
   */
  focusPrev: function DVFS_focusPrev() {
    let prevIndex = this.selectedIndex - 1;
    if (prevIndex < 0) {
      prevIndex = this.itemCount - 1;
    }
    this._updateSelection(this.getItemAtIndex(prevIndex));
  },

  /**
   * The click listener for this container.
   */
  _onClick: function DVFS__onClick(e) {
    let locationItem = this.getItemForElement(e.target);
    if (locationItem) {
      this._updateSelection(locationItem);
      DebuggerView.Filtering.clearSearch();
    }
  },

  /**
   * Updates the selected item in this container and other views.
   *
   * @param MenuItem aItem
   *        The item associated with the element to select.
   */
  _updateSelection: function DVFS__updateSelection(aItem) {
    this.selectedItem = aItem;
    DebuggerView.Filtering._target.selectedValue = aItem.attachment.fullValue;
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
   */
  _createItemView:
  function DVFS__createItemView(aElementNode, aAttachment, aLabel, aValue) {
    let labelNode = document.createElement("label");
    labelNode.className = "plain dbg-source-item-name";
    labelNode.setAttribute("value", aLabel);

    let valueNode = document.createElement("label");
    valueNode.setAttribute("value", aValue);
    valueNode.className = "plain dbg-source-item-details";

    aElementNode.className = "light dbg-source-item";
    aElementNode.appendChild(labelNode);
    aElementNode.appendChild(valueNode);
  },

  _searchbox: null
});

/**
 * Preliminary setup for the DebuggerView object.
 */
DebuggerView.Toolbar = new ToolbarView();
DebuggerView.Options = new OptionsView();
DebuggerView.Filtering = new FilterView();
DebuggerView.FilteredSources = new FilteredSourcesView();
DebuggerView.ChromeGlobals = new ChromeGlobalsView();
DebuggerView.StackFrames = new StackFramesView();

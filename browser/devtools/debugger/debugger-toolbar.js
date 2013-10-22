/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// A time interval sufficient for the options popup panel to finish hiding
// itself.
const POPUP_HIDDEN_DELAY = 100; // ms

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
  initialize: function() {
    dumpn("Initializing the ToolbarView");

    this._instrumentsPaneToggleButton = document.getElementById("instruments-pane-toggle");
    this._resumeOrderPanel = document.getElementById("resumption-order-panel");
    this._resumeButton = document.getElementById("resume");
    this._stepOverButton = document.getElementById("step-over");
    this._stepInButton = document.getElementById("step-in");
    this._stepOutButton = document.getElementById("step-out");
    this._chromeGlobals = document.getElementById("chrome-globals");

    let resumeKey = ShortcutUtils.prettifyShortcut(document.getElementById("resumeKey"));
    let stepOverKey = ShortcutUtils.prettifyShortcut(document.getElementById("stepOverKey"));
    let stepInKey = ShortcutUtils.prettifyShortcut(document.getElementById("stepInKey"));
    let stepOutKey = ShortcutUtils.prettifyShortcut(document.getElementById("stepOutKey"));
    this._resumeTooltip = L10N.getFormatStr("resumeButtonTooltip", resumeKey);
    this._pauseTooltip = L10N.getFormatStr("pauseButtonTooltip", resumeKey);
    this._stepOverTooltip = L10N.getFormatStr("stepOverTooltip", stepOverKey);
    this._stepInTooltip = L10N.getFormatStr("stepInTooltip", stepInKey);
    this._stepOutTooltip = L10N.getFormatStr("stepOutTooltip", stepOutKey);

    this._instrumentsPaneToggleButton.addEventListener("mousedown", this._onTogglePanesPressed, false);
    this._resumeButton.addEventListener("mousedown", this._onResumePressed, false);
    this._stepOverButton.addEventListener("mousedown", this._onStepOverPressed, false);
    this._stepInButton.addEventListener("mousedown", this._onStepInPressed, false);
    this._stepOutButton.addEventListener("mousedown", this._onStepOutPressed, false);

    this._stepOverButton.setAttribute("tooltiptext", this._stepOverTooltip);
    this._stepInButton.setAttribute("tooltiptext", this._stepInTooltip);
    this._stepOutButton.setAttribute("tooltiptext", this._stepOutTooltip);

    // TODO: bug 806775 - group scripts by globals using hostAnnotations.
    // this.toggleChromeGlobalsContainer(window._isChromeDebugger);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the ToolbarView");

    this._instrumentsPaneToggleButton.removeEventListener("mousedown", this._onTogglePanesPressed, false);
    this._resumeButton.removeEventListener("mousedown", this._onResumePressed, false);
    this._stepOverButton.removeEventListener("mousedown", this._onStepOverPressed, false);
    this._stepInButton.removeEventListener("mousedown", this._onStepInPressed, false);
    this._stepOutButton.removeEventListener("mousedown", this._onStepOutPressed, false);
  },

  /**
   * Display a warning when trying to resume a debuggee while another is paused.
   * Debuggees must be unpaused in a Last-In-First-Out order.
   *
   * @param string aPausedUrl
   *        The URL of the last paused debuggee.
   */
  showResumeWarning: function(aPausedUrl) {
    let label = L10N.getFormatStr("resumptionOrderPanelTitle", aPausedUrl);
    let descriptionNode = document.getElementById("resumption-panel-desc");
    descriptionNode.setAttribute("value", label);

    this._resumeOrderPanel.openPopup(this._resumeButton);
  },

  /**
   * Sets the resume button state based on the debugger active thread.
   *
   * @param string aState
   *        Either "paused" or "attached".
   */
  toggleResumeButtonState: function(aState) {
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
  toggleChromeGlobalsContainer: function(aVisibleFlag) {
    this._chromeGlobals.setAttribute("hidden", !aVisibleFlag);
  },

  /**
   * Listener handling the toggle button click event.
   */
  _onTogglePanesPressed: function() {
    DebuggerView.toggleInstrumentsPane({
      visible: DebuggerView.instrumentsPaneHidden,
      animated: true,
      delayed: true
    });
  },

  /**
   * Listener handling the pause/resume button click event.
   */
  _onResumePressed: function() {
    if (DebuggerController.activeThread.paused) {
      let warn = DebuggerController._ensureResumptionOrder;
      DebuggerController.activeThread.resume(warn);
    } else {
      DebuggerController.activeThread.interrupt();
    }
  },

  /**
   * Listener handling the step over button click event.
   */
  _onStepOverPressed: function() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepOver();
    }
  },

  /**
   * Listener handling the step in button click event.
   */
  _onStepInPressed: function() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepIn();
    }
  },

  /**
   * Listener handling the step out button click event.
   */
  _onStepOutPressed: function() {
    if (DebuggerController.activeThread.paused) {
      DebuggerController.activeThread.stepOut();
    }
  },

  _instrumentsPaneToggleButton: null,
  _resumeOrderPanel: null,
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
  this._toggleIgnoreCaughtExceptions = this._toggleIgnoreCaughtExceptions.bind(this);
  this._toggleShowPanesOnStartup = this._toggleShowPanesOnStartup.bind(this);
  this._toggleShowVariablesOnlyEnum = this._toggleShowVariablesOnlyEnum.bind(this);
  this._toggleShowVariablesFilterBox = this._toggleShowVariablesFilterBox.bind(this);
  this._toggleShowOriginalSource = this._toggleShowOriginalSource.bind(this);
}

OptionsView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the OptionsView");

    this._button = document.getElementById("debugger-options");
    this._pauseOnExceptionsItem = document.getElementById("pause-on-exceptions");
    this._ignoreCaughtExceptionsItem = document.getElementById("ignore-caught-exceptions");
    this._showPanesOnStartupItem = document.getElementById("show-panes-on-startup");
    this._showVariablesOnlyEnumItem = document.getElementById("show-vars-only-enum");
    this._showVariablesFilterBoxItem = document.getElementById("show-vars-filter-box");
    this._showOriginalSourceItem = document.getElementById("show-original-source");

    this._pauseOnExceptionsItem.setAttribute("checked", Prefs.pauseOnExceptions);
    this._ignoreCaughtExceptionsItem.setAttribute("checked", Prefs.ignoreCaughtExceptions);
    this._showPanesOnStartupItem.setAttribute("checked", Prefs.panesVisibleOnStartup);
    this._showVariablesOnlyEnumItem.setAttribute("checked", Prefs.variablesOnlyEnumVisible);
    this._showVariablesFilterBoxItem.setAttribute("checked", Prefs.variablesSearchboxVisible);
    this._showOriginalSourceItem.setAttribute("checked", Prefs.sourceMapsEnabled);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the OptionsView");
    // Nothing to do here yet.
  },

  /**
   * Listener handling the 'gear menu' popup showing event.
   */
  _onPopupShowing: function() {
    this._button.setAttribute("open", "true");
    window.emit(EVENTS.OPTIONS_POPUP_SHOWING);
  },

  /**
   * Listener handling the 'gear menu' popup hiding event.
   */
  _onPopupHiding: function() {
    this._button.removeAttribute("open");
  },

  /**
   * Listener handling the 'gear menu' popup hidden event.
   */
  _onPopupHidden: function() {
    window.emit(EVENTS.OPTIONS_POPUP_HIDDEN);
  },

  /**
   * Listener handling the 'pause on exceptions' menuitem command.
   */
  _togglePauseOnExceptions: function() {
    Prefs.pauseOnExceptions =
      this._pauseOnExceptionsItem.getAttribute("checked") == "true";

    DebuggerController.activeThread.pauseOnExceptions(
      Prefs.pauseOnExceptions,
      Prefs.ignoreCaughtExceptions);
  },

  _toggleIgnoreCaughtExceptions: function() {
    Prefs.ignoreCaughtExceptions =
      this._ignoreCaughtExceptionsItem.getAttribute("checked") == "true";

    DebuggerController.activeThread.pauseOnExceptions(
      Prefs.pauseOnExceptions,
      Prefs.ignoreCaughtExceptions);
  },

  /**
   * Listener handling the 'show panes on startup' menuitem command.
   */
  _toggleShowPanesOnStartup: function() {
    Prefs.panesVisibleOnStartup =
      this._showPanesOnStartupItem.getAttribute("checked") == "true";
  },

  /**
   * Listener handling the 'show non-enumerables' menuitem command.
   */
  _toggleShowVariablesOnlyEnum: function() {
    let pref = Prefs.variablesOnlyEnumVisible =
      this._showVariablesOnlyEnumItem.getAttribute("checked") == "true";

    DebuggerView.Variables.onlyEnumVisible = pref;
  },

  /**
   * Listener handling the 'show variables searchbox' menuitem command.
   */
  _toggleShowVariablesFilterBox: function() {
    let pref = Prefs.variablesSearchboxVisible =
      this._showVariablesFilterBoxItem.getAttribute("checked") == "true";

    DebuggerView.Variables.searchEnabled = pref;
  },

  /**
   * Listener handling the 'show original source' menuitem command.
   */
  _toggleShowOriginalSource: function() {
    let pref = Prefs.sourceMapsEnabled =
      this._showOriginalSourceItem.getAttribute("checked") == "true";

    // Don't block the UI while reconfiguring the server.
    window.once(EVENTS.OPTIONS_POPUP_HIDDEN, () => {
      // The popup panel needs more time to hide after triggering onpopuphidden.
      window.setTimeout(() => {
        DebuggerController.reconfigureThread(pref);
      }, POPUP_HIDDEN_DELAY);
    }, false);
  },

  _button: null,
  _pauseOnExceptionsItem: null,
  _showPanesOnStartupItem: null,
  _showVariablesOnlyEnumItem: null,
  _showVariablesFilterBoxItem: null,
  _showOriginalSourceItem: null
};

/**
 * Functions handling the chrome globals UI.
 */
function ChromeGlobalsView() {
  dumpn("ChromeGlobalsView was instantiated");

  this._onSelect = this._onSelect.bind(this);
  this._onClick = this._onClick.bind(this);
}

ChromeGlobalsView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the ChromeGlobalsView");

    this.widget = document.getElementById("chrome-globals");
    this.emptyText = L10N.getStr("noGlobalsText");

    this.widget.addEventListener("select", this._onSelect, false);
    this.widget.addEventListener("click", this._onClick, false);

    // Show an empty label by default.
    this.empty();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the ChromeGlobalsView");

    this.widget.removeEventListener("select", this._onSelect, false);
    this.widget.removeEventListener("click", this._onClick, false);
  },

  /**
   * The select listener for the chrome globals container.
   */
  _onSelect: function() {
    // TODO: bug 806775, do something useful for chrome debugging.
  },

  /**
   * The click listener for the chrome globals container.
   */
  _onClick: function() {
    // Use this container as a filtering target.
    DebuggerView.Filtering.target = this;
  }
});

/**
 * Functions handling the stackframes UI.
 */
function StackFramesView() {
  dumpn("StackFramesView was instantiated");

  this._onStackframeRemoved = this._onStackframeRemoved.bind(this);
  this._onSelect = this._onSelect.bind(this);
  this._onScroll = this._onScroll.bind(this);
  this._afterScroll = this._afterScroll.bind(this);
}

StackFramesView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the StackFramesView");

    let commandset = this._commandset = document.createElement("commandset");
    let menupopup = this._menupopup = document.createElement("menupopup");
    commandset.id = "stackframesCommandset";
    menupopup.id = "stackframesMenupopup";

    document.getElementById("debuggerPopupset").appendChild(menupopup);
    document.getElementById("debuggerCommands").appendChild(commandset);

    this.widget = new BreadcrumbsWidget(document.getElementById("stackframes"));
    this.widget.addEventListener("select", this._onSelect, false);
    this.widget.addEventListener("scroll", this._onScroll, true);
    window.addEventListener("resize", this._onScroll, true);

    this.autoFocusOnFirstItem = false;
    this.autoFocusOnSelection = false;
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the StackFramesView");

    this.widget.removeEventListener("select", this._onSelect, false);
    this.widget.removeEventListener("scroll", this._onScroll, true);
    window.removeEventListener("resize", this._onScroll, true);
  },

  /**
   * Adds a frame in this stackframes container.
   *
   * @param string aTitle
   *        The frame title (function name).
   * @param string aUrl
   *        The frame source url.
   * @param string aLine
   *        The frame line number.
   * @param number aDepth
   *        The frame depth in the stack.
   * @param boolean aIsBlackBoxed
   *        Whether or not the frame is black boxed.
   */
  addFrame: function(aTitle, aUrl, aLine, aDepth, aIsBlackBoxed) {
    // Blackboxed stack frames are collapsed into a single entry in
    // the view. By convention, only the first frame is displayed.
    if (aIsBlackBoxed) {
      if (this._prevBlackBoxedUrl == aUrl) {
        return;
      }
      this._prevBlackBoxedUrl = aUrl;
    } else {
      this._prevBlackBoxedUrl = null;
    }

    // Create the element node and menu entry for the stack frame item.
    let frameView = this._createFrameView.apply(this, arguments);
    let menuEntry = this._createMenuEntry.apply(this, arguments);

    // Append a stack frame item to this container.
    this.push([frameView, aTitle, aUrl], {
      index: 0, /* specifies on which position should the item be appended */
      attachment: {
        popup: menuEntry,
        depth: aDepth
      },
      attributes: [
        ["contextmenu", "stackframesMenupopup"]
      ],
      // Make sure that when the stack frame item is removed, the corresponding
      // menuitem and command are also destroyed.
      finalize: this._onStackframeRemoved
    });
  },

  /**
   * Selects the frame at the specified depth in this container.
   * @param number aDepth
   */
  set selectedDepth(aDepth) {
    this.selectedItem = aItem => aItem.attachment.depth == aDepth;
  },

  /**
   * Specifies if the active thread has more frames that need to be loaded.
   */
  dirty: false,

  /**
   * Customization function for creating an item's UI.
   *
   * @param string aTitle
   *        The frame title to be displayed in the list.
   * @param string aUrl
   *        The frame source url.
   * @param string aLine
   *        The frame line number.
   * @param number aDepth
   *        The frame depth in the stack.
   * @param boolean aIsBlackBoxed
   *        Whether or not the frame is black boxed.
   * @return nsIDOMNode
   *         The stack frame view.
   */
  _createFrameView: function(aTitle, aUrl, aLine, aDepth, aIsBlackBoxed) {
    let container = document.createElement("hbox");
    container.id = "stackframe-" + aDepth;
    container.className = "dbg-stackframe";

    let frameDetails = SourceUtils.trimUrlLength(
      SourceUtils.getSourceLabel(aUrl),
      STACK_FRAMES_SOURCE_URL_MAX_LENGTH,
      STACK_FRAMES_SOURCE_URL_TRIM_SECTION);

    if (aIsBlackBoxed) {
      container.classList.add("dbg-stackframe-black-boxed");
    } else {
      let frameTitleNode = document.createElement("label");
      frameTitleNode.className = "plain dbg-stackframe-title breadcrumbs-widget-item-tag";
      frameTitleNode.setAttribute("value", aTitle);
      container.appendChild(frameTitleNode);

      frameDetails += SEARCH_LINE_FLAG + aLine;
    }

    let frameDetailsNode = document.createElement("label");
    frameDetailsNode.className = "plain dbg-stackframe-details breadcrumbs-widget-item-id";
    frameDetailsNode.setAttribute("value", frameDetails);
    container.appendChild(frameDetailsNode);

    return container;
  },

  /**
   * Customization function for populating an item's context menu.
   *
   * @param string aTitle
   *        The frame title to be displayed in the list.
   * @param string aUrl
   *        The frame source url.
   * @param string aLine
   *        The frame line number.
   * @param number aDepth
   *        The frame depth in the stack.
   * @param boolean aIsBlackBoxed
   *        Whether or not the frame is black boxed.
   * @return object
   *         An object containing the stack frame command and menu item.
   */
  _createMenuEntry: function(aTitle, aUrl, aLine, aDepth, aIsBlackBoxed) {
    let frameDescription = SourceUtils.trimUrlLength(
      SourceUtils.getSourceLabel(aUrl),
      STACK_FRAMES_POPUP_SOURCE_URL_MAX_LENGTH,
      STACK_FRAMES_POPUP_SOURCE_URL_TRIM_SECTION) +
      SEARCH_LINE_FLAG + aLine;

    let prefix = "sf-cMenu-"; // "stackframes context menu"
    let commandId = prefix + aDepth + "-" + "-command";
    let menuitemId = prefix + aDepth + "-" + "-menuitem";

    let command = document.createElement("command");
    command.id = commandId;
    command.addEventListener("command", () => this.selectedDepth = aDepth, false);

    let menuitem = document.createElement("menuitem");
    menuitem.id = menuitemId;
    menuitem.className = "dbg-stackframe-menuitem";
    menuitem.setAttribute("type", "checkbox");
    menuitem.setAttribute("command", commandId);
    menuitem.setAttribute("tooltiptext", aUrl);

    let labelNode = document.createElement("label");
    labelNode.className = "plain dbg-stackframe-menuitem-title";
    labelNode.setAttribute("value", aTitle);
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
   * Function called each time a stack frame item is removed.
   *
   * @param object aItem
   *        The corresponding item.
   */
  _onStackframeRemoved: function(aItem) {
    dumpn("Finalizing stackframe item: " + aItem);

    // Destroy the context menu item for the stack frame.
    let contextItem = aItem.attachment.popup;
    contextItem.command.remove();
    contextItem.menuitem.remove();

    // Forget the previously blackboxed stack frame url.
    this._prevBlackBoxedUrl = null;
  },

  /**
   * The select listener for the stackframes container.
   */
  _onSelect: function(e) {
    let stackframeItem = this.selectedItem;
    if (stackframeItem) {
      // The container is not empty and an actual item was selected.
      DebuggerController.StackFrames.selectFrame(stackframeItem.attachment.depth);

      // Update the context menu to show the currently selected stackframe item
      // as a checked entry.
      for (let otherItem in this) {
        if (otherItem != stackframeItem) {
          otherItem.attachment.popup.menuitem.removeAttribute("checked");
        } else {
          otherItem.attachment.popup.menuitem.setAttribute("checked", "");
        }
      }
    }
  },

  /**
   * The scroll listener for the stackframes container.
   */
  _onScroll: function() {
    // Update the stackframes container only if we have to.
    if (!this.dirty) {
      return;
    }
    // Allow requests to settle down first.
    setNamedTimeout("stack-scroll", STACK_FRAMES_SCROLL_DELAY, this._afterScroll);
  },

  /**
   * Requests the addition of more frames from the controller.
   */
  _afterScroll: function() {
    // TODO: Accessing private widget properties. Figure out what's the best
    // way to expose such things. Bug 876271.
    let list = this.widget._list;
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

  _commandset: null,
  _menupopup: null,
  _prevBlackBoxedUrl: null
});

/**
 * Functions handling the filtering UI.
 */
function FilterView() {
  dumpn("FilterView was instantiated");

  this._onClick = this._onClick.bind(this);
  this._onInput = this._onInput.bind(this);
  this._onKeyPress = this._onKeyPress.bind(this);
  this._onBlur = this._onBlur.bind(this);
}

FilterView.prototype = {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the FilterView");

    this._searchbox = document.getElementById("searchbox");
    this._searchboxHelpPanel = document.getElementById("searchbox-help-panel");
    this._filterLabel = document.getElementById("filter-label");
    this._globalOperatorButton = document.getElementById("global-operator-button");
    this._globalOperatorLabel = document.getElementById("global-operator-label");
    this._functionOperatorButton = document.getElementById("function-operator-button");
    this._functionOperatorLabel = document.getElementById("function-operator-label");
    this._tokenOperatorButton = document.getElementById("token-operator-button");
    this._tokenOperatorLabel = document.getElementById("token-operator-label");
    this._lineOperatorButton = document.getElementById("line-operator-button");
    this._lineOperatorLabel = document.getElementById("line-operator-label");
    this._variableOperatorButton = document.getElementById("variable-operator-button");
    this._variableOperatorLabel = document.getElementById("variable-operator-label");

    this._fileSearchKey = ShortcutUtils.prettifyShortcut(document.getElementById("fileSearchKey"));
    this._globalSearchKey = ShortcutUtils.prettifyShortcut(document.getElementById("globalSearchKey"));
    this._filteredFunctionsKey = ShortcutUtils.prettifyShortcut(document.getElementById("functionSearchKey"));
    this._tokenSearchKey = ShortcutUtils.prettifyShortcut(document.getElementById("tokenSearchKey"));
    this._lineSearchKey = ShortcutUtils.prettifyShortcut(document.getElementById("lineSearchKey"));
    this._variableSearchKey = ShortcutUtils.prettifyShortcut(document.getElementById("variableSearchKey"));

    this._searchbox.addEventListener("click", this._onClick, false);
    this._searchbox.addEventListener("select", this._onInput, false);
    this._searchbox.addEventListener("input", this._onInput, false);
    this._searchbox.addEventListener("keypress", this._onKeyPress, false);
    this._searchbox.addEventListener("blur", this._onBlur, false);

    this._globalOperatorButton.setAttribute("label", SEARCH_GLOBAL_FLAG);
    this._functionOperatorButton.setAttribute("label", SEARCH_FUNCTION_FLAG);
    this._tokenOperatorButton.setAttribute("label", SEARCH_TOKEN_FLAG);
    this._lineOperatorButton.setAttribute("label", SEARCH_LINE_FLAG);
    this._variableOperatorButton.setAttribute("label", SEARCH_VARIABLE_FLAG);

    this._filterLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelFilter", this._fileSearchKey));
    this._globalOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelGlobal", this._globalSearchKey));
    this._functionOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelFunction", this._filteredFunctionsKey));
    this._tokenOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelToken", this._tokenSearchKey));
    this._lineOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelGoToLine", this._lineSearchKey));
    this._variableOperatorLabel.setAttribute("value",
      L10N.getFormatStr("searchPanelVariable", this._variableSearchKey));

    // TODO: bug 806775 - group scripts by globals using hostAnnotations.
    // if (window._isChromeDebugger) {
    //   this.target = DebuggerView.ChromeGlobals;
    // } else {
    this.target = DebuggerView.Sources;
    // }
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the FilterView");

    this._searchbox.removeEventListener("click", this._onClick, false);
    this._searchbox.removeEventListener("select", this._onInput, false);
    this._searchbox.removeEventListener("input", this._onInput, false);
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
        placeholder = L10N.getFormatStr("emptyChromeGlobalsFilterText", this._fileSearchKey);
        break;
      case DebuggerView.Sources:
        placeholder = L10N.getFormatStr("emptySearchText", this._fileSearchKey);
        break;
    }
    this._searchbox.setAttribute("placeholder", placeholder);
    this._target = aView;
  },

  /**
   * Gets the target container to be currently filtered.
   * @return object
   */
  get target() this._target,

  /**
   * Gets the entered operator and arguments in the searchbox.
   * @return array
   */
  get searchData() {
    let operator = "", args = [];

    let rawValue = this._searchbox.value;
    let rawLength = rawValue.length;
    let globalFlagIndex = rawValue.indexOf(SEARCH_GLOBAL_FLAG);
    let functionFlagIndex = rawValue.indexOf(SEARCH_FUNCTION_FLAG);
    let variableFlagIndex = rawValue.indexOf(SEARCH_VARIABLE_FLAG);
    let tokenFlagIndex = rawValue.lastIndexOf(SEARCH_TOKEN_FLAG);
    let lineFlagIndex = rawValue.lastIndexOf(SEARCH_LINE_FLAG);

    // This is not a global, function or variable search, allow file/line flags.
    if (globalFlagIndex != 0 && functionFlagIndex != 0 && variableFlagIndex != 0) {
      // Token search has precedence over line search.
      if (tokenFlagIndex != -1) {
        operator = SEARCH_TOKEN_FLAG;
        args.push(rawValue.slice(0, tokenFlagIndex)); // file
        args.push(rawValue.substr(tokenFlagIndex + 1, rawLength)); // token
      } else if (lineFlagIndex != -1) {
        operator = SEARCH_LINE_FLAG;
        args.push(rawValue.slice(0, lineFlagIndex)); // file
        args.push(+rawValue.substr(lineFlagIndex + 1, rawLength) || 0); // line
      } else {
        args.push(rawValue);
      }
    }
    // Global searches dissalow the use of file or line flags.
    else if (globalFlagIndex == 0) {
      operator = SEARCH_GLOBAL_FLAG;
      args.push(rawValue.slice(1));
    }
    // Function searches dissalow the use of file or line flags.
    else if (functionFlagIndex == 0) {
      operator = SEARCH_FUNCTION_FLAG;
      args.push(rawValue.slice(1));
    }
    // Variable searches dissalow the use of file or line flags.
    else if (variableFlagIndex == 0) {
      operator = SEARCH_VARIABLE_FLAG;
      args.push(rawValue.slice(1));
    }

    return [operator, args];
  },

  /**
   * Returns the current search operator.
   * @return string
   */
  get searchOperator() this.searchData[0],

  /**
   * Returns the current search arguments.
   * @return array
   */
  get searchArguments() this.searchData[1],

  /**
   * Clears the text from the searchbox and any changed views.
   */
  clearSearch: function() {
    this._searchbox.value = "";
    this.clearViews();
  },

  /**
   * Clears all the views that may pop up when searching.
   */
  clearViews: function() {
    DebuggerView.GlobalSearch.clearView();
    DebuggerView.FilteredSources.clearView();
    DebuggerView.FilteredFunctions.clearView();
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Performs a line search if necessary.
   * (Jump to lines in the currently visible source).
   *
   * @param number aLine
   *        The source line number to jump to.
   */
  _performLineSearch: function(aLine) {
    // Make sure we're actually searching for a valid line.
    if (!aLine) {
      return;
    }
    DebuggerView.editor.setCaretPosition(aLine - 1);
  },

  /**
   * Performs a token search if necessary.
   * (Search for tokens in the currently visible source).
   *
   * @param string aToken
   *        The source token to find.
   */
  _performTokenSearch: function(aToken) {
    // Make sure we're actually searching for a valid token.
    if (!aToken) {
      return;
    }
    let offset = DebuggerView.editor.find(aToken, { ignoreCase: true });
    if (offset > -1) {
      DebuggerView.editor.setSelection(offset, offset + aToken.length)
    }
  },

  /**
   * The click listener for the search container.
   */
  _onClick: function() {
    // If there's some text in the searchbox, displaying a panel would
    // interfere with double/triple click default behaviors.
    if (!this._searchbox.value) {
      this._searchboxHelpPanel.openPopup(this._searchbox);
    }
  },

  /**
   * The input listener for the search container.
   */
  _onInput: function() {
    this.clearViews();

    // Make sure we're actually searching for something.
    if (!this._searchbox.value) {
      return;
    }

    // Perform the required search based on the specified operator.
    switch (this.searchOperator) {
      case SEARCH_GLOBAL_FLAG:
        // Schedule a global search for when the user stops typing.
        DebuggerView.GlobalSearch.scheduleSearch(this.searchArguments[0]);
        break;
      case SEARCH_FUNCTION_FLAG:
        // Schedule a function search for when the user stops typing.
        DebuggerView.FilteredFunctions.scheduleSearch(this.searchArguments[0]);
        break;
      case SEARCH_VARIABLE_FLAG:
        // Schedule a variable search for when the user stops typing.
        DebuggerView.Variables.scheduleSearch(this.searchArguments[0]);
        break;
      case SEARCH_TOKEN_FLAG:
        // Schedule a file+token search for when the user stops typing.
        DebuggerView.FilteredSources.scheduleSearch(this.searchArguments[0]);
        this._performTokenSearch(this.searchArguments[1]);
        break;
      case SEARCH_LINE_FLAG:
        // Schedule a file+line search for when the user stops typing.
        DebuggerView.FilteredSources.scheduleSearch(this.searchArguments[0]);
        this._performLineSearch(this.searchArguments[1]);
        break;
      default:
        // Schedule a file only search for when the user stops typing.
        DebuggerView.FilteredSources.scheduleSearch(this.searchArguments[0]);
        break;
    }
  },

  /**
   * The key press listener for the search container.
   */
  _onKeyPress: function(e) {
    // This attribute is not implemented in Gecko at this time, see bug 680830.
    e.char = String.fromCharCode(e.charCode);

    // Perform the required action based on the specified operator.
    let [operator, args] = this.searchData;
    let isGlobalSearch = operator == SEARCH_GLOBAL_FLAG;
    let isFunctionSearch = operator == SEARCH_FUNCTION_FLAG;
    let isVariableSearch = operator == SEARCH_VARIABLE_FLAG;
    let isTokenSearch = operator == SEARCH_TOKEN_FLAG;
    let isLineSearch = operator == SEARCH_LINE_FLAG;
    let isFileOnlySearch = !operator && args.length == 1;

    // Depending on the pressed keys, determine to correct action to perform.
    let actionToPerform;

    // Meta+G and Ctrl+N focus next matches.
    if ((e.char == "g" && e.metaKey) || e.char == "n" && e.ctrlKey) {
      actionToPerform = "selectNext";
    }
    // Meta+Shift+G and Ctrl+P focus previous matches.
    else if ((e.char == "G" && e.metaKey) || e.char == "p" && e.ctrlKey) {
      actionToPerform = "selectPrev";
    }
    // Return, enter, down and up keys focus next or previous matches, while
    // the escape key switches focus from the search container.
    else switch (e.keyCode) {
      case e.DOM_VK_RETURN:
      case e.DOM_VK_ENTER:
        var isReturnKey = true;
        // If the shift key is pressed, focus on the previous result
        actionToPerform = e.shiftKey ? "selectPrev" : "selectNext";
        break;
      case e.DOM_VK_DOWN:
        actionToPerform = "selectNext";
        break;
      case e.DOM_VK_UP:
        actionToPerform = "selectPrev";
        break;
    }

    // If there's no action to perform, or no operator, file line or token
    // were specified, then this is either a broken or empty search.
    if (!actionToPerform || (!operator && !args.length)) {
      DebuggerView.editor.dropSelection();
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    // Jump to the next/previous entry in the global search, or perform
    // a new global search immediately
    if (isGlobalSearch) {
      let targetView = DebuggerView.GlobalSearch;
      if (!isReturnKey) {
        targetView[actionToPerform]();
      } else if (targetView.hidden) {
        targetView.scheduleSearch(args[0], 0);
      }
      return;
    }

    // Jump to the next/previous entry in the function search, perform
    // a new function search immediately, or clear it.
    if (isFunctionSearch) {
      let targetView = DebuggerView.FilteredFunctions;
      if (!isReturnKey) {
        targetView[actionToPerform]();
      } else if (targetView.hidden) {
        targetView.scheduleSearch(args[0], 0);
      } else {
        this.clearSearch();
      }
      return;
    }

    // Perform a new variable search immediately.
    if (isVariableSearch) {
      let targetView = DebuggerView.Variables;
      if (isReturnKey) {
        targetView.scheduleSearch(args[0], 0);
      }
      return;
    }

    // Jump to the next/previous entry in the file search, perform
    // a new file search immediately, or clear it.
    if (isFileOnlySearch) {
      let targetView = DebuggerView.FilteredSources;
      if (!isReturnKey) {
        targetView[actionToPerform]();
      } else if (targetView.hidden) {
        targetView.scheduleSearch(args[0], 0);
      } else {
        this.clearSearch();
      }
      return;
    }

    // Jump to the next/previous instance of the currently searched token.
    if (isTokenSearch) {
      let [, token] = args;
      let methods = { selectNext: "findNext", selectPrev: "findPrevious" };

      // Search for the token and select it.
      let offset = DebuggerView.editor[methods[actionToPerform]](true);
      if (offset > -1) {
        DebuggerView.editor.setSelection(offset, offset + token.length)
      }
      return;
    }

    // Increment/decrement the currently searched caret line.
    if (isLineSearch) {
      let [, line] = args;
      let amounts = { selectNext: 1, selectPrev: -1 };

      // Modify the line number and jump to it.
      line += !isReturnKey ? amounts[actionToPerform] : 0;
      let lineCount = DebuggerView.editor.getLineCount();
      let lineTarget = line < 1 ? 1 : line > lineCount ? lineCount : line;
      this._doSearch(SEARCH_LINE_FLAG, lineTarget);
      return;
    }
  },

  /**
   * The blur listener for the search container.
   */
  _onBlur: function() {
    this.clearViews();
  },

  /**
   * Called when a filtering key sequence was pressed.
   *
   * @param string aOperator
   *        The operator to use for filtering.
   */
  _doSearch: function(aOperator = "", aText = "") {
    this._searchbox.focus();
    this._searchbox.value = ""; // Need to clear value beforehand. Bug 779738.
    this._searchbox.value = aOperator + (aText || DebuggerView.editor.getSelectedText());
  },

  /**
   * Called when the source location filter key sequence was pressed.
   */
  _doFileSearch: function() {
    this._doSearch();
    this._searchboxHelpPanel.openPopup(this._searchbox);
  },

  /**
   * Called when the global search filter key sequence was pressed.
   */
  _doGlobalSearch: function() {
    this._doSearch(SEARCH_GLOBAL_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the source function filter key sequence was pressed.
   */
  _doFunctionSearch: function() {
    this._doSearch(SEARCH_FUNCTION_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the source token filter key sequence was pressed.
   */
  _doTokenSearch: function() {
    this._doSearch(SEARCH_TOKEN_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the source line filter key sequence was pressed.
   */
  _doLineSearch: function() {
    this._doSearch(SEARCH_LINE_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the variable search filter key sequence was pressed.
   */
  _doVariableSearch: function() {
    this._doSearch(SEARCH_VARIABLE_FLAG);
    this._searchboxHelpPanel.hidePopup();
  },

  /**
   * Called when the variables focus key sequence was pressed.
   */
  _doVariablesFocus: function() {
    DebuggerView.showInstrumentsPane();
    DebuggerView.Variables.focusFirstVisibleItem();
  },

  _searchbox: null,
  _searchboxHelpPanel: null,
  _globalOperatorButton: null,
  _globalOperatorLabel: null,
  _functionOperatorButton: null,
  _functionOperatorLabel: null,
  _tokenOperatorButton: null,
  _tokenOperatorLabel: null,
  _lineOperatorButton: null,
  _lineOperatorLabel: null,
  _variableOperatorButton: null,
  _variableOperatorLabel: null,
  _fileSearchKey: "",
  _globalSearchKey: "",
  _filteredFunctionsKey: "",
  _tokenSearchKey: "",
  _lineSearchKey: "",
  _variableSearchKey: "",
  _target: null
};

/**
 * Functions handling the filtered sources UI.
 */
function FilteredSourcesView() {
  dumpn("FilteredSourcesView was instantiated");

  this._onClick = this._onClick.bind(this);
  this._onSelect = this._onSelect.bind(this);
}

FilteredSourcesView.prototype = Heritage.extend(ResultsPanelContainer.prototype, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the FilteredSourcesView");

    this.anchor = document.getElementById("searchbox");
    this.widget.addEventListener("select", this._onSelect, false);
    this.widget.addEventListener("click", this._onClick, false);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the FilteredSourcesView");

    this.widget.removeEventListener("select", this._onSelect, false);
    this.widget.removeEventListener("click", this._onClick, false);
    this.anchor = null;
  },

  /**
   * Schedules searching for a source.
   *
   * @param string aToken
   *        The function to search for.
   * @param number aWait
   *        The amount of milliseconds to wait until draining.
   */
  scheduleSearch: function(aToken, aWait) {
    // The amount of time to wait for the requests to settle.
    let maxDelay = FILE_SEARCH_ACTION_MAX_DELAY;
    let delay = aWait === undefined ? maxDelay / aToken.length : aWait;

    // Allow requests to settle down first.
    setNamedTimeout("sources-search", delay, () => this._doSearch(aToken));
  },

  /**
   * Finds file matches in all the displayed sources.
   *
   * @param string aToken
   *        The string to search for.
   */
  _doSearch: function(aToken, aStore = []) {
    // Don't continue filtering if the searched token is an empty string.
    // In contrast with function searching, in this case we don't want to
    // show a list of all the files when no search token was supplied.
    if (!aToken) {
      return;
    }

    for (let item of DebuggerView.Sources.items) {
      let lowerCaseLabel = item.label.toLowerCase();
      let lowerCaseToken = aToken.toLowerCase();
      if (lowerCaseLabel.match(lowerCaseToken)) {
        aStore.push(item);
      }

      // Once the maximum allowed number of results is reached, proceed
      // with building the UI immediately.
      if (aStore.length >= RESULTS_PANEL_MAX_RESULTS) {
        this._syncView(aStore);
        return;
      }
    }

    // Couldn't reach the maximum allowed number of results, but that's ok,
    // continue building the UI.
    this._syncView(aStore);
  },

  /**
   * Updates the list of sources displayed in this container.
   *
   * @param array aSearchResults
   *        The results array, containing search details for each source.
   */
  _syncView: function(aSearchResults) {
    // If there are no matches found, keep the popup hidden and avoid
    // creating the view.
    if (!aSearchResults.length) {
      window.emit(EVENTS.FILE_SEARCH_MATCH_NOT_FOUND);
      return;
    }

    for (let item of aSearchResults) {
      // Append a location item to this container for each match.
      let trimmedLabel = SourceUtils.trimUrlLength(item.label);
      let trimmedValue = SourceUtils.trimUrlLength(item.value, 0, "start");

      this.push([trimmedLabel, trimmedValue], {
        index: -1, /* specifies on which position should the item be appended */
        relaxed: true, /* this container should allow dupes & degenerates */
        attachment: {
          url: item.value
        }
      });
    }

    // Select the first entry in this container.
    this.selectedIndex = 0;
    this.hidden = false;

    // Signal that file search matches were found and displayed.
    window.emit(EVENTS.FILE_SEARCH_MATCH_FOUND);
  },

  /**
   * The click listener for this container.
   */
  _onClick: function(e) {
    let locationItem = this.getItemForElement(e.target);
    if (locationItem) {
      this.selectedItem = locationItem;
      DebuggerView.Filtering.clearSearch();
    }
  },

  /**
   * The select listener for this container.
   *
   * @param object aItem
   *        The item associated with the element to select.
   */
  _onSelect: function({ detail: locationItem }) {
    if (locationItem) {
      DebuggerView.setEditorLocation(locationItem.attachment.url, undefined, {
        noCaret: true,
        noDebug: true
      });
    }
  }
});

/**
 * Functions handling the function search UI.
 */
function FilteredFunctionsView() {
  dumpn("FilteredFunctionsView was instantiated");

  this._onClick = this._onClick.bind(this);
  this._onSelect = this._onSelect.bind(this);
}

FilteredFunctionsView.prototype = Heritage.extend(ResultsPanelContainer.prototype, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the FilteredFunctionsView");

    this.anchor = document.getElementById("searchbox");
    this.widget.addEventListener("select", this._onSelect, false);
    this.widget.addEventListener("click", this._onClick, false);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the FilteredFunctionsView");

    this.widget.removeEventListener("select", this._onSelect, false);
    this.widget.removeEventListener("click", this._onClick, false);
    this.anchor = null;
  },

  /**
   * Schedules searching for a function in all of the sources.
   *
   * @param string aToken
   *        The function to search for.
   * @param number aWait
   *        The amount of milliseconds to wait until draining.
   */
  scheduleSearch: function(aToken, aWait) {
    // The amount of time to wait for the requests to settle.
    let maxDelay = FUNCTION_SEARCH_ACTION_MAX_DELAY;
    let delay = aWait === undefined ? maxDelay / aToken.length : aWait;

    // Allow requests to settle down first.
    setNamedTimeout("function-search", delay, () => {
      // Start fetching as many sources as possible, then perform the search.
      let urls = DebuggerView.Sources.values;
      let sourcesFetched = DebuggerController.SourceScripts.getTextForSources(urls);
      sourcesFetched.then(aSources => this._doSearch(aToken, aSources));
    });
  },

  /**
   * Finds function matches in all the sources stored in the cache, and groups
   * them by location and line number.
   *
   * @param string aToken
   *        The string to search for.
   * @param array aSources
   *        An array of [url, text] tuples for each source.
   */
  _doSearch: function(aToken, aSources, aStore = []) {
    // Continue parsing even if the searched token is an empty string, to
    // cache the syntax tree nodes generated by the reflection API.

    // Make sure the currently displayed source is parsed first. Once the
    // maximum allowed number of resutls are found, parsing will be halted.
    let currentUrl = DebuggerView.Sources.selectedValue;
    let currentSource = aSources.filter(([sourceUrl]) => sourceUrl == currentUrl)[0];
    aSources.splice(aSources.indexOf(currentSource), 1);
    aSources.unshift(currentSource);

    // If not searching for a specific function, only parse the displayed source,
    // which is now the first item in the sources array.
    if (!aToken) {
      aSources.splice(1);
    }

    for (let [location, contents] of aSources) {
      let parserMethods = DebuggerController.Parser.get(location, contents);
      let sourceResults = parserMethods.getNamedFunctionDefinitions(aToken);

      for (let scriptResult of sourceResults) {
        for (let parseResult of scriptResult.parseResults) {
          aStore.push({
            sourceUrl: scriptResult.sourceUrl,
            scriptOffset: scriptResult.scriptOffset,
            functionName: parseResult.functionName,
            functionLocation: parseResult.functionLocation,
            inferredName: parseResult.inferredName,
            inferredChain: parseResult.inferredChain,
            inferredLocation: parseResult.inferredLocation
          });

          // Once the maximum allowed number of results is reached, proceed
          // with building the UI immediately.
          if (aStore.length >= RESULTS_PANEL_MAX_RESULTS) {
            this._syncView(aStore);
            return;
          }
        }
      }
    }

    // Couldn't reach the maximum allowed number of results, but that's ok,
    // continue building the UI.
    this._syncView(aStore);
  },

  /**
   * Updates the list of functions displayed in this container.
   *
   * @param array aSearchResults
   *        The results array, containing search details for each source.
   */
  _syncView: function(aSearchResults) {
    // If there are no matches found, keep the popup hidden and avoid
    // creating the view.
    if (!aSearchResults.length) {
      window.emit(EVENTS.FUNCTION_SEARCH_MATCH_NOT_FOUND);
      return;
    }

    for (let item of aSearchResults) {
      // Some function expressions don't necessarily have a name, but the
      // parser provides us with an inferred name from an enclosing
      // VariableDeclarator, AssignmentExpression, ObjectExpression node.
      if (item.functionName && item.inferredName &&
          item.functionName != item.inferredName) {
        let s = " " + L10N.getStr("functionSearchSeparatorLabel") + " ";
        item.displayedName = item.inferredName + s + item.functionName;
      }
      // The function doesn't have an explicit name, but it could be inferred.
      else if (item.inferredName) {
        item.displayedName = item.inferredName;
      }
      // The function only has an explicit name.
      else {
        item.displayedName = item.functionName;
      }

      // Some function expressions have unexpected bounds, since they may not
      // necessarily have an associated name defining them.
      if (item.inferredLocation) {
        item.actualLocation = item.inferredLocation;
      } else {
        item.actualLocation = item.functionLocation;
      }

      // Append a function item to this container for each match.
      let trimmedLabel = SourceUtils.trimUrlLength(item.displayedName + "()");
      let trimmedValue = SourceUtils.trimUrlLength(item.sourceUrl, 0, "start");
      let description = (item.inferredChain || []).join(".");

      this.push([trimmedLabel, trimmedValue, description], {
        index: -1, /* specifies on which position should the item be appended */
        relaxed: true, /* this container should allow dupes & degenerates */
        attachment: item
      });
    }

    // Select the first entry in this container.
    this.selectedIndex = 0;
    this.hidden = false;

    // Signal that function search matches were found and displayed.
    window.emit(EVENTS.FUNCTION_SEARCH_MATCH_FOUND);
  },

  /**
   * The click listener for this container.
   */
  _onClick: function(e) {
    let functionItem = this.getItemForElement(e.target);
    if (functionItem) {
      this.selectedItem = functionItem;
      DebuggerView.Filtering.clearSearch();
    }
  },

  /**
   * The select listener for this container.
   */
  _onSelect: function({ detail: functionItem }) {
    if (functionItem) {
      let sourceUrl = functionItem.attachment.sourceUrl;
      let scriptOffset = functionItem.attachment.scriptOffset;
      let actualLocation = functionItem.attachment.actualLocation;

      DebuggerView.setEditorLocation(sourceUrl, actualLocation.start.line, {
        charOffset: scriptOffset,
        columnOffset: actualLocation.start.column,
        noDebug: true
      });
    }
  },

  _searchTimeout: null,
  _searchFunction: null,
  _searchedToken: ""
});

/**
 * Preliminary setup for the DebuggerView object.
 */
DebuggerView.Toolbar = new ToolbarView();
DebuggerView.Options = new OptionsView();
DebuggerView.Filtering = new FilterView();
DebuggerView.FilteredSources = new FilteredSourcesView();
DebuggerView.FilteredFunctions = new FilteredFunctionsView();
DebuggerView.ChromeGlobals = new ChromeGlobalsView();
DebuggerView.StackFrames = new StackFramesView();

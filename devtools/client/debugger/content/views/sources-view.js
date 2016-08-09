/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../debugger-controller.js */

const utils = require("../utils");
const {
  getSelectedSource,
  getSourceByURL,
  getBreakpoint,
  getBreakpoints,
  makeLocationId
} = require("../queries");
const actions = Object.assign(
  {},
  require("../actions/sources"),
  require("../actions/breakpoints")
);
const { bindActionCreators } = require("devtools/client/shared/vendor/redux");
const {
  Heritage,
  WidgetMethods,
  setNamedTimeout
} = require("devtools/client/shared/widgets/view-helpers");
const { Task } = require("devtools/shared/task");
const { SideMenuWidget } = require("resource://devtools/client/shared/widgets/SideMenuWidget.jsm");
const { gDevTools } = require("devtools/client/framework/devtools");

const NEW_SOURCE_DISPLAY_DELAY = 200; // ms
const FUNCTION_SEARCH_POPUP_POSITION = "topcenter bottomleft";
const BREAKPOINT_LINE_TOOLTIP_MAX_LENGTH = 1000; // chars
const BREAKPOINT_CONDITIONAL_POPUP_POSITION = "before_start";
const BREAKPOINT_CONDITIONAL_POPUP_OFFSET_X = 7; // px
const BREAKPOINT_CONDITIONAL_POPUP_OFFSET_Y = -3; // px

/**
 * Functions handling the sources UI.
 */
function SourcesView(controller, DebuggerView) {
  dumpn("SourcesView was instantiated");

  utils.onReducerEvents(controller, {
    "source": this.renderSource,
    "blackboxed": this.renderBlackBoxed,
    "prettyprinted": this.updateToolbarButtonsState,
    "source-selected": this.renderSourceSelected,
    "breakpoint-updated": bp => this.renderBreakpoint(bp),
    "breakpoint-enabled": bp => this.renderBreakpoint(bp),
    "breakpoint-disabled": bp => this.renderBreakpoint(bp),
    "breakpoint-removed": bp => this.renderBreakpoint(bp, true),
  }, this);

  this.getState = controller.getState;
  this.actions = bindActionCreators(actions, controller.dispatch);
  this.DebuggerView = DebuggerView;
  this.Parser = DebuggerController.Parser;

  this.togglePrettyPrint = this.togglePrettyPrint.bind(this);
  this.toggleBlackBoxing = this.toggleBlackBoxing.bind(this);
  this.toggleBreakpoints = this.toggleBreakpoints.bind(this);

  this._onEditorCursorActivity = this._onEditorCursorActivity.bind(this);
  this._onMouseDown = this._onMouseDown.bind(this);
  this._onSourceSelect = this._onSourceSelect.bind(this);
  this._onStopBlackBoxing = this._onStopBlackBoxing.bind(this);
  this._onBreakpointRemoved = this._onBreakpointRemoved.bind(this);
  this._onBreakpointClick = this._onBreakpointClick.bind(this);
  this._onBreakpointCheckboxClick = this._onBreakpointCheckboxClick.bind(this);
  this._onConditionalPopupShowing = this._onConditionalPopupShowing.bind(this);
  this._onConditionalPopupShown = this._onConditionalPopupShown.bind(this);
  this._onConditionalPopupHiding = this._onConditionalPopupHiding.bind(this);
  this._onConditionalTextboxKeyPress = this._onConditionalTextboxKeyPress.bind(this);
  this._onEditorContextMenuOpen = this._onEditorContextMenuOpen.bind(this);
  this._onCopyUrlCommand = this._onCopyUrlCommand.bind(this);
  this._onNewTabCommand = this._onNewTabCommand.bind(this);
  this._onConditionalPopupHidden = this._onConditionalPopupHidden.bind(this);
}

SourcesView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function (isWorker) {
    dumpn("Initializing the SourcesView");

    this.widget = new SideMenuWidget(document.getElementById("sources"), {
      contextMenu: document.getElementById("debuggerSourcesContextMenu"),
      showArrows: true
    });

    this._preferredSourceURL = null;
    this._unnamedSourceIndex = 0;
    this.emptyText = L10N.getStr("noSourcesText");
    this._blackBoxCheckboxTooltip = L10N.getStr("blackBoxCheckboxTooltip");

    this._commandset = document.getElementById("debuggerCommands");
    this._popupset = document.getElementById("debuggerPopupset");
    this._cmPopup = document.getElementById("sourceEditorContextMenu");
    this._cbPanel = document.getElementById("conditional-breakpoint-panel");
    this._cbTextbox = document.getElementById("conditional-breakpoint-panel-textbox");
    this._blackBoxButton = document.getElementById("black-box");
    this._stopBlackBoxButton = document.getElementById("black-boxed-message-button");
    this._prettyPrintButton = document.getElementById("pretty-print");
    this._toggleBreakpointsButton = document.getElementById("toggle-breakpoints");
    this._newTabMenuItem = document.getElementById("debugger-sources-context-newtab");
    this._copyUrlMenuItem = document.getElementById("debugger-sources-context-copyurl");

    this._noResultsFoundToolTip = new Tooltip(document);
    this._noResultsFoundToolTip.defaultPosition = FUNCTION_SEARCH_POPUP_POSITION;

    // We don't show the pretty print button if debugger a worker
    // because it simply doesn't work yet. (bug 1273730)
    if (Prefs.prettyPrintEnabled && !isWorker) {
      this._prettyPrintButton.removeAttribute("hidden");
    }

    this._editorContainer = document.getElementById("editor");
    this._editorContainer.addEventListener("mousedown", this._onMouseDown, false);

    this.widget.addEventListener("select", this._onSourceSelect, false);

    this._stopBlackBoxButton.addEventListener("click", this._onStopBlackBoxing, false);
    this._cbPanel.addEventListener("popupshowing", this._onConditionalPopupShowing, false);
    this._cbPanel.addEventListener("popupshown", this._onConditionalPopupShown, false);
    this._cbPanel.addEventListener("popuphiding", this._onConditionalPopupHiding, false);
    this._cbPanel.addEventListener("popuphidden", this._onConditionalPopupHidden, false);
    this._cbTextbox.addEventListener("keypress", this._onConditionalTextboxKeyPress, false);
    this._copyUrlMenuItem.addEventListener("command", this._onCopyUrlCommand, false);
    this._newTabMenuItem.addEventListener("command", this._onNewTabCommand, false);

    this._cbPanel.hidden = true;
    this.allowFocusOnRightClick = true;
    this.autoFocusOnSelection = false;
    this.autoFocusOnFirstItem = false;

    // Sort the contents by the displayed label.
    this.sortContents((aFirst, aSecond) => {
      return +(aFirst.attachment.label.toLowerCase() >
               aSecond.attachment.label.toLowerCase());
    });

    // Sort known source groups towards the end of the list
    this.widget.groupSortPredicate = function (a, b) {
      if ((a in KNOWN_SOURCE_GROUPS) == (b in KNOWN_SOURCE_GROUPS)) {
        return a.localeCompare(b);
      }
      return (a in KNOWN_SOURCE_GROUPS) ? 1 : -1;
    };

    this.DebuggerView.editor.on("popupOpen", this._onEditorContextMenuOpen);

    this._addCommands();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function () {
    dumpn("Destroying the SourcesView");

    this.widget.removeEventListener("select", this._onSourceSelect, false);
    this._stopBlackBoxButton.removeEventListener("click", this._onStopBlackBoxing, false);
    this._cbPanel.removeEventListener("popupshowing", this._onConditionalPopupShowing, false);
    this._cbPanel.removeEventListener("popupshown", this._onConditionalPopupShown, false);
    this._cbPanel.removeEventListener("popuphiding", this._onConditionalPopupHiding, false);
    this._cbPanel.removeEventListener("popuphidden", this._onConditionalPopupHidden, false);
    this._cbTextbox.removeEventListener("keypress", this._onConditionalTextboxKeyPress, false);
    this._copyUrlMenuItem.removeEventListener("command", this._onCopyUrlCommand, false);
    this._newTabMenuItem.removeEventListener("command", this._onNewTabCommand, false);
    this.DebuggerView.editor.off("popupOpen", this._onEditorContextMenuOpen, false);
  },

  empty: function () {
    WidgetMethods.empty.call(this);
    this._unnamedSourceIndex = 0;
    this._selectedBreakpoint = null;
  },

  /**
   * Add commands that XUL can fire.
   */
  _addCommands: function () {
    XULUtils.addCommands(this._commandset, {
      addBreakpointCommand: e => this._onCmdAddBreakpoint(e),
      addConditionalBreakpointCommand: e => this._onCmdAddConditionalBreakpoint(e),
      blackBoxCommand: () => this.toggleBlackBoxing(),
      unBlackBoxButton: () => this._onStopBlackBoxing(),
      prettyPrintCommand: () => this.togglePrettyPrint(),
      toggleBreakpointsCommand: () =>this.toggleBreakpoints(),
      togglePromiseDebuggerCommand: () => this.togglePromiseDebugger(),
      nextSourceCommand: () => this.selectNextItem(),
      prevSourceCommand: () => this.selectPrevItem()
    });
  },

  /**
   * Sets the preferred location to be selected in this sources container.
   * @param string aUrl
   */
  set preferredSource(aUrl) {
    this._preferredValue = aUrl;

    // Selects the element with the specified value in this sources container,
    // if already inserted.
    if (this.containsValue(aUrl)) {
      this.selectedValue = aUrl;
    }
  },

  sourcesDidUpdate: function () {
    if (!getSelectedSource(this.getState())) {
      let url = this._preferredSourceURL;
      let source = url && getSourceByURL(this.getState(), url);
      if (source) {
        this.actions.selectSource(source);
      }
      else {
        setNamedTimeout("new-source", NEW_SOURCE_DISPLAY_DELAY, () => {
          if (!getSelectedSource(this.getState()) && this.itemCount > 0) {
            this.actions.selectSource(this.getItemAtIndex(0).attachment.source);
          }
        });
      }
    }
  },

  renderSource: function (source) {
    this.addSource(source, { staged: false });
    for (let bp of getBreakpoints(this.getState())) {
      if (bp.location.actor === source.actor) {
        this.renderBreakpoint(bp);
      }
    }
    this.sourcesDidUpdate();
  },

  /**
   * Adds a source to this sources container.
   *
   * @param object aSource
   *        The source object coming from the active thread.
   * @param object aOptions [optional]
   *        Additional options for adding the source. Supported options:
   *        - staged: true to stage the item to be appended later
   */
  addSource: function (aSource, aOptions = {}) {
    if (!aSource.url && !aOptions.force) {
      // We don't show any unnamed eval scripts yet (see bug 1124106)
      return;
    }

    let { label, group, unicodeUrl } = this._parseUrl(aSource);

    let contents = document.createElement("label");
    contents.className = "plain dbg-source-item";
    contents.setAttribute("value", label);
    contents.setAttribute("crop", "start");
    contents.setAttribute("flex", "1");
    contents.setAttribute("tooltiptext", unicodeUrl);

    if (aSource.introductionType === "wasm") {
      const wasm = document.createElement("box");
      wasm.className = "dbg-wasm-item";
      const icon = document.createElement("box");
      icon.setAttribute("tooltiptext", L10N.getStr("experimental"));
      icon.className = "icon";
      wasm.appendChild(icon);
      wasm.appendChild(contents);

      contents = wasm;
    }

    // If the source is blackboxed, apply the appropriate style.
    if (gThreadClient.source(aSource).isBlackBoxed) {
      contents.classList.add("black-boxed");
    }

    // Append a source item to this container.
    this.push([contents, aSource.actor], {
      staged: aOptions.staged, /* stage the item to be appended later? */
      attachment: {
        label: label,
        group: group,
        checkboxState: !aSource.isBlackBoxed,
        checkboxTooltip: this._blackBoxCheckboxTooltip,
        source: aSource
      }
    });
  },

  _parseUrl: function (aSource) {
    let fullUrl = aSource.url;
    let url, unicodeUrl, label, group;

    if (!fullUrl) {
      unicodeUrl = "SCRIPT" + this._unnamedSourceIndex++;
      label = unicodeUrl;
      group = L10N.getStr("anonymousSourcesLabel");
    }
    else {
      let url = fullUrl.split(" -> ").pop();
      label = aSource.addonPath ? aSource.addonPath : SourceUtils.getSourceLabel(url);
      group = aSource.addonID ? aSource.addonID : SourceUtils.getSourceGroup(url);
      unicodeUrl = NetworkHelper.convertToUnicode(unescape(fullUrl));
    }

    return {
      label: label,
      group: group,
      unicodeUrl: unicodeUrl
    };
  },

  renderBreakpoint: function (breakpoint, removed) {
    if (removed) {
      // Be defensive about the breakpoint not existing.
      if (this._getBreakpoint(breakpoint)) {
        this._removeBreakpoint(breakpoint);
      }
    }
    else {
      if (this._getBreakpoint(breakpoint)) {
        this._updateBreakpointStatus(breakpoint);
      }
      else {
        this._addBreakpoint(breakpoint);
      }
    }
  },

  /**
   * Adds a breakpoint to this sources container.
   *
   * @param object aBreakpointClient
   *               See Breakpoints.prototype._showBreakpoint
   * @param object aOptions [optional]
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _addBreakpoint: function (breakpoint, options = {}) {
    let disabled = breakpoint.disabled;
    let location = breakpoint.location;

    // Get the source item to which the breakpoint should be attached.
    let sourceItem = this.getItemByValue(location.actor);
    if (!sourceItem) {
      return;
    }

    // Create the element node and menu popup for the breakpoint item.
    let breakpointArgs = Heritage.extend(breakpoint.asMutable(), options);
    let breakpointView = this._createBreakpointView.call(this, breakpointArgs);
    let contextMenu = this._createContextMenu.call(this, breakpointArgs);

    // Append a breakpoint child item to the corresponding source item.
    sourceItem.append(breakpointView.container, {
      attachment: Heritage.extend(breakpointArgs, {
        actor: location.actor,
        line: location.line,
        view: breakpointView,
        popup: contextMenu
      }),
      attributes: [
        ["contextmenu", contextMenu.menupopupId]
      ],
      // Make sure that when the breakpoint item is removed, the corresponding
      // menupopup and commandset are also destroyed.
      finalize: this._onBreakpointRemoved
    });

    if (typeof breakpoint.condition === "string") {
      this.highlightBreakpoint(breakpoint.location, {
        openPopup: true,
        noEditorUpdate: true
      });
    }

    window.emit(EVENTS.BREAKPOINT_SHOWN_IN_PANE);
  },

  /**
   * Removes a breakpoint from this sources container.
   * It does not also remove the breakpoint from the controller. Be careful.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _removeBreakpoint: function (breakpoint) {
    // When a parent source item is removed, all the child breakpoint items are
    // also automagically removed.
    let sourceItem = this.getItemByValue(breakpoint.location.actor);
    if (!sourceItem) {
      return;
    }

    // Clear the breakpoint view.
    sourceItem.remove(this._getBreakpoint(breakpoint));

    if (this._selectedBreakpoint &&
       (queries.makeLocationId(this._selectedBreakpoint.location) ===
        queries.makeLocationId(breakpoint.location))) {
      this._selectedBreakpoint = null;
    }

    window.emit(EVENTS.BREAKPOINT_HIDDEN_IN_PANE);
  },

  _getBreakpoint: function (bp) {
    return this.getItemForPredicate(item => {
      return item.attachment.actor === bp.location.actor &&
        item.attachment.line === bp.location.line;
    });
  },

  /**
   * Updates a breakpoint.
   *
   * @param object breakpoint
   */
  _updateBreakpointStatus: function (breakpoint) {
    let location = breakpoint.location;
    let breakpointItem = this._getBreakpoint(getBreakpoint(this.getState(), location));
    if (!breakpointItem) {
      return promise.reject(new Error("No breakpoint found."));
    }

    // Breakpoint will now be enabled.
    let attachment = breakpointItem.attachment;

    // Update the corresponding menu items to reflect the enabled state.
    let prefix = "bp-cMenu-"; // "breakpoints context menu"
    let identifier = makeLocationId(location);
    let enableSelfId = prefix + "enableSelf-" + identifier + "-menuitem";
    let disableSelfId = prefix + "disableSelf-" + identifier + "-menuitem";
    let enableSelf = document.getElementById(enableSelfId);
    let disableSelf = document.getElementById(disableSelfId);

    if (breakpoint.disabled) {
      enableSelf.removeAttribute("hidden");
      disableSelf.setAttribute("hidden", true);
      attachment.view.checkbox.removeAttribute("checked");
    }
    else {
      enableSelf.setAttribute("hidden", true);
      disableSelf.removeAttribute("hidden");
      attachment.view.checkbox.setAttribute("checked", "true");

      // Update the breakpoint toggle button checked state.
      this._toggleBreakpointsButton.removeAttribute("checked");
    }

  },

  /**
   * Highlights a breakpoint in this sources container.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   * @param object aOptions [optional]
   *        An object containing some of the following boolean properties:
   *          - openPopup: tells if the expression popup should be shown.
   *          - noEditorUpdate: tells if you want to skip editor updates.
   */
  highlightBreakpoint: function (aLocation, aOptions = {}) {
    let breakpoint = getBreakpoint(this.getState(), aLocation);
    if (!breakpoint) {
      return;
    }

    // Breakpoint will now be selected.
    this._selectBreakpoint(breakpoint);

    // Update the editor location if necessary.
    if (!aOptions.noEditorUpdate) {
      this.DebuggerView.setEditorLocation(aLocation.actor, aLocation.line, { noDebug: true });
    }

    // If the breakpoint requires a new conditional expression, display
    // the panel to input the corresponding expression.
    if (aOptions.openPopup) {
      return this._openConditionalPopup();
    } else {
      return this._hideConditionalPopup();
    }
  },

  /**
   * Highlight the breakpoint on the current currently focused line/column
   * if it exists.
   */
  highlightBreakpointAtCursor: function () {
    let actor = this.selectedValue;
    let line = this.DebuggerView.editor.getCursor().line + 1;

    let location = { actor: actor, line: line };
    this.highlightBreakpoint(location, { noEditorUpdate: true });
  },

  /**
   * Unhighlights the current breakpoint in this sources container.
   */
  unhighlightBreakpoint: function () {
    this._hideConditionalPopup();
    this._unselectBreakpoint();
  },

   /**
    * Display the message thrown on breakpoint condition
    */
  showBreakpointConditionThrownMessage: function (aLocation, aMessage = "") {
    let breakpointItem = this._getBreakpoint(getBreakpoint(this.getState(), aLocation));
    if (!breakpointItem) {
      return;
    }
    let attachment = breakpointItem.attachment;
    attachment.view.container.classList.add("dbg-breakpoint-condition-thrown");
    attachment.view.message.setAttribute("value", aMessage);
  },

  /**
   * Update the checked/unchecked and enabled/disabled states of the buttons in
   * the sources toolbar based on the currently selected source's state.
   */
  updateToolbarButtonsState: function (source) {
    if (source.isBlackBoxed) {
      this._blackBoxButton.setAttribute("checked", true);
      this._prettyPrintButton.setAttribute("checked", true);
    } else {
      this._blackBoxButton.removeAttribute("checked");
      this._prettyPrintButton.removeAttribute("checked");
    }

    if (source.isPrettyPrinted) {
      this._prettyPrintButton.setAttribute("checked", true);
    } else {
      this._prettyPrintButton.removeAttribute("checked");
    }
  },

  /**
   * Toggle the pretty printing of the selected source.
   */
  togglePrettyPrint: function () {
    if (this._prettyPrintButton.hasAttribute("disabled")) {
      return;
    }

    this.DebuggerView.showProgressBar();
    const source = getSelectedSource(this.getState());
    const sourceClient = gThreadClient.source(source);
    const shouldPrettyPrint = !source.isPrettyPrinted;

    // This is only here to give immediate feedback,
    // `renderPrettyPrinted` will set the final status of the buttons
    if (shouldPrettyPrint) {
      this._prettyPrintButton.setAttribute("checked", true);
    } else {
      this._prettyPrintButton.removeAttribute("checked");
    }

    this.actions.togglePrettyPrint(source);
  },

  /**
   * Toggle the black boxed state of the selected source.
   */
  toggleBlackBoxing: Task.async(function* () {
    const source = getSelectedSource(this.getState());
    const shouldBlackBox = !source.isBlackBoxed;

    // Be optimistic that the (un-)black boxing will succeed, so
    // enable/disable the pretty print button and check/uncheck the
    // black box button immediately.
    if (shouldBlackBox) {
      this._prettyPrintButton.setAttribute("disabled", true);
      this._blackBoxButton.setAttribute("checked", true);
    } else {
      this._prettyPrintButton.removeAttribute("disabled");
      this._blackBoxButton.removeAttribute("checked");
    }

    this.actions.blackbox(source, shouldBlackBox);
  }),

  renderBlackBoxed: function (source) {
    const sourceItem = this.getItemByValue(source.actor);
    sourceItem.prebuiltNode.classList.toggle(
      "black-boxed",
      source.isBlackBoxed
    );

    if (getSelectedSource(this.getState()).actor === source.actor) {
      this.updateToolbarButtonsState(source);
    }
  },

  /**
   * Toggles all breakpoints enabled/disabled.
   */
  toggleBreakpoints: function () {
    let breakpoints = getBreakpoints(this.getState());
    let hasBreakpoints = breakpoints.length > 0;
    let hasEnabledBreakpoints = breakpoints.some(bp => !bp.disabled);

    if (hasBreakpoints && hasEnabledBreakpoints) {
      this._toggleBreakpointsButton.setAttribute("checked", true);
      this._onDisableAll();
    } else {
      this._toggleBreakpointsButton.removeAttribute("checked");
      this._onEnableAll();
    }
  },

  togglePromiseDebugger: function () {
    if (Prefs.promiseDebuggerEnabled) {
      let promisePane = this.DebuggerView._promisePane;
      promisePane.hidden = !promisePane.hidden;

      if (!this.DebuggerView._promiseDebuggerIframe) {
        this.DebuggerView._initializePromiseDebugger();
      }
    }
  },

  hidePrettyPrinting: function () {
    this._prettyPrintButton.style.display = "none";

    if (this._blackBoxButton.style.display === "none") {
      let sep = document.querySelector("#sources-toolbar .devtools-separator");
      sep.style.display = "none";
    }
  },

  hideBlackBoxing: function () {
    this._blackBoxButton.style.display = "none";

    if (this._prettyPrintButton.style.display === "none") {
      let sep = document.querySelector("#sources-toolbar .devtools-separator");
      sep.style.display = "none";
    }
  },

  getDisplayURL: function (source) {
    if (!source.url) {
      return this.getItemByValue(source.actor).attachment.label;
    }
    return NetworkHelper.convertToUnicode(unescape(source.url));
  },

  /**
   * Marks a breakpoint as selected in this sources container.
   *
   * @param object aItem
   *        The breakpoint item to select.
   */
  _selectBreakpoint: function (bp) {
    if (this._selectedBreakpoint === bp) {
      return;
    }
    this._unselectBreakpoint();
    this._selectedBreakpoint = bp;

    const item = this._getBreakpoint(bp);
    item.target.classList.add("selected");

    // Ensure the currently selected breakpoint is visible.
    this.widget.ensureElementIsVisible(item.target);
  },

  /**
   * Marks the current breakpoint as unselected in this sources container.
   */
  _unselectBreakpoint: function () {
    if (!this._selectedBreakpoint) {
      return;
    }

    const item = this._getBreakpoint(this._selectedBreakpoint);
    item.target.classList.remove("selected");

    this._selectedBreakpoint = null;
  },

  /**
   * Opens a conditional breakpoint's expression input popup.
   */
  _openConditionalPopup: function () {
    let breakpointItem = this._getBreakpoint(this._selectedBreakpoint);
    let attachment = breakpointItem.attachment;
    // Check if this is an enabled conditional breakpoint, and if so,
    // retrieve the current conditional epression.
    let bp = getBreakpoint(this.getState(), attachment);
    let expr = (bp ? (bp.condition || "") : "");
    let cbPanel = this._cbPanel;

    // Update the conditional expression textbox. If no expression was
    // previously set, revert to using an empty string by default.
    this._cbTextbox.value = expr;

    function openPopup() {
      // Show the conditional expression panel. The popup arrow should be pointing
      // at the line number node in the breakpoint item view.
      cbPanel.hidden = false;
      cbPanel.openPopup(breakpointItem.attachment.view.lineNumber,
                              BREAKPOINT_CONDITIONAL_POPUP_POSITION,
                              BREAKPOINT_CONDITIONAL_POPUP_OFFSET_X,
                              BREAKPOINT_CONDITIONAL_POPUP_OFFSET_Y);

      cbPanel.removeEventListener("popuphidden", openPopup, false);
    }

    // Wait until the other cb panel is closed
    if (!this._cbPanel.hidden) {
      this._cbPanel.addEventListener("popuphidden", openPopup, false);
    } else {
      openPopup();
    }
  },

  /**
   * Hides a conditional breakpoint's expression input popup.
   */
  _hideConditionalPopup: function () {
    // Sometimes this._cbPanel doesn't have hidePopup method which doesn't
    // break anything but simply outputs an exception to the console.
    if (this._cbPanel.hidePopup) {
      this._cbPanel.hidePopup();
    }
  },

  /**
   * Customization function for creating a breakpoint item's UI.
   *
   * @param object aOptions
   *        A couple of options or flags supported by this operation:
   *          - location: the breakpoint's source location and line number
   *          - disabled: the breakpoint's disabled state, boolean
   *          - text: the breakpoint's line text to be displayed
   *          - message: thrown string when the breakpoint condition throws
   * @return object
   *         An object containing the breakpoint container, checkbox,
   *         line number and line text nodes.
   */
  _createBreakpointView: function (aOptions) {
    let { location, disabled, text, message } = aOptions;
    let identifier = makeLocationId(location);

    let checkbox = document.createElement("checkbox");
    if (!disabled) {
      checkbox.setAttribute("checked", true);
    }
    checkbox.className = "dbg-breakpoint-checkbox";

    let lineNumberNode = document.createElement("label");
    lineNumberNode.className = "plain dbg-breakpoint-line";
    lineNumberNode.setAttribute("value", location.line);

    let lineTextNode = document.createElement("label");
    lineTextNode.className = "plain dbg-breakpoint-text";
    lineTextNode.setAttribute("value", text);
    lineTextNode.setAttribute("crop", "end");
    lineTextNode.setAttribute("flex", "1");

    let tooltip = text ? text.substr(0, BREAKPOINT_LINE_TOOLTIP_MAX_LENGTH) : "";
    lineTextNode.setAttribute("tooltiptext", tooltip);

    let thrownNode = document.createElement("label");
    thrownNode.className = "plain dbg-breakpoint-condition-thrown-message dbg-breakpoint-text";
    thrownNode.setAttribute("value", message);
    thrownNode.setAttribute("crop", "end");
    thrownNode.setAttribute("flex", "1");

    let bpLineContainer = document.createElement("hbox");
    bpLineContainer.className = "plain dbg-breakpoint-line-container";
    bpLineContainer.setAttribute("flex", "1");

    bpLineContainer.appendChild(lineNumberNode);
    bpLineContainer.appendChild(lineTextNode);

    let bpDetailContainer = document.createElement("vbox");
    bpDetailContainer.className = "plain dbg-breakpoint-detail-container";
    bpDetailContainer.setAttribute("flex", "1");

    bpDetailContainer.appendChild(bpLineContainer);
    bpDetailContainer.appendChild(thrownNode);

    let container = document.createElement("hbox");
    container.id = "breakpoint-" + identifier;
    container.className = "dbg-breakpoint side-menu-widget-item-other";
    container.classList.add("devtools-monospace");
    container.setAttribute("align", "center");
    container.setAttribute("flex", "1");

    container.addEventListener("click", this._onBreakpointClick, false);
    checkbox.addEventListener("click", this._onBreakpointCheckboxClick, false);

    container.appendChild(checkbox);
    container.appendChild(bpDetailContainer);

    return {
      container: container,
      checkbox: checkbox,
      lineNumber: lineNumberNode,
      lineText: lineTextNode,
      message: thrownNode
    };
  },

  /**
   * Creates a context menu for a breakpoint element.
   *
   * @param object aOptions
   *        A couple of options or flags supported by this operation:
   *          - location: the breakpoint's source location and line number
   *          - disabled: the breakpoint's disabled state, boolean
   * @return object
   *         An object containing the breakpoint commandset and menu popup ids.
   */
  _createContextMenu: function (aOptions) {
    let { location, disabled } = aOptions;
    let identifier = makeLocationId(location);

    let commandset = document.createElement("commandset");
    let menupopup = document.createElement("menupopup");
    commandset.id = "bp-cSet-" + identifier;
    menupopup.id = "bp-mPop-" + identifier;

    createMenuItem.call(this, "enableSelf", !disabled);
    createMenuItem.call(this, "disableSelf", disabled);
    createMenuItem.call(this, "deleteSelf");
    createMenuSeparator();
    createMenuItem.call(this, "setConditional");
    createMenuSeparator();
    createMenuItem.call(this, "enableOthers");
    createMenuItem.call(this, "disableOthers");
    createMenuItem.call(this, "deleteOthers");
    createMenuSeparator();
    createMenuItem.call(this, "enableAll");
    createMenuItem.call(this, "disableAll");
    createMenuSeparator();
    createMenuItem.call(this, "deleteAll");

    this._popupset.appendChild(menupopup);
    this._commandset.appendChild(commandset);

    return {
      commandsetId: commandset.id,
      menupopupId: menupopup.id
    };

    /**
     * Creates a menu item specified by a name with the appropriate attributes
     * (label and handler).
     *
     * @param string aName
     *        A global identifier for the menu item.
     * @param boolean aHiddenFlag
     *        True if this menuitem should be hidden.
     */
    function createMenuItem(aName, aHiddenFlag) {
      let menuitem = document.createElement("menuitem");
      let command = document.createElement("command");

      let prefix = "bp-cMenu-"; // "breakpoints context menu"
      let commandId = prefix + aName + "-" + identifier + "-command";
      let menuitemId = prefix + aName + "-" + identifier + "-menuitem";

      let label = L10N.getStr("breakpointMenuItem." + aName);
      let func = "_on" + aName.charAt(0).toUpperCase() + aName.slice(1);

      command.id = commandId;
      command.setAttribute("label", label);
      command.addEventListener("command", () => this[func](location), false);

      menuitem.id = menuitemId;
      menuitem.setAttribute("command", commandId);
      aHiddenFlag && menuitem.setAttribute("hidden", "true");

      commandset.appendChild(command);
      menupopup.appendChild(menuitem);
    }

    /**
     * Creates a simple menu separator element and appends it to the current
     * menupopup hierarchy.
     */
    function createMenuSeparator() {
      let menuseparator = document.createElement("menuseparator");
      menupopup.appendChild(menuseparator);
    }
  },

  /**
   * Copy the source url from the currently selected item.
   */
  _onCopyUrlCommand: function () {
    let selected = this.selectedItem && this.selectedItem.attachment;
    if (!selected) {
      return;
    }
    clipboardHelper.copyString(selected.source.url);
  },

  /**
   * Opens selected item source in a new tab.
   */
  _onNewTabCommand: function () {
    let win = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
    let selected = this.selectedItem.attachment;
    win.openUILinkIn(selected.source.url, "tab", { relatedToCurrent: true });
  },

  /**
   * Function called each time a breakpoint item is removed.
   *
   * @param object aItem
   *        The corresponding item.
   */
  _onBreakpointRemoved: function (aItem) {
    dumpn("Finalizing breakpoint item: " + aItem.stringify());

    // Destroy the context menu for the breakpoint.
    let contextMenu = aItem.attachment.popup;
    document.getElementById(contextMenu.commandsetId).remove();
    document.getElementById(contextMenu.menupopupId).remove();
  },

  _onMouseDown: function (e) {
    this.hideNoResultsTooltip();

    if (!e.metaKey) {
      return;
    }

    let editor = this.DebuggerView.editor;
    let identifier = this._findIdentifier(e.clientX, e.clientY);

    if (!identifier) {
      return;
    }

    let foundDefinitions = this._getFunctionDefinitions(identifier);

    if (!foundDefinitions || !foundDefinitions.definitions) {
      return;
    }

    this._showFunctionDefinitionResults(identifier, foundDefinitions.definitions, editor);
  },

  /**
   * Searches for function definition of a function in a given source file
   */

  _findDefinition: function (parsedSource, aName) {
    let functionDefinitions = parsedSource.getNamedFunctionDefinitions(aName);

    let resultList = [];

    if (!functionDefinitions || !functionDefinitions.length || !functionDefinitions[0].length) {
      return {
        definitions: resultList
      };
    }

    // functionDefinitions is a list with an object full of metadata,
    // extract the data and use to construct a more useful, less
    // cluttered, contextual list
    for (let i = 0; i < functionDefinitions.length; i++) {
      let functionDefinition = {
        source: functionDefinitions[i].sourceUrl,
        startLine: functionDefinitions[i][0].functionLocation.start.line,
        startColumn: functionDefinitions[i][0].functionLocation.start.column,
        name: functionDefinitions[i][0].functionName
      };

      resultList.push(functionDefinition);
    }

    return {
      definitions: resultList
    };
  },

  /**
   * Searches for an identifier underneath the specified position in the
   * source editor.
   *
   * @param number x, y
   *        The left/top coordinates where to look for an identifier.
   */
  _findIdentifier: function (x, y) {
    let parsedSource = SourceUtils.parseSource(this.DebuggerView, this.Parser);
    let identifierInfo = SourceUtils.findIdentifier(this.DebuggerView.editor, parsedSource, x, y);

    // Not hovering over an identifier
    if (!identifierInfo) {
      return;
    }

    return identifierInfo;
  },

  /**
   * The selection listener for the source editor.
   */
  _onEditorCursorActivity: function (e) {
    let editor = this.DebuggerView.editor;
    let start = editor.getCursor("start").line + 1;
    let end = editor.getCursor().line + 1;
    let source = getSelectedSource(this.getState());

    if (source) {
      let location = { actor: source.actor, line: start };
      if (getBreakpoint(this.getState(), location) && start == end) {
        this.highlightBreakpoint(location, { noEditorUpdate: true });
      } else {
        this.unhighlightBreakpoint();
      }
    }
  },

  /*
   * Uses function definition data to perform actions in different
   * cases of how many locations were found: zero, one, or multiple definitions
   */
  _showFunctionDefinitionResults: function (aHoveredFunction, aDefinitionList, aEditor) {
    let definitions = aDefinitionList;
    let hoveredFunction = aHoveredFunction;

    // show a popup saying no results were found
    if (definitions.length == 0) {
      this._noResultsFoundToolTip.setTextContent({
        messages: [L10N.getStr("noMatchingStringsText")]
      });

      this._markedIdentifier = aEditor.markText(
        { line: hoveredFunction.location.start.line - 1, ch: hoveredFunction.location.start.column },
        { line: hoveredFunction.location.end.line - 1, ch: hoveredFunction.location.end.column });

      this._noResultsFoundToolTip.show(this._markedIdentifier.anchor);

    } else if (definitions.length == 1) {
      this.DebuggerView.setEditorLocation(definitions[0].source, definitions[0].startLine);
    } else {
      // TODO: multiple definitions found, do something else
      this.DebuggerView.setEditorLocation(definitions[0].source, definitions[0].startLine);
    }
  },

  /**
   * Hides the tooltip and clear marked text popup.
   */
  hideNoResultsTooltip: function () {
    this._noResultsFoundToolTip.hide();
    if (this._markedIdentifier) {
      this._markedIdentifier.clear();
      this._markedIdentifier = null;
    }
  },

  /*
   * Gets the definition locations from function metadata
   */
  _getFunctionDefinitions: function (aIdentifierInfo) {
    let parsedSource = SourceUtils.parseSource(this.DebuggerView, this.Parser);
    let definition_info = this._findDefinition(parsedSource, aIdentifierInfo.name);

    // Did not find any definitions for the identifier
    if (!definition_info) {
      return;
    }

    return definition_info;
  },

  /**
   * The select listener for the sources container.
   */
  _onSourceSelect: function ({ detail: sourceItem }) {
    if (!sourceItem) {
      return;
    }

    const { source } = sourceItem.attachment;
    this.actions.selectSource(source);
  },

  renderSourceSelected: function (source) {
    if (source.url) {
      this._preferredSourceURL = source.url;
    }
    this.updateToolbarButtonsState(source);
    this._selectItem(this.getItemByValue(source.actor));
  },

  /**
   * The click listener for the "stop black boxing" button.
   */
  _onStopBlackBoxing: Task.async(function* () {
    this.actions.blackbox(getSelectedSource(this.getState()), false);
  }),

  /**
   * The source editor's contextmenu handler.
   * - Toggles "Add Conditional Breakpoint" and "Edit Conditional Breakpoint" items
   */
  _onEditorContextMenuOpen: function (message, ev, popup) {
    let actor = this.selectedValue;
    let line = this.DebuggerView.editor.getCursor().line + 1;
    let location = { actor, line };

    let breakpoint = getBreakpoint(this.getState(), location);
    let addConditionalBreakpointMenuItem = popup.querySelector("#se-dbg-cMenu-addConditionalBreakpoint");
    let editConditionalBreakpointMenuItem = popup.querySelector("#se-dbg-cMenu-editConditionalBreakpoint");

    if (breakpoint && !!breakpoint.condition) {
      editConditionalBreakpointMenuItem.removeAttribute("hidden");
      addConditionalBreakpointMenuItem.setAttribute("hidden", true);
    }
    else {
      addConditionalBreakpointMenuItem.removeAttribute("hidden");
      editConditionalBreakpointMenuItem.setAttribute("hidden", true);
    }
  },

  /**
   * The click listener for a breakpoint container.
   */
  _onBreakpointClick: function (e) {
    let sourceItem = this.getItemForElement(e.target);
    let breakpointItem = this.getItemForElement.call(sourceItem, e.target);
    let attachment = breakpointItem.attachment;
    let bp = getBreakpoint(this.getState(), attachment);
    if (bp) {
      this.highlightBreakpoint(bp.location, {
        openPopup: bp.condition && e.button == 0
      });
    } else {
      this.highlightBreakpoint(bp.location);
    }
  },

  /**
   * The click listener for a breakpoint checkbox.
   */
  _onBreakpointCheckboxClick: function (e) {
    let sourceItem = this.getItemForElement(e.target);
    let breakpointItem = this.getItemForElement.call(sourceItem, e.target);
    let bp = getBreakpoint(this.getState(), breakpointItem.attachment);

    if (bp.disabled) {
      this.actions.enableBreakpoint(bp.location);
    }
    else {
      this.actions.disableBreakpoint(bp.location);
    }

    // Don't update the editor location (avoid propagating into _onBreakpointClick).
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * The popup showing listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupShowing: function () {
    this._conditionalPopupVisible = true; // Used in tests.
  },

  /**
   * The popup shown listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupShown: function () {
    this._cbTextbox.focus();
    this._cbTextbox.select();
    window.emit(EVENTS.CONDITIONAL_BREAKPOINT_POPUP_SHOWN);
  },

  /**
   * The popup hiding listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupHiding: function () {
    this._conditionalPopupVisible = false; // Used in tests.

    // Check if this is an enabled conditional breakpoint, and if so,
    // save the current conditional expression.
    let bp = this._selectedBreakpoint;
    if (bp) {
      let condition = this._cbTextbox.value;
      this.actions.setBreakpointCondition(bp.location, condition);
    }
  },

  /**
   * The popup hidden listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupHidden: function () {
    this._cbPanel.hidden = true;
    window.emit(EVENTS.CONDITIONAL_BREAKPOINT_POPUP_HIDDEN);
  },

  /**
   * The keypress listener for the breakpoints conditional expression textbox.
   */
  _onConditionalTextboxKeyPress: function (e) {
    if (e.keyCode == e.DOM_VK_RETURN) {
      this._hideConditionalPopup();
    }
  },

  /**
   * Called when the add breakpoint key sequence was pressed.
   */
  _onCmdAddBreakpoint: function (e) {
    let actor = this.selectedValue;
    let line = (this.DebuggerView.clickedLine ?
                this.DebuggerView.clickedLine + 1 :
                this.DebuggerView.editor.getCursor().line + 1);
    let location = { actor, line };
    let bp = getBreakpoint(this.getState(), location);

    // If a breakpoint already existed, remove it now.
    if (bp) {
      this.actions.removeBreakpoint(bp.location);
    }
    // No breakpoint existed at the required location, add one now.
    else {
      this.actions.addBreakpoint(location);
    }
  },

  /**
   * Called when the add conditional breakpoint key sequence was pressed.
   */
  _onCmdAddConditionalBreakpoint: function (e) {
    let actor = this.selectedValue;
    let line = (this.DebuggerView.clickedLine ?
                this.DebuggerView.clickedLine + 1 :
                this.DebuggerView.editor.getCursor().line + 1);

    let location = { actor, line };
    let bp = getBreakpoint(this.getState(), location);

    // If a breakpoint already existed or wasn't a conditional, morph it now.
    if (bp) {
      this.highlightBreakpoint(bp.location, { openPopup: true });
    }
    // No breakpoint existed at the required location, add one now.
    else {
      this.actions.addBreakpoint(location, "");
    }
  },

  getOtherBreakpoints: function (location) {
    const bps = getBreakpoints(this.getState());
    if (location) {
      return bps.filter(bp => {
        return (bp.location.actor !== location.actor ||
                bp.location.line !== location.line);
      });
    }
    return bps;
  },

  /**
   * Function invoked on the "setConditional" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onSetConditional: function (aLocation) {
    // Highlight the breakpoint and show a conditional expression popup.
    this.highlightBreakpoint(aLocation, { openPopup: true });
  },

  /**
   * Function invoked on the "enableSelf" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onEnableSelf: function (aLocation) {
    // Enable the breakpoint, in this container and the controller store.
    this.actions.enableBreakpoint(aLocation);
  },

  /**
   * Function invoked on the "disableSelf" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onDisableSelf: function (aLocation) {
    const bp = getBreakpoint(this.getState(), aLocation);
    if (!bp.disabled) {
      this.actions.disableBreakpoint(aLocation);
    }
  },

  /**
   * Function invoked on the "deleteSelf" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onDeleteSelf: function (aLocation) {
    this.actions.removeBreakpoint(aLocation);
  },

  /**
   * Function invoked on the "enableOthers" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onEnableOthers: function (aLocation) {
    let other = this.getOtherBreakpoints(aLocation);
    // TODO(jwl): batch these and interrupt the thread for all of them
    other.forEach(bp => this._onEnableSelf(bp.location));
  },

  /**
   * Function invoked on the "disableOthers" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onDisableOthers: function (aLocation) {
    let other = this.getOtherBreakpoints(aLocation);
    other.forEach(bp => this._onDisableSelf(bp.location));
  },

  /**
   * Function invoked on the "deleteOthers" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onDeleteOthers: function (aLocation) {
    let other = this.getOtherBreakpoints(aLocation);
    other.forEach(bp => this._onDeleteSelf(bp.location));
  },

  /**
   * Function invoked on the "enableAll" menuitem command.
   */
  _onEnableAll: function () {
    this._onEnableOthers(undefined);
  },

  /**
   * Function invoked on the "disableAll" menuitem command.
   */
  _onDisableAll: function () {
    this._onDisableOthers(undefined);
  },

  /**
   * Function invoked on the "deleteAll" menuitem command.
   */
  _onDeleteAll: function () {
    this._onDeleteOthers(undefined);
  },

  _commandset: null,
  _popupset: null,
  _cmPopup: null,
  _cbPanel: null,
  _cbTextbox: null,
  _selectedBreakpointItem: null,
  _conditionalPopupVisible: false,
  _noResultsFoundToolTip: null,
  _markedIdentifier: null,
  _selectedBreakpoint: null,
  _conditionalPopupVisible: false
});

module.exports = SourcesView;

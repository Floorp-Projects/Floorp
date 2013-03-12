/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the sources UI.
 */
function SourcesView() {
  dumpn("SourcesView was instantiated");

  this._breakpointsCache = new Map(); // Can't use a WeakMap because keys are strings.
  this._onBreakpointRemoved = this._onBreakpointRemoved.bind(this);
  this._onEditorLoad = this._onEditorLoad.bind(this);
  this._onEditorUnload = this._onEditorUnload.bind(this);
  this._onEditorSelection = this._onEditorSelection.bind(this);
  this._onEditorContextMenu = this._onEditorContextMenu.bind(this);
  this._onSourceMouseDown = this._onSourceMouseDown.bind(this);
  this._onSourceSelect = this._onSourceSelect.bind(this);
  this._onSourceClick = this._onSourceClick.bind(this);
  this._onBreakpointClick = this._onBreakpointClick.bind(this);
  this._onBreakpointCheckboxClick = this._onBreakpointCheckboxClick.bind(this);
  this._onConditionalPopupShowing = this._onConditionalPopupShowing.bind(this);
  this._onConditionalPopupShown = this._onConditionalPopupShown.bind(this);
  this._onConditionalPopupHiding = this._onConditionalPopupHiding.bind(this);
  this._onConditionalTextboxInput = this._onConditionalTextboxInput.bind(this);
  this._onConditionalTextboxKeyPress = this._onConditionalTextboxKeyPress.bind(this);
}

create({ constructor: SourcesView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVS_initialize() {
    dumpn("Initializing the SourcesView");

    this.node = new SideMenuWidget(document.getElementById("sources"));
    this.emptyText = L10N.getStr("noSourcesText");
    this.unavailableText = L10N.getStr("noMatchingSourcesText");

    this._commandset = document.getElementById("debuggerCommands");
    this._popupset = document.getElementById("debuggerPopupset");
    this._cmPopup = document.getElementById("sourceEditorContextMenu");
    this._cbPanel = document.getElementById("conditional-breakpoint-panel");
    this._cbTextbox = document.getElementById("conditional-breakpoint-panel-textbox");

    window.addEventListener("Debugger:EditorLoaded", this._onEditorLoad, false);
    window.addEventListener("Debugger:EditorUnloaded", this._onEditorUnload, false);
    this.node.addEventListener("mousedown", this._onSourceMouseDown, false);
    this.node.addEventListener("select", this._onSourceSelect, false);
    this.node.addEventListener("click", this._onSourceClick, false);
    this._cbPanel.addEventListener("popupshowing", this._onConditionalPopupShowing, false);
    this._cbPanel.addEventListener("popupshown", this._onConditionalPopupShown, false);
    this._cbPanel.addEventListener("popuphiding", this._onConditionalPopupHiding, false);
    this._cbTextbox.addEventListener("input", this._onConditionalTextboxInput, false);
    this._cbTextbox.addEventListener("keypress", this._onConditionalTextboxKeyPress, false);

    // Show an empty label by default.
    this.empty();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVS_destroy() {
    dumpn("Destroying the SourcesView");

    window.removeEventListener("Debugger:EditorLoaded", this._onEditorLoad, false);
    window.removeEventListener("Debugger:EditorUnloaded", this._onEditorUnload, false);
    this.node.removeEventListener("mousedown", this._onSourceMouseDown, false);
    this.node.removeEventListener("select", this._onSourceSelect, false);
    this.node.removeEventListener("click", this._onSourceClick, false);
    this._cbPanel.removeEventListener("popupshowing", this._onConditionalPopupShowing, false);
    this._cbPanel.removeEventListener("popupshowing", this._onConditionalPopupShown, false);
    this._cbPanel.removeEventListener("popuphiding", this._onConditionalPopupHiding, false);
    this._cbTextbox.removeEventListener("input", this._onConditionalTextboxInput, false);
    this._cbTextbox.removeEventListener("keypress", this._onConditionalTextboxKeyPress, false);
  },

  /**
   * Sets the preferred location to be selected in this sources container.
   * @param string aSourceLocation
   */
  set preferredSource(aSourceLocation) {
    this._preferredValue = aSourceLocation;

    // Selects the element with the specified value in this sources container,
    // if already inserted.
    if (this.containsValue(aSourceLocation)) {
      this.selectedValue = aSourceLocation;
    }
  },

  /**
   * Adds a source to this sources container.
   *
   * @param object aSource
   *        The source object coming from the active thread.
   * @param object aOptions [optional]
   *        Additional options for adding the source. Supported options:
   *        - forced: force the source to be immediately added
   */
  addSource: function DVS_addSource(aSource, aOptions = {}) {
    let url = aSource.url;
    let label = SourceUtils.getSourceLabel(url.split(" -> ").pop());
    let group = SourceUtils.getSourceGroup(url.split(" -> ").pop());

    // Append a source item to this container.
    let sourceItem = this.push([label, url, group], {
      staged: aOptions.staged, /* stage the item to be appended later? */
      attachment: {
        source: aSource
      }
    });
  },

  /**
   * Adds a breakpoint to this sources container.
   *
   * @param object aOptions
   *        Several options or flags supported by this operation:
   *          - string sourceLocation
   *            The breakpoint's source location.
   *          - number lineNumber
   *            The breakpoint's line number to be displayed.
   *          - string lineText
   *            The breakpoint's line text to be displayed.
   *          - string actor
   *            A breakpoint identifier specified by the debugger controller.
   *          - boolean openPopupFlag [optional]
   *            A flag specifying if the expression popup should be shown.
   */
  addBreakpoint: function DVS_addBreakpoint(aOptions) {
    let { sourceLocation: url, lineNumber: line } = aOptions;

    // Make sure we're not duplicating anything. If a breakpoint at the
    // specified source location and line number already exists, just enable it.
    if (this.getBreakpoint(url, line)) {
      this.enableBreakpoint(url, line, { id: aOptions.actor });
      return;
    }

    // Get the source item to which the breakpoint should be attached.
    let sourceItem = this.getItemByValue(url);

    // Create the element node and menu popup for the breakpoint item.
    let breakpointView = this._createBreakpointView.call(this, aOptions);
    let contextMenu = this._createContextMenu.call(this, aOptions);

    // Append a breakpoint child item to the corresponding source item.
    let breakpointItem = sourceItem.append(breakpointView.container, {
      attachment: Object.create(aOptions, {
        view: { value: breakpointView },
        popup: { value: contextMenu }
      }),
      attributes: [
        ["contextmenu", contextMenu.menupopupId]
      ],
      // Make sure that when the breakpoint item is removed, the corresponding
      // menupopup and commandset are also destroyed.
      finalize: this._onBreakpointRemoved
    });

    this._breakpointsCache.set(this._getBreakpointKey(url, line), breakpointItem);

    // If this is a conditional breakpoint, display a panel to input the
    // corresponding conditional expression.
    if (aOptions.openPopupFlag) {
      this.highlightBreakpoint(url, line, { openPopup: true });
    }
  },

  /**
   * Removes a breakpoint from this sources container.
   *
   * @param string aSourceLocation
   *        The breakpoint source location.
   * @param number aLineNumber
   *        The breakpoint line number.
   */
  removeBreakpoint: function DVS_removeBreakpoint(aSourceLocation, aLineNumber) {
    // When a parent source item is removed, all the child breakpoint items are
    // also automagically removed.
    let sourceItem = this.getItemByValue(aSourceLocation);
    if (!sourceItem) {
      return;
    }
    let breakpointItem = this.getBreakpoint(aSourceLocation, aLineNumber);
    if (!breakpointItem) {
      return;
    }

    sourceItem.remove(breakpointItem);

    if (this._selectedBreakpoint == breakpointItem) {
      this._selectedBreakpoint = null;
    }
  },

  /**
   * Returns the breakpoint at the specified source location and line number.
   *
   * @param string aSourceLocation
   *        The breakpoint source location.
   * @param number aLineNumber
   *        The breakpoint line number.
   * @return MenuItem
   *         The corresponding breakpoint item if found, null otherwise.
   */
  getBreakpoint: function DVS_getBreakpoint(aSourceLocation, aLineNumber) {
    let breakpointKey = this._getBreakpointKey(aSourceLocation, aLineNumber);
    return this._breakpointsCache.get(breakpointKey);
  },

  /**
   * Enables a breakpoint.
   *
   * @param string aSourceLocation
   *        The breakpoint source location.
   * @param number aLineNumber
   *        The breakpoint line number.
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - silent: pass true to not update the checkbox checked state;
   *                    this is usually necessary when the checked state will
   *                    be updated automatically (e.g: on a checkbox click).
   *          - callback: function to invoke once the breakpoint is enabled
   *          - id: a new id to be applied to the corresponding element node
   * @return boolean
   *         True if breakpoint existed and was enabled, false otherwise.
   */
  enableBreakpoint:
  function DVS_enableBreakpoint(aSourceLocation, aLineNumber, aOptions = {}) {
    let breakpointItem = this.getBreakpoint(aSourceLocation, aLineNumber);
    if (!breakpointItem) {
      return false;
    }

    // Set a new id to the corresponding breakpoint element if required.
    if (aOptions.id) {
      breakpointItem.attachment.view.container.id = "breakpoint-" + aOptions.id;
    }

    // Update the checkbox state if necessary.
    if (!aOptions.silent) {
      breakpointItem.attachment.view.checkbox.setAttribute("checked", "true");
    }

    let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;
    let breakpointLocation = { url: url, line: line };
    DebuggerController.Breakpoints.addBreakpoint(breakpointLocation, aOptions.callback, {
      noPaneUpdate: true,
      noPaneHighlight: true,
      conditionalExpression: breakpointItem.attachment.conditionalExpression
    });

    // Breakpoint is now enabled.
    breakpointItem.attachment.disabled = false;
    return true;
  },

  /**
   * Disables a breakpoint.
   *
   * @param string aSourceLocation
   *        The breakpoint source location.
   * @param number aLineNumber
   *        The breakpoint line number.
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - silent: pass true to not update the checkbox checked state;
   *                    this is usually necessary when the checked state will
   *                    be updated automatically (e.g: on a checkbox click).
   *          - callback: function to invoke once the breakpoint is disabled
   * @return boolean
   *         True if breakpoint existed and was disabled, false otherwise.
   */
  disableBreakpoint:
  function DVS_disableBreakpoint(aSourceLocation, aLineNumber, aOptions = {}) {
    let breakpointItem = this.getBreakpoint(aSourceLocation, aLineNumber);
    if (!breakpointItem) {
      return false;
    }

    // Update the checkbox state if necessary.
    if (!aOptions.silent) {
      breakpointItem.attachment.view.checkbox.removeAttribute("checked");
    }

    let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;
    let breakpointClient = DebuggerController.Breakpoints.getBreakpoint(url, line);
    DebuggerController.Breakpoints.removeBreakpoint(breakpointClient, aOptions.callback, {
      noPaneUpdate: true
    });

    // Remember the conditional expression for when the breakpoint is enabled.
    breakpointItem.attachment.conditionalExpression = breakpointClient.conditionalExpression;

    // Breakpoint is now disabled.
    breakpointItem.attachment.disabled = true;
    return true;
  },

  /**
   * Highlights a breakpoint in this sources container.
   *
   * @param string aSourceLocation
   *        The breakpoint source location.
   * @param number aLineNumber
   *        The breakpoint line number.
   * @param object aFlags [optional]
   *        An object containing some of the following boolean properties:
   *          - updateEditor: true if editor updates should be allowed
   *          - openPopup: true if the expression popup should be shown
   */
  highlightBreakpoint:
  function DVS_highlightBreakpoint(aSourceLocation, aLineNumber, aFlags = {}) {
    let breakpointItem = this.getBreakpoint(aSourceLocation, aLineNumber);
    if (!breakpointItem) {
      return;
    }

    // Breakpoint is now selected.
    this._selectBreakpoint(breakpointItem);

    // Update the editor source location and line number if necessary.
    if (aFlags.updateEditor) {
      DebuggerView.updateEditor(aSourceLocation, aLineNumber, { noDebug: true });
    }

    // If the breakpoint requires a new conditional expression, display
    // the panel to input the corresponding expression.
    if (aFlags.openPopup) {
      this._openConditionalPopup();
    } else {
      this._hideConditionalPopup();
    }
  },

  /**
   * Unhighlights the current breakpoint in this sources container.
   */
  unhighlightBreakpoint: function DVS_unhighlightBreakpoint() {
    this._unselectBreakpoint();
    this._hideConditionalPopup();
  },

  /**
   * Gets the currently selected breakpoint item.
   * @return object
   */
  get selectedBreakpoint() this._selectedBreakpoint,

  /**
   * Gets the currently selected breakpoint client.
   * @return object
   */
  get selectedClient() {
    let breakpointItem = this._selectedBreakpoint;
    if (breakpointItem) {
      let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;
      return DebuggerController.Breakpoints.getBreakpoint(url, line);
    }
    return null;
  },

  /**
   * Marks a breakpoint as selected in this sources container.
   *
   * @param MenuItem aItem
   *        The breakpoint item to select.
   */
  _selectBreakpoint: function DVS__selectBreakpoint(aItem) {
    if (this._selectedBreakpoint == aItem) {
      return;
    }
    this._unselectBreakpoint();
    this._selectedBreakpoint = aItem;
    this._selectedBreakpoint.markSelected();

    // Ensure the currently selected breakpoint is visible.
    this.node.ensureElementIsVisible(aItem.target);
  },

  /**
   * Marks the current breakpoint as unselected in this sources container.
   */
  _unselectBreakpoint: function DVS__unselectBreakpoint() {
    if (this._selectedBreakpoint) {
      this._selectedBreakpoint.markDeselected();
      this._selectedBreakpoint = null;
    }
  },

  /**
   * Opens a conditional breakpoint's expression input popup.
   */
  _openConditionalPopup: function DVS__openConditionalPopup() {
    let selectedBreakpoint = this.selectedBreakpoint;
    let selectedClient = this.selectedClient;

    if (selectedClient.conditionalExpression === undefined) {
      this._cbTextbox.value = selectedClient.conditionalExpression = "";
    } else {
      this._cbTextbox.value = selectedClient.conditionalExpression;
    }

    this._cbPanel.hidden = false;
    this._cbPanel.openPopup(this.selectedBreakpoint.attachment.view.lineNumber,
      BREAKPOINT_CONDITIONAL_POPUP_POSITION,
      BREAKPOINT_CONDITIONAL_POPUP_OFFSET_X,
      BREAKPOINT_CONDITIONAL_POPUP_OFFSET_Y);
  },

  /**
   * Hides a conditional breakpoint's expression input popup.
   */
  _hideConditionalPopup: function DVS__hideConditionalPopup() {
    this._cbPanel.hidden = true;
    this._cbPanel.hidePopup();
  },

  /**
   * Customization function for creating a breakpoint item's UI.
   *
   * @param object aOptions
   *        Additional options or flags supported by this operation:
   *          - number lineNumber
   *            The line number specified by the debugger controller.
   *          - string lineText
   *            The line text to be displayed.
   * @return object
   *         An object containing the breakpoint container, checkbox,
   *         line number and line text nodes.
   */
  _createBreakpointView: function DVS_createBreakpointView(aOptions) {
    let { lineNumber, lineText } = aOptions;

    let checkbox = document.createElement("checkbox");
    checkbox.setAttribute("checked", "true");

    let lineNumberNode = document.createElement("label");
    lineNumberNode.className = "plain dbg-breakpoint-line";
    lineNumberNode.setAttribute("value", lineNumber);

    let lineTextNode = document.createElement("label");
    lineTextNode.className = "plain dbg-breakpoint-text";
    lineTextNode.setAttribute("value", lineText);
    lineTextNode.setAttribute("crop", "end");
    lineTextNode.setAttribute("flex", "1");
    lineTextNode.setAttribute("tooltiptext",
      lineText.substr(0, BREAKPOINT_LINE_TOOLTIP_MAX_LENGTH));

    let container = document.createElement("hbox");
    container.id = "breakpoint-" + aOptions.actor;
    container.className = "dbg-breakpoint side-menu-widget-item-other";
    container.setAttribute("align", "center");
    container.setAttribute("flex", "1");

    container.addEventListener("click", this._onBreakpointClick, false);
    checkbox.addEventListener("click", this._onBreakpointCheckboxClick, false);

    container.appendChild(checkbox);
    container.appendChild(lineNumberNode);
    container.appendChild(lineTextNode);

    return {
      container: container,
      checkbox: checkbox,
      lineNumber: lineNumberNode,
      lineText: lineTextNode
    };
  },

  /**
   * Creates a context menu for a breakpoint element.
   *
   * @param aOptions
   *        Additional options or flags supported by this operation:
   *          - string actor
   *            A breakpoint identifier specified by the debugger controller.
   * @return object
   *         An object containing the breakpoint commandset and menu popup ids.
   */
  _createContextMenu: function DVS__createContextMenu(aOptions) {
    let commandsetId = "bp-cSet-" + aOptions.actor;
    let menupopupId = "bp-mPop-" + aOptions.actor;

    let commandset = document.createElement("commandset");
    let menupopup = document.createElement("menupopup");
    commandset.id = commandsetId;
    menupopup.id = menupopupId;

    createMenuItem.call(this, "enableSelf", true);
    createMenuItem.call(this, "disableSelf");
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
      commandsetId: commandsetId,
      menupopupId: menupopupId
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
      let commandId = prefix + aName + "-" + aOptions.actor + "-command";
      let menuitemId = prefix + aName + "-" + aOptions.actor + "-menuitem";

      let label = L10N.getStr("breakpointMenuItem." + aName);
      let func = this["_on" + aName.charAt(0).toUpperCase() + aName.slice(1)];

      command.id = commandId;
      command.setAttribute("label", label);
      command.addEventListener("command", func.bind(this, aOptions), false);

      menuitem.id = menuitemId;
      menuitem.setAttribute("command", commandId);
      menuitem.setAttribute("command", commandId);
      menuitem.setAttribute("hidden", aHiddenFlag);

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
   * Destroys the context menu for a breakpoint.
   *
   * @param object aContextMenu
   *        An object containing the breakpoint commandset and menu popup ids.
   */
  _destroyContextMenu: function DVS__destroyContextMenu(aContextMenu) {
    dumpn("Destroying context menu: " +
      aContextMenu.commandsetId + " & " + aContextMenu.menupopupId);

    let commandset = document.getElementById(aContextMenu.commandsetId);
    let menupopup = document.getElementById(aContextMenu.menupopupId);
    commandset.parentNode.removeChild(commandset);
    menupopup.parentNode.removeChild(menupopup);
  },

  /**
   * Function called each time a breakpoint item is removed.
   *
   * @param MenuItem aItem
   *        The corresponding menu item.
   */
  _onBreakpointRemoved: function DVS__onBreakpointRemoved(aItem) {
    dumpn("Finalizing breakpoint item: " + aItem);

    let { sourceLocation: url, lineNumber: line, popup } = aItem.attachment;
    this._destroyContextMenu(popup);
    this._breakpointsCache.delete(this._getBreakpointKey(url, line));
  },

  /**
   * The load listener for the source editor.
   */
  _onEditorLoad: function DVS__onEditorLoad({ detail: editor }) {
    editor.addEventListener("Selection", this._onEditorSelection, false);
    editor.addEventListener("ContextMenu", this._onEditorContextMenu, false);
  },

  /**
   * The unload listener for the source editor.
   */
  _onEditorUnload: function DVS__onEditorUnload({ detail: editor }) {
    editor.removeEventListener("Selection", this._onEditorSelection, false);
    editor.removeEventListener("ContextMenu", this._onEditorContextMenu, false);
  },

  /**
   * The selection listener for the source editor.
   */
  _onEditorSelection: function DVS__onEditorSelection(e) {
    let { start, end } = e.newValue;

    let sourceLocation = this.selectedValue;
    let lineStart = DebuggerView.editor.getLineAtOffset(start) + 1;
    let lineEnd = DebuggerView.editor.getLineAtOffset(end) + 1;

    if (this.getBreakpoint(sourceLocation, lineStart) && lineStart == lineEnd) {
      this.highlightBreakpoint(sourceLocation, lineStart);
    } else {
      this.unhighlightBreakpoint();
    }
  },

  /**
   * The context menu listener for the source editor.
   */
  _onEditorContextMenu: function DVS__onEditorContextMenu({ x, y }) {
    let offset = DebuggerView.editor.getOffsetAtLocation(x, y);
    let line = DebuggerView.editor.getLineAtOffset(offset);
    this._editorContextMenuLineNumber = line;
  },

  /**
   * The mouse down listener for the sources container.
   */
  _onSourceMouseDown: function DVS__onSourceMouseDown(e) {
    let item = this.getItemForElement(e.target);
    if (item) {
      // The container is not empty and we clicked on an actual item.
      this.selectedItem = item;
    }
  },

  /**
   * The select listener for the sources container.
   */
  _onSourceSelect: function DVS__onSourceSelect() {
    if (!this.refresh()) {
      return;
    }

    let selectedSource = this.selectedItem.attachment.source;
    if (DebuggerView.editorSource != selectedSource) {
      DebuggerView.editorSource = selectedSource;
    }
  },

  /**
   * The click listener for the sources container.
   */
  _onSourceClick: function DVS__onSourceClick() {
    // Use this container as a filtering target.
    DebuggerView.Filtering.target = this;
  },

  /**
   * The click listener for a breakpoint container.
   */
  _onBreakpointClick: function DVS__onBreakpointClick(e) {
    let sourceItem = this.getItemForElement(e.target);
    let breakpointItem = this.getItemForElement.call(sourceItem, e.target);
    let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;
    let breakpointClient = DebuggerController.Breakpoints.getBreakpoint(url, line);
    let conditionalExpression = (breakpointClient || {}).conditionalExpression;

    this.highlightBreakpoint(url, line, {
      updateEditor: true,
      openPopup: conditionalExpression !== undefined && e.button == 0
    });
  },

  /**
   * The click listener for a breakpoint checkbox.
   */
  _onBreakpointCheckboxClick: function DVS__onBreakpointCheckboxClick(e) {
    let sourceItem = this.getItemForElement(e.target);
    let breakpointItem = this.getItemForElement.call(sourceItem, e.target);
    let { sourceLocation: url, lineNumber: line, disabled } = breakpointItem.attachment;

    this[disabled ? "enableBreakpoint" : "disableBreakpoint"](url, line, {
      silent: true
    });

    // Don't update the editor location (propagate into DVS__onBreakpointClick).
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * The popup showing listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupShowing: function DVS__onConditionalPopupShowing() {
    this._conditionalPopupVisible = true;
  },

  /**
   * The popup shown listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupShown: function DVS__onConditionalPopupShown() {
    this._cbTextbox.focus();
    this._cbTextbox.select();
  },

  /**
   * The popup hiding listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupHiding: function DVS__onConditionalPopupHiding() {
    this._conditionalPopupVisible = false;
  },

  /**
   * The input listener for the breakpoints conditional expression textbox.
   */
  _onConditionalTextboxInput: function DVS__onConditionalTextboxInput() {
    this.selectedClient.conditionalExpression = this._cbTextbox.value;
  },

  /**
   * The keypress listener for the breakpoints conditional expression textbox.
   */
  _onConditionalTextboxKeyPress: function DVS__onConditionalTextboxKeyPress(e) {
    if (e.keyCode == e.DOM_VK_RETURN || e.keyCode == e.DOM_VK_ENTER) {
      this._hideConditionalPopup();
    }
  },

  /**
   * Called when the add breakpoint key sequence was pressed.
   */
  _onCmdAddBreakpoint: function BP__onCmdAddBreakpoint() {
    // If this command was executed via the context menu, add the breakpoint
    // on the currently hovered line in the source editor.
    if (this._editorContextMenuLineNumber >= 0) {
      DebuggerView.editor.setCaretPosition(this._editorContextMenuLineNumber);
    }
    // Avoid placing breakpoints incorrectly when using key shortcuts.
    this._editorContextMenuLineNumber = -1;

    let url = DebuggerView.Sources.selectedValue;
    let line = DebuggerView.editor.getCaretPosition().line + 1;
    let breakpointItem = this.getBreakpoint(url, line);

    // If a breakpoint already existed, remove it now.
    if (breakpointItem) {
      let breakpointClient = DebuggerController.Breakpoints.getBreakpoint(url, line);
      DebuggerController.Breakpoints.removeBreakpoint(breakpointClient);
    }
    // No breakpoint existed at the required location, add one now.
    else {
      let breakpointLocation = { url: url, line: line };
      DebuggerController.Breakpoints.addBreakpoint(breakpointLocation);
    }
  },

  /**
   * Called when the add conditional breakpoint key sequence was pressed.
   */
  _onCmdAddConditionalBreakpoint: function BP__onCmdAddConditionalBreakpoint() {
    // If this command was executed via the context menu, add the breakpoint
    // on the currently hovered line in the source editor.
    if (this._editorContextMenuLineNumber >= 0) {
      DebuggerView.editor.setCaretPosition(this._editorContextMenuLineNumber);
    }
    // Avoid placing breakpoints incorrectly when using key shortcuts.
    this._editorContextMenuLineNumber = -1;

    let url =  DebuggerView.Sources.selectedValue;
    let line = DebuggerView.editor.getCaretPosition().line + 1;
    let breakpointItem = this.getBreakpoint(url, line);

    // If a breakpoint already existed or wasn't a conditional, morph it now.
    if (breakpointItem) {
      let breakpointClient = DebuggerController.Breakpoints.getBreakpoint(url, line);
      this.highlightBreakpoint(url, line, { openPopup: true });
    }
    // No breakpoint existed at the required location, add one now.
    else {
      DebuggerController.Breakpoints.addBreakpoint({ url: url, line: line }, null, {
        conditionalExpression: "",
        openPopup: true
      });
    }
  },

  /**
   * Listener handling the "setConditional" menuitem command.
   *
   * @param object aDetails
   *        The breakpoint details (sourceLocation, lineNumber etc.).
   */
  _onSetConditional: function DVS__onSetConditional(aDetails) {
    let { sourceLocation: url, lineNumber: line, actor } = aDetails;
    let breakpointItem = this.getBreakpoint(url, line);
    this.highlightBreakpoint(url, line, { openPopup: true });
  },

  /**
   * Listener handling the "enableSelf" menuitem command.
   *
   * @param object aDetails
   *        The breakpoint details (sourceLocation, lineNumber etc.).
   */
  _onEnableSelf: function DVS__onEnableSelf(aDetails) {
    let { sourceLocation: url, lineNumber: line, actor } = aDetails;

    if (this.enableBreakpoint(url, line)) {
      let prefix = "bp-cMenu-"; // "breakpoints context menu"
      let enableSelfId = prefix + "enableSelf-" + actor + "-menuitem";
      let disableSelfId = prefix + "disableSelf-" + actor + "-menuitem";
      document.getElementById(enableSelfId).setAttribute("hidden", "true");
      document.getElementById(disableSelfId).removeAttribute("hidden");
    }
  },

  /**
   * Listener handling the "disableSelf" menuitem command.
   *
   * @param object aDetails
   *        The breakpoint details (sourceLocation, lineNumber etc.).
   */
  _onDisableSelf: function DVS__onDisableSelf(aDetails) {
    let { sourceLocation: url, lineNumber: line, actor } = aDetails;

    if (this.disableBreakpoint(url, line)) {
      let prefix = "bp-cMenu-"; // "breakpoints context menu"
      let enableSelfId = prefix + "enableSelf-" + actor + "-menuitem";
      let disableSelfId = prefix + "disableSelf-" + actor + "-menuitem";
      document.getElementById(enableSelfId).removeAttribute("hidden");
      document.getElementById(disableSelfId).setAttribute("hidden", "true");
    }
  },

  /**
   * Listener handling the "deleteSelf" menuitem command.
   *
   * @param object aDetails
   *        The breakpoint details (sourceLocation, lineNumber etc.).
   */
  _onDeleteSelf: function DVS__onDeleteSelf(aDetails) {
    let { sourceLocation: url, lineNumber: line } = aDetails;
    let breakpointClient = DebuggerController.Breakpoints.getBreakpoint(url, line);

    this.removeBreakpoint(url, line);
    DebuggerController.Breakpoints.removeBreakpoint(breakpointClient);
  },

  /**
   * Listener handling the "enableOthers" menuitem command.
   *
   * @param object aDetails
   *        The breakpoint details (sourceLocation, lineNumber etc.).
   */
  _onEnableOthers: function DVS__onEnableOthers(aDetails) {
    for (let [, item] of this._breakpointsCache) {
      if (item.attachment.actor != aDetails.actor) {
        this._onEnableSelf(item.attachment);
      }
    }
  },

  /**
   * Listener handling the "disableOthers" menuitem command.
   *
   * @param object aDetails
   *        The breakpoint details (sourceLocation, lineNumber etc.).
   */
  _onDisableOthers: function DVS__onDisableOthers(aDetails) {
    for (let [, item] of this._breakpointsCache) {
      if (item.attachment.actor != aDetails.actor) {
        this._onDisableSelf(item.attachment);
      }
    }
  },

  /**
   * Listener handling the "deleteOthers" menuitem command.
   *
   * @param object aDetails
   *        The breakpoint details (sourceLocation, lineNumber etc.).
   */
  _onDeleteOthers: function DVS__onDeleteOthers(aDetails) {
    for (let [, item] of this._breakpointsCache) {
      if (item.attachment.actor != aDetails.actor) {
        this._onDeleteSelf(item.attachment);
      }
    }
  },

  /**
   * Listener handling the "enableAll" menuitem command.
   *
   * @param object aDetails
   *        The breakpoint details (sourceLocation, lineNumber etc.).
   */
  _onEnableAll: function DVS__onEnableAll(aDetails) {
    this._onEnableOthers(aDetails);
    this._onEnableSelf(aDetails);
  },

  /**
   * Listener handling the "disableAll" menuitem command.
   *
   * @param object aDetails
   *        The breakpoint details (sourceLocation, lineNumber etc.).
   */
  _onDisableAll: function DVS__onDisableAll(aDetails) {
    this._onDisableOthers(aDetails);
    this._onDisableSelf(aDetails);
  },

  /**
   * Listener handling the "deleteAll" menuitem command.
   *
   * @param object aDetails
   *        The breakpoint details (sourceLocation, lineNumber etc.).
   */
  _onDeleteAll: function DVS__onDeleteAll(aDetails) {
    this._onDeleteOthers(aDetails);
    this._onDeleteSelf(aDetails);
  },

  /**
   * Gets an identifier for a breakpoint's details in the current cache.
   *
   * @param string aSourceLocation
   *        The breakpoint source location.
   * @param number aLineNumber
   *        The breakpoint line number.
   * @return string
   *         The breakpoint identifier.
   */
  _getBreakpointKey: function DVS__getBreakpointKey(aSourceLocation, aLineNumber) {
    return [aSourceLocation, aLineNumber].join();
  },

  _breakpointsCache: null,
  _commandset: null,
  _popupset: null,
  _cmPopup: null,
  _cbPanel: null,
  _cbTextbox: null,
  _selectedBreakpoint: null,
  _editorContextMenuLineNumber: -1,
  _conditionalPopupVisible: false
});

/**
 * Utility functions for handling sources.
 */
let SourceUtils = {
  _labelsCache: new Map(), // Can't use WeakMaps because keys are strings.
  _groupsCache: new Map(),

  /**
   * Clears the labels cache, populated by methods like
   * SourceUtils.getSourceLabel or Source Utils.getSourceGroup.
   * This should be done every time the content location changes.
   */
  clearCache: function SU_clearCache() {
    this._labelsCache.clear();
    this._groupsCache.clear();
  },

  /**
   * Gets a unique, simplified label from a source url.
   *
   * @param string aUrl
   *        The source url.
   * @return string
   *         The simplified label.
   */
  getSourceLabel: function SU_getSourceLabel(aUrl) {
    let cachedLabel = this._labelsCache.get(aUrl);
    if (cachedLabel) {
      return cachedLabel;
    }

    let sourceLabel = this.trimUrl(aUrl);
    let unicodeLabel = NetworkHelper.convertToUnicode(unescape(sourceLabel));
    this._labelsCache.set(aUrl, unicodeLabel);
    return unicodeLabel;
  },

  /**
   * Gets as much information as possible about the hostname and directory paths
   * of an url to create a short url group identifier.
   *
   * @param string aUrl
   *        The source url.
   * @return string
   *         The simplified group.
   */
  getSourceGroup: function SU_getSourceGroup(aUrl) {
    let cachedGroup = this._groupsCache.get(aUrl);
    if (cachedGroup) {
      return cachedGroup;
    }

    try {
      // Use an nsIURL to parse all the url path parts.
      var uri = Services.io.newURI(aUrl, null, null).QueryInterface(Ci.nsIURL);
    } catch (e) {
      // This doesn't look like a url, or nsIURL can't handle it.
      return "";
    }

    let { scheme, hostPort, directory, fileName } = uri;
    let lastDir = directory.split("/").reverse()[1];
    let group = [];

    // Only show interesting schemes, http is implicit.
    if (scheme != "http") {
      group.push(scheme);
    }
    // Hostnames don't always exist for files or some resource urls.
    // e.g. file://foo/bar.js or resource:///foo/bar.js don't have a host.
    if (hostPort) {
      // If the hostname is a dot-separated identifier, show the first 2 parts.
      group.push(hostPort.split(".").slice(0, 2).join("."));
    }
    // Append the last directory if the path leads to an actual file.
    // e.g. http://foo.org/bar/ should only show "foo.org", not "foo.org bar"
    if (fileName) {
      group.push(lastDir);
    }

    let groupLabel = group.join(" ");
    let unicodeLabel = NetworkHelper.convertToUnicode(unescape(groupLabel));
    this._groupsCache.set(aUrl, unicodeLabel)
    return unicodeLabel;
  },

  /**
   * Trims the url by shortening it if it exceeds a certain length, adding an
   * ellipsis at the end.
   *
   * @param string aUrl
   *        The source url.
   * @param number aLength [optional]
   *        The expected source url length.
   * @param number aSection [optional]
   *        The section to trim. Supported values: "start", "center", "end"
   * @return string
   *         The shortened url.
   */
  trimUrlLength: function SU_trimUrlLength(aUrl, aLength, aSection) {
    aLength = aLength || SOURCE_URL_DEFAULT_MAX_LENGTH;
    aSection = aSection || "end";

    if (aUrl.length > aLength) {
      switch (aSection) {
        case "start":
          return L10N.ellipsis + aUrl.slice(-aLength);
          break;
        case "center":
          return aUrl.substr(0, aLength / 2 - 1) + L10N.ellipsis + aUrl.slice(-aLength / 2 + 1);
          break;
        case "end":
          return aUrl.substr(0, aLength) + L10N.ellipsis;
          break;
      }
    }
    return aUrl;
  },

  /**
   * Trims the query part or reference identifier of a url string, if necessary.
   *
   * @param string aUrl
   *        The source url.
   * @return string
   *         The shortened url.
   */
  trimUrlQuery: function SU_trimUrlQuery(aUrl) {
    let length = aUrl.length;
    let q1 = aUrl.indexOf('?');
    let q2 = aUrl.indexOf('&');
    let q3 = aUrl.indexOf('#');
    let q = Math.min(q1 != -1 ? q1 : length,
                     q2 != -1 ? q2 : length,
                     q3 != -1 ? q3 : length);

    return aUrl.slice(0, q);
  },

  /**
   * Trims as much as possible from a url, while keeping the label unique
   * in the sources container.
   *
   * @param string | nsIURL aUrl
   *        The source url.
   * @param string aLabel [optional]
   *        The resulting label at each step.
   * @param number aSeq [optional]
   *        The current iteration step.
   * @return string
   *         The resulting label at the final step.
   */
  trimUrl: function SU_trimUrl(aUrl, aLabel, aSeq) {
    if (!(aUrl instanceof Ci.nsIURL)) {
      try {
        // Use an nsIURL to parse all the url path parts.
        aUrl = Services.io.newURI(aUrl, null, null).QueryInterface(Ci.nsIURL);
      } catch (e) {
        // This doesn't look like a url, or nsIURL can't handle it.
        return aUrl;
      }
    }
    if (!aSeq) {
      let name = aUrl.fileName;
      if (name) {
        // This is a regular file url, get only the file name (contains the
        // base name and extension if available).

        // If this url contains an invalid query, unfortunately nsIURL thinks
        // it's part of the file extension. It must be removed.
        aLabel = aUrl.fileName.replace(/\&.*/, "");
      } else {
        // This is not a file url, hence there is no base name, nor extension.
        // Proceed using other available information.
        aLabel = "";
      }
      aSeq = 1;
    }

    // If we have a label and it doesn't only contain a query...
    if (aLabel && aLabel.indexOf("?") != 0) {
      // A page may contain multiple requests to the same url but with different
      // queries. It is *not* redundant to show each one.
      if (!DebuggerView.Sources.containsLabel(aLabel)) {
        return aLabel;
      }
    }

    // Append the url query.
    if (aSeq == 1) {
      let query = aUrl.query;
      if (query) {
        return this.trimUrl(aUrl, aLabel + "?" + query, aSeq + 1);
      }
      aSeq++;
    }
    // Append the url reference.
    if (aSeq == 2) {
      let ref = aUrl.ref;
      if (ref) {
        return this.trimUrl(aUrl, aLabel + "#" + aUrl.ref, aSeq + 1);
      }
      aSeq++;
    }
    // Prepend the url directory.
    if (aSeq == 3) {
      let dir = aUrl.directory;
      if (dir) {
        return this.trimUrl(aUrl, dir.replace(/^\//, "") + aLabel, aSeq + 1);
      }
      aSeq++;
    }
    // Prepend the hostname and port number.
    if (aSeq == 4) {
      let host = aUrl.hostPort;
      if (host) {
        return this.trimUrl(aUrl, host + "/" + aLabel, aSeq + 1);
      }
      aSeq++;
    }
    // Use the whole url spec but ignoring the reference.
    if (aSeq == 5) {
      return this.trimUrl(aUrl, aUrl.specIgnoringRef, aSeq + 1);
    }
    // Give up.
    return aUrl.spec;
  }
};

/**
 * Functions handling the watch expressions UI.
 */
function WatchExpressionsView() {
  dumpn("WatchExpressionsView was instantiated");

  this._cache = []; // Array instead of a map because indices are important.
  this.switchExpression = this.switchExpression.bind(this);
  this.deleteExpression = this.deleteExpression.bind(this);
  this._createItemView = this._createItemView.bind(this);
  this._onClick = this._onClick.bind(this);
  this._onClose = this._onClose.bind(this);
  this._onBlur = this._onBlur.bind(this);
  this._onKeyPress = this._onKeyPress.bind(this);
}

create({ constructor: WatchExpressionsView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVWE_initialize() {
    dumpn("Initializing the WatchExpressionsView");

    this.node = new ListWidget(document.getElementById("expressions"));
    this._variables = document.getElementById("variables");

    this.node.permaText = L10N.getStr("addWatchExpressionText");
    this.node.itemFactory = this._createItemView;
    this.node.setAttribute("context", "debuggerWatchExpressionsContextMenu");
    this.node.addEventListener("click", this._onClick, false);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVWE_destroy() {
    dumpn("Destroying the WatchExpressionsView");

    this.node.removeEventListener("click", this._onClick, false);
  },

  /**
   * Adds a watch expression in this container.
   *
   * @param string aExpression [optional]
   *        An optional initial watch expression text.
   */
  addExpression: function DVWE_addExpression(aExpression = "") {
    // Watch expressions are UI elements which benefit from visible panes.
    DebuggerView.showInstrumentsPane();

    // Append a watch expression item to this container.
    let expressionItem = this.push([, aExpression], {
      index: 0, /* specifies on which position should the item be appended */
      relaxed: true, /* this container should allow dupes & degenerates */
      attachment: {
        initialExpression: aExpression,
        currentExpression: "",
        id: this._generateId()
      }
    });

    // Automatically focus the new watch expression input.
    expressionItem.attachment.inputNode.select();
    expressionItem.attachment.inputNode.focus();
    this._variables.scrollTop = 0;

    this._cache.splice(0, 0, expressionItem);
  },

  /**
   * Removes the watch expression with the specified index from this container.
   *
   * @param number aIndex
   *        The index used to identify the watch expression.
   */
  removeExpressionAt: function DVWE_removeExpressionAt(aIndex) {
    this.remove(this._cache[aIndex]);
    this._cache.splice(aIndex, 1);
  },

  /**
   * Changes the watch expression corresponding to the specified variable item.
   * This function is called whenever a watch expression's code is edited in
   * the variables view container.
   *
   * @param Variable aVar
   *        The variable representing the watch expression evaluation.
   * @param string aExpression
   *        The new watch expression text.
   */
  switchExpression: function DVWE_switchExpression(aVar, aExpression) {
    let expressionItem =
      [i for (i of this._cache) if (i.attachment.currentExpression == aVar.name)][0];

    // Remove the watch expression if it's going to be empty or a duplicate.
    if (!aExpression || this.getExpressions().indexOf(aExpression) != -1) {
      this.deleteExpression(aVar);
      return;
    }

    // Save the watch expression code string.
    expressionItem.attachment.currentExpression = aExpression;
    expressionItem.attachment.inputNode.value = aExpression;

    // Synchronize with the controller's watch expressions store.
    DebuggerController.StackFrames.syncWatchExpressions();
  },

  /**
   * Removes the watch expression corresponding to the specified variable item.
   * This function is called whenever a watch expression's value is edited in
   * the variables view container.
   *
   * @param Variable aVar
   *        The variable representing the watch expression evaluation.
   */
  deleteExpression: function DVWE_deleteExpression(aVar) {
    let expressionItem =
      [i for (i of this._cache) if (i.attachment.currentExpression == aVar.name)][0];

    // Remove the watch expression at its respective index.
    this.removeExpressionAt(this._cache.indexOf(expressionItem));

    // Synchronize with the controller's watch expressions store.
    DebuggerController.StackFrames.syncWatchExpressions();
  },

  /**
   * Gets the watch expression code string for an item in this container.
   *
   * @param number aIndex
   *        The index used to identify the watch expression.
   * @return string
   *         The watch expression code string.
   */
  getExpression: function DVWE_getExpression(aIndex) {
    return this._cache[aIndex].attachment.currentExpression;
  },

  /**
   * Gets the watch expressions code strings for all items in this container.
   *
   * @return array
   *         The watch expressions code strings.
   */
  getExpressions: function DVWE_getExpressions() {
    return [item.attachment.currentExpression for (item of this._cache)];
  },

  /**
   * Customization function for creating an item's UI.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed item.
   * @param any aAttachment
   *        Some attached primitive/object.
   */
  _createItemView: function DVWE__createItemView(aElementNode, aAttachment) {
    let arrowNode = document.createElement("box");
    arrowNode.className = "dbg-expression-arrow";

    let inputNode = document.createElement("textbox");
    inputNode.className = "plain dbg-expression-input";
    inputNode.setAttribute("value", aAttachment.initialExpression);
    inputNode.setAttribute("flex", "1");

    let closeNode = document.createElement("toolbarbutton");
    closeNode.className = "plain variables-view-delete";

    closeNode.addEventListener("click", this._onClose, false);
    inputNode.addEventListener("blur", this._onBlur, false);
    inputNode.addEventListener("keypress", this._onKeyPress, false);

    aElementNode.id = "expression-" + aAttachment.id;
    aElementNode.className = "dbg-expression title";

    aElementNode.appendChild(arrowNode);
    aElementNode.appendChild(inputNode);
    aElementNode.appendChild(closeNode);

    aAttachment.arrowNode = arrowNode;
    aAttachment.inputNode = inputNode;
    aAttachment.closeNode = closeNode;
  },

  /**
   * Called when the add watch expression key sequence was pressed.
   */
  _onCmdAddExpression: function BP__onCmdAddExpression(aText) {
    // Only add a new expression if there's no pending input.
    if (this.getExpressions().indexOf("") == -1) {
      this.addExpression(aText || DebuggerView.editor.getSelectedText());
    }
  },

  /**
   * Called when the remove all watch expressions key sequence was pressed.
   */
  _onCmdRemoveAllExpressions: function BP__onCmdRemoveAllExpressions() {
    // Empty the view of all the watch expressions and clear the cache.
    this.empty();
    this._cache.length = 0;

    // Synchronize with the controller's watch expressions store.
    DebuggerController.StackFrames.syncWatchExpressions();
  },

  /**
   * The click listener for this container.
   */
  _onClick: function DVWE__onClick(e) {
    if (e.button != 0) {
      // Only allow left-click to trigger this event.
      return;
    }
    let expressionItem = this.getItemForElement(e.target);
    if (!expressionItem) {
      // The container is empty or we didn't click on an actual item.
      this.addExpression();
    }
  },

  /**
   * The click listener for a watch expression's close button.
   */
  _onClose: function DVWE__onClose(e) {
    let expressionItem = this.getItemForElement(e.target);
    this.removeExpressionAt(this._cache.indexOf(expressionItem));

    // Synchronize with the controller's watch expressions store.
    DebuggerController.StackFrames.syncWatchExpressions();

    // Prevent clicking the expression element itself.
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * The blur listener for a watch expression's textbox.
   */
  _onBlur: function DVWE__onBlur({ target: textbox }) {
    let expressionItem = this.getItemForElement(textbox);
    let oldExpression = expressionItem.attachment.currentExpression;
    let newExpression = textbox.value.trim();

    // Remove the watch expression if it's empty.
    if (!newExpression) {
      this.removeExpressionAt(this._cache.indexOf(expressionItem));
    }
    // Remove the watch expression if it's a duplicate.
    else if (!oldExpression && this.getExpressions().indexOf(newExpression) != -1) {
      this.removeExpressionAt(this._cache.indexOf(expressionItem));
    }
    // Expression is eligible.
    else {
      expressionItem.attachment.currentExpression = newExpression;
    }

    // Synchronize with the controller's watch expressions store.
    DebuggerController.StackFrames.syncWatchExpressions();
  },

  /**
   * The keypress listener for a watch expression's textbox.
   */
  _onKeyPress: function DVWE__onKeyPress(e) {
    switch(e.keyCode) {
      case e.DOM_VK_RETURN:
      case e.DOM_VK_ENTER:
      case e.DOM_VK_ESCAPE:
        DebuggerView.editor.focus();
        return;
    }
  },

  /**
   * Gets an identifier for a new watch expression item in the current cache.
   * @return string
   */
  _generateId: (function() {
    let count = 0;
    return function DVWE__generateId() {
      return (++count) + "";
    };
  })(),

  _variables: null,
  _cache: null
});

/**
 * Functions handling the global search UI.
 */
function GlobalSearchView() {
  dumpn("GlobalSearchView was instantiated");

  this._startSearch = this._startSearch.bind(this);
  this._performGlobalSearch = this._performGlobalSearch.bind(this);
  this._createItemView = this._createItemView.bind(this);
  this._onScroll = this._onScroll.bind(this);
  this._onHeaderClick = this._onHeaderClick.bind(this);
  this._onLineClick = this._onLineClick.bind(this);
  this._onMatchClick = this._onMatchClick.bind(this);
}

create({ constructor: GlobalSearchView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVGS_initialize() {
    dumpn("Initializing the GlobalSearchView");

    this.node = new ListWidget(document.getElementById("globalsearch"));
    this._splitter = document.querySelector("#globalsearch + .devtools-horizontal-splitter");

    this.node.emptyText = L10N.getStr("noMatchingStringsText");
    this.node.itemFactory = this._createItemView;
    this.node.addEventListener("scroll", this._onScroll, false);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVGS_destroy() {
    dumpn("Destroying the GlobalSearchView");

    this.node.removeEventListener("scroll", this._onScroll, false);
  },

  /**
   * Gets the visibility state of the global search container.
   * @return boolean
   */
  get hidden()
    this.node.getAttribute("hidden") == "true" ||
    this._splitter.getAttribute("hidden") == "true",

  /**
   * Sets the results container hidden or visible. It's hidden by default.
   * @param boolean aFlag
   */
  set hidden(aFlag) {
    this.node.setAttribute("hidden", aFlag);
    this._splitter.setAttribute("hidden", aFlag);
  },

  /**
   * Hides and removes all items from this search container.
   */
  clearView: function DVGS_clearView() {
    this.hidden = true;
    this.empty();
    window.dispatchEvent(document, "Debugger:GlobalSearch:ViewCleared");
  },

  /**
   * Focuses the next found match in the source editor.
   */
  focusNextMatch: function DVGS_focusNextMatch() {
    let totalLineResults = LineResults.size();
    if (!totalLineResults) {
      return;
    }
    if (++this._currentlyFocusedMatch >= totalLineResults) {
      this._currentlyFocusedMatch = 0;
    }
    this._onMatchClick({
      target: LineResults.getElementAtIndex(this._currentlyFocusedMatch)
    });
  },

  /**
   * Focuses the previously found match in the source editor.
   */
  focusPrevMatch: function DVGS_focusPrevMatch() {
    let totalLineResults = LineResults.size();
    if (!totalLineResults) {
      return;
    }
    if (--this._currentlyFocusedMatch < 0) {
      this._currentlyFocusedMatch = totalLineResults - 1;
    }
    this._onMatchClick({
      target: LineResults.getElementAtIndex(this._currentlyFocusedMatch)
    });
  },

  /**
   * Allows searches to be scheduled and delayed to avoid redundant calls.
   */
  delayedSearch: true,

  /**
   * Schedules searching for a string in all of the sources.
   *
   * @param string aQuery
   *        The string to search for.
   */
  scheduleSearch: function DVGS_scheduleSearch(aQuery) {
    if (!this.delayedSearch) {
      this.performSearch(aQuery);
      return;
    }
    let delay = Math.max(GLOBAL_SEARCH_ACTION_MAX_DELAY / aQuery.length, 0);

    window.clearTimeout(this._searchTimeout);
    this._searchFunction = this._startSearch.bind(this, aQuery);
    this._searchTimeout = window.setTimeout(this._searchFunction, delay);
  },

  /**
   * Immediately searches for a string in all of the sources.
   *
   * @param string aQuery
   *        The string to search for.
   */
  performSearch: function DVGS_performSearch(aQuery) {
    window.clearTimeout(this._searchTimeout);
    this._searchFunction = null;
    this._startSearch(aQuery);
  },

  /**
   * Starts searching for a string in all of the sources.
   *
   * @param string aQuery
   *        The string to search for.
   */
  _startSearch: function DVGS__startSearch(aQuery) {
    this._searchedToken = aQuery;

    DebuggerController.SourceScripts.fetchSources(DebuggerView.Sources.values, {
      onFinished: this._performGlobalSearch
    });
  },

  /**
   * Finds string matches in all the sources stored in the controller's cache,
   * and groups them by location and line number.
   */
  _performGlobalSearch: function DVGS__performGlobalSearch() {
    // Get the currently searched token from the filtering input.
    let token = this._searchedToken;

    // Make sure we're actually searching for something.
    if (!token) {
      this.clearView();
      window.dispatchEvent(document, "Debugger:GlobalSearch:TokenEmpty");
      return;
    }

    // Search is not case sensitive, prepare the actual searched token.
    let lowerCaseToken = token.toLowerCase();
    let tokenLength = token.length;

    // Prepare the results map, containing search details for each line.
    let globalResults = new GlobalResults();
    let sourcesCache = DebuggerController.SourceScripts.getCache();

    for (let [location, contents] of sourcesCache) {
      // Verify that the search token is found anywhere in the source.
      if (!contents.toLowerCase().contains(lowerCaseToken)) {
        continue;
      }
      let lines = contents.split("\n");
      let sourceResults = new SourceResults();

      for (let i = 0, len = lines.length; i < len; i++) {
        let line = lines[i];
        let lowerCaseLine = line.toLowerCase();

        // Search is not case sensitive, and is tied to each line in the source.
        if (!lowerCaseLine.contains(lowerCaseToken)) {
          continue;
        }

        let lineNumber = i;
        let lineResults = new LineResults();

        lowerCaseLine.split(lowerCaseToken).reduce(function(prev, curr, index, {length}) {
          let prevLength = prev.length;
          let currLength = curr.length;
          let unmatched = line.substr(prevLength, currLength);
          lineResults.add(unmatched);

          if (index != length - 1) {
            let matched = line.substr(prevLength + currLength, tokenLength);
            let range = {
              start: prevLength + currLength,
              length: matched.length
            };
            lineResults.add(matched, range, true);
            sourceResults.matchCount++;
          }
          return prev + token + curr;
        }, "");

        if (sourceResults.matchCount) {
          sourceResults.add(lineNumber, lineResults);
        }
      }
      if (sourceResults.matchCount) {
        globalResults.add(location, sourceResults);
      }
    }

    // Empty this container to rebuild the search results.
    this.empty();

    // Signal if there are any matches, and the rebuild the results.
    if (globalResults.itemCount) {
      this.hidden = false;
      this._currentlyFocusedMatch = -1;
      this._createGlobalResultsUI(globalResults);
      window.dispatchEvent(document, "Debugger:GlobalSearch:MatchFound");
    } else {
      window.dispatchEvent(document, "Debugger:GlobalSearch:MatchNotFound");
    }
  },

  /**
   * Creates global search results entries and adds them to this container.
   *
   * @param GlobalResults aGlobalResults
   *        An object containing all source results, grouped by source location.
   */
  _createGlobalResultsUI: function DVGS__createGlobalResultsUI(aGlobalResults) {
    let i = 0;

    for (let [location, sourceResults] in aGlobalResults) {
      if (i++ == 0) {
        this._createSourceResultsUI(location, sourceResults, true);
      } else {
        // Dispatch subsequent document manipulation operations, to avoid
        // blocking the main thread when a large number of search results
        // is found, thus giving the impression of faster searching.
        Services.tm.currentThread.dispatch({ run:
          this._createSourceResultsUI.bind(this, location, sourceResults) }, 0);
      }
    }
  },

  /**
   * Creates source search results entries and adds them to this container.
   *
   * @param string aLocation
   *        The location of the source.
   * @param SourceResults aSourceResults
   *        An object containing all the matched lines for a specific source.
   * @param boolean aExpandFlag
   *        True to expand the source results.
   */
  _createSourceResultsUI:
  function DVGS__createSourceResultsUI(aLocation, aSourceResults, aExpandFlag) {
    // Append a source results item to this container.
    let sourceResultsItem = this.push([aLocation, aSourceResults.matchCount], {
      index: -1, /* specifies on which position should the item be appended */
      relaxed: true, /* this container should allow dupes & degenerates */
      attachment: {
        sourceResults: aSourceResults,
        expandFlag: aExpandFlag
      }
    });
  },

  /**
   * Customization function for creating an item's UI.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed item.
   * @param any aAttachment
   *        Some attached primitive/object.
   * @param string aLocation
   *        The source result's location.
   * @param string aMatchCount
   *        The source result's match count.
   */
  _createItemView:
  function DVGS__createItemView(aElementNode, aAttachment, aLocation, aMatchCount) {
    let { sourceResults, expandFlag } = aAttachment;

    sourceResults.createView(aElementNode, aLocation, aMatchCount, expandFlag, {
      onHeaderClick: this._onHeaderClick,
      onLineClick: this._onLineClick,
      onMatchClick: this._onMatchClick
    });
  },

  /**
   * The click listener for a results header.
   */
  _onHeaderClick: function DVGS__onHeaderClick(e) {
    let sourceResultsItem = SourceResults.getItemForElement(e.target);
    sourceResultsItem.instance.toggle(e);
  },

  /**
   * The click listener for a results line.
   */
  _onLineClick: function DVGLS__onLineClick(e) {
    let lineResultsItem = LineResults.getItemForElement(e.target);
    this._onMatchClick({ target: lineResultsItem.firstMatch });
  },

  /**
   * The click listener for a result match.
   */
  _onMatchClick: function DVGLS__onMatchClick(e) {
    if (e instanceof Event) {
      e.preventDefault();
      e.stopPropagation();
    }
    let target = e.target;
    let sourceResultsItem = SourceResults.getItemForElement(target);
    let lineResultsItem = LineResults.getItemForElement(target);

    sourceResultsItem.instance.expand();
    this._currentlyFocusedMatch = LineResults.indexOfElement(target);
    this._scrollMatchIntoViewIfNeeded(target);
    this._bounceMatch(target);

    let location = sourceResultsItem.location;
    let lineNumber = lineResultsItem.lineNumber;
    DebuggerView.updateEditor(location, lineNumber + 1, { noDebug: true });

    let editor = DebuggerView.editor;
    let offset = editor.getCaretOffset();
    let { start, length } = lineResultsItem.lineData.range;
    editor.setSelection(offset + start, offset + start + length);
  },

  /**
   * The scroll listener for the global search container.
   */
  _onScroll: function DVGS__onScroll(e) {
    for (let item in this) {
      this._expandResultsIfNeeded(item.target);
    }
  },

  /**
   * Expands the source results it they are currently visible.
   *
   * @param nsIDOMNode aTarget
   *        The element associated with the displayed item.
   */
  _expandResultsIfNeeded: function DVGS__expandResultsIfNeeded(aTarget) {
    let sourceResultsItem = SourceResults.getItemForElement(aTarget);
    if (sourceResultsItem.instance.toggled ||
        sourceResultsItem.instance.expanded) {
      return;
    }
    let { top, height } = aTarget.getBoundingClientRect();
    let { clientHeight } = this.node._parent;

    if (top - height <= clientHeight || this._forceExpandResults) {
      sourceResultsItem.instance.expand();
    }
  },

  /**
   * Scrolls a match into view if not already visible.
   *
   * @param nsIDOMNode aMatch
   *        The match to scroll into view.
   */
  _scrollMatchIntoViewIfNeeded:  function DVGS__scrollMatchIntoViewIfNeeded(aMatch) {
    let boxObject = this.node._parent.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    boxObject.ensureElementIsVisible(aMatch);
  },

  /**
   * Starts a bounce animation for a match.
   *
   * @param nsIDOMNode aMatch
   *        The match to start a bounce animation for.
   */
  _bounceMatch: function DVGS__bounceMatch(aMatch) {
    Services.tm.currentThread.dispatch({ run: function() {
      aMatch.addEventListener("transitionend", function onEvent() {
        aMatch.removeEventListener("transitionend", onEvent);
        aMatch.removeAttribute("focused");
      });
      aMatch.setAttribute("focused", "");
    }}, 0);
    aMatch.setAttribute("focusing", "");
  },

  _splitter: null,
  _currentlyFocusedMatch: -1,
  _forceExpandResults: false,
  _searchTimeout: null,
  _searchFunction: null,
  _searchedToken: ""
});

/**
 * An object containing all source results, grouped by source location.
 * Iterable via "for (let [location, sourceResults] in globalResults) { }".
 */
function GlobalResults() {
  this._store = new Map();
  SourceResults._itemsByElement = new Map();
  LineResults._itemsByElement = new Map();
}

GlobalResults.prototype = {
  /**
   * Adds source results to this store.
   *
   * @param string aLocation
   *        The location of the source.
   * @param SourceResults aSourceResults
   *        An object containing all the matched lines for a specific source.
   */
  add: function GR_add(aLocation, aSourceResults) {
    this._store.set(aLocation, aSourceResults);
  },

  /**
   * Gets the number of source results in this store.
   */
  get itemCount() this._store.size,

  _store: null
};

/**
 * An object containing all the matched lines for a specific source.
 * Iterable via "for (let [lineNumber, lineResults] in sourceResults) { }".
 */
function SourceResults() {
  this._store = new Map();
  this.matchCount = 0;
}

SourceResults.prototype = {
  /**
   * Adds line results to this store.
   *
   * @param number aLineNumber
   *        The line location in the source.
   * @param LineResults aLineResults
   *        An object containing all the matches for a specific line.
   */
  add: function SR_add(aLineNumber, aLineResults) {
    this._store.set(aLineNumber, aLineResults);
  },

  /**
   * The number of matches in this store. One line may have multiple matches.
   */
  matchCount: -1,

  /**
   * Expands the element, showing all the added details.
   */
  expand: function SR_expand() {
    this._target.resultsContainer.removeAttribute("hidden")
    this._target.arrow.setAttribute("open", "");
  },

  /**
   * Collapses the element, hiding all the added details.
   */
  collapse: function SR_collapse() {
    this._target.resultsContainer.setAttribute("hidden", "true");
    this._target.arrow.removeAttribute("open");
  },

  /**
   * Toggles between the element collapse/expand state.
   */
  toggle: function SR_toggle(e) {
    if (e instanceof Event) {
      this._userToggled = true;
    }
    this.expanded ^= 1;
  },

  /**
   * Relaxes the auto-expand rules to always show as many results as possible.
   */
  alwaysExpand: true,

  /**
   * Gets this element's expanded state.
   * @return boolean
   */
  get expanded()
    this._target.resultsContainer.getAttribute("hidden") != "true" &&
    this._target.arrow.hasAttribute("open"),

  /**
   * Sets this element's expanded state.
   * @param boolean aFlag
   */
  set expanded(aFlag) this[aFlag ? "expand" : "collapse"](),

  /**
   * Returns if this element was ever toggled via user interaction.
   * @return boolean
   */
  get toggled() this._userToggled,

  /**
   * Gets the element associated with this item.
   * @return nsIDOMNode
   */
  get target() this._target,

  /**
   * Customization function for creating this item's UI.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed item.
   * @param string aLocation
   *        The source result's location.
   * @param string aMatchCount
   *        The source result's match count.
   * @param boolean aExpandFlag
   *        True to expand the source results.
   * @param object aCallbacks
   *        An object containing all the necessary callback functions:
   *          - onHeaderClick
   *          - onMatchClick
   */
  createView:
  function SR_createView(aElementNode, aLocation, aMatchCount, aExpandFlag, aCallbacks) {
    this._target = aElementNode;

    let arrow = document.createElement("box");
    arrow.className = "arrow";

    let locationNode = document.createElement("label");
    locationNode.className = "plain dbg-results-header-location";
    locationNode.setAttribute("value", SourceUtils.trimUrlLength(aLocation));

    let matchCountNode = document.createElement("label");
    matchCountNode.className = "plain dbg-results-header-match-count";
    matchCountNode.setAttribute("value", "(" + aMatchCount + ")");

    let resultsHeader = document.createElement("hbox");
    resultsHeader.className = "dbg-results-header";
    resultsHeader.setAttribute("align", "center")
    resultsHeader.appendChild(arrow);
    resultsHeader.appendChild(locationNode);
    resultsHeader.appendChild(matchCountNode);
    resultsHeader.addEventListener("click", aCallbacks.onHeaderClick, false);

    let resultsContainer = document.createElement("vbox");
    resultsContainer.className = "dbg-results-container";
    resultsContainer.setAttribute("hidden", "true");

    for (let [lineNumber, lineResults] of this._store) {
      lineResults.createView(resultsContainer, lineNumber, aCallbacks)
    }

    aElementNode.arrow = arrow;
    aElementNode.resultsHeader = resultsHeader;
    aElementNode.resultsContainer = resultsContainer;

    if ((aExpandFlag || this.alwaysExpand) &&
         aMatchCount < GLOBAL_SEARCH_EXPAND_MAX_RESULTS) {
      this.expand();
    }

    let resultsBox = document.createElement("vbox");
    resultsBox.setAttribute("flex", "1");
    resultsBox.appendChild(resultsHeader);
    resultsBox.appendChild(resultsContainer);

    aElementNode.id = "source-results-" + aLocation;
    aElementNode.className = "dbg-source-results";
    aElementNode.appendChild(resultsBox);

    SourceResults._itemsByElement.set(aElementNode, {
      location: aLocation,
      matchCount: aMatchCount,
      autoExpand: aExpandFlag,
      instance: this
    });
  },

  _store: null,
  _target: null,
  _userToggled: false
};

/**
 * An object containing all the matches for a specific line.
 * Iterable via "for (let chunk in lineResults) { }".
 */
function LineResults() {
  this._store = [];
}

LineResults.prototype = {
  /**
   * Adds string details to this store.
   *
   * @param string aString
   *        The text contents chunk in the line.
   * @param object aRange
   *        An object containing the { start, length } of the chunk.
   * @param boolean aMatchFlag
   *        True if the chunk is a matched string, false if just text content.
   */
  add: function LC_add(aString, aRange, aMatchFlag) {
    this._store.push({
      string: aString,
      range: aRange,
      match: !!aMatchFlag
    });
  },

  /**
   * Gets the element associated with this item.
   * @return nsIDOMNode
   */
  get target() this._target,

  /**
   * Customization function for creating this item's UI.
   *
   * @param nsIDOMNode aContainer
   *        The element associated with the displayed item.
   * @param number aLineNumber
   *        The line location in the source.
   * @param object aCallbacks
   *        An object containing all the necessary callback functions:
   *          - onMatchClick
   *          - onLineClick
   */
  createView: function LR_createView(aContainer, aLineNumber, aCallbacks) {
    this._target = aContainer;

    let lineNumberNode = document.createElement("label");
    let lineContentsNode = document.createElement("hbox");
    let lineString = "";
    let lineLength = 0;
    let firstMatch = null;

    lineNumberNode.className = "plain dbg-results-line-number";
    lineNumberNode.setAttribute("value", aLineNumber + 1);
    lineContentsNode.className = "light list-widget-item dbg-results-line-contents";
    lineContentsNode.setAttribute("flex", "1");

    for (let chunk of this._store) {
      let { string, range, match } = chunk;
      lineString = string.substr(0, GLOBAL_SEARCH_LINE_MAX_LENGTH - lineLength);
      lineLength += string.length;

      let label = document.createElement("label");
      label.className = "plain dbg-results-line-contents-string";
      label.setAttribute("value", lineString);
      label.setAttribute("match", match);
      lineContentsNode.appendChild(label);

      if (match) {
        this._entangleMatch(aLineNumber, label, chunk);
        label.addEventListener("click", aCallbacks.onMatchClick, false);
        firstMatch = firstMatch || label;
      }
      if (lineLength >= GLOBAL_SEARCH_LINE_MAX_LENGTH) {
        lineContentsNode.appendChild(this._ellipsis.cloneNode());
        break;
      }
    }

    this._entangleLine(lineContentsNode, firstMatch);
    lineContentsNode.addEventListener("click", aCallbacks.onLineClick, false);

    let searchResult = document.createElement("hbox");
    searchResult.className = "dbg-search-result";
    searchResult.appendChild(lineNumberNode);
    searchResult.appendChild(lineContentsNode);
    aContainer.appendChild(searchResult);
  },

  /**
   * Handles a match while creating the view.
   * @param number aLineNumber
   * @param nsIDOMNode aNode
   * @param object aMatchChunk
   */
  _entangleMatch: function LR__entangleMatch(aLineNumber, aNode, aMatchChunk) {
    LineResults._itemsByElement.set(aNode, {
      lineNumber: aLineNumber,
      lineData: aMatchChunk
    });
  },

  /**
   * Handles a line while creating the view.
   * @param nsIDOMNode aNode
   * @param nsIDOMNode aFirstMatch
   */
  _entangleLine: function LR__entangleLine(aNode, aFirstMatch) {
    LineResults._itemsByElement.set(aNode, {
      firstMatch: aFirstMatch,
      nonenumerable: true
    });
  },

  /**
   * An nsIDOMNode label with an ellipsis value.
   */
  _ellipsis: (function() {
    let label = document.createElement("label");
    label.className = "plain dbg-results-line-contents-string";
    label.setAttribute("value", L10N.ellipsis);
    return label;
  })(),

  _store: null,
  _target: null
};

/**
 * A generator-iterator over the global, source or line results.
 */
GlobalResults.prototype.__iterator__ =
SourceResults.prototype.__iterator__ =
LineResults.prototype.__iterator__ = function DVGS_iterator() {
  for (let item of this._store) {
    yield item;
  }
};

/**
 * Gets the item associated with the specified element.
 *
 * @param nsIDOMNode aElement
 *        The element used to identify the item.
 * @return object
 *         The matched item, or null if nothing is found.
 */
SourceResults.getItemForElement =
LineResults.getItemForElement = function DVGS_getItemForElement(aElement) {
  return MenuContainer.prototype.getItemForElement.call(this, aElement);
};

/**
 * Gets the element associated with a particular item at a specified index.
 *
 * @param number aIndex
 *        The index used to identify the item.
 * @return nsIDOMNode
 *         The matched element, or null if nothing is found.
 */
SourceResults.getElementAtIndex =
LineResults.getElementAtIndex = function DVGS_getElementAtIndex(aIndex) {
  for (let [element, item] of this._itemsByElement) {
    if (!item.nonenumerable && !aIndex--) {
      return element;
    }
  }
  return null;
};

/**
 * Gets the index of an item associated with the specified element.
 *
 * @param nsIDOMNode aElement
 *        The element to get the index for.
 * @return number
 *         The index of the matched element, or -1 if nothing is found.
 */
SourceResults.indexOfElement =
LineResults.indexOfElement = function DVGS_indexOFElement(aElement) {
  let count = 0;
  for (let [element, item] of this._itemsByElement) {
    if (element == aElement) {
      return count;
    }
    if (!item.nonenumerable) {
      count++;
    }
  }
  return -1;
};

/**
 * Gets the number of cached items associated with a specified element.
 *
 * @return number
 *         The number of key/value pairs in the corresponding map.
 */
SourceResults.size =
LineResults.size = function DVGS_size() {
  let count = 0;
  for (let [, item] of this._itemsByElement) {
    if (!item.nonenumerable) {
      count++;
    }
  }
  return count;
};

/**
 * Preliminary setup for the DebuggerView object.
 */
DebuggerView.Sources = new SourcesView();
DebuggerView.WatchExpressions = new WatchExpressionsView();
DebuggerView.GlobalSearch = new GlobalSearchView();

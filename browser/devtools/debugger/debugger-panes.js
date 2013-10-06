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

  this.prettyPrint = this.prettyPrint.bind(this);
  this._onEditorLoad = this._onEditorLoad.bind(this);
  this._onEditorUnload = this._onEditorUnload.bind(this);
  this._onEditorSelection = this._onEditorSelection.bind(this);
  this._onEditorContextMenu = this._onEditorContextMenu.bind(this);
  this._onSourceSelect = this._onSourceSelect.bind(this);
  this._onSourceClick = this._onSourceClick.bind(this);
  this._onBreakpointRemoved = this._onBreakpointRemoved.bind(this);
  this._onSourceCheck = this._onSourceCheck.bind(this);
  this._onStopBlackBoxing = this._onStopBlackBoxing.bind(this);
  this._onBreakpointClick = this._onBreakpointClick.bind(this);
  this._onBreakpointCheckboxClick = this._onBreakpointCheckboxClick.bind(this);
  this._onConditionalPopupShowing = this._onConditionalPopupShowing.bind(this);
  this._onConditionalPopupShown = this._onConditionalPopupShown.bind(this);
  this._onConditionalPopupHiding = this._onConditionalPopupHiding.bind(this);
  this._onConditionalTextboxInput = this._onConditionalTextboxInput.bind(this);
  this._onConditionalTextboxKeyPress = this._onConditionalTextboxKeyPress.bind(this);
}

SourcesView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the SourcesView");

    this.widget = new SideMenuWidget(document.getElementById("sources"), {
      showItemCheckboxes: true,
      showArrows: true
    });

    this.emptyText = L10N.getStr("noSourcesText");
    this._blackBoxCheckboxTooltip = L10N.getStr("blackBoxCheckboxTooltip");

    this._commandset = document.getElementById("debuggerCommands");
    this._popupset = document.getElementById("debuggerPopupset");
    this._cmPopup = document.getElementById("sourceEditorContextMenu");
    this._cbPanel = document.getElementById("conditional-breakpoint-panel");
    this._cbTextbox = document.getElementById("conditional-breakpoint-panel-textbox");
    this._editorDeck = document.getElementById("editor-deck");
    this._stopBlackBoxButton = document.getElementById("black-boxed-message-button");
    this._prettyPrintButton = document.getElementById("pretty-print");

    window.on(EVENTS.EDITOR_LOADED, this._onEditorLoad, false);
    window.on(EVENTS.EDITOR_UNLOADED, this._onEditorUnload, false);
    this.widget.addEventListener("select", this._onSourceSelect, false);
    this.widget.addEventListener("click", this._onSourceClick, false);
    this.widget.addEventListener("check", this._onSourceCheck, false);
    this._stopBlackBoxButton.addEventListener("click", this._onStopBlackBoxing, false);
    this._prettyPrintButton.addEventListener("click", this.prettyPrint, false);
    this._cbPanel.addEventListener("popupshowing", this._onConditionalPopupShowing, false);
    this._cbPanel.addEventListener("popupshown", this._onConditionalPopupShown, false);
    this._cbPanel.addEventListener("popuphiding", this._onConditionalPopupHiding, false);
    this._cbTextbox.addEventListener("input", this._onConditionalTextboxInput, false);
    this._cbTextbox.addEventListener("keypress", this._onConditionalTextboxKeyPress, false);

    this.autoFocusOnSelection = false;

    // Show an empty label by default.
    this.empty();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the SourcesView");

    window.off(EVENTS.EDITOR_LOADED, this._onEditorLoad, false);
    window.off(EVENTS.EDITOR_UNLOADED, this._onEditorUnload, false);
    this.widget.removeEventListener("select", this._onSourceSelect, false);
    this.widget.removeEventListener("click", this._onSourceClick, false);
    this.widget.removeEventListener("check", this._onSourceCheck, false);
    this._stopBlackBoxButton.removeEventListener("click", this._onStopBlackBoxing, false);
    this._prettyPrintButton.removeEventListener("click", this.prettyPrint, false);
    this._cbPanel.removeEventListener("popupshowing", this._onConditionalPopupShowing, false);
    this._cbPanel.removeEventListener("popupshowing", this._onConditionalPopupShown, false);
    this._cbPanel.removeEventListener("popuphiding", this._onConditionalPopupHiding, false);
    this._cbTextbox.removeEventListener("input", this._onConditionalTextboxInput, false);
    this._cbTextbox.removeEventListener("keypress", this._onConditionalTextboxKeyPress, false);
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

  /**
   * Adds a source to this sources container.
   *
   * @param object aSource
   *        The source object coming from the active thread.
   * @param object aOptions [optional]
   *        Additional options for adding the source. Supported options:
   *        - staged: true to stage the item to be appended later
   */
  addSource: function(aSource, aOptions = {}) {
    let url = aSource.url;
    let label = SourceUtils.getSourceLabel(url.split(" -> ").pop());
    let group = SourceUtils.getSourceGroup(url.split(" -> ").pop());

    // Append a source item to this container.
    this.push([label, url, group], {
      staged: aOptions.staged, /* stage the item to be appended later? */
      attachment: {
        checkboxState: !aSource.isBlackBoxed,
        checkboxTooltip: this._blackBoxCheckboxTooltip,
        source: aSource
      }
    });
  },

  /**
   * Adds a breakpoint to this sources container.
   *
   * @param object aBreakpointData
   *        Information about the breakpoint to be shown.
   *        This object must have the following properties:
   *          - location: the breakpoint's source location and line number
   *          - disabled: the breakpoint's disabled state, boolean
   *          - text: the breakpoint's line text to be displayed
   * @param object aOptions [optional]
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  addBreakpoint: function(aBreakpointData, aOptions = {}) {
    let { location, disabled } = aBreakpointData;

    // Make sure we're not duplicating anything. If a breakpoint at the
    // specified source url and line already exists, just toggle it.
    if (this.getBreakpoint(location)) {
      this[disabled ? "disableBreakpoint" : "enableBreakpoint"](location);
      return;
    }

    // Get the source item to which the breakpoint should be attached.
    let sourceItem = this.getItemByValue(location.url);

    // Create the element node and menu popup for the breakpoint item.
    let breakpointArgs = Heritage.extend(aBreakpointData, aOptions);
    let breakpointView = this._createBreakpointView.call(this, breakpointArgs);
    let contextMenu = this._createContextMenu.call(this, breakpointArgs);

    // Append a breakpoint child item to the corresponding source item.
    sourceItem.append(breakpointView.container, {
      attachment: Heritage.extend(breakpointArgs, {
        url: location.url,
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

    // Highlight the newly appended breakpoint child item if necessary.
    if (aOptions.openPopup || !aOptions.noEditorUpdate) {
      this.highlightBreakpoint(location, aOptions);
    }
  },

  /**
   * Removes a breakpoint from this sources container.
   * It does not also remove the breakpoint from the controller. Be careful.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  removeBreakpoint: function(aLocation) {
    // When a parent source item is removed, all the child breakpoint items are
    // also automagically removed.
    let sourceItem = this.getItemByValue(aLocation.url);
    if (!sourceItem) {
      return;
    }
    let breakpointItem = this.getBreakpoint(aLocation);
    if (!breakpointItem) {
      return;
    }

    // Clear the breakpoint view.
    sourceItem.remove(breakpointItem);
  },

  /**
   * Returns the breakpoint at the specified source url and line.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   * @return object
   *         The corresponding breakpoint item if found, null otherwise.
   */
  getBreakpoint: function(aLocation) {
    return this.getItemForPredicate(aItem =>
      aItem.attachment.url == aLocation.url &&
      aItem.attachment.line == aLocation.line);
  },

  /**
   * Returns all breakpoints which are not at the specified source url and line.
   *
   * @param object aLocation [optional]
   *        @see DebuggerController.Breakpoints.addBreakpoint
   * @param array aStore [optional]
   *        A list in which to store the corresponding breakpoints.
   * @return array
   *         The corresponding breakpoints if found, an empty array otherwise.
   */
  getOtherBreakpoints: function(aLocation = {}, aStore = []) {
    for (let source in this) {
      for (let breakpointItem in source) {
        let { url, line } = breakpointItem.attachment;
        if (url != aLocation.url || line != aLocation.line) {
          aStore.push(breakpointItem);
        }
      }
    }
    return aStore;
  },

  /**
   * Enables a breakpoint.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - silent: pass true to not update the checkbox checked state;
   *                    this is usually necessary when the checked state will
   *                    be updated automatically (e.g: on a checkbox click).
   * @return object
   *         A promise that is resolved after the breakpoint is enabled, or
   *         rejected if no breakpoint was found at the specified location.
   */
  enableBreakpoint: function(aLocation, aOptions = {}) {
    let breakpointItem = this.getBreakpoint(aLocation);
    if (!breakpointItem) {
      return promise.reject(new Error("No breakpoint found."));
    }

    // Breakpoint will now be enabled.
    let attachment = breakpointItem.attachment;
    attachment.disabled = false;

    // Update the corresponding menu items to reflect the enabled state.
    let prefix = "bp-cMenu-"; // "breakpoints context menu"
    let identifier = DebuggerController.Breakpoints.getIdentifier(attachment);
    let enableSelfId = prefix + "enableSelf-" + identifier + "-menuitem";
    let disableSelfId = prefix + "disableSelf-" + identifier + "-menuitem";
    document.getElementById(enableSelfId).setAttribute("hidden", "true");
    document.getElementById(disableSelfId).removeAttribute("hidden");

    // Update the checkbox state if necessary.
    if (!aOptions.silent) {
      attachment.view.checkbox.setAttribute("checked", "true");
    }

    return DebuggerController.Breakpoints.addBreakpoint(aLocation, {
      // No need to update the pane, since this method is invoked because
      // a breakpoint's view was interacted with.
      noPaneUpdate: true
    });
  },

  /**
   * Disables a breakpoint.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   * @param object aOptions [optional]
   *        Additional options or flags supported by this operation:
   *          - silent: pass true to not update the checkbox checked state;
   *                    this is usually necessary when the checked state will
   *                    be updated automatically (e.g: on a checkbox click).
   * @return object
   *         A promise that is resolved after the breakpoint is disabled, or
   *         rejected if no breakpoint was found at the specified location.
   */
  disableBreakpoint: function(aLocation, aOptions = {}) {
    let breakpointItem = this.getBreakpoint(aLocation);
    if (!breakpointItem) {
      return promise.reject(new Error("No breakpoint found."));
    }

    // Breakpoint will now be disabled.
    let attachment = breakpointItem.attachment;
    attachment.disabled = true;

    // Update the corresponding menu items to reflect the disabled state.
    let prefix = "bp-cMenu-"; // "breakpoints context menu"
    let identifier = DebuggerController.Breakpoints.getIdentifier(attachment);
    let enableSelfId = prefix + "enableSelf-" + identifier + "-menuitem";
    let disableSelfId = prefix + "disableSelf-" + identifier + "-menuitem";
    document.getElementById(enableSelfId).removeAttribute("hidden");
    document.getElementById(disableSelfId).setAttribute("hidden", "true");

    // Update the checkbox state if necessary.
    if (!aOptions.silent) {
      attachment.view.checkbox.removeAttribute("checked");
    }

    return DebuggerController.Breakpoints.removeBreakpoint(aLocation, {
      // No need to update this pane, since this method is invoked because
      // a breakpoint's view was interacted with.
      noPaneUpdate: true,
      // Mark this breakpoint as being "disabled", not completely removed.
      // This makes sure it will not be forgotten across target navigations.
      rememberDisabled: true
    });
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
  highlightBreakpoint: function(aLocation, aOptions = {}) {
    let breakpointItem = this.getBreakpoint(aLocation);
    if (!breakpointItem) {
      return;
    }

    // Breakpoint will now be selected.
    this._selectBreakpoint(breakpointItem);

    // Update the editor location if necessary.
    if (!aOptions.noEditorUpdate) {
      DebuggerView.setEditorLocation(aLocation.url, aLocation.line, { noDebug: true });
    }

    // If the breakpoint requires a new conditional expression, display
    // the panel to input the corresponding expression.
    if (aOptions.openPopup) {
      this._openConditionalPopup();
    } else {
      this._hideConditionalPopup();
    }
  },

  /**
   * Unhighlights the current breakpoint in this sources container.
   */
  unhighlightBreakpoint: function() {
    this._unselectBreakpoint();
    this._hideConditionalPopup();
  },

  /**
   * Pretty print the selected source.
   */
  prettyPrint: function() {
    const resetEditor = ([{ url }]) => {
      // Only set the text when the source is still selected.
      if (url == this.selectedValue) {
        DebuggerView.setEditorLocation(url, 0, { force: true });
      }
    };
    const printError = ([{ url }, error]) => {
      let err = DevToolsUtils.safeErrorString(error);
      let msg = "Couldn't prettify source: " + url + "\n" + err;
      Cu.reportError(msg);
      dumpn(msg);
      return;
    }

    let { source } = this.selectedItem.attachment;
    let prettyPrinted = DebuggerController.SourceScripts.prettyPrint(source);
    prettyPrinted.then(resetEditor, printError);
  },

  /**
   * Marks a breakpoint as selected in this sources container.
   *
   * @param object aItem
   *        The breakpoint item to select.
   */
  _selectBreakpoint: function(aItem) {
    if (this._selectedBreakpointItem == aItem) {
      return;
    }
    this._unselectBreakpoint();
    this._selectedBreakpointItem = aItem;
    this._selectedBreakpointItem.target.classList.add("selected");

    // Ensure the currently selected breakpoint is visible.
    this.widget.ensureElementIsVisible(aItem.target);
  },

  /**
   * Marks the current breakpoint as unselected in this sources container.
   */
  _unselectBreakpoint: function() {
    if (!this._selectedBreakpointItem) {
      return;
    }
    this._selectedBreakpointItem.target.classList.remove("selected");
    this._selectedBreakpointItem = null;
  },

  /**
   * Opens a conditional breakpoint's expression input popup.
   */
  _openConditionalPopup: function() {
    let breakpointItem = this._selectedBreakpointItem;
    let attachment = breakpointItem.attachment;

    // Check if this is an enabled conditional breakpoint, and if so,
    // retrieve the current conditional epression.
    let breakpointPromise = DebuggerController.Breakpoints._getAdded(attachment);
    if (breakpointPromise) {
      breakpointPromise.then(aBreakpointClient => {
        let isConditionalBreakpoint = "conditionalExpression" in aBreakpointClient;
        let conditionalExpression = aBreakpointClient.conditionalExpression;
        doOpen.call(this, isConditionalBreakpoint ? conditionalExpression : "")
      });
    } else {
      doOpen.call(this, "")
    }

    function doOpen(aConditionalExpression) {
      // Update the conditional expression textbox. If no expression was
      // previously set, revert to using an empty string by default.
      this._cbTextbox.value = aConditionalExpression;

      // Show the conditional expression panel. The popup arrow should be pointing
      // at the line number node in the breakpoint item view.
      this._cbPanel.hidden = false;
      this._cbPanel.openPopup(breakpointItem.attachment.view.lineNumber,
        BREAKPOINT_CONDITIONAL_POPUP_POSITION,
        BREAKPOINT_CONDITIONAL_POPUP_OFFSET_X,
        BREAKPOINT_CONDITIONAL_POPUP_OFFSET_Y);
    }
  },

  /**
   * Hides a conditional breakpoint's expression input popup.
   */
  _hideConditionalPopup: function() {
    this._cbPanel.hidden = true;
    this._cbPanel.hidePopup();
  },

  /**
   * Customization function for creating a breakpoint item's UI.
   *
   * @param object aOptions
   *        A couple of options or flags supported by this operation:
   *          - location: the breakpoint's source location and line number
   *          - disabled: the breakpoint's disabled state, boolean
   *          - text: the breakpoint's line text to be displayed
   * @return object
   *         An object containing the breakpoint container, checkbox,
   *         line number and line text nodes.
   */
  _createBreakpointView: function(aOptions) {
    let { location, disabled, text } = aOptions;
    let identifier = DebuggerController.Breakpoints.getIdentifier(location);

    let checkbox = document.createElement("checkbox");
    checkbox.setAttribute("checked", !disabled);
    checkbox.className = "dbg-breakpoint-checkbox";

    let lineNumberNode = document.createElement("label");
    lineNumberNode.className = "plain dbg-breakpoint-line";
    lineNumberNode.setAttribute("value", location.line);

    let lineTextNode = document.createElement("label");
    lineTextNode.className = "plain dbg-breakpoint-text";
    lineTextNode.setAttribute("value", text);
    lineTextNode.setAttribute("crop", "end");
    lineTextNode.setAttribute("flex", "1");

    let tooltip = text.substr(0, BREAKPOINT_LINE_TOOLTIP_MAX_LENGTH);
    lineTextNode.setAttribute("tooltiptext", tooltip);

    let container = document.createElement("hbox");
    container.id = "breakpoint-" + identifier;
    container.className = "dbg-breakpoint side-menu-widget-item-other";
    container.classList.add("devtools-monospace");
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
   * @param object aOptions
   *        A couple of options or flags supported by this operation:
   *          - location: the breakpoint's source location and line number
   *          - disabled: the breakpoint's disabled state, boolean
   * @return object
   *         An object containing the breakpoint commandset and menu popup ids.
   */
  _createContextMenu: function(aOptions) {
    let { location, disabled } = aOptions;
    let identifier = DebuggerController.Breakpoints.getIdentifier(location);

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
   * Function called each time a breakpoint item is removed.
   *
   * @param object aItem
   *        The corresponding item.
   */
  _onBreakpointRemoved: function(aItem) {
    dumpn("Finalizing breakpoint item: " + aItem);

    // Destroy the context menu for the breakpoint.
    let contextMenu = aItem.attachment.popup;
    document.getElementById(contextMenu.commandsetId).remove();
    document.getElementById(contextMenu.menupopupId).remove();

    // Clear the breakpoint selection.
    if (this._selectedBreakpointItem == aItem) {
      this._selectedBreakpointItem = null;
    }
  },

  /**
   * The load listener for the source editor.
   */
  _onEditorLoad: function(aName, aEditor) {
    aEditor.addEventListener(SourceEditor.EVENTS.SELECTION, this._onEditorSelection, false);
    aEditor.addEventListener(SourceEditor.EVENTS.CONTEXT_MENU, this._onEditorContextMenu, false);
  },

  /**
   * The unload listener for the source editor.
   */
  _onEditorUnload: function(aName, aEditor) {
    aEditor.removeEventListener(SourceEditor.EVENTS.SELECTION, this._onEditorSelection, false);
    aEditor.removeEventListener(SourceEditor.EVENTS.CONTEXT_MENU, this._onEditorContextMenu, false);
  },

  /**
   * The selection listener for the source editor.
   */
  _onEditorSelection: function(e) {
    let { start, end } = e.newValue;

    let url = this.selectedValue;
    let lineStart = DebuggerView.editor.getLineAtOffset(start) + 1;
    let lineEnd = DebuggerView.editor.getLineAtOffset(end) + 1;
    let location = { url: url, line: lineStart };

    if (this.getBreakpoint(location) && lineStart == lineEnd) {
      this.highlightBreakpoint(location, { noEditorUpdate: true });
    } else {
      this.unhighlightBreakpoint();
    }
  },

  /**
   * The context menu listener for the source editor.
   */
  _onEditorContextMenu: function({ x, y }) {
    let offset = DebuggerView.editor.getOffsetAtLocation(x, y);
    let line = DebuggerView.editor.getLineAtOffset(offset);
    this._editorContextMenuLineNumber = line;
  },

  /**
   * The select listener for the sources container.
   */
  _onSourceSelect: function({ detail: sourceItem }) {
    if (!sourceItem) {
      return;
    }
    // The container is not empty and an actual item was selected.
    DebuggerView.setEditorLocation(sourceItem.value);
    this.maybeShowBlackBoxMessage();
  },

  /**
   * Show or hide the black box message vs. source editor depending on if the
   * selected source is black boxed or not.
   */
  maybeShowBlackBoxMessage: function() {
    let sourceForm = this.selectedItem.attachment.source;
    let sourceClient = DebuggerController.activeThread.source(sourceForm);
    this._editorDeck.selectedIndex = sourceClient.isBlackBoxed ? 1 : 0;
  },

  /**
   * The click listener for the sources container.
   */
  _onSourceClick: function() {
    // Use this container as a filtering target.
    DebuggerView.Filtering.target = this;
  },

  /**
   * The check listener for the sources container.
   */
  _onSourceCheck: function({ detail: { checked }, target }) {
    let item = this.getItemForElement(target);
    DebuggerController.SourceScripts.blackBox(item.attachment.source, !checked);
  },

  /**
   * The click listener for the "stop black boxing" button.
   */
  _onStopBlackBoxing: function() {
    let sourceForm = this.selectedItem.attachment.source;
    DebuggerController.SourceScripts.blackBox(sourceForm, false);
  },

  /**
   * The click listener for a breakpoint container.
   */
  _onBreakpointClick: function(e) {
    let sourceItem = this.getItemForElement(e.target);
    let breakpointItem = this.getItemForElement.call(sourceItem, e.target);
    let attachment = breakpointItem.attachment;

    // Check if this is an enabled conditional breakpoint.
    let breakpointPromise = DebuggerController.Breakpoints._getAdded(attachment);
    if (breakpointPromise) {
      breakpointPromise.then(aBreakpointClient => {
        doHighlight.call(this, "conditionalExpression" in aBreakpointClient);
      });
    } else {
      doHighlight.call(this, false);
    }

    function doHighlight(aConditionalBreakpointFlag) {
      // Highlight the breakpoint in this pane and in the editor.
      this.highlightBreakpoint(attachment, {
        // Don't show the conditional expression popup if this is not a
        // conditional breakpoint, or the right mouse button was pressed (to
        // avoid clashing the popup with the context menu).
        openPopup: aConditionalBreakpointFlag && e.button == 0
      });
    }
  },

  /**
   * The click listener for a breakpoint checkbox.
   */
  _onBreakpointCheckboxClick: function(e) {
    let sourceItem = this.getItemForElement(e.target);
    let breakpointItem = this.getItemForElement.call(sourceItem, e.target);
    let attachment = breakpointItem.attachment;

    // Toggle the breakpoint enabled or disabled.
    this[attachment.disabled ? "enableBreakpoint" : "disableBreakpoint"](attachment, {
      // Do this silently (don't update the checkbox checked state), since
      // this listener is triggered because a checkbox was already clicked.
      silent: true
    });

    // Don't update the editor location (avoid propagating into _onBreakpointClick).
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * The popup showing listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupShowing: function() {
    this._conditionalPopupVisible = true; // Used in tests.
    window.emit(EVENTS.CONDITIONAL_BREAKPOINT_POPUP_SHOWING);
  },

  /**
   * The popup shown listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupShown: function() {
    this._cbTextbox.focus();
    this._cbTextbox.select();
  },

  /**
   * The popup hiding listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupHiding: function() {
    this._conditionalPopupVisible = false; // Used in tests.
    window.emit(EVENTS.CONDITIONAL_BREAKPOINT_POPUP_HIDING);
  },

  /**
   * The input listener for the breakpoints conditional expression textbox.
   */
  _onConditionalTextboxInput: function() {
    let breakpointItem = this._selectedBreakpointItem;
    let attachment = breakpointItem.attachment;

    // Check if this is an enabled conditional breakpoint, and if so,
    // save the current conditional epression.
    let breakpointPromise = DebuggerController.Breakpoints._getAdded(attachment);
    if (breakpointPromise) {
      breakpointPromise.then(aBreakpointClient => {
        aBreakpointClient.conditionalExpression = this._cbTextbox.value;
      });
    }
  },

  /**
   * The keypress listener for the breakpoints conditional expression textbox.
   */
  _onConditionalTextboxKeyPress: function(e) {
    if (e.keyCode == e.DOM_VK_RETURN || e.keyCode == e.DOM_VK_ENTER) {
      this._hideConditionalPopup();
    }
  },

  /**
   * Called when the add breakpoint key sequence was pressed.
   */
  _onCmdAddBreakpoint: function() {
    // If this command was executed via the context menu, add the breakpoint
    // on the currently hovered line in the source editor.
    if (this._editorContextMenuLineNumber >= 0) {
      DebuggerView.editor.setCaretPosition(this._editorContextMenuLineNumber);
    }
    // Avoid placing breakpoints incorrectly when using key shortcuts.
    this._editorContextMenuLineNumber = -1;

    let url = DebuggerView.Sources.selectedValue;
    let line = DebuggerView.editor.getCaretPosition().line + 1;
    let location = { url: url, line: line };
    let breakpointItem = this.getBreakpoint(location);

    // If a breakpoint already existed, remove it now.
    if (breakpointItem) {
      DebuggerController.Breakpoints.removeBreakpoint(location);
    }
    // No breakpoint existed at the required location, add one now.
    else {
      DebuggerController.Breakpoints.addBreakpoint(location);
    }
  },

  /**
   * Called when the add conditional breakpoint key sequence was pressed.
   */
  _onCmdAddConditionalBreakpoint: function() {
    // If this command was executed via the context menu, add the breakpoint
    // on the currently hovered line in the source editor.
    if (this._editorContextMenuLineNumber >= 0) {
      DebuggerView.editor.setCaretPosition(this._editorContextMenuLineNumber);
    }
    // Avoid placing breakpoints incorrectly when using key shortcuts.
    this._editorContextMenuLineNumber = -1;

    let url =  DebuggerView.Sources.selectedValue;
    let line = DebuggerView.editor.getCaretPosition().line + 1;
    let location = { url: url, line: line };
    let breakpointItem = this.getBreakpoint(location);

    // If a breakpoint already existed or wasn't a conditional, morph it now.
    if (breakpointItem) {
      this.highlightBreakpoint(location, { openPopup: true });
    }
    // No breakpoint existed at the required location, add one now.
    else {
      DebuggerController.Breakpoints.addBreakpoint(location, { openPopup: true });
    }
  },

  /**
   * Function invoked on the "setConditional" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onSetConditional: function(aLocation) {
    // Highlight the breakpoint and show a conditional expression popup.
    this.highlightBreakpoint(aLocation, { openPopup: true });
  },

  /**
   * Function invoked on the "enableSelf" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onEnableSelf: function(aLocation) {
    // Enable the breakpoint, in this container and the controller store.
    this.enableBreakpoint(aLocation);
  },

  /**
   * Function invoked on the "disableSelf" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onDisableSelf: function(aLocation) {
    // Disable the breakpoint, in this container and the controller store.
    this.disableBreakpoint(aLocation);
  },

  /**
   * Function invoked on the "deleteSelf" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onDeleteSelf: function(aLocation) {
    // Remove the breakpoint, from this container and the controller store.
    this.removeBreakpoint(aLocation);
    DebuggerController.Breakpoints.removeBreakpoint(aLocation);
  },

  /**
   * Function invoked on the "enableOthers" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onEnableOthers: function(aLocation) {
    let enableOthers = aCallback => {
      let other = this.getOtherBreakpoints(aLocation);
      let outstanding = other.map(e => this.enableBreakpoint(e.attachment));
      promise.all(outstanding).then(aCallback);
    }

    // Breakpoints can only be set while the debuggee is paused. To avoid
    // an avalanche of pause/resume interrupts of the main thread, simply
    // pause it beforehand if it's not already.
    if (gThreadClient.state != "paused") {
      gThreadClient.interrupt(() => enableOthers(() => gThreadClient.resume()));
    } else {
      enableOthers();
    }
  },

  /**
   * Function invoked on the "disableOthers" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onDisableOthers: function(aLocation) {
    let other = this.getOtherBreakpoints(aLocation);
    other.forEach(e => this._onDisableSelf(e.attachment));
  },

  /**
   * Function invoked on the "deleteOthers" menuitem command.
   *
   * @param object aLocation
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  _onDeleteOthers: function(aLocation) {
    let other = this.getOtherBreakpoints(aLocation);
    other.forEach(e => this._onDeleteSelf(e.attachment));
  },

  /**
   * Function invoked on the "enableAll" menuitem command.
   */
  _onEnableAll: function() {
    this._onEnableOthers(undefined);
  },

  /**
   * Function invoked on the "disableAll" menuitem command.
   */
  _onDisableAll: function() {
    this._onDisableOthers(undefined);
  },

  /**
   * Function invoked on the "deleteAll" menuitem command.
   */
  _onDeleteAll: function() {
    this._onDeleteOthers(undefined);
  },

  _commandset: null,
  _popupset: null,
  _cmPopup: null,
  _cbPanel: null,
  _cbTextbox: null,
  _selectedBreakpointItem: null,
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
   * Returns true if the specified url and/or content type are specific to
   * javascript files.
   *
   * @return boolean
   *         True if the source is likely javascript.
   */
  isJavaScript: function(aUrl, aContentType = "") {
    return /\.jsm?$/.test(this.trimUrlQuery(aUrl)) ||
           aContentType.contains("javascript");
  },

  /**
   * Clears the labels cache, populated by methods like
   * SourceUtils.getSourceLabel or Source Utils.getSourceGroup.
   * This should be done every time the content location changes.
   */
  clearCache: function() {
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
  getSourceLabel: function(aUrl) {
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
  getSourceGroup: function(aUrl) {
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

    let { scheme, directory, fileName } = uri;
    let hostPort;
    // Add-on SDK jar: URLs will cause accessing hostPort to throw.
    if (scheme != "jar") {
      hostPort = uri.hostPort;
    }
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
  trimUrlLength: function(aUrl, aLength, aSection) {
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
  trimUrlQuery: function(aUrl) {
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
  trimUrl: function(aUrl, aLabel, aSeq) {
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

  this.switchExpression = this.switchExpression.bind(this);
  this.deleteExpression = this.deleteExpression.bind(this);
  this._createItemView = this._createItemView.bind(this);
  this._onClick = this._onClick.bind(this);
  this._onClose = this._onClose.bind(this);
  this._onBlur = this._onBlur.bind(this);
  this._onKeyPress = this._onKeyPress.bind(this);
}

WatchExpressionsView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the WatchExpressionsView");

    this.widget = new ListWidget(document.getElementById("expressions"));
    this.widget.permaText = L10N.getStr("addWatchExpressionText");
    this.widget.itemFactory = this._createItemView;
    this.widget.setAttribute("context", "debuggerWatchExpressionsContextMenu");
    this.widget.addEventListener("click", this._onClick, false);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the WatchExpressionsView");

    this.widget.removeEventListener("click", this._onClick, false);
  },

  /**
   * Adds a watch expression in this container.
   *
   * @param string aExpression [optional]
   *        An optional initial watch expression text.
   */
  addExpression: function(aExpression = "") {
    // Watch expressions are UI elements which benefit from visible panes.
    DebuggerView.showInstrumentsPane();

    // Append a watch expression item to this container.
    let expressionItem = this.push([, aExpression], {
      index: 0, /* specifies on which position should the item be appended */
      relaxed: true, /* this container should allow dupes & degenerates */
      attachment: {
        initialExpression: aExpression,
        currentExpression: ""
      }
    });

    // Automatically focus the new watch expression input.
    expressionItem.attachment.inputNode.select();
    expressionItem.attachment.inputNode.focus();
    DebuggerView.Variables.parentNode.scrollTop = 0;
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
  switchExpression: function(aVar, aExpression) {
    let expressionItem =
      [i for (i in this) if (i.attachment.currentExpression == aVar.name)][0];

    // Remove the watch expression if it's going to be empty or a duplicate.
    if (!aExpression || this.getAllStrings().indexOf(aExpression) != -1) {
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
  deleteExpression: function(aVar) {
    let expressionItem =
      [i for (i in this) if (i.attachment.currentExpression == aVar.name)][0];

    // Remove the watch expression.
    this.remove(expressionItem);

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
  getString: function(aIndex) {
    return this.getItemAtIndex(aIndex).attachment.currentExpression;
  },

  /**
   * Gets the watch expressions code strings for all items in this container.
   *
   * @return array
   *         The watch expressions code strings.
   */
  getAllStrings: function() {
    return this.items.map(e => e.attachment.currentExpression);
  },

  /**
   * Customization function for creating an item's UI.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed item.
   * @param any aAttachment
   *        Some attached primitive/object.
   */
  _createItemView: function(aElementNode, aAttachment) {
    let arrowNode = document.createElement("box");
    arrowNode.className = "dbg-expression-arrow";

    let inputNode = document.createElement("textbox");
    inputNode.className = "plain dbg-expression-input devtools-monospace";
    inputNode.setAttribute("value", aAttachment.initialExpression);
    inputNode.setAttribute("flex", "1");

    let closeNode = document.createElement("toolbarbutton");
    closeNode.className = "plain variables-view-delete";

    closeNode.addEventListener("click", this._onClose, false);
    inputNode.addEventListener("blur", this._onBlur, false);
    inputNode.addEventListener("keypress", this._onKeyPress, false);

    aElementNode.className = "dbg-expression";
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
  _onCmdAddExpression: function(aText) {
    // Only add a new expression if there's no pending input.
    if (this.getAllStrings().indexOf("") == -1) {
      this.addExpression(aText || DebuggerView.editor.getSelectedText());
    }
  },

  /**
   * Called when the remove all watch expressions key sequence was pressed.
   */
  _onCmdRemoveAllExpressions: function() {
    // Empty the view of all the watch expressions and clear the cache.
    this.empty();

    // Synchronize with the controller's watch expressions store.
    DebuggerController.StackFrames.syncWatchExpressions();
  },

  /**
   * The click listener for this container.
   */
  _onClick: function(e) {
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
  _onClose: function(e) {
    // Remove the watch expression.
    this.remove(this.getItemForElement(e.target));

    // Synchronize with the controller's watch expressions store.
    DebuggerController.StackFrames.syncWatchExpressions();

    // Prevent clicking the expression element itself.
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * The blur listener for a watch expression's textbox.
   */
  _onBlur: function({ target: textbox }) {
    let expressionItem = this.getItemForElement(textbox);
    let oldExpression = expressionItem.attachment.currentExpression;
    let newExpression = textbox.value.trim();

    // Remove the watch expression if it's empty.
    if (!newExpression) {
      this.remove(expressionItem);
    }
    // Remove the watch expression if it's a duplicate.
    else if (!oldExpression && this.getAllStrings().indexOf(newExpression) != -1) {
      this.remove(expressionItem);
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
  _onKeyPress: function(e) {
    switch(e.keyCode) {
      case e.DOM_VK_RETURN:
      case e.DOM_VK_ENTER:
      case e.DOM_VK_ESCAPE:
        DebuggerView.editor.focus();
        return;
    }
  }
});

/**
 * Functions handling the event listeners UI.
 */
function EventListenersView() {
  dumpn("EventListenersView was instantiated");

  this._onCheck = this._onCheck.bind(this);
  this._onClick = this._onClick.bind(this);
}

EventListenersView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the EventListenersView");

    this.widget = new SideMenuWidget(document.getElementById("event-listeners"), {
      theme: "light",
      showItemCheckboxes: true,
      showGroupCheckboxes: true
    });

    this.emptyText = L10N.getStr("noEventListenersText");
    this._eventCheckboxTooltip = L10N.getStr("eventCheckboxTooltip");
    this._onSelectorString = " " + L10N.getStr("eventOnSelector") + " ";
    this._inSourceString = " " + L10N.getStr("eventInSource") + " ";
    this._inNativeCodeString = L10N.getStr("eventNative");

    this.widget.addEventListener("check", this._onCheck, false);
    this.widget.addEventListener("click", this._onClick, false);

    // Show an empty label by default.
    this.empty();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the EventListenersView");

    this.widget.removeEventListener("check", this._onCheck, false);
    this.widget.removeEventListener("click", this._onClick, false);
  },

  /**
   * Adds an event to this event listeners container.
   *
   * @param object aListener
   *        The listener object coming from the active thread.
   * @param object aOptions [optional]
   *        Additional options for adding the source. Supported options:
   *        - staged: true to stage the item to be appended later
   */
  addListener: function(aListener, aOptions = {}) {
    let { node: { selector }, function: { url }, type } = aListener;
    if (!type) return;

    // Some listener objects may be added from plugins, thus getting
    // translated to native code.
    if (!url) {
      url = this._inNativeCodeString;
    }

    // If an event item for this listener's url and type was already added,
    // avoid polluting the view and simply increase the "targets" count.
    let eventItem = this.getItemForPredicate(aItem =>
      aItem.attachment.url == url &&
      aItem.attachment.type == type);
    if (eventItem) {
      let { selectors, view: { targets } } = eventItem.attachment;
      if (selectors.indexOf(selector) == -1) {
        selectors.push(selector);
        targets.setAttribute("value", L10N.getFormatStr("eventNodes", selectors.length));
      }
      return;
    }

    // There's no easy way of grouping event types into higher-level groups,
    // so we need to do this by hand.
    let is = (...args) => args.indexOf(type) != -1;
    let has = str => type.contains(str);
    let starts = str => type.startsWith(str);
    let group;

    if (starts("animation")) {
      group = L10N.getStr("animationEvents");
    } else if (starts("audio")) {
      group = L10N.getStr("audioEvents");
    } else if (is("levelchange")) {
      group = L10N.getStr("batteryEvents");
    } else if (is("cut", "copy", "paste")) {
      group = L10N.getStr("clipboardEvents");
    } else if (starts("composition")) {
      group = L10N.getStr("compositionEvents");
    } else if (starts("device")) {
      group = L10N.getStr("deviceEvents");
    } else if (is("fullscreenchange", "fullscreenerror", "orientationchange",
      "overflow", "resize", "scroll", "underflow", "zoom")) {
      group = L10N.getStr("displayEvents");
    } else if (starts("drag") || starts("drop")) {
      group = L10N.getStr("Drag and dropEvents");
    } else if (starts("gamepad")) {
      group = L10N.getStr("gamepadEvents");
    } else if (is("canplay", "canplaythrough", "durationchange", "emptied",
      "ended", "loadeddata", "loadedmetadata", "pause", "play", "playing",
      "ratechange", "seeked", "seeking", "stalled", "suspend", "timeupdate",
      "volumechange", "waiting")) {
      group = L10N.getStr("mediaEvents");
    } else if (is("blocked", "complete", "success", "upgradeneeded", "versionchange")) {
      group = L10N.getStr("indexedDBEvents");
    } else if (is("blur", "change", "focus", "focusin", "focusout", "invalid",
      "reset", "select", "submit")) {
      group = L10N.getStr("interactionEvents");
    } else if (starts("key") || is("input")) {
      group = L10N.getStr("keyboardEvents");
    } else if (starts("mouse") || has("click") || is("contextmenu", "show", "wheel")) {
      group = L10N.getStr("mouseEvents");
    } else if (starts("DOM")) {
      group = L10N.getStr("mutationEvents");
    } else if (is("abort", "error", "hashchange", "load", "loadend", "loadstart",
      "pagehide", "pageshow", "progress", "timeout", "unload", "uploadprogress",
      "visibilitychange")) {
      group = L10N.getStr("navigationEvents");
    } else if (is("pointerlockchange", "pointerlockerror")) {
      group = L10N.getStr("Pointer lockEvents");
    } else if (is("compassneedscalibration", "userproximity")) {
      group = L10N.getStr("sensorEvents");
    } else if (starts("storage")) {
      group = L10N.getStr("storageEvents");
    } else if (is("beginEvent", "endEvent", "repeatEvent")) {
      group = L10N.getStr("timeEvents");
    } else if (starts("touch")) {
      group = L10N.getStr("touchEvents");
    } else {
      group = L10N.getStr("otherEvents");
    }

    // Create the element node for the event listener item.
    let itemView = this._createItemView(type, selector, url);

    // Event breakpoints survive target navigations. Make sure the newly
    // inserted event item is correctly checked.
    let checkboxState =
      DebuggerController.Breakpoints.DOM.activeEventNames.indexOf(type) != -1;

    // Append an event listener item to this container.
    this.push([itemView.container, url, group], {
      staged: aOptions.staged, /* stage the item to be appended later? */
      attachment: {
        url: url,
        type: type,
        view: itemView,
        selectors: [selector],
        checkboxState: checkboxState,
        checkboxTooltip: this._eventCheckboxTooltip
      }
    });
  },

  /**
   * Gets all the event types known to this container.
   *
   * @return array
   *         List of event types, for example ["load", "click"...]
   */
  getAllEvents: function() {
    return this.attachments.map(e => e.type);
  },

  /**
   * Gets the checked event types in this container.
   *
   * @return array
   *         List of event types, for example ["load", "click"...]
   */
  getCheckedEvents: function() {
    return this.attachments.filter(e => e.checkboxState).map(e => e.type);
  },

  /**
   * Customization function for creating an item's UI.
   *
   * @param string aType
   *        The event type, for example "click".
   * @param string aSelector
   *        The target element's selector.
   * @param string url
   *        The source url in which the event listener is located.
   * @return object
   *         An object containing the event listener view nodes.
   */
  _createItemView: function(aType, aSelector, aUrl) {
    let container = document.createElement("hbox");
    container.className = "dbg-event-listener";

    let eventType = document.createElement("label");
    eventType.className = "plain dbg-event-listener-type";
    eventType.setAttribute("value", aType);
    container.appendChild(eventType);

    let typeSeparator = document.createElement("label");
    typeSeparator.className = "plain dbg-event-listener-separator";
    typeSeparator.setAttribute("value", this._onSelectorString);
    container.appendChild(typeSeparator);

    let eventTargets = document.createElement("label");
    eventTargets.className = "plain dbg-event-listener-targets";
    eventTargets.setAttribute("value", aSelector);
    container.appendChild(eventTargets);

    let selectorSeparator = document.createElement("label");
    selectorSeparator.className = "plain dbg-event-listener-separator";
    selectorSeparator.setAttribute("value", this._inSourceString);
    container.appendChild(selectorSeparator);

    let eventLocation = document.createElement("label");
    eventLocation.className = "plain dbg-event-listener-location";
    eventLocation.setAttribute("value", SourceUtils.getSourceLabel(aUrl));
    eventLocation.setAttribute("flex", "1");
    eventLocation.setAttribute("crop", "center");
    container.appendChild(eventLocation);

    return {
      container: container,
      type: eventType,
      targets: eventTargets,
      location: eventLocation
    };
  },

  /**
   * The check listener for the event listeners container.
   */
  _onCheck: function({ detail: { description, checked }, target }) {
    if (description == "item") {
      this.getItemForElement(target).attachment.checkboxState = checked;
      DebuggerController.Breakpoints.DOM.scheduleEventBreakpointsUpdate();
      return;
    }

    // Check all the event items in this group.
    this.items
      .filter(e => e.description == description)
      .forEach(e => this.callMethod("checkItem", e.target, checked));
  },

  /**
   * The select listener for the event listeners container.
   */
  _onClick: function({ target }) {
    // Changing the checkbox state is handled by the _onCheck event. Avoid
    // handling that again in this click event, so pass in "noSiblings"
    // when retrieving the target's item, to ignore the checkbox.
    let eventItem = this.getItemForElement(target, { noSiblings: true });
    if (eventItem) {
      let newState = eventItem.attachment.checkboxState ^= 1;
      this.callMethod("checkItem", eventItem.target, newState);
    }
  },

  _eventCheckboxTooltip: "",
  _onSelectorString: "",
  _inSourceString: "",
  _inNativeCodeString: ""
});

/**
 * Functions handling the global search UI.
 */
function GlobalSearchView() {
  dumpn("GlobalSearchView was instantiated");

  this._createItemView = this._createItemView.bind(this);
  this._onHeaderClick = this._onHeaderClick.bind(this);
  this._onLineClick = this._onLineClick.bind(this);
  this._onMatchClick = this._onMatchClick.bind(this);
}

GlobalSearchView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the GlobalSearchView");

    this.widget = new ListWidget(document.getElementById("globalsearch"));
    this._splitter = document.querySelector("#globalsearch + .devtools-horizontal-splitter");

    this.widget.emptyText = L10N.getStr("noMatchingStringsText");
    this.widget.itemFactory = this._createItemView;
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the GlobalSearchView");
  },

  /**
   * Sets the results container hidden or visible. It's hidden by default.
   * @param boolean aFlag
   */
  set hidden(aFlag) {
    this.widget.setAttribute("hidden", aFlag);
    this._splitter.setAttribute("hidden", aFlag);
  },

  /**
   * Gets the visibility state of the global search container.
   * @return boolean
   */
  get hidden()
    this.widget.getAttribute("hidden") == "true" ||
    this._splitter.getAttribute("hidden") == "true",

  /**
   * Hides and removes all items from this search container.
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
   * Selects the previously found item in this container.
   * Does not change the currently focused node.
   */
  selectPrev: function() {
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
   * Schedules searching for a string in all of the sources.
   *
   * @param string aToken
   *        The string to search for.
   * @param number aWait
   *        The amount of milliseconds to wait until draining.
   */
  scheduleSearch: function(aToken, aWait) {
    // The amount of time to wait for the requests to settle.
    let maxDelay = GLOBAL_SEARCH_ACTION_MAX_DELAY;
    let delay = aWait === undefined ? maxDelay / aToken.length : aWait;

    // Allow requests to settle down first.
    setNamedTimeout("global-search", delay, () => {
      // Start fetching as many sources as possible, then perform the search.
      let urls = DebuggerView.Sources.values;
      let sourcesFetched = DebuggerController.SourceScripts.getTextForSources(urls);
      sourcesFetched.then(aSources => this._doSearch(aToken, aSources));
    });
  },

  /**
   * Finds string matches in all the sources stored in the controller's cache,
   * and groups them by url and line number.
   *
   * @param string aToken
   *        The string to search for.
   * @param array aSources
   *        An array of [url, text] tuples for each source.
   */
  _doSearch: function(aToken, aSources) {
    // Don't continue filtering if the searched token is an empty string.
    if (!aToken) {
      this.clearView();
      return;
    }

    // Search is not case sensitive, prepare the actual searched token.
    let lowerCaseToken = aToken.toLowerCase();
    let tokenLength = aToken.length;

    // Create a Map containing search details for each source.
    let globalResults = new GlobalResults();

    // Search for the specified token in each source's text.
    for (let [url, text] of aSources) {
      // Verify that the search token is found anywhere in the source.
      if (!text.toLowerCase().contains(lowerCaseToken)) {
        continue;
      }
      // ...and if so, create a Map containing search details for each line.
      let sourceResults = new SourceResults(url, globalResults);

      // Search for the specified token in each line's text.
      text.split("\n").forEach((aString, aLine) => {
        // Search is not case sensitive, prepare the actual searched line.
        let lowerCaseLine = aString.toLowerCase();

        // Verify that the search token is found anywhere in this line.
        if (!lowerCaseLine.contains(lowerCaseToken)) {
          return;
        }
        // ...and if so, create a Map containing search details for each word.
        let lineResults = new LineResults(aLine, sourceResults);

        // Search for the specified token this line's text.
        lowerCaseLine.split(lowerCaseToken).reduce((aPrev, aCurr, aIndex, aArray) => {
          let prevLength = aPrev.length;
          let currLength = aCurr.length;

          // Everything before the token is unmatched.
          let unmatched = aString.substr(prevLength, currLength);
          lineResults.add(unmatched);

          // The lowered-case line was split by the lowered-case token. So,
          // get the actual matched text from the original line's text.
          if (aIndex != aArray.length - 1) {
            let matched = aString.substr(prevLength + currLength, tokenLength);
            let range = { start: prevLength + currLength, length: matched.length };
            lineResults.add(matched, range, true);
          }

          // Continue with the next sub-region in this line's text.
          return aPrev + aToken + aCurr;
        }, "");

        if (lineResults.matchCount) {
          sourceResults.add(lineResults);
        }
      });

      if (sourceResults.matchCount) {
        globalResults.add(sourceResults);
      }
    }

    // Rebuild the results, then signal if there are any matches.
    if (globalResults.matchCount) {
      this.hidden = false;
      this._currentlyFocusedMatch = -1;
      this._createGlobalResultsUI(globalResults);
      window.emit(EVENTS.GLOBAL_SEARCH_MATCH_FOUND);
    } else {
      window.emit(EVENTS.GLOBAL_SEARCH_MATCH_NOT_FOUND);
    }
  },

  /**
   * Creates global search results entries and adds them to this container.
   *
   * @param GlobalResults aGlobalResults
   *        An object containing all source results, grouped by source location.
   */
  _createGlobalResultsUI: function(aGlobalResults) {
    let i = 0;

    for (let sourceResults in aGlobalResults) {
      if (i++ == 0) {
        this._createSourceResultsUI(sourceResults);
      } else {
        // Dispatch subsequent document manipulation operations, to avoid
        // blocking the main thread when a large number of search results
        // is found, thus giving the impression of faster searching.
        Services.tm.currentThread.dispatch({ run:
          this._createSourceResultsUI.bind(this, sourceResults)
        }, 0);
      }
    }
  },

  /**
   * Creates source search results entries and adds them to this container.
   *
   * @param SourceResults aSourceResults
   *        An object containing all the matched lines for a specific source.
   */
  _createSourceResultsUI: function(aSourceResults) {
    // Append a source results item to this container.
    this.push([], {
      index: -1, /* specifies on which position should the item be appended */
      relaxed: true, /* this container should allow dupes & degenerates */
      attachment: {
        sourceResults: aSourceResults
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
   */
  _createItemView: function(aElementNode, aAttachment) {
    aAttachment.sourceResults.createView(aElementNode, {
      onHeaderClick: this._onHeaderClick,
      onLineClick: this._onLineClick,
      onMatchClick: this._onMatchClick
    });
  },

  /**
   * The click listener for a results header.
   */
  _onHeaderClick: function(e) {
    let sourceResultsItem = SourceResults.getItemForElement(e.target);
    sourceResultsItem.instance.toggle(e);
  },

  /**
   * The click listener for a results line.
   */
  _onLineClick: function(e) {
    let lineResultsItem = LineResults.getItemForElement(e.target);
    this._onMatchClick({ target: lineResultsItem.firstMatch });
  },

  /**
   * The click listener for a result match.
   */
  _onMatchClick: function(e) {
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

    let url = sourceResultsItem.instance.url;
    let line = lineResultsItem.instance.line;
    DebuggerView.setEditorLocation(url, line + 1, { noDebug: true });

    let editor = DebuggerView.editor;
    let offset = editor.getCaretOffset();
    let { start, length } = lineResultsItem.lineData.range;
    editor.setSelection(offset + start, offset + start + length);
  },

  /**
   * Scrolls a match into view if not already visible.
   *
   * @param nsIDOMNode aMatch
   *        The match to scroll into view.
   */
  _scrollMatchIntoViewIfNeeded: function(aMatch) {
    // TODO: Accessing private widget properties. Figure out what's the best
    // way to expose such things. Bug 876271.
    let boxObject = this.widget._parent.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    boxObject.ensureElementIsVisible(aMatch);
  },

  /**
   * Starts a bounce animation for a match.
   *
   * @param nsIDOMNode aMatch
   *        The match to start a bounce animation for.
   */
  _bounceMatch: function(aMatch) {
    Services.tm.currentThread.dispatch({ run: () => {
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
  _forceExpandResults: false
});

/**
 * An object containing all source results, grouped by source location.
 * Iterable via "for (let [location, sourceResults] in globalResults) { }".
 */
function GlobalResults() {
  this._store = [];
  SourceResults._itemsByElement = new Map();
  LineResults._itemsByElement = new Map();
}

GlobalResults.prototype = {
  /**
   * Adds source results to this store.
   *
   * @param SourceResults aSourceResults
   *        An object containing search results for a specific source.
   */
  add: function(aSourceResults) {
    this._store.push(aSourceResults);
  },

  /**
   * Gets the number of source results in this store.
   */
  get matchCount() this._store.length
};

/**
 * An object containing all the matched lines for a specific source.
 * Iterable via "for (let [lineNumber, lineResults] in sourceResults) { }".
 *
 * @param string aUrl
 *        The target source url.
 * @param GlobalResults aGlobalResults
 *        An object containing all source results, grouped by source location.
 */
function SourceResults(aUrl, aGlobalResults) {
  this.url = aUrl;
  this._globalResults = aGlobalResults;
  this._store = [];
}

SourceResults.prototype = {
  /**
   * Adds line results to this store.
   *
   * @param LineResults aLineResults
   *        An object containing search results for a specific line.
   */
  add: function(aLineResults) {
    this._store.push(aLineResults);
  },

  /**
   * Gets the number of line results in this store.
   */
  get matchCount() this._store.length,

  /**
   * Expands the element, showing all the added details.
   */
  expand: function() {
    this._resultsContainer.removeAttribute("hidden");
    this._arrow.setAttribute("open", "");
  },

  /**
   * Collapses the element, hiding all the added details.
   */
  collapse: function() {
    this._resultsContainer.setAttribute("hidden", "true");
    this._arrow.removeAttribute("open");
  },

  /**
   * Toggles between the element collapse/expand state.
   */
  toggle: function(e) {
    this.expanded ^= 1;
  },

  /**
   * Gets this element's expanded state.
   * @return boolean
   */
  get expanded()
    this._resultsContainer.getAttribute("hidden") != "true" &&
    this._arrow.hasAttribute("open"),

  /**
   * Sets this element's expanded state.
   * @param boolean aFlag
   */
  set expanded(aFlag) this[aFlag ? "expand" : "collapse"](),

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
   * @param object aCallbacks
   *        An object containing all the necessary callback functions:
   *          - onHeaderClick
   *          - onMatchClick
   */
  createView: function(aElementNode, aCallbacks) {
    this._target = aElementNode;

    let arrow = this._arrow = document.createElement("box");
    arrow.className = "arrow";

    let locationNode = document.createElement("label");
    locationNode.className = "plain dbg-results-header-location";
    locationNode.setAttribute("value", this.url);

    let matchCountNode = document.createElement("label");
    matchCountNode.className = "plain dbg-results-header-match-count";
    matchCountNode.setAttribute("value", "(" + this.matchCount + ")");

    let resultsHeader = this._resultsHeader = document.createElement("hbox");
    resultsHeader.className = "dbg-results-header";
    resultsHeader.setAttribute("align", "center")
    resultsHeader.appendChild(arrow);
    resultsHeader.appendChild(locationNode);
    resultsHeader.appendChild(matchCountNode);
    resultsHeader.addEventListener("click", aCallbacks.onHeaderClick, false);

    let resultsContainer = this._resultsContainer = document.createElement("vbox");
    resultsContainer.className = "dbg-results-container";
    resultsContainer.setAttribute("hidden", "true");

    // Create lines search results entries and add them to this container.
    // Afterwards, if the number of matches is reasonable, expand this
    // container automatically.
    for (let lineResults of this._store) {
      lineResults.createView(resultsContainer, aCallbacks);
    }
    if (this.matchCount < GLOBAL_SEARCH_EXPAND_MAX_RESULTS) {
      this.expand();
    }

    let resultsBox = document.createElement("vbox");
    resultsBox.setAttribute("flex", "1");
    resultsBox.appendChild(resultsHeader);
    resultsBox.appendChild(resultsContainer);

    aElementNode.id = "source-results-" + this.url;
    aElementNode.className = "dbg-source-results";
    aElementNode.appendChild(resultsBox);

    SourceResults._itemsByElement.set(aElementNode, { instance: this });
  },

  url: "",
  _globalResults: null,
  _store: null,
  _target: null,
  _arrow: null,
  _resultsHeader: null,
  _resultsContainer: null
};

/**
 * An object containing all the matches for a specific line.
 * Iterable via "for (let chunk in lineResults) { }".
 *
 * @param number aLine
 *        The target line in the source.
 * @param SourceResults aSourceResults
 *        An object containing all the matched lines for a specific source.
 */
function LineResults(aLine, aSourceResults) {
  this.line = aLine;
  this._sourceResults = aSourceResults;
  this._store = [];
  this._matchCount = 0;
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
  add: function(aString, aRange, aMatchFlag) {
    this._store.push({ string: aString, range: aRange, match: !!aMatchFlag });
    this._matchCount += aMatchFlag ? 1 : 0;
  },

  /**
   * Gets the number of word results in this store.
   */
  get matchCount() this._matchCount,

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
   * @param object aCallbacks
   *        An object containing all the necessary callback functions:
   *          - onMatchClick
   *          - onLineClick
   */
  createView: function(aElementNode, aCallbacks) {
    this._target = aElementNode;

    let lineNumberNode = document.createElement("label");
    lineNumberNode.className = "plain dbg-results-line-number";
    lineNumberNode.classList.add("devtools-monospace");
    lineNumberNode.setAttribute("value", this.line + 1);

    let lineContentsNode = document.createElement("hbox");
    lineContentsNode.className = "light list-widget-item dbg-results-line-contents";
    lineContentsNode.classList.add("devtools-monospace");
    lineContentsNode.setAttribute("flex", "1");

    let lineString = "";
    let lineLength = 0;
    let firstMatch = null;

    for (let lineChunk of this._store) {
      let { string, range, match } = lineChunk;
      lineString = string.substr(0, GLOBAL_SEARCH_LINE_MAX_LENGTH - lineLength);
      lineLength += string.length;

      let lineChunkNode = document.createElement("label");
      lineChunkNode.className = "plain dbg-results-line-contents-string";
      lineChunkNode.setAttribute("value", lineString);
      lineChunkNode.setAttribute("match", match);
      lineContentsNode.appendChild(lineChunkNode);

      if (match) {
        this._entangleMatch(lineChunkNode, lineChunk);
        lineChunkNode.addEventListener("click", aCallbacks.onMatchClick, false);
        firstMatch = firstMatch || lineChunkNode;
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

    aElementNode.appendChild(searchResult);
  },

  /**
   * Handles a match while creating the view.
   * @param nsIDOMNode aNode
   * @param object aMatchChunk
   */
  _entangleMatch: function(aNode, aMatchChunk) {
    LineResults._itemsByElement.set(aNode, {
      instance: this,
      lineData: aMatchChunk
    });
  },

  /**
   * Handles a line while creating the view.
   * @param nsIDOMNode aNode
   * @param nsIDOMNode aFirstMatch
   */
  _entangleLine: function(aNode, aFirstMatch) {
    LineResults._itemsByElement.set(aNode, {
      instance: this,
      firstMatch: aFirstMatch,
      ignored: true
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

  line: 0,
  _sourceResults: null,
  _store: null,
  _target: null
};

/**
 * A generator-iterator over the global, source or line results.
 */
GlobalResults.prototype.__iterator__ =
SourceResults.prototype.__iterator__ =
LineResults.prototype.__iterator__ = function() {
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
LineResults.getItemForElement = function(aElement) {
  return WidgetMethods.getItemForElement.call(this, aElement, { noSiblings: true });
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
LineResults.getElementAtIndex = function(aIndex) {
  for (let [element, item] of this._itemsByElement) {
    if (!item.ignored && !aIndex--) {
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
LineResults.indexOfElement = function(aElement) {
  let count = 0;
  for (let [element, item] of this._itemsByElement) {
    if (element == aElement) {
      return count;
    }
    if (!item.ignored) {
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
LineResults.size = function() {
  let count = 0;
  for (let [, item] of this._itemsByElement) {
    if (!item.ignored) {
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
DebuggerView.EventListeners = new EventListenersView();
DebuggerView.GlobalSearch = new GlobalSearchView();

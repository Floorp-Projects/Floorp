/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the stackframes UI.
 */
function StackFramesView() {
  dumpn("StackFramesView was instantiated");
  MenuContainer.call(this);
  this._onClick = this._onClick.bind(this);
  this._onScroll = this._onScroll.bind(this);
}

create({ constructor: StackFramesView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVSF_initialize() {
    dumpn("Initializing the StackFramesView");
    this._container = new StackList(document.getElementById("stackframes"));
    this._container.emptyText = L10N.getStr("emptyStackText");

    this._container.addEventListener("click", this._onClick, false);
    this._container.addEventListener("scroll", this._onScroll, true);
    window.addEventListener("resize", this._onScroll, true);

    this._cache = new Map();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVSF_destroy() {
    dumpn("Destroying the StackFramesView");
    this._container.removeEventListener("click", this._onClick, false);
    this._container.removeEventListener("scroll", this._onScroll, true);
    window.removeEventListener("resize", this._onScroll, true);
  },

  /**
   * Adds a frame in this stackframes container.
   *
   * @param string aFrameName
   *        Name to be displayed in the list.
   * @param string aFrameDetails
   *        Details to be displayed in the list.
   * @param number aDepth
   *        The frame depth specified by the debugger.
   */
  addFrame: function DVSF_addFrame(aFrameName, aFrameDetails, aDepth) {
    // Stackframes are UI elements which benefit from visible panes.
    DebuggerView.showPanesSoon();

    // Append a stackframe item to this container.
    let stackframeItem = this.push(aFrameName, aFrameDetails, {
      forced: true,
      unsorted: true,
      relaxed: true,
      attachment: {
        depth: aDepth
      }
    });

    // Check if stackframe was already appended.
    if (!stackframeItem) {
      return;
    }

    let element = stackframeItem.target;
    element.id = "stackframe-" + aDepth;
    element.className = "dbg-stackframe list-item";
    element.labelNode.className = "dbg-stackframe-name plain";
    element.valueNode.className = "dbg-stackframe-details plain";

    this._cache.set(aDepth, stackframeItem);
  },

  /**
   * Highlights a frame in this stackframes container.
   *
   * @param number aDepth
   *        The frame depth specified by the debugger controller.
   */
  highlightFrame: function DVSF_highlightFrame(aDepth) {
    this._container.selectedItem = this._cache.get(aDepth).target;
  },

  /**
   * Specifies if the active thread has more frames that need to be loaded.
   */
  dirty: false,

  /**
   * The click listener for the stackframes container.
   */
  _onClick: function DVSF__onClick(e) {
    let item = this.getItemForElement(e.target);
    if (item) {
      // The container is not empty and we clicked on an actual item.
      DebuggerController.StackFrames.selectFrame(item.attachment.depth);
    }
  },

  /**
   * The scroll listener for the stackframes container.
   */
  _onScroll: function DVSF__onScroll() {
    // Update the stackframes container only if we have to.
    if (this.dirty) {
      let list = this._container._list;

      // If the stackframes container was scrolled past 95% of the height,
      // load more content.
      if (list.scrollTop >= (list.scrollHeight - list.clientHeight) * 0.95) {
        DebuggerController.StackFrames.addMoreFrames();
        this.dirty = false;
      }
    }
  },

  _cache: null
});

/**
 * Utility functions for handling stackframes.
 */
let StackFrameUtils = {
  /**
   * Create a textual representation for the specified stack frame
   * to display in the stack frame container.
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
  }
};

/**
 * Functions handling the breakpoints UI.
 */
function BreakpointsView() {
  dumpn("BreakpointsView was instantiated");
  MenuContainer.call(this);
  this._createItemView = this._createItemView.bind(this);
  this._onBreakpointRemoved = this._onBreakpointRemoved.bind(this);
  this._onEditorLoad = this._onEditorLoad.bind(this);
  this._onEditorUnload = this._onEditorUnload.bind(this);
  this._onEditorSelection = this._onEditorSelection.bind(this);
  this._onEditorContextMenu = this._onEditorContextMenu.bind(this);
  this._onEditorContextMenuPopupHidden = this._onEditorContextMenuPopupHidden.bind(this);
  this._onBreakpointClick = this._onBreakpointClick.bind(this);
  this._onCheckboxClick = this._onCheckboxClick.bind(this);
  this._onConditionalPopupShowing = this._onConditionalPopupShowing.bind(this);
  this._onConditionalPopupShown = this._onConditionalPopupShown.bind(this);
  this._onConditionalPopupHiding = this._onConditionalPopupHiding.bind(this);
  this._onConditionalTextboxKeyPress = this._onConditionalTextboxKeyPress.bind(this);
}

create({ constructor: BreakpointsView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVB_initialize() {
    dumpn("Initializing the BreakpointsView");
    this._container = new StackList(document.getElementById("breakpoints"));
    this._commandset = document.getElementById("debuggerCommands");
    this._popupset = document.getElementById("debuggerPopupset");
    this._cmPopup = document.getElementById("sourceEditorContextMenu");
    this._cbPanel = document.getElementById("conditional-breakpoint-panel");
    this._cbTextbox = document.getElementById("conditional-breakpoint-textbox");

    this._container.emptyText = L10N.getStr("emptyBreakpointsText");
    this._container.itemFactory = this._createItemView;
    this._container.uniquenessQualifier = 2;

    window.addEventListener("Debugger:EditorLoaded", this._onEditorLoad, false);
    window.addEventListener("Debugger:EditorUnloaded", this._onEditorUnload, false);
    this._container.addEventListener("click", this._onBreakpointClick, false);
    this._cmPopup.addEventListener("popuphidden", this._onEditorContextMenuPopupHidden, false);
    this._cbPanel.addEventListener("popupshowing", this._onConditionalPopupShowing, false);
    this._cbPanel.addEventListener("popupshown", this._onConditionalPopupShown, false);
    this._cbPanel.addEventListener("popuphiding", this._onConditionalPopupHiding, false);
    this._cbTextbox.addEventListener("keypress", this._onConditionalTextboxKeyPress, false);

    this._cache = new Map();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVB_destroy() {
    dumpn("Destroying the BreakpointsView");
    window.removeEventListener("Debugger:EditorLoaded", this._onEditorLoad, false);
    window.removeEventListener("Debugger:EditorUnloaded", this._onEditorUnload, false);
    this._container.removeEventListener("click", this._onBreakpointClick, false);
    this._cmPopup.removeEventListener("popuphidden", this._onEditorContextMenuPopupHidden, false);
    this._cbPanel.removeEventListener("popupshowing", this._onConditionalPopupShowing, false);
    this._cbPanel.removeEventListener("popupshown", this._onConditionalPopupShown, false);
    this._cbPanel.removeEventListener("popuphiding", this._onConditionalPopupHiding, false);
    this._cbTextbox.removeEventListener("keypress", this._onConditionalTextboxKeyPress, false);

    this._cbPanel.hidePopup();
  },

  /**
   * Adds a breakpoint in this breakpoints container.
   *
   * @param string aSourceLocation
   *        The breakpoint source location specified by the debugger controller.
   * @param number aLineNumber
   *        The breakpoint line number specified by the debugger controller.
   * @param string aActor
   *        A breakpoint identifier specified by the debugger controller.
   * @param string aLineInfo
   *        Line information (parent source etc.) to be displayed in the list.
   * @param string aLineText
   *        Line text to be displayed in the list.
   * @param boolean aConditionalFlag [optional]
   *        A flag specifying if this is a conditional breakpoint.
   * @param boolean aOpenPopupFlag [optional]
   *        A flag specifying if the expression popup should be shown.
   */
  addBreakpoint: function DVB_addBreakpoint(aSourceLocation, aLineNumber,
                                            aActor, aLineInfo, aLineText,
                                            aConditionalFlag, aOpenPopupFlag) {
    // Append a breakpoint item to this container.
    let breakpointItem = this.push(aLineInfo.trim(), aLineText.trim(), {
      forced: true,
      attachment: {
        enabled: true,
        sourceLocation: aSourceLocation,
        lineNumber: aLineNumber,
        isConditional: aConditionalFlag
      }
    });

    // Check if breakpoint was already appended.
    if (!breakpointItem) {
      this.enableBreakpoint(aSourceLocation, aLineNumber, { id: aActor });
      return;
    }

    let element = breakpointItem.target;
    element.id = "breakpoint-" + aActor;
    element.className = "dbg-breakpoint list-item";
    element.infoNode.className = "dbg-breakpoint-info plain";
    element.textNode.className = "dbg-breakpoint-text plain";
    element.setAttribute("contextmenu", this._createContextMenu(element));

    breakpointItem.finalize = this._onBreakpointRemoved;
    this._cache.set(this._key(aSourceLocation, aLineNumber), breakpointItem);

    // If this is a conditional breakpoint, display the panes and a panel
    // to input the corresponding conditional expression.
    if (aConditionalFlag && aOpenPopupFlag) {
      this.highlightBreakpoint(aSourceLocation, aLineNumber, { openPopup: true });
    }
  },

  /**
   * Removes a breakpoint from this breakpoints container.
   *
   * @param string aSourceLocation
   *        The breakpoint source location.
   * @param number aLineNumber
   *        The breakpoint line number.
   */
  removeBreakpoint: function DVB_removeBreakpoint(aSourceLocation, aLineNumber) {
    let breakpointItem = this.getBreakpoint(aSourceLocation, aLineNumber);
    if (breakpointItem) {
      this.remove(breakpointItem);
      this._cache.delete(this._key(aSourceLocation, aLineNumber));
    }
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
   *          - callback: function to invoke once the breakpoint is disabled
   *          - id: a new id to be applied to the corresponding element node
   * @return boolean
   *         True if breakpoint existed and was enabled, false otherwise.
   */
  enableBreakpoint:
  function DVB_enableBreakpoint(aSourceLocation, aLineNumber, aOptions = {}) {
    let breakpointItem = this.getBreakpoint(aSourceLocation, aLineNumber);
    if (breakpointItem) {
      // Set a new id to the corresponding breakpoint element if required.
      if (aOptions.id) {
        breakpointItem.target.id = "breakpoint-" + aOptions.id;
      }

      // Update the checkbox state if necessary.
      if (!aOptions.silent) {
        breakpointItem.target.checkbox.setAttribute("checked", "true");
      }

      let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;
      let breakpointLocation = { url: url, line: line };
      DebuggerController.Breakpoints.addBreakpoint(breakpointLocation, aOptions.callback, {
        noPaneUpdate: true
      });

      // Breakpoint is now enabled.
      breakpointItem.attachment.enabled = true;
      return true;
    }
    return false;
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
  function DVB_disableBreakpoint(aSourceLocation, aLineNumber, aOptions = {}) {
    let breakpointItem = this.getBreakpoint(aSourceLocation, aLineNumber);
    if (breakpointItem) {
      // Update the checkbox state if necessary.
      if (!aOptions.silent) {
        breakpointItem.target.checkbox.removeAttribute("checked");
      }

      let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;
      let breakpointClient = DebuggerController.Breakpoints.getBreakpoint(url, line);
      DebuggerController.Breakpoints.removeBreakpoint(breakpointClient, aOptions.callback, {
        noPaneUpdate: true
      });

      // Breakpoint is now disabled.
      breakpointItem.attachment.enabled = false;
      return true;
    }
    return false;
  },

  /**
   * Highlights a breakpoint in this breakpoints container.
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
  function DVB_highlightBreakpoint(aSourceLocation, aLineNumber, aFlags = {}) {
    let breakpointItem = this.getBreakpoint(aSourceLocation, aLineNumber);
    if (breakpointItem) {
      // Update the editor source location and line number if necessary.
      if (aFlags.updateEditor) {
        DebuggerView.updateEditor(aSourceLocation, aLineNumber, { noDebug: true });
      }

      // If the breakpoint requires a new conditional expression, display
      // the panes and the panel to input the corresponding expression.
      if (aFlags.openPopup && breakpointItem.attachment.isConditional) {
        let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;
        let breakpointClient = DebuggerController.Breakpoints.getBreakpoint(url, line);

        // The conditional expression popup can only be shown with visible panes.
        DebuggerView.showPanesSoon(function() {
          // Verify if the breakpoint wasn't removed before the panes were shown.
          if (this.getBreakpoint(aSourceLocation, aLineNumber)) {
            this._cbTextbox.value = breakpointClient.conditionalExpression || "";
            this._cbPanel.openPopup(breakpointItem.target,
              BREAKPOINT_CONDITIONAL_POPUP_POSITION,
              BREAKPOINT_CONDITIONAL_POPUP_OFFSET);
          }
        }.bind(this));
      } else {
        this._cbPanel.hidePopup();
      }

      // Breakpoint is now highlighted.
      this._container.selectedItem = breakpointItem.target;
    }
    // Can't find a breakpoint at the requested source location and line number.
    else {
      this._container.selectedIndex = -1;
      this._cbPanel.hidePopup();
    }
  },

  /**
   * Unhighlights the current breakpoint in this breakpoints container.
   */
  unhighlightBreakpoint: function DVB_highlightBreakpoint() {
    this.highlightBreakpoint(null);
  },

  /**
   * Checks whether a breakpoint with the specified source location and
   * line number exists in this container, and returns the corresponding item
   * if true, null otherwise.
   *
   * @param string aSourceLocation
   *        The breakpoint source location.
   * @param number aLineNumber
   *        The breakpoint line number.
   * @return object
   *         The corresponding item.
   */
  getBreakpoint: function DVB_getBreakpoint(aSourceLocation, aLineNumber) {
    return this._cache.get(this._key(aSourceLocation, aLineNumber));
  },

  /**
   * Gets the currently selected breakpoint client.
   * @return object
   */
  get selectedClient() {
    let selectedItem = this.selectedItem;
    if (selectedItem) {
      let { sourceLocation: url, lineNumber: line } = selectedItem.attachment;
      return DebuggerController.Breakpoints.getBreakpoint(url, line);
    }
    return null;
  },

  /**
   * Customization function for creating an item's UI.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed item.
   * @param string aInfo
   *        The breakpoint's line info.
   * @param string aText
   *        The breakpoint's line text.
   * @param any aAttachment [optional]
   *        Some attached primitive/object.
   */
  _createItemView: function DVB__createItemView(aElementNode, aInfo, aText, aAttachment) {
    let checkbox = document.createElement("checkbox");
    checkbox.setAttribute("checked", "true");
    checkbox.addEventListener("click", this._onCheckboxClick, false);

    let lineInfo = document.createElement("label");
    lineInfo.setAttribute("value", aInfo);
    lineInfo.setAttribute("crop", "end");

    let lineText = document.createElement("label");
    lineText.setAttribute("value", aText);
    lineText.setAttribute("crop", "end");
    lineText.setAttribute("tooltiptext",
      aText.substr(0, BREAKPOINT_LINE_TOOLTIP_MAX_LENGTH));

    let state = document.createElement("vbox");
    state.className = "state";
    state.setAttribute("pack", "center");
    state.appendChild(checkbox);

    let content = document.createElement("vbox");
    content.className = "content";
    content.setAttribute("flex", "1");
    content.appendChild(lineInfo);
    content.appendChild(lineText);

    aElementNode.appendChild(state);
    aElementNode.appendChild(content);

    aElementNode.checkbox = checkbox;
    aElementNode.infoNode = lineInfo;
    aElementNode.textNode = lineText;
  },

  /**
   * Creates a context menu for a breakpoint element.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed breakpoint item.
   * @return string
   *         The newly created menupopup id.
   */
  _createContextMenu: function DVB__createContextMenu(aElementNode) {
    let breakpointId = aElementNode.id;
    let commandsetId = "breakpointCommandset-" + breakpointId;
    let menupopupId = "breakpointMenupopup-" + breakpointId;

    let commandset = document.createElement("commandset");
    let menupopup = document.createElement("menupopup");
    commandset.setAttribute("id", commandsetId);
    menupopup.setAttribute("id", menupopupId);

    createMenuItem.call(this, "deleteAll");
    createMenuSeparator();
    createMenuItem.call(this, "enableAll");
    createMenuItem.call(this, "disableAll");
    createMenuSeparator();
    createMenuItem.call(this, "enableOthers");
    createMenuItem.call(this, "disableOthers");
    createMenuItem.call(this, "deleteOthers");
    createMenuSeparator();
    createMenuItem.call(this, "setConditional");
    createMenuSeparator();
    createMenuItem.call(this, "enableSelf", true);
    createMenuItem.call(this, "disableSelf");
    createMenuItem.call(this, "deleteSelf");

    this._popupset.appendChild(menupopup);
    this._commandset.appendChild(commandset);

    aElementNode.commandset = commandset;
    aElementNode.menupopup = menupopup;
    return menupopupId;

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

      let prefix = "bp-cMenu-";
      let commandId = prefix + aName + "-" + breakpointId + "-command";
      let menuitemId = prefix + aName + "-" + breakpointId + "-menuitem";

      let label = L10N.getStr("breakpointMenuItem." + aName);
      let func = this["_on" + aName.charAt(0).toUpperCase() + aName.slice(1)];

      command.setAttribute("id", commandId);
      command.setAttribute("label", label);
      command.addEventListener("command", func.bind(this, aElementNode), false);

      menuitem.setAttribute("id", menuitemId);
      menuitem.setAttribute("command", commandId);
      menuitem.setAttribute("hidden", aHiddenFlag);

      commandset.appendChild(command);
      menupopup.appendChild(menuitem);
      aElementNode[aName] = { menuitem: menuitem, command: command };
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
   * Destroys a context menu for a breakpoint.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed breakpoint item.
   */
  _destroyContextMenu: function DVB__destroyContextMenu(aElementNode) {
    let commandset = aElementNode.commandset;
    let menupopup = aElementNode.menupopup;

    commandset.parentNode.removeChild(commandset);
    menupopup.parentNode.removeChild(menupopup);
  },

  /**
   * Function called each time a breakpoint item is removed.
   */
  _onBreakpointRemoved: function DVB__onBreakpointRemoved(aItem) {
    this._destroyContextMenu(aItem.target);
  },

  /**
   * The load listener for the source editor.
   */
  _onEditorLoad: function DVB__onEditorLoad({ detail: editor }) {
    editor.addEventListener("Selection", this._onEditorSelection, false);
    editor.addEventListener("ContextMenu", this._onEditorContextMenu, false);
  },

  /**
   * The unload listener for the source editor.
   */
  _onEditorUnload: function DVB__onEditorUnload({ detail: editor }) {
    editor.removeEventListener("Selection", this._onEditorSelection, false);
    editor.removeEventListener("ContextMenu", this._onEditorContextMenu, false);
  },

  /**
   * The selection listener for the source editor.
   */
  _onEditorSelection: function DVB__onEditorSelection(e) {
    let { start, end } = e.newValue;

    let sourceLocation = DebuggerView.Sources.selectedValue;
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
  _onEditorContextMenu: function DVB__onEditorContextMenu({ x, y }) {
    let offset = DebuggerView.editor.getOffsetAtLocation(x, y);
    let line = DebuggerView.editor.getLineAtOffset(offset);
    this._editorContextMenuLineNumber = line;
  },

  /**
   * The context menu popup hidden listener for the source editor.
   */
  _onEditorContextMenuPopupHidden: function DVB__onEditorContextMenuPopupHidden() {
    this._editorContextMenuLineNumber = -1;
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
      let breakpointClient = DebuggerController.Breakpoints.getBreakpoint(url, line)
      DebuggerController.Breakpoints.removeBreakpoint(breakpointClient);
      DebuggerView.Breakpoints.unhighlightBreakpoint();
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
      breakpointItem.attachment.isConditional = true;
      this.selectedClient.conditionalExpression = "";
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
   * The popup showing listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupShowing: function DVB__onConditionalPopupShowing() {
    this._popupShown = true;
  },

  /**
   * The popup shown listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupShown: function DVB__onConditionalPopupShown() {
    this._cbTextbox.focus();
    this._cbTextbox.select();
  },

  /**
   * The popup hiding listener for the breakpoints conditional expression panel.
   */
  _onConditionalPopupHiding: function DVB__onConditionalPopupHiding() {
    this._popupShown = false;
    this.selectedClient.conditionalExpression = this._cbTextbox.value;
  },

  /**
   * The keypress listener for the breakpoints conditional expression textbox.
   */
  _onConditionalTextboxKeyPress: function DVB__onConditionalTextboxKeyPress(e) {
    if (e.keyCode == e.DOM_VK_RETURN || e.keyCode == e.DOM_VK_ENTER) {
      this._cbPanel.hidePopup();
    }
  },

  /**
   * The click listener for the breakpoints container.
   */
  _onBreakpointClick: function DVB__onBreakpointClick(e) {
    let breakpointItem = this.getItemForElement(e.target);
    if (!breakpointItem) {
      // The container is empty or we didn't click on an actual item.
      return;
    }
    let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;
    this.highlightBreakpoint(url, line, { updateEditor: true, openPopup: e.button == 0 });
  },

  /**
   * The click listener for a breakpoint checkbox.
   */
  _onCheckboxClick: function DVB__onCheckboxClick(e) {
    let breakpointItem = this.getItemForElement(e.target);
    if (!breakpointItem) {
      // The container is empty or we didn't click on an actual item.
      return;
    }
    let { sourceLocation: url, lineNumber: line, enabled } = breakpointItem.attachment;
    this[enabled ? "disableBreakpoint" : "enableBreakpoint"](url, line, { silent: true });

    // Don't update the editor location.
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * Listener handling the "setConditional" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element node.
   */
  _onSetConditional: function DVB__onSetConditional(aTarget) {
    if (!aTarget) {
      return;
    }
    let breakpointItem = this.getItemForElement(aTarget);
    let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;

    breakpointItem.attachment.isConditional = true;
    this.highlightBreakpoint(url, line, { openPopup: true });
  },

  /**
   * Listener handling the "enableSelf" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element node.
   */
  _onEnableSelf: function DVB__onEnableSelf(aTarget) {
    if (!aTarget) {
      return;
    }
    let breakpointItem = this.getItemForElement(aTarget);
    let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;

    if (this.enableBreakpoint(url, line)) {
      aTarget.enableSelf.menuitem.setAttribute("hidden", "true");
      aTarget.disableSelf.menuitem.removeAttribute("hidden");
    }
  },

  /**
   * Listener handling the "disableSelf" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element node.
   */
  _onDisableSelf: function DVB__onDisableSelf(aTarget) {
    if (!aTarget) {
      return;
    }
    let breakpointItem = this.getItemForElement(aTarget);
    let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;

    if (this.disableBreakpoint(url, line)) {
      aTarget.enableSelf.menuitem.removeAttribute("hidden");
      aTarget.disableSelf.menuitem.setAttribute("hidden", "true");
    }
  },

  /**
   * Listener handling the "deleteSelf" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element node.
   */
  _onDeleteSelf: function DVB__onDeleteSelf(aTarget) {
    if (!aTarget) {
      return;
    }
    let breakpointItem = this.getItemForElement(aTarget);
    let { sourceLocation: url, lineNumber: line } = breakpointItem.attachment;
    let breakpointClient = DebuggerController.Breakpoints.getBreakpoint(url, line);

    this.removeBreakpoint(url, line);
    DebuggerController.Breakpoints.removeBreakpoint(breakpointClient);
  },

  /**
   * Listener handling the "enableOthers" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element node.
   */
  _onEnableOthers: function DVB__onEnableOthers(aTarget) {
    for (let item in this) {
      if (item.target != aTarget) {
        this._onEnableSelf(item.target);
      }
    }
  },

  /**
   * Listener handling the "disableOthers" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element node.
   */
  _onDisableOthers: function DVB__onDisableOthers(aTarget) {
    for (let item in this) {
      if (item.target != aTarget) {
        this._onDisableSelf(item.target);
      }
    }
  },

  /**
   * Listener handling the "deleteOthers" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element node.
   */
  _onDeleteOthers: function DVB__onDeleteOthers(aTarget) {
    for (let item in this) {
      if (item.target != aTarget) {
        this._onDeleteSelf(item.target);
      }
    }
  },

  /**
   * Listener handling the "enableAll" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element node.
   */
  _onEnableAll: function DVB__onEnableAll(aTarget) {
    this._onEnableOthers(aTarget);
    this._onEnableSelf(aTarget);
  },

  /**
   * Listener handling the "disableAll" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element node.
   */
  _onDisableAll: function DVB__onDisableAll(aTarget) {
    this._onDisableOthers(aTarget);
    this._onDisableSelf(aTarget);
  },

  /**
   * Listener handling the "deleteAll" menuitem command.
   *
   * @param object aTarget
   *        The corresponding breakpoint element node.
   */
  _onDeleteAll: function DVB__onDeleteAll(aTarget) {
    this._onDeleteOthers(aTarget);
    this._onDeleteSelf(aTarget);
  },

  /**
   * Gets an identifier for a breakpoint item in the current cache.
   * @return string
   */
  _key: function DVB__key(aSourceLocation, aLineNumber) {
    return aSourceLocation + aLineNumber;
  },

  _popupset: null,
  _commandset: null,
  _cmPopup: null,
  _cbPanel: null,
  _cbTextbox: null,
  _popupShown: false,
  _cache: null,
  _editorContextMenuLineNumber: -1
});

/**
 * Functions handling the watch expressions UI.
 */
function WatchExpressionsView() {
  dumpn("WatchExpressionsView was instantiated");
  MenuContainer.call(this);
  this.switchExpression = this.switchExpression.bind(this);
  this.deleteExpression = this.deleteExpression.bind(this);
  this._createItemView = this._createItemView.bind(this);
  this._onClick = this._onClick.bind(this);
  this._onClose = this._onClose.bind(this);
  this._onBlur = this._onBlur.bind(this);
  this._onKeyPress = this._onKeyPress.bind(this);
  this._onMouseOver = this._onMouseOver.bind(this);
  this._onMouseOut = this._onMouseOut.bind(this);
}

create({ constructor: WatchExpressionsView, proto: MenuContainer.prototype }, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function DVWE_initialize() {
    dumpn("Initializing the WatchExpressionsView");
    this._container = new StackList(document.getElementById("expressions"));
    this._variables = document.getElementById("variables");

    this._container.setAttribute("context", "debuggerWatchExpressionsContextMenu");
    this._container.permaText = L10N.getStr("addWatchExpressionText");
    this._container.itemFactory = this._createItemView;
    this._container.addEventListener("click", this._onClick, false);

    this._cache = [];
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVWE_destroy() {
    dumpn("Destroying the WatchExpressionsView");
    this._container.removeEventListener("click", this._onClick, false);
  },

  /**
   * Adds a watch expression in this container.
   *
   * @param string aExpression [optional]
   *        An optional initial watch expression text.
   */
  addExpression: function DVWE_addExpression(aExpression = "") {
    // Watch expressions are UI elements which benefit from visible panes.
    DebuggerView.showPanesSoon();

    // Append a watch expression item to this container.
    let expressionItem = this.push("", aExpression, {
      forced: { atIndex: 0 },
      unsorted: true,
      relaxed: true,
      attachment: {
        expression: "",
        initialExpression: aExpression,
        id: this._generateId()
      }
    });

    // Check if watch expression was already appended.
    if (!expressionItem) {
      return;
    }

    let element = expressionItem.target;
    element.id = "expression-" + expressionItem.attachment.id;
    element.className = "dbg-expression list-item";
    element.arrowNode.className = "dbg-expression-arrow";
    element.inputNode.className = "dbg-expression-input plain";
    element.closeNode.className = "dbg-expression-delete plain devtools-closebutton";

    // Automatically focus the new watch expression input and
    // scroll the variables view to top.
    element.inputNode.value = aExpression;
    element.inputNode.select();
    element.inputNode.focus();
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
   *
   * @param Variable aVar
   *        The variable representing the watch expression evaluation.
   * @param string aExpression
   *        The new watch expression text.
   */
  switchExpression: function DVWE_switchExpression(aVar, aExpression) {
    let expressionItem =
      [i for (i of this._cache) if (i.attachment.expression == aVar.name)][0];

    // Remove the watch expression if it's going to be a duplicate.
    if (!aExpression || this.getExpressions().indexOf(aExpression) != -1) {
      this.deleteExpression(aVar);
      return;
    }

    // Save the watch expression code string.
    expressionItem.attachment.expression = aExpression;
    expressionItem.target.inputNode.value = aExpression;

    // Synchronize with the controller's watch expressions store.
    DebuggerController.StackFrames.syncWatchExpressions();
  },

  /**
   * Removes the watch expression corresponding to the specified variable item.
   *
   * @param Variable aVar
   *        The variable representing the watch expression evaluation.
   */
  deleteExpression: function DVWE_deleteExpression(aVar) {
    let expressionItem =
      [i for (i of this._cache) if (i.attachment.expression == aVar.name)][0];

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
    return this._cache[aIndex].attachment.expression;
  },

  /**
   * Gets the watch expressions code strings for all items in this container.
   *
   * @return array
   *         The watch expressions code strings.
   */
  getExpressions: function DVWE_getExpressions() {
    return [item.attachment.expression for (item of this._cache)];
  },

  /**
   * Customization function for creating an item's UI.
   *
   * @param nsIDOMNode aElementNode
   *        The element associated with the displayed item.
   * @param string aExpression
   *        The initial watch expression text.
   */
  _createItemView: function DVWE__createItemView(aElementNode, aExpression) {
    let arrowNode = document.createElement("box");
    let inputNode = document.createElement("textbox");
    let closeNode = document.createElement("toolbarbutton");

    inputNode.setAttribute("value", aExpression);
    inputNode.setAttribute("flex", "1");

    closeNode.addEventListener("click", this._onClose, false);
    inputNode.addEventListener("blur", this._onBlur, false);
    inputNode.addEventListener("keypress", this._onKeyPress, false);
    aElementNode.addEventListener("mouseover", this._onMouseOver, false);
    aElementNode.addEventListener("mouseout", this._onMouseOut, false);

    aElementNode.appendChild(arrowNode);
    aElementNode.appendChild(inputNode);
    aElementNode.appendChild(closeNode);
    aElementNode.arrowNode = arrowNode;
    aElementNode.inputNode = inputNode;
    aElementNode.closeNode = closeNode;
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
    this._cache = [];

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

    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * The blur listener for a watch expression's textbox.
   */
  _onBlur: function DVWE__onBlur({ target: textbox }) {
    let expressionItem = this.getItemForElement(textbox);
    let oldExpression = expressionItem.attachment.expression;
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
      // Save the watch expression code string.
      expressionItem.attachment.expression = newExpression;
      // Make sure the close button is hidden when the textbox is unfocused.
      expressionItem.target.closeNode.hidden = true;
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
   * The mouse over listener for a watch expression.
   */
  _onMouseOver: function DVWE__onMouseOver({ target: element }) {
    this.getItemForElement(element).target.closeNode.hidden = false;
  },

  /**
   * The mouse out listener for a watch expression.
   */
  _onMouseOut: function DVWE__onMouseOut({ target: element }) {
    this.getItemForElement(element).target.closeNode.hidden = true;
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
  MenuContainer.call(this);
  this._startSearch = this._startSearch.bind(this);
  this._onFetchSourceFinished = this._onFetchSourceFinished.bind(this);
  this._onFetchSourceTimeout = this._onFetchSourceTimeout.bind(this);
  this._onFetchSourcesFinished = this._onFetchSourcesFinished.bind(this);
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
    this._container = new StackList(document.getElementById("globalsearch"));
    this._splitter = document.getElementById("globalsearch-splitter");

    this._container.emptyText = L10N.getStr("noMatchingStringsText");
    this._container.itemFactory = this._createItemView;
    this._container.addEventListener("scroll", this._onScroll, false);

    this._cache = new Map();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function DVGS_destroy() {
    dumpn("Destroying the GlobalSearchView");
    this._container.removeEventListener("scroll", this._onScroll, false);
  },

  /**
   * Gets the visibility state of the global search container.
   * @return boolean
   */
  get hidden() this._container.getAttribute("hidden") == "true",

  /**
   * Sets the results container hidden or visible. It's hidden by default.
   * @param boolean aFlag
   */
  set hidden(aFlag) {
    this._container.setAttribute("hidden", aFlag);
    this._splitter.setAttribute("hidden", aFlag);
  },

  /**
   * Removes all items from this container and hides it.
   */
  clearView: function DVGS_clearView() {
    this.hidden = true;
    this.empty();
    window.dispatchEvent("Debugger:GlobalSearch:ViewCleared");
  },

  /**
   * Clears all the fetched sources from cache.
   */
  clearCache: function DVGS_clearCache() {
    this._cache = new Map();
    window.dispatchEvent("Debugger:GlobalSearch:CacheCleared");
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
    let delay = Math.max(GLOBAL_SEARCH_ACTION_MAX_DELAY / aQuery.length);

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
    let locations = DebuggerView.Sources.values;
    this._sourcesCount = locations.length;
    this._searchedToken = aQuery;

    this._fetchSources(locations, {
      onFetch: this._onFetchSourceFinished,
      onTimeout: this._onFetchSourceTimeout,
      onFinished: this._onFetchSourcesFinished
    });
  },

  /**
   * Starts fetching all the sources, silently.
   *
   * @param array aLocations
   *        The locations for the sources to fetch.
   * @param object aCallbacks
   *        An object containing the callback functions to invoke:
   *          - onFetch: called after each source is fetched
   *          - onTimeout: called when a source's text takes too long to fetch
   *          - onFinished: called if all the sources were already fetched
   */
  _fetchSources:
  function DVGS__fetchSources(aLocations, { onFetch, onTimeout, onFinished }) {
    // If all the sources were already fetched, then don't do anything.
    if (this._cache.size == aLocations.length) {
      onFinished();
      return;
    }

    // Fetch each new source.
    for (let location of aLocations) {
      if (this._cache.has(location)) {
        continue;
      }
      let sourceItem = DebuggerView.Sources.getItemByValue(location);
      let sourceObject = sourceItem.attachment;
      DebuggerController.SourceScripts.getText(sourceObject, onFetch, onTimeout);
    }
  },

  /**
   * Called when a source has been fetched.
   *
   * @param string aLocation
   *        The location of the source.
   * @param string aContents
   *        The text contents of the source.
   */
  _onFetchSourceFinished: function DVGS__onFetchSourceFinished(aLocation, aContents) {
    // Remember the source in a cache so we don't have to fetch it again.
    this._cache.set(aLocation, aContents);

    // Check if all sources were fetched and stored in the cache.
    if (this._cache.size == this._sourcesCount) {
      this._onFetchSourcesFinished();
    }
  },

  /**
   * Called when a source's text takes too long to fetch.
   */
  _onFetchSourceTimeout: function DVGS__onFetchSourceTimeout() {
    // Remove the source from the load queue.
    this._sourcesCount--;

    // Check if the remaining sources were fetched and stored in the cache.
    if (this._cache.size == this._sourcesCount) {
      this._onFetchSourcesFinished();
    }
  },

  /**
   * Called when all the sources have been fetched.
   */
  _onFetchSourcesFinished: function DVGS__onFetchSourcesFinished() {
    // At least one source needs to be present to perform a global search.
    if (!this._sourcesCount) {
      return;
    }
    // All sources are fetched and stored in the cache, we can start searching.
    this._performGlobalSearch();
    this._sourcesCount = 0;
  },

  /**
   * Finds string matches in all the  sources stored in the cache, and groups
   * them by location and line number.
   */
  _performGlobalSearch: function DVGS__performGlobalSearch() {
    // Get the currently searched token from the filtering input.
    let token = this._searchedToken;

    // Make sure we're actually searching for something.
    if (!token) {
      this.clearView();
      window.dispatchEvent("Debugger:GlobalSearch:TokenEmpty");
      return;
    }

    // Search is not case sensitive, prepare the actual searched token.
    let lowerCaseToken = token.toLowerCase();
    let tokenLength = token.length;

    // Prepare the results map, containing search details for each line.
    let globalResults = new GlobalResults();

    for (let [location, contents] of this._cache) {
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
      window.dispatchEvent("Debugger:GlobalSearch:MatchFound");
    } else {
      window.dispatchEvent("Debugger:GlobalSearch:MatchNotFound");
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
    let sourceResultsItem = this.push(aLocation, aSourceResults.matchCount, {
      forced: true,
      unsorted: true,
      relaxed: true,
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
   * @param string aLocation
   *        The source result's location.
   * @param string aMatchCount
   *        The source result's match count.
   * @param any aAttachment [optional]
   *        Some attached primitive/object.
   */
  _createItemView:
  function DVGS__createItemView(aElementNode, aLocation, aMatchCount, aAttachment) {
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
    let { clientHeight } = this._container._parent;

    if (top - height <= clientHeight || this._forceExpandResults) {
      sourceResultsItem.instance.expand();
    }
  },

  /**
   * Scrolls a match into view.
   *
   * @param nsIDOMNode aMatch
   *        The match to scroll into view.
   */
  _scrollMatchIntoViewIfNeeded:  function DVGS__scrollMatchIntoViewIfNeeded(aMatch) {
    let { top, height } = aMatch.getBoundingClientRect();
    let { clientHeight } = this._container._parent;

    let style = window.getComputedStyle(aMatch);
    let topBorderSize = window.parseInt(style.getPropertyValue("border-top-width"));
    let bottomBorderSize = window.parseInt(style.getPropertyValue("border-bottom-width"));

    let marginY = top - (height + topBorderSize + bottomBorderSize) * 2;
    if (marginY <= 0) {
      this._container._parent.scrollTop += marginY;
    }
    if (marginY + height > clientHeight) {
      this._container._parent.scrollTop += height - (clientHeight - marginY);
    }
  },

  /**
   * Starts a bounce animation for a match.
   *
   * @param nsIDOMNode aMatch
   *        The match to start a bounce animation for.
   */
  _bounceMatch: function DVGS__bounceMatch(aMatch) {
    Services.tm.currentThread.dispatch({ run: function() {
      aMatch.setAttribute("focused", "");

      aMatch.addEventListener("transitionend", function onEvent() {
        aMatch.removeEventListener("transitionend", onEvent);
        aMatch.removeAttribute("focused");
      });
    }}, 0);
  },

  _splitter: null,
  _currentlyFocusedMatch: -1,
  _forceExpandResults: false,
  _searchTimeout: null,
  _searchFunction: null,
  _searchedToken: "",
  _sourcesCount: -1,
  _cache: null
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
   *
   * @param boolean aSkipAnimationFlag
   *        Pass true to not show an opening animation.
   */
  expand: function SR_expand(aSkipAnimationFlag) {
    this._target.resultsContainer.setAttribute("open", "");
    this._target.arrow.setAttribute("open", "");

    if (!aSkipAnimationFlag) {
      this._target.resultsContainer.setAttribute("animated", "");
    }
  },

  /**
   * Collapses the element, hiding all the added details.
   */
  collapse: function SR_collapse() {
    this._target.resultsContainer.removeAttribute("animated");
    this._target.resultsContainer.removeAttribute("open");
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
  get expanded() this._target.resultsContainer.hasAttribute("open"),

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
    locationNode.className = "plain location";
    locationNode.setAttribute("value", SourceUtils.trimUrlLength(aLocation));

    let matchCountNode = document.createElement("label");
    matchCountNode.className = "plain match-count";
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

    lineNumberNode.className = "plain line-number";
    lineNumberNode.setAttribute("value", aLineNumber + 1);
    lineContentsNode.className = "line-contents";
    lineContentsNode.setAttribute("flex", "1");

    for (let chunk of this._store) {
      let { string, range, match } = chunk;
      lineString = string.substr(0, GLOBAL_SEARCH_LINE_MAX_LENGTH - lineLength);
      lineLength += string.length;

      let label = document.createElement("label");
      label.className = "plain string";
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
    label.className = "plain string";
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
};

/**
 * Gets the index of an item associated with the specified element.
 *
 * @return number
 *         The index of the matched item, or -1 if nothing is found.
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
DebuggerView.StackFrames = new StackFramesView();
DebuggerView.Breakpoints = new BreakpointsView();
DebuggerView.WatchExpressions = new WatchExpressionsView();
DebuggerView.GlobalSearch = new GlobalSearchView();

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Maps known URLs to friendly source group names and put them at the
// bottom of source list.
const KNOWN_SOURCE_GROUPS = {
  "Add-on SDK": "resource://gre/modules/commonjs/",
};

KNOWN_SOURCE_GROUPS[L10N.getStr("anonymousSourcesLabel")] = "anonymous";

/**
 * Functions handling the sources UI.
 */
function SourcesView(DebuggerController, DebuggerView) {
  dumpn("SourcesView was instantiated");

  this.Breakpoints = DebuggerController.Breakpoints;
  this.SourceScripts = DebuggerController.SourceScripts;
  this.DebuggerView = DebuggerView;

  this.togglePrettyPrint = this.togglePrettyPrint.bind(this);
  this.toggleBlackBoxing = this.toggleBlackBoxing.bind(this);
  this.toggleBreakpoints = this.toggleBreakpoints.bind(this);

  this._onEditorLoad = this._onEditorLoad.bind(this);
  this._onEditorUnload = this._onEditorUnload.bind(this);
  this._onEditorCursorActivity = this._onEditorCursorActivity.bind(this);
  this._onSourceSelect = this._onSourceSelect.bind(this);
  this._onStopBlackBoxing = this._onStopBlackBoxing.bind(this);
  this._onBreakpointRemoved = this._onBreakpointRemoved.bind(this);
  this._onBreakpointClick = this._onBreakpointClick.bind(this);
  this._onBreakpointCheckboxClick = this._onBreakpointCheckboxClick.bind(this);
  this._onConditionalPopupShowing = this._onConditionalPopupShowing.bind(this);
  this._onConditionalPopupShown = this._onConditionalPopupShown.bind(this);
  this._onConditionalPopupHiding = this._onConditionalPopupHiding.bind(this);
  this._onConditionalTextboxKeyPress = this._onConditionalTextboxKeyPress.bind(this);
  this._onCopyUrlCommand = this._onCopyUrlCommand.bind(this);
  this._onNewTabCommand = this._onNewTabCommand.bind(this);
}

SourcesView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the SourcesView");

    this.widget = new SideMenuWidget(document.getElementById("sources"), {
      contextMenu: document.getElementById("debuggerSourcesContextMenu"),
      showArrows: true
    });

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

    if (Prefs.prettyPrintEnabled) {
      this._prettyPrintButton.removeAttribute("hidden");
    }

    window.on(EVENTS.EDITOR_LOADED, this._onEditorLoad, false);
    window.on(EVENTS.EDITOR_UNLOADED, this._onEditorUnload, false);
    this.widget.addEventListener("select", this._onSourceSelect, false);
    this._stopBlackBoxButton.addEventListener("click", this._onStopBlackBoxing, false);
    this._cbPanel.addEventListener("popupshowing", this._onConditionalPopupShowing, false);
    this._cbPanel.addEventListener("popupshown", this._onConditionalPopupShown, false);
    this._cbPanel.addEventListener("popuphiding", this._onConditionalPopupHiding, false);
    this._cbTextbox.addEventListener("keypress", this._onConditionalTextboxKeyPress, false);
    this._copyUrlMenuItem.addEventListener("command", this._onCopyUrlCommand, false);
    this._newTabMenuItem.addEventListener("command", this._onNewTabCommand, false);

    this.allowFocusOnRightClick = true;
    this.autoFocusOnSelection = false;

    // Sort the contents by the displayed label.
    this.sortContents((aFirst, aSecond) => {
      return +(aFirst.attachment.label.toLowerCase() >
               aSecond.attachment.label.toLowerCase());
    });

    // Sort known source groups towards the end of the list
    this.widget.groupSortPredicate = function(a, b) {
      if ((a in KNOWN_SOURCE_GROUPS) == (b in KNOWN_SOURCE_GROUPS)) {
        return a.localeCompare(b);
      }
      return (a in KNOWN_SOURCE_GROUPS) ? 1 : -1;
    };

    this._addCommands();
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the SourcesView");

    window.off(EVENTS.EDITOR_LOADED, this._onEditorLoad, false);
    window.off(EVENTS.EDITOR_UNLOADED, this._onEditorUnload, false);
    this.widget.removeEventListener("select", this._onSourceSelect, false);
    this._stopBlackBoxButton.removeEventListener("click", this._onStopBlackBoxing, false);
    this._cbPanel.removeEventListener("popupshowing", this._onConditionalPopupShowing, false);
    this._cbPanel.removeEventListener("popupshowing", this._onConditionalPopupShown, false);
    this._cbPanel.removeEventListener("popuphiding", this._onConditionalPopupHiding, false);
    this._cbTextbox.removeEventListener("keypress", this._onConditionalTextboxKeyPress, false);
    this._copyUrlMenuItem.removeEventListener("command", this._onCopyUrlCommand, false);
    this._newTabMenuItem.removeEventListener("command", this._onNewTabCommand, false);
  },

  empty: function() {
    WidgetMethods.empty.call(this);
    this._unnamedSourceIndex = 0;
  },

  /**
   * Add commands that XUL can fire.
   */
  _addCommands: function() {
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

  _parseUrl: function(aSource) {
    let fullUrl = aSource.url;
    let url, unicodeUrl, label, group;

    if(!fullUrl) {
      unicodeUrl = 'SCRIPT' + this._unnamedSourceIndex++;
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

  /**
   * Adds a breakpoint to this sources container.
   *
   * @param object aBreakpointClient
   *               See Breakpoints.prototype._showBreakpoint
   * @param object aOptions [optional]
   *        @see DebuggerController.Breakpoints.addBreakpoint
   */
  addBreakpoint: function(aBreakpointClient, aOptions = {}) {
    let { location, disabled } = aBreakpointClient;

    // Make sure we're not duplicating anything. If a breakpoint at the
    // specified source url and line already exists, just toggle it.
    if (this.getBreakpoint(location)) {
      this[disabled ? "disableBreakpoint" : "enableBreakpoint"](location);
      return;
    }

    // Get the source item to which the breakpoint should be attached.
    let sourceItem = this.getItemByValue(this.getActorForLocation(location));

    // Create the element node and menu popup for the breakpoint item.
    let breakpointArgs = Heritage.extend(aBreakpointClient, aOptions);
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

    // Highlight the newly appended breakpoint child item if
    // necessary.
    if (aOptions.openPopup || !aOptions.noEditorUpdate) {
      this.highlightBreakpoint(location, aOptions);
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
  removeBreakpoint: function(aLocation) {
    // When a parent source item is removed, all the child breakpoint items are
    // also automagically removed.
    let sourceItem = this.getItemByValue(aLocation.actor);
    if (!sourceItem) {
      return;
    }
    let breakpointItem = this.getBreakpoint(aLocation);
    if (!breakpointItem) {
      return;
    }

    // Clear the breakpoint view.
    sourceItem.remove(breakpointItem);

    window.emit(EVENTS.BREAKPOINT_HIDDEN_IN_PANE);
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
      aItem.attachment.actor == aLocation.actor &&
      aItem.attachment.line == aLocation.line);
  },

  /**
   * Returns all breakpoints for all sources.
   *
   * @return array
   *         The breakpoints for all sources if any, an empty array otherwise.
   */
  getAllBreakpoints: function(aStore = []) {
    return this.getOtherBreakpoints(undefined, aStore);
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
    for (let source of this) {
      for (let breakpointItem of source) {
        let { actor, line } = breakpointItem.attachment;
        if (actor != aLocation.actor || line != aLocation.line) {
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
    let identifier = this.Breakpoints.getIdentifier(attachment);
    let enableSelfId = prefix + "enableSelf-" + identifier + "-menuitem";
    let disableSelfId = prefix + "disableSelf-" + identifier + "-menuitem";
    document.getElementById(enableSelfId).setAttribute("hidden", "true");
    document.getElementById(disableSelfId).removeAttribute("hidden");

    // Update the breakpoint toggle button checked state.
    this._toggleBreakpointsButton.removeAttribute("checked");

    // Update the checkbox state if necessary.
    if (!aOptions.silent) {
      attachment.view.checkbox.setAttribute("checked", "true");
    }

    return this.Breakpoints.addBreakpoint(aLocation, {
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
    let identifier = this.Breakpoints.getIdentifier(attachment);
    let enableSelfId = prefix + "enableSelf-" + identifier + "-menuitem";
    let disableSelfId = prefix + "disableSelf-" + identifier + "-menuitem";
    document.getElementById(enableSelfId).removeAttribute("hidden");
    document.getElementById(disableSelfId).setAttribute("hidden", "true");

    // Update the checkbox state if necessary.
    if (!aOptions.silent) {
      attachment.view.checkbox.removeAttribute("checked");
    }

    return this.Breakpoints.removeBreakpoint(aLocation, {
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
      this.DebuggerView.setEditorLocation(aLocation.actor, aLocation.line, { noDebug: true });
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
   * Highlight the breakpoint on the current currently focused line/column
   * if it exists.
   */
  highlightBreakpointAtCursor: function() {
    let actor = this.selectedValue;
    let line = this.DebuggerView.editor.getCursor().line + 1;

    let location = { actor: actor, line: line };
    this.highlightBreakpoint(location, { noEditorUpdate: true });
  },

  /**
   * Unhighlights the current breakpoint in this sources container.
   */
  unhighlightBreakpoint: function() {
    this._hideConditionalPopup();
    this._unselectBreakpoint();
  },

   /**
    * Display the message thrown on breakpoint condition
    */
  showBreakpointConditionThrownMessage: function(aLocation, aMessage = "") {
    let breakpointItem = this.getBreakpoint(aLocation);
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
  updateToolbarButtonsState: function() {
    const { source } = this.selectedItem.attachment;
    const sourceClient = gThreadClient.source(source);

    if (sourceClient.isBlackBoxed) {
      this._prettyPrintButton.setAttribute("disabled", true);
      this._blackBoxButton.setAttribute("checked", true);
    } else {
      this._prettyPrintButton.removeAttribute("disabled");
      this._blackBoxButton.removeAttribute("checked");
    }

    if (sourceClient.isPrettyPrinted) {
      this._prettyPrintButton.setAttribute("checked", true);
    } else {
      this._prettyPrintButton.removeAttribute("checked");
    }
  },

  /**
   * Toggle the pretty printing of the selected source.
   */
  togglePrettyPrint: Task.async(function*() {
    if (this._prettyPrintButton.hasAttribute("disabled")) {
      return;
    }

    const resetEditor = ([{ actor }]) => {
      // Only set the text when the source is still selected.
      if (actor == this.selectedValue) {
        this.DebuggerView.setEditorLocation(actor, 0, { force: true });
      }
    };

    const printError = ([{ url }, error]) => {
      DevToolsUtils.reportException("togglePrettyPrint", error);
    };

    this.DebuggerView.showProgressBar();
    const { source } = this.selectedItem.attachment;
    const sourceClient = gThreadClient.source(source);
    const shouldPrettyPrint = !sourceClient.isPrettyPrinted;

    if (shouldPrettyPrint) {
      this._prettyPrintButton.setAttribute("checked", true);
    } else {
      this._prettyPrintButton.removeAttribute("checked");
    }

    try {
      let resolution = yield this.SourceScripts.togglePrettyPrint(source);
      resetEditor(resolution);
    } catch (rejection) {
      printError(rejection);
    }

    this.DebuggerView.showEditor();
    this.updateToolbarButtonsState();
  }),

  /**
   * Toggle the black boxed state of the selected source.
   */
  toggleBlackBoxing: Task.async(function*() {
    const { source } = this.selectedItem.attachment;
    const sourceClient = gThreadClient.source(source);
    const shouldBlackBox = !sourceClient.isBlackBoxed;

    // Be optimistic that the (un-)black boxing will succeed, so enable/disable
    // the pretty print button and check/uncheck the black box button immediately.
    // Then, once we actually get the results from the server, make sure that
    // it is in the correct state again by calling `updateToolbarButtonsState`.

    if (shouldBlackBox) {
      this._prettyPrintButton.setAttribute("disabled", true);
      this._blackBoxButton.setAttribute("checked", true);
    } else {
      this._prettyPrintButton.removeAttribute("disabled");
      this._blackBoxButton.removeAttribute("checked");
    }

    try {
      yield this.SourceScripts.setBlackBoxing(source, shouldBlackBox);
    } catch (e) {
      // Continue execution in this task even if blackboxing failed.
    }

    this.updateToolbarButtonsState();
  }),

  /**
   * Toggles all breakpoints enabled/disabled.
   */
  toggleBreakpoints: function() {
    let breakpoints = this.getAllBreakpoints();
    let hasBreakpoints = breakpoints.length > 0;
    let hasEnabledBreakpoints = breakpoints.some(e => !e.attachment.disabled);

    if (hasBreakpoints && hasEnabledBreakpoints) {
      this._toggleBreakpointsButton.setAttribute("checked", true);
      this._onDisableAll();
    } else {
      this._toggleBreakpointsButton.removeAttribute("checked");
      this._onEnableAll();
    }
  },

  togglePromiseDebugger: function() {
    if (Prefs.promiseDebuggerEnabled) {
      let promisePane = this.DebuggerView._promisePane;
      promisePane.hidden = !promisePane.hidden;

      if (!this.DebuggerView._promiseDebuggerIframe) {
        this.DebuggerView._initializePromiseDebugger();
      }
    }
  },

  hidePrettyPrinting: function() {
    this._prettyPrintButton.style.display = 'none';

    if (this._blackBoxButton.style.display === 'none') {
      let sep = document.querySelector('#sources-toolbar .devtools-separator');
      sep.style.display = 'none';
    }
  },

  hideBlackBoxing: function() {
    this._blackBoxButton.style.display = 'none';

    if (this._prettyPrintButton.style.display === 'none') {
      let sep = document.querySelector('#sources-toolbar .devtools-separator');
      sep.style.display = 'none';
    }
  },

  /**
   * Look up a source actor id for a location. This is necessary for
   * backwards compatibility; otherwise we could just use the `actor`
   * property. Older servers don't use the same actor ids for sources
   * across reloads, so we resolve a url to the current actor if a url
   * exists.
   *
   * @param object aLocation
   *        An object with the following properties:
   *        - actor: the source actor id
   *        - url: a url (might be null)
   */
  getActorForLocation: function(aLocation) {
    if (aLocation.url) {
      for (var item of this) {
        let source = item.attachment.source;

        if (aLocation.url === source.url) {
          return source.actor;
        }
      }
    }
    return aLocation.actor;
  },

  getDisplayURL: function(source) {
    if(!source.url) {
      return this.getItemByValue(source.actor).attachment.label;
    }
    return NetworkHelper.convertToUnicode(unescape(source.url))
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
    let breakpointPromise = this.Breakpoints._getAdded(attachment);
    if (breakpointPromise) {
      breakpointPromise.then(aBreakpointClient => {
        let isConditionalBreakpoint = aBreakpointClient.hasCondition();
        let condition = aBreakpointClient.getCondition();
        doOpen.call(this, isConditionalBreakpoint ? condition : "")
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
  _createBreakpointView: function(aOptions) {
    let { location, disabled, text, message } = aOptions;
    let identifier = this.Breakpoints.getIdentifier(location);

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
  _createContextMenu: function(aOptions) {
    let { location, disabled } = aOptions;
    let identifier = this.Breakpoints.getIdentifier(location);

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
  _onCopyUrlCommand: function() {
    let selected = this.selectedItem && this.selectedItem.attachment;
    if (!selected) {
      return;
    }
    clipboardHelper.copyString(selected.source.url);
  },

  /**
   * Opens selected item source in a new tab.
   */
  _onNewTabCommand: function() {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    let selected = this.selectedItem.attachment;
    win.openUILinkIn(selected.source.url, "tab", { relatedToCurrent: true });
  },

  /**
   * Function called each time a breakpoint item is removed.
   *
   * @param object aItem
   *        The corresponding item.
   */
  _onBreakpointRemoved: function(aItem) {
    dumpn("Finalizing breakpoint item: " + aItem.stringify());

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
    aEditor.on("cursorActivity", this._onEditorCursorActivity);
  },

  /**
   * The unload listener for the source editor.
   */
  _onEditorUnload: function(aName, aEditor) {
    aEditor.off("cursorActivity", this._onEditorCursorActivity);
  },

  /**
   * The selection listener for the source editor.
   */
  _onEditorCursorActivity: function(e) {
    let editor = this.DebuggerView.editor;
    let start  = editor.getCursor("start").line + 1;
    let end    = editor.getCursor().line + 1;
    let actor    = this.selectedValue;

    let location = { actor: actor, line: start };

    if (this.getBreakpoint(location) && start == end) {
      this.highlightBreakpoint(location, { noEditorUpdate: true });
    } else {
      this.unhighlightBreakpoint();
    }
  },

  /**
   * The select listener for the sources container.
   */
  _onSourceSelect: Task.async(function*({ detail: sourceItem }) {
    if (!sourceItem) {
      return;
    }
    const { source } = sourceItem.attachment;
    const sourceClient = gThreadClient.source(source);

    // The container is not empty and an actual item was selected.
    this.DebuggerView.setEditorLocation(sourceItem.value);

    // Attempt to automatically pretty print minified source code.
    if (Prefs.autoPrettyPrint && !sourceClient.isPrettyPrinted) {
      let isMinified = yield SourceUtils.isMinified(sourceClient);
      if (isMinified) {
        this.togglePrettyPrint();
      }
    }

    // Set window title. No need to split the url by " -> " here, because it was
    // already sanitized when the source was added.
    document.title = L10N.getFormatStr("DebuggerWindowScriptTitle",
                                       sourceItem.attachment.source.url);

    this.DebuggerView.maybeShowBlackBoxMessage();
    this.updateToolbarButtonsState();
  }),

  /**
   * The click listener for the "stop black boxing" button.
   */
  _onStopBlackBoxing: Task.async(function*() {
    const { source } = this.selectedItem.attachment;

    try {
      yield this.SourceScripts.setBlackBoxing(source, false);
    } catch (e) {
      // Continue execution in this task even if blackboxing failed.
    }

    this.updateToolbarButtonsState();
  }),

  /**
   * The click listener for a breakpoint container.
   */
  _onBreakpointClick: function(e) {
    let sourceItem = this.getItemForElement(e.target);
    let breakpointItem = this.getItemForElement.call(sourceItem, e.target);
    let attachment = breakpointItem.attachment;

    // Check if this is an enabled conditional breakpoint.
    let breakpointPromise = this.Breakpoints._getAdded(attachment);
    if (breakpointPromise) {
      breakpointPromise.then(aBreakpointClient => {
        doHighlight.call(this, aBreakpointClient.hasCondition());
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
  _onConditionalPopupHiding: Task.async(function*() {
    this._conditionalPopupVisible = false; // Used in tests.

    let breakpointItem = this._selectedBreakpointItem;
    let attachment = breakpointItem.attachment;

    // Check if this is an enabled conditional breakpoint, and if so,
    // save the current conditional expression.
    let breakpointPromise = this.Breakpoints._getAdded(attachment);
    if (breakpointPromise) {
      let { location } = yield breakpointPromise;
      let condition = this._cbTextbox.value;
      yield this.Breakpoints.updateCondition(location, condition);
    }

    window.emit(EVENTS.CONDITIONAL_BREAKPOINT_POPUP_HIDING);
  }),

  /**
   * The keypress listener for the breakpoints conditional expression textbox.
   */
  _onConditionalTextboxKeyPress: function(e) {
    if (e.keyCode == e.DOM_VK_RETURN) {
      this._hideConditionalPopup();
    }
  },

  /**
   * Called when the add breakpoint key sequence was pressed.
   */
  _onCmdAddBreakpoint: function(e) {
    let actor = this.selectedValue;
    let line = (e && e.sourceEvent.target.tagName == 'menuitem' ?
                this.DebuggerView.clickedLine + 1 :
                this.DebuggerView.editor.getCursor().line + 1);
    let location = { actor, line };
    let breakpointItem = this.getBreakpoint(location);

    // If a breakpoint already existed, remove it now.
    if (breakpointItem) {
      this.Breakpoints.removeBreakpoint(location);
    }
    // No breakpoint existed at the required location, add one now.
    else {
      this.Breakpoints.addBreakpoint(location);
    }
  },

  /**
   * Called when the add conditional breakpoint key sequence was pressed.
   */
  _onCmdAddConditionalBreakpoint: function(e) {
    let actor = this.selectedValue;
    let line = (e && e.sourceEvent.target.tagName == 'menuitem' ?
                this.DebuggerView.clickedLine + 1 :
                this.DebuggerView.editor.getCursor().line + 1);
    let location = { actor, line };
    let breakpointItem = this.getBreakpoint(location);

    // If a breakpoint already existed or wasn't a conditional, morph it now.
    if (breakpointItem) {
      this.highlightBreakpoint(location, { openPopup: true });
    }
    // No breakpoint existed at the required location, add one now.
    else {
      this.Breakpoints.addBreakpoint(location, { openPopup: true });
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
    this.Breakpoints.removeBreakpoint(aLocation);
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
  _conditionalPopupVisible: false
});

DebuggerView.Sources = new SourcesView(DebuggerController, DebuggerView);

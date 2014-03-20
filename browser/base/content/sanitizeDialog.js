/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

var gSanitizePromptDialog = {

  get bundleBrowser()
  {
    if (!this._bundleBrowser)
      this._bundleBrowser = document.getElementById("bundleBrowser");
    return this._bundleBrowser;
  },

  get selectedTimespan()
  {
    var durList = document.getElementById("sanitizeDurationChoice");
    return parseInt(durList.value);
  },

  get sanitizePreferences()
  {
    if (!this._sanitizePreferences) {
      this._sanitizePreferences =
        document.getElementById("sanitizePreferences");
    }
    return this._sanitizePreferences;
  },

  get warningBox()
  {
    return document.getElementById("sanitizeEverythingWarningBox");
  },

  init: function ()
  {
    // This is used by selectByTimespan() to determine if the window has loaded.
    this._inited = true;

    var s = new Sanitizer();
    s.prefDomain = "privacy.cpd.";

    let sanitizeItemList = document.querySelectorAll("#itemList > [preference]");
    for (let i = 0; i < sanitizeItemList.length; i++) {
      let prefItem = sanitizeItemList[i];
      let name = s.getNameFromPreference(prefItem.getAttribute("preference"));
      s.canClearItem(name, function canClearCallback(aItem, aCanClear, aPrefItem) {
        if (!aCanClear) {
          aPrefItem.preference = null;
          aPrefItem.checked = false;
          aPrefItem.disabled = true;
        }
      }, prefItem);
    }

    document.documentElement.getButton("accept").label =
      this.bundleBrowser.getString("sanitizeButtonOK");

    if (this.selectedTimespan === Sanitizer.TIMESPAN_EVERYTHING) {
      this.prepareWarning();
      this.warningBox.hidden = false;
      document.title =
        this.bundleBrowser.getString("sanitizeDialog2.everything.title");
    }
    else
      this.warningBox.hidden = true;
  },

  selectByTimespan: function ()
  {
    // This method is the onselect handler for the duration dropdown.  As a
    // result it's called a couple of times before onload calls init().
    if (!this._inited)
      return;

    var warningBox = this.warningBox;

    // If clearing everything
    if (this.selectedTimespan === Sanitizer.TIMESPAN_EVERYTHING) {
      this.prepareWarning();
      if (warningBox.hidden) {
        warningBox.hidden = false;
        window.resizeBy(0, warningBox.boxObject.height);
      }
      window.document.title =
        this.bundleBrowser.getString("sanitizeDialog2.everything.title");
      return;
    }

    // If clearing a specific time range
    if (!warningBox.hidden) {
      window.resizeBy(0, -warningBox.boxObject.height);
      warningBox.hidden = true;
    }
    window.document.title =
      window.document.documentElement.getAttribute("noneverythingtitle");
  },

  sanitize: function ()
  {
    // Update pref values before handing off to the sanitizer (bug 453440)
    this.updatePrefs();
    var s = new Sanitizer();
    s.prefDomain = "privacy.cpd.";

    s.range = Sanitizer.getClearRange(this.selectedTimespan);
    s.ignoreTimespan = !s.range;

    // As the sanitize is async, we disable the buttons, update the label on
    // the 'accept' button to indicate things are happening and return false -
    // once the async operation completes (either with or without errors)
    // we close the window.
    let docElt = document.documentElement;
    let acceptButton = docElt.getButton("accept");
    acceptButton.disabled = true;
    acceptButton.setAttribute("label",
                              this.bundleBrowser.getString("sanitizeButtonClearing"));
    docElt.getButton("cancel").disabled = true;
    try {
      s.sanitize().then(null, Components.utils.reportError)
                  .then(() => window.close())
                  .then(null, Components.utils.reportError);
    } catch (er) {
      Components.utils.reportError("Exception during sanitize: " + er);
      return true; // We *do* want to close immediately on error.
    }
    return false;
  },

  /**
   * If the panel that displays a warning when the duration is "Everything" is
   * not set up, sets it up.  Otherwise does nothing.
   *
   * @param aDontShowItemList Whether only the warning message should be updated.
   *                          True means the item list visibility status should not
   *                          be changed.
   */
  prepareWarning: function (aDontShowItemList) {
    // If the date and time-aware locale warning string is ever used again,
    // initialize it here.  Currently we use the no-visits warning string,
    // which does not include date and time.  See bug 480169 comment 48.

    var warningStringID;
    if (this.hasNonSelectedItems()) {
      warningStringID = "sanitizeSelectedWarning";
      if (!aDontShowItemList)
        this.showItemList();
    }
    else {
      warningStringID = "sanitizeEverythingWarning2";
    }

    var warningDesc = document.getElementById("sanitizeEverythingWarning");
    warningDesc.textContent =
      this.bundleBrowser.getString(warningStringID);
  },

  /**
   * Called when the value of a preference element is synced from the actual
   * pref.  Enables or disables the OK button appropriately.
   */
  onReadGeneric: function ()
  {
    var found = false;

    // Find any other pref that's checked and enabled.
    var i = 0;
    while (!found && i < this.sanitizePreferences.childNodes.length) {
      var preference = this.sanitizePreferences.childNodes[i];

      found = !!preference.value &&
              !preference.disabled;
      i++;
    }

    try {
      document.documentElement.getButton("accept").disabled = !found;
    }
    catch (e) { }

    // Update the warning prompt if needed
    this.prepareWarning(true);

    return undefined;
  },

  /**
   * Sanitizer.prototype.sanitize() requires the prefs to be up-to-date.
   * Because the type of this prefwindow is "child" -- and that's needed because
   * without it the dialog has no OK and Cancel buttons -- the prefs are not
   * updated on dialogaccept on platforms that don't support instant-apply
   * (i.e., Windows).  We must therefore manually set the prefs from their
   * corresponding preference elements.
   */
  updatePrefs : function ()
  {
    var tsPref = document.getElementById("privacy.sanitize.timeSpan");
    Sanitizer.prefs.setIntPref("timeSpan", this.selectedTimespan);

    // Keep the pref for the download history in sync with the history pref.
    document.getElementById("privacy.cpd.downloads").value =
      document.getElementById("privacy.cpd.history").value;

    // Now manually set the prefs from their corresponding preference
    // elements.
    var prefs = this.sanitizePreferences.rootBranch;
    for (let i = 0; i < this.sanitizePreferences.childNodes.length; ++i) {
      var p = this.sanitizePreferences.childNodes[i];
      prefs.setBoolPref(p.name, p.value);
    }
  },

  /**
   * Check if all of the history items have been selected like the default status.
   */
  hasNonSelectedItems: function () {
    let checkboxes = document.querySelectorAll("#itemList > [preference]");
    for (let i = 0; i < checkboxes.length; ++i) {
      let pref = document.getElementById(checkboxes[i].getAttribute("preference"));
      if (!pref.value)
        return true;
    }
    return false;
  },

  /**
   * Show the history items list.
   */
  showItemList: function () {
    var itemList = document.getElementById("itemList");
    var expanderButton = document.getElementById("detailsExpander");

    if (itemList.collapsed) {
      expanderButton.className = "expander-up";
      itemList.setAttribute("collapsed", "false");
      if (document.documentElement.boxObject.height)
        window.resizeBy(0, itemList.boxObject.height);
    }
  },

  /**
   * Hide the history items list.
   */
  hideItemList: function () {
    var itemList = document.getElementById("itemList");
    var expanderButton = document.getElementById("detailsExpander");

    if (!itemList.collapsed) {
      expanderButton.className = "expander-down";
      window.resizeBy(0, -itemList.boxObject.height);
      itemList.setAttribute("collapsed", "true");
    }
  },

  /**
   * Called by the item list expander button to toggle the list's visibility.
   */
  toggleItemList: function ()
  {
    var itemList = document.getElementById("itemList");

    if (itemList.collapsed)
      this.showItemList();
    else
      this.hideItemList();
  }

#ifdef CRH_DIALOG_TREE_VIEW
  // A duration value; used in the same context as Sanitizer.TIMESPAN_HOUR,
  // Sanitizer.TIMESPAN_2HOURS, et al.  This should match the value attribute
  // of the sanitizeDurationCustom menuitem.
  get TIMESPAN_CUSTOM()
  {
    return -1;
  },

  get placesTree()
  {
    if (!this._placesTree)
      this._placesTree = document.getElementById("placesTree");
    return this._placesTree;
  },

  init: function ()
  {
    // This is used by selectByTimespan() to determine if the window has loaded.
    this._inited = true;

    var s = new Sanitizer();
    s.prefDomain = "privacy.cpd.";

    let sanitizeItemList = document.querySelectorAll("#itemList > [preference]");
    for (let i = 0; i < sanitizeItemList.length; i++) {
      let prefItem = sanitizeItemList[i];
      let name = s.getNameFromPreference(prefItem.getAttribute("preference"));
      s.canClearItem(name, function canClearCallback(aCanClear) {
        if (!aCanClear) {
          prefItem.preference = null;
          prefItem.checked = false;
          prefItem.disabled = true;
        }
      });
    }

    document.documentElement.getButton("accept").label =
      this.bundleBrowser.getString("sanitizeButtonOK");

    this.selectByTimespan();
  },

  /**
   * Sets up the hashes this.durationValsToRows, which maps duration values
   * to rows in the tree, this.durationRowsToVals, which maps rows in
   * the tree to duration values, and this.durationStartTimes, which maps
   * duration values to their corresponding start times.
   */
  initDurationDropdown: function ()
  {
    // First, calculate the start times for each duration.
    this.durationStartTimes = {};
    var durVals = [];
    var durPopup = document.getElementById("sanitizeDurationPopup");
    var durMenuitems = durPopup.childNodes;
    for (let i = 0; i < durMenuitems.length; i++) {
      let durMenuitem = durMenuitems[i];
      let durVal = parseInt(durMenuitem.value);
      if (durMenuitem.localName === "menuitem" &&
          durVal !== Sanitizer.TIMESPAN_EVERYTHING &&
          durVal !== this.TIMESPAN_CUSTOM) {
        durVals.push(durVal);
        let durTimes = Sanitizer.getClearRange(durVal);
        this.durationStartTimes[durVal] = durTimes[0];
      }
    }

    // Sort the duration values ascending.  Because one tree index can map to
    // more than one duration, this ensures that this.durationRowsToVals maps
    // a row index to the largest duration possible in the code below.
    durVals.sort();

    // Now calculate the rows in the tree of the durations' start times.  For
    // each duration, we are looking for the node in the tree whose time is the
    // smallest time greater than or equal to the duration's start time.
    this.durationRowsToVals = {};
    this.durationValsToRows = {};
    var view = this.placesTree.view;
    // For all rows in the tree except the grippy row...
    for (let i = 0; i < view.rowCount - 1; i++) {
      let unfoundDurVals = [];
      let nodeTime = view.QueryInterface(Ci.nsINavHistoryResultTreeViewer).
                     nodeForTreeIndex(i).time;
      // For all durations whose rows have not yet been found in the tree, see
      // if index i is their index.  An index may map to more than one duration,
      // in which case the final duration (the largest) wins.
      for (let j = 0; j < durVals.length; j++) {
        let durVal = durVals[j];
        let durStartTime = this.durationStartTimes[durVal];
        if (nodeTime < durStartTime) {
          this.durationValsToRows[durVal] = i - 1;
          this.durationRowsToVals[i - 1] = durVal;
        }
        else
          unfoundDurVals.push(durVal);
      }
      durVals = unfoundDurVals;
    }

    // If any durations were not found above, then every node in the tree has a
    // time greater than or equal to the duration.  In other words, those
    // durations include the entire tree (except the grippy row).
    for (let i = 0; i < durVals.length; i++) {
      let durVal = durVals[i];
      this.durationValsToRows[durVal] = view.rowCount - 2;
      this.durationRowsToVals[view.rowCount - 2] = durVal;
    }
  },

  /**
   * If the Places tree is not set up, sets it up.  Otherwise does nothing.
   */
  ensurePlacesTreeIsInited: function ()
  {
    if (this._placesTreeIsInited)
      return;

    this._placesTreeIsInited = true;

    // Either "Last Four Hours" or "Today" will have the most history.  If
    // it's been more than 4 hours since today began, "Today" will. Otherwise
    // "Last Four Hours" will.
    var times = Sanitizer.getClearRange(Sanitizer.TIMESPAN_TODAY);

    // If it's been less than 4 hours since today began, use the past 4 hours.
    if (times[1] - times[0] < 14400000000) { // 4*60*60*1000000
      times = Sanitizer.getClearRange(Sanitizer.TIMESPAN_4HOURS);
    }

    var histServ = Cc["@mozilla.org/browser/nav-history-service;1"].
                   getService(Ci.nsINavHistoryService);
    var query = histServ.getNewQuery();
    query.beginTimeReference = query.TIME_RELATIVE_EPOCH;
    query.beginTime = times[0];
    query.endTimeReference = query.TIME_RELATIVE_EPOCH;
    query.endTime = times[1];
    var opts = histServ.getNewQueryOptions();
    opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
    opts.queryType = opts.QUERY_TYPE_HISTORY;
    var result = histServ.executeQuery(query, opts);

    var view = gContiguousSelectionTreeHelper.setTree(this.placesTree,
                                                      new PlacesTreeView());
    result.addObserver(view, false);
    this.initDurationDropdown();
  },

  /**
   * Called on select of the duration dropdown and when grippyMoved() sets a
   * duration based on the location of the grippy row.  Selects all the nodes in
   * the tree that are contained in the selected duration.  If clearing
   * everything, the warning panel is shown instead.
   */
  selectByTimespan: function ()
  {
    // This method is the onselect handler for the duration dropdown.  As a
    // result it's called a couple of times before onload calls init().
    if (!this._inited)
      return;

    var durDeck = document.getElementById("durationDeck");
    var durList = document.getElementById("sanitizeDurationChoice");
    var durVal = parseInt(durList.value);
    var durCustom = document.getElementById("sanitizeDurationCustom");

    // If grippy row is not at a duration boundary, show the custom menuitem;
    // otherwise, hide it.  Since the user cannot specify a custom duration by
    // using the dropdown, this conditional is true only when this method is
    // called onselect from grippyMoved(), so no selection need be made.
    if (durVal === this.TIMESPAN_CUSTOM) {
      durCustom.hidden = false;
      return;
    }
    durCustom.hidden = true;

    // If clearing everything, show the warning and change the dialog's title.
    if (durVal === Sanitizer.TIMESPAN_EVERYTHING) {
      this.prepareWarning();
      durDeck.selectedIndex = 1;
      window.document.title =
        this.bundleBrowser.getString("sanitizeDialog2.everything.title");
      document.documentElement.getButton("accept").disabled = false;
      return;
    }

    // Otherwise -- if clearing a specific time range -- select that time range
    // in the tree.
    this.ensurePlacesTreeIsInited();
    durDeck.selectedIndex = 0;
    window.document.title =
      window.document.documentElement.getAttribute("noneverythingtitle");
    var durRow = this.durationValsToRows[durVal];
    gContiguousSelectionTreeHelper.rangedSelect(durRow);
    gContiguousSelectionTreeHelper.scrollToGrippy();

    // If duration is empty (there are no selected rows), disable the dialog's
    // OK button.
    document.documentElement.getButton("accept").disabled = durRow < 0;
  },

  sanitize: function ()
  {
    // Update pref values before handing off to the sanitizer (bug 453440)
    this.updatePrefs();
    var s = new Sanitizer();
    s.prefDomain = "privacy.cpd.";

    var durList = document.getElementById("sanitizeDurationChoice");
    var durValue = parseInt(durList.value);
    s.ignoreTimespan = durValue === Sanitizer.TIMESPAN_EVERYTHING;

    // Set the sanitizer's time range if we're not clearing everything.
    if (!s.ignoreTimespan) {
      // If user selected a custom timespan, use that.
      if (durValue === this.TIMESPAN_CUSTOM) {
        var view = this.placesTree.view;
        var now = Date.now() * 1000;
        // We disable the dialog's OK button if there's no selection, but we'll
        // handle that case just in... case.
        if (view.selection.getRangeCount() === 0)
          s.range = [now, now];
        else {
          var startIndexRef = {};
          // Tree sorted by visit date DEscending, so start time time comes last.
          view.selection.getRangeAt(0, {}, startIndexRef);
          view.QueryInterface(Ci.nsINavHistoryResultTreeViewer);
          var startNode = view.nodeForTreeIndex(startIndexRef.value);
          s.range = [startNode.time, now];
        }
      }
      // Otherwise use the predetermined range.
      else
        s.range = [this.durationStartTimes[durValue], Date.now() * 1000];
    }

    try {
      s.sanitize();
    } catch (er) {
      Components.utils.reportError("Exception during sanitize: " + er);
    }
    return true;
  },

  /**
   * In order to mark the custom Places tree view and its nsINavHistoryResult
   * for garbage collection, we need to break the reference cycle between the
   * two.
   */
  unload: function ()
  {
    let result = this.placesTree.getResult();
    result.removeObserver(this.placesTree.view);
    this.placesTree.view = null;
  },

  /**
   * Called when the user moves the grippy by dragging it, clicking in the tree,
   * or on keypress.  Updates the duration dropdown so that it displays the
   * appropriate specific or custom duration.
   *
   * @param aEventName
   *        The name of the event whose handler called this method, e.g.,
   *        "ondragstart", "onkeypress", etc.
   * @param aEvent
   *        The event captured in the event handler.
   */
  grippyMoved: function (aEventName, aEvent)
  {
    gContiguousSelectionTreeHelper[aEventName](aEvent);
    var lastSelRow = gContiguousSelectionTreeHelper.getGrippyRow() - 1;
    var durList = document.getElementById("sanitizeDurationChoice");
    var durValue = parseInt(durList.value);

    // Multiple durations can map to the same row.  Don't update the dropdown
    // if the current duration is valid for lastSelRow.
    if ((durValue !== this.TIMESPAN_CUSTOM ||
         lastSelRow in this.durationRowsToVals) &&
        (durValue === this.TIMESPAN_CUSTOM ||
         this.durationValsToRows[durValue] !== lastSelRow)) {
      // Setting durList.value causes its onselect handler to fire, which calls
      // selectByTimespan().
      if (lastSelRow in this.durationRowsToVals)
        durList.value = this.durationRowsToVals[lastSelRow];
      else
        durList.value = this.TIMESPAN_CUSTOM;
    }

    // If there are no selected rows, disable the dialog's OK button.
    document.documentElement.getButton("accept").disabled = lastSelRow < 0;
  }
#endif

};


#ifdef CRH_DIALOG_TREE_VIEW
/**
 * A helper for handling contiguous selection in the tree.
 */
var gContiguousSelectionTreeHelper = {

  /**
   * Gets the tree associated with this helper.
   */
  get tree()
  {
    return this._tree;
  },

  /**
   * Sets the tree that this module handles.  The tree is assigned a new view
   * that is equipped to handle contiguous selection.  You can pass in an
   * object that will be used as the prototype of the new view.  Otherwise
   * the tree's current view is used as the prototype.
   *
   * @param  aTreeElement
   *         The tree element
   * @param  aProtoTreeView
   *         If defined, this will be used as the prototype of the tree's new
   *         view
   * @return The new view
   */
  setTree: function CSTH_setTree(aTreeElement, aProtoTreeView)
  {
    this._tree = aTreeElement;
    var newView = this._makeTreeView(aProtoTreeView || aTreeElement.view);
    aTreeElement.view = newView;
    return newView;
  },

  /**
   * The index of the row that the grippy occupies.  Note that the index of the
   * last selected row is getGrippyRow() - 1.  If getGrippyRow() is 0, then
   * no selection exists.
   *
   * @return The row index of the grippy
   */
  getGrippyRow: function CSTH_getGrippyRow()
  {
    var sel = this.tree.view.selection;
    var rangeCount = sel.getRangeCount();
    if (rangeCount === 0)
      return 0;
    if (rangeCount !== 1) {
      throw "contiguous selection tree helper: getGrippyRow called with " +
            "multiple selection ranges";
    }
    var max = {};
    sel.getRangeAt(0, {}, max);
    return max.value + 1;
  },

  /**
   * Helper function for the dragover event.  Your dragover listener should
   * call this.  It updates the selection in the tree under the mouse.
   *
   * @param aEvent
   *        The observed dragover event
   */
  ondragover: function CSTH_ondragover(aEvent)
  {
    // Without this when dragging on Windows the mouse cursor is a "no" sign.
    // This makes it a drop symbol.
    var ds = Cc["@mozilla.org/widget/dragservice;1"].
             getService(Ci.nsIDragService).
             getCurrentSession();
    ds.canDrop = true;
    ds.dragAction = 0;

    var tbo = this.tree.treeBoxObject;
    aEvent.QueryInterface(Ci.nsIDOMMouseEvent);
    var hoverRow = tbo.getRowAt(aEvent.clientX, aEvent.clientY);

    if (hoverRow < 0)
      return;

    this.rangedSelect(hoverRow - 1);
  },

  /**
   * Helper function for the dragstart event.  Your dragstart listener should
   * call this.  It starts a drag session.
   *
   * @param aEvent
   *        The observed dragstart event
   */
  ondragstart: function CSTH_ondragstart(aEvent)
  {
    var tbo = this.tree.treeBoxObject;
    var clickedRow = tbo.getRowAt(aEvent.clientX, aEvent.clientY);

    if (clickedRow !== this.getGrippyRow())
      return;

    // This part is a hack.  What we really want is a grab and slide, not
    // drag and drop.  Start a move drag session with dummy data and a
    // dummy region.  Set the region's coordinates to (Infinity, Infinity)
    // so it's drawn offscreen and its size to (1, 1).
    var arr = Cc["@mozilla.org/supports-array;1"].
              createInstance(Ci.nsISupportsArray);
    var trans = Cc["@mozilla.org/widget/transferable;1"].
                createInstance(Ci.nsITransferable);
    trans.init(null);
    trans.setTransferData('dummy-flavor', null, 0);
    arr.AppendElement(trans);
    var reg = Cc["@mozilla.org/gfx/region;1"].
              createInstance(Ci.nsIScriptableRegion);
    reg.setToRect(Infinity, Infinity, 1, 1);
    var ds = Cc["@mozilla.org/widget/dragservice;1"].
             getService(Ci.nsIDragService);
    ds.invokeDragSession(aEvent.target, arr, reg, ds.DRAGDROP_ACTION_MOVE);
  },

  /**
   * Helper function for the keypress event.  Your keypress listener should
   * call this.  Users can use Up, Down, Page Up/Down, Home, and End to move
   * the bottom of the selection window.
   *
   * @param aEvent
   *        The observed keypress event
   */
  onkeypress: function CSTH_onkeypress(aEvent)
  {
    var grippyRow = this.getGrippyRow();
    var tbo = this.tree.treeBoxObject;
    var rangeEnd;
    switch (aEvent.keyCode) {
    case aEvent.DOM_VK_HOME:
      rangeEnd = 0;
      break;
    case aEvent.DOM_VK_PAGE_UP:
      rangeEnd = grippyRow - tbo.getPageLength();
      break;
    case aEvent.DOM_VK_UP:
      rangeEnd = grippyRow - 2;
      break;
    case aEvent.DOM_VK_DOWN:
      rangeEnd = grippyRow;
      break;
    case aEvent.DOM_VK_PAGE_DOWN:
      rangeEnd = grippyRow + tbo.getPageLength();
      break;
    case aEvent.DOM_VK_END:
      rangeEnd = this.tree.view.rowCount - 2;
      break;
    default:
      return;
      break;
    }

    aEvent.stopPropagation();

    // First, clip rangeEnd.  this.rangedSelect() doesn't clip the range if we
    // select past the ends of the tree.
    if (rangeEnd < 0)
      rangeEnd = -1;
    else if (this.tree.view.rowCount - 2 < rangeEnd)
      rangeEnd = this.tree.view.rowCount - 2;

    // Next, (de)select.
    this.rangedSelect(rangeEnd);

    // Finally, scroll the tree.  We always want one row above and below the
    // grippy row to be visible if possible.
    if (rangeEnd < grippyRow) // moved up
      tbo.ensureRowIsVisible(rangeEnd < 0 ? 0 : rangeEnd);
    else {                    // moved down
      if (rangeEnd + 2 < this.tree.view.rowCount)
        tbo.ensureRowIsVisible(rangeEnd + 2);
      else if (rangeEnd + 1 < this.tree.view.rowCount)
        tbo.ensureRowIsVisible(rangeEnd + 1);
    }
  },

  /**
   * Helper function for the mousedown event.  Your mousedown listener should
   * call this.  Users can click on individual rows to make the selection
   * jump to them immediately.
   *
   * @param aEvent
   *        The observed mousedown event
   */
  onmousedown: function CSTH_onmousedown(aEvent)
  {
    var tbo = this.tree.treeBoxObject;
    var clickedRow = tbo.getRowAt(aEvent.clientX, aEvent.clientY);

    if (clickedRow < 0 || clickedRow >= this.tree.view.rowCount)
      return;

    if (clickedRow < this.getGrippyRow())
      this.rangedSelect(clickedRow);
    else if (clickedRow > this.getGrippyRow())
      this.rangedSelect(clickedRow - 1);
  },

  /**
   * Selects range [0, aEndRow] in the tree.  The grippy row will then be at
   * index aEndRow + 1.  aEndRow may be -1, in which case the selection is
   * cleared and the grippy row will be at index 0.
   *
   * @param aEndRow
   *        The range [0, aEndRow] will be selected.
   */
  rangedSelect: function CSTH_rangedSelect(aEndRow)
  {
    var tbo = this.tree.treeBoxObject;
    if (aEndRow < 0)
      this.tree.view.selection.clearSelection();
    else
      this.tree.view.selection.rangedSelect(0, aEndRow, false);
    tbo.invalidateRange(tbo.getFirstVisibleRow(), tbo.getLastVisibleRow());
  },

  /**
   * Scrolls the tree so that the grippy row is in the center of the view.
   */
  scrollToGrippy: function CSTH_scrollToGrippy()
  {
    var rowCount = this.tree.view.rowCount;
    var tbo = this.tree.treeBoxObject;
    var pageLen = tbo.getPageLength() ||
                  parseInt(this.tree.getAttribute("rows")) ||
                  10;

    // All rows fit on a single page.
    if (rowCount <= pageLen)
      return;

    var scrollToRow = this.getGrippyRow() - Math.ceil(pageLen / 2.0);

    // Grippy row is in first half of first page.
    if (scrollToRow < 0)
      scrollToRow = 0;

    // Grippy row is in last half of last page.
    else if (rowCount < scrollToRow + pageLen)
      scrollToRow = rowCount - pageLen;

    tbo.scrollToRow(scrollToRow);
  },

  /**
   * Creates a new tree view suitable for contiguous selection.  If
   * aProtoTreeView is specified, it's used as the new view's prototype.
   * Otherwise the tree's current view is used as the prototype.
   *
   * @param aProtoTreeView
   *        Used as the new view's prototype if specified
   */
  _makeTreeView: function CSTH__makeTreeView(aProtoTreeView)
  {
    var view = aProtoTreeView;
    var that = this;

    //XXXadw: When Alex gets the grippy icon done, this may or may not change,
    //        depending on how we style it.
    view.isSeparator = function CSTH_View_isSeparator(aRow)
    {
      return aRow === that.getGrippyRow();
    };

    // rowCount includes the grippy row.
    view.__defineGetter__("_rowCount", view.__lookupGetter__("rowCount"));
    view.__defineGetter__("rowCount",
      function CSTH_View_rowCount()
      {
        return this._rowCount + 1;
      });

    // This has to do with visual feedback in the view itself, e.g., drawing
    // a small line underneath the dropzone.  Not what we want.
    view.canDrop = function CSTH_View_canDrop() { return false; };

    // No clicking headers to sort the tree or sort feedback on columns.
    view.cycleHeader = function CSTH_View_cycleHeader() {};
    view.sortingChanged = function CSTH_View_sortingChanged() {};

    // Override a bunch of methods to account for the grippy row.

    view._getCellProperties = view.getCellProperties;
    view.getCellProperties =
      function CSTH_View_getCellProperties(aRow, aCol)
      {
        var grippyRow = that.getGrippyRow();
        if (aRow === grippyRow)
          return "grippyRow";
        if (aRow < grippyRow)
          return this._getCellProperties(aRow, aCol);

        return this._getCellProperties(aRow - 1, aCol);
      };

    view._getRowProperties = view.getRowProperties;
    view.getRowProperties =
      function CSTH_View_getRowProperties(aRow)
      {
        var grippyRow = that.getGrippyRow();
        if (aRow === grippyRow)
          return "grippyRow";

        if (aRow < grippyRow)
          return this._getRowProperties(aRow);

        return this._getRowProperties(aRow - 1);
      };

    view._getCellText = view.getCellText;
    view.getCellText =
      function CSTH_View_getCellText(aRow, aCol)
      {
        var grippyRow = that.getGrippyRow();
        if (aRow === grippyRow)
          return "";
        aRow = aRow < grippyRow ? aRow : aRow - 1;
        return this._getCellText(aRow, aCol);
      };

    view._getImageSrc = view.getImageSrc;
    view.getImageSrc =
      function CSTH_View_getImageSrc(aRow, aCol)
      {
        var grippyRow = that.getGrippyRow();
        if (aRow === grippyRow)
          return "";
        aRow = aRow < grippyRow ? aRow : aRow - 1;
        return this._getImageSrc(aRow, aCol);
      };

    view.isContainer = function CSTH_View_isContainer(aRow) { return false; };
    view.getParentIndex = function CSTH_View_getParentIndex(aRow) { return -1; };
    view.getLevel = function CSTH_View_getLevel(aRow) { return 0; };
    view.hasNextSibling = function CSTH_View_hasNextSibling(aRow, aAfterIndex)
    {
      return aRow < this.rowCount - 1;
    };

    return view;
  }
};
#endif

//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Places System.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const PREF_PLACES_GROUPING_GENERIC = "browser.places.grouping.generic";
const PREF_PLACES_GROUPING_BOOKMARK = "browser.places.grouping.bookmark";

// Default Search Queries
const QUERY_MONTH_HISTORY = "place:&group=2&sort=1&type=1";
const QUERY_DAY_HISTORY = "place:&beginTimeRef=1&endTimeRef=2&sort=4&type=1";
const QUERY_BOOKMARKS_MENU = "place:&folders=3&group=3";
const INDEX_HISTORY = 0;
const INDEX_BOOKMARKS = 1;

var PlacesUIHook = {
  _tabbrowser: null,
  _topWindow: null,
  _placesURI: "chrome://browser/content/places/places.xul",
  _bundle: null,
  
  init: function PUIH_init(placesList) {
    try {
      this._topWindow = placesList.browserWindow;
      this._tabbrowser = this._topWindow.getBrowser();

      // Hook into the tab strip to get notifications about when the Places Page is
      // selected so that the browser UI can be modified. 
      var self = this;
      function onTabSelect(event) {
        self.onTabSelect(event);
      }
      this._tabbrowser.mTabContainer.addEventListener("select", onTabSelect, false);

      this._showPlacesUI();
    }
    catch (e) { 
    }
    
    this._bundle = document.getElementById("placeBundle");
    
    // Stop the browser from handling certain types of events. 
    function onDragEvent(event) {
      event.stopPropagation();
    }
    window.addEventListener("draggesture", onDragEvent, false);
    window.addEventListener("dragover", onDragEvent, false);
    window.addEventListener("dragdrop", onDragEvent, false);
  },

  _commands: ["Browser:SavePage", "Browser:SaveFrame", "Browser:SendLink", 
              "cmd_pageSetup", "cmd_print", "cmd_printPreview", 
              "cmd_findAgain", "cmd_switchTextDirection", "Browser:Stop",
              "Browser:Reload", "viewTextZoomMenu", "pageStyleMenu", 
              "charsetMenu", "View:PageSource", "View:FullScreen", 
              "documentDirection-swap", "Browser:AddBookmarkAs", 
              "Browser:ShowBookmarks", "Browser:ShowHistory", "View:PageInfo", 
              "cmd_toggleTaskbar"],
  
  /**
   * Disable commands that are not relevant to the Places page, so that all 
   * applicable UI becomes inactive. 
   */
  _disableCommands: function PUIH__disableCommands() {
    for (var i = 0; i < this._commands.length; ++i)
      this._topWindow.document.getElementById(this._commands[i]).
        setAttribute("disabled", true);
  },
  
  /**
   * Enable commands that aren't updated automatically by the command updater
   * when we switch away from the Places page. 
   */
  _enableCommands: function PUIH__enableCommands() {
    for (var i = 0; i < this._commands.length; ++i)
      this._topWindow.document.getElementById(this._commands[i]).
        removeAttribute("disabled");
  },
  
  uninit: function PUIH_uninit() {
    this._hidePlacesUI();
  },

  onTabSelect: function PP_onTabSelect(event) {
    var tabURI = this._tabbrowser.selectedBrowser.currentURI.spec;
    var isPlaces = tabURI.substr(0, this._placesURI.length) == this._placesURI;
    isPlaces ? this._showPlacesUI() : this._hidePlacesUI();
  },
  
  _topElement: function PUIH__topElement(id) {
    return this._topWindow.document.getElementById(id);
  },

  _showPlacesUI: function PP__showPlacesUI() {
    this._tabbrowser.setAttribute("places", "true");
    var statusbar = this._topElement("status-bar");
    statusbar.hidden = true;
    this._disableCommands();
    
    var findItem = this._topWindow.document.getElementById("menu_find");
    findItem.setAttribute("label", this._bundle.getString("findPlaceLabel"));
  },

  _hidePlacesUI: function PP__hidePlacesUI() {
    this._tabbrowser.removeAttribute("places");
    
    // Approaches that cache the value of the status bar before the Places page
    // is loaded and the status bar hidden are unreliable. This is because the
    // cached state can get confused when tabs are opened and switched to. Thus,
    // always read the user's actual preferred value (held in the checked state
    // of the menuitem, not what state the status bar was in "last" - because 
    // "last" may not be a state we want to restore to. See bug 318820. 
    var statusbarMenu = this._topWindow.document.getElementById("toggle_taskbar");
    var statusbar = this._topElement("status-bar");
    statusbar.hidden = statusbarMenu.getAttribute("checked") != "true";
    this._enableCommands();

    var findItem = this._topWindow.document.getElementById("menu_find");
    findItem.setAttribute("label", this._bundle.getString("findPageLabel"));
  },
};

var PlacesPage = {
  _content: null,
  _places: null,
  
  // the NavHistory service
  __hist: null,
  get _hist() {
    if (!this.__hist) {
      this.__hist =
        Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
    }
    return this.__hist;
  },

  init: function PP_init() {
    // Attach the Command Controller to the Places Views. 
    this._places = document.getElementById("placesList");
    this._content = document.getElementById("placeContent");  
    this._places.controllers.appendController(PlacesController);
    this._content.controllers.appendController(PlacesController);
    
    this._places.init(new ViewConfig([TYPE_X_MOZ_PLACE_CONTAINER],
                                     ViewConfig.GENERIC_DROP_TYPES,
                                     Ci.nsINavHistoryQuery.INCLUDE_QUERIES,
                                     ViewConfig.GENERIC_FILTER_OPTIONS, 3));
    this._content.init(new ViewConfig(ViewConfig.GENERIC_DROP_TYPES,
                                      ViewConfig.GENERIC_DROP_TYPES,
                                      ViewConfig.GENERIC_FILTER_OPTIONS, 0));
                                      
    PlacesController.groupableView = this._content;

    var GroupingSerializer = {
      serialize: function GS_serialize(raw) {
        return raw.join(",");
      },
      deserialize: function GS_deserialize(str) {
        return str === "" ? [] : str.split(",");
      }
    };
    PlacesController.groupers.generic = 
      new PrefHandler(PREF_PLACES_GROUPING_GENERIC, 
        [Ci.nsINavHistoryQueryOptions.GROUP_BY_DOMAIN], GroupingSerializer);
    PlacesController.groupers.bookmark = 
      new PrefHandler(PREF_PLACES_GROUPING_BOOKMARK,
        [Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], GroupingSerializer);
    
    // Hook the browser UI
    PlacesUIHook.init(this._content);

    // Attach the Places model to the Place View
    // XXXben - move this to an attribute/property on the tree view
    var bms = PlacesController._bms;
    this._places.loadFolder(bms.placesRoot);
    
    // Now load the appropriate folder in the Content View, and select the 
    // corresponding entry in the Places View. This is a little fragile. 
    var params = window.location.search;
    var index = params == "?history" ? INDEX_HISTORY : INDEX_BOOKMARKS;
    this._places.view.selection.select(index);
    
    // Set up the search UI.
    PlacesSearchBox.init();
    
    // Set up the advanced query builder UI
    PlacesQueryBuilder.init();
  },

  uninit: function PP_uninit() {
    PlacesUIHook.uninit();
  },
  
  /**
   * A range has been selected from the calendar picker. Update the view
   * to show only those results within the selected range. 
   */
  rangeSelected: function PP_rangeSelected() {
    var result = this._content.getResult();
    var queries = result.getQueries({ });
    
    var calendar = document.getElementById("historyCalendar");
    var begin = calendar.beginrange.getTime();
    var end = calendar.endrange.getTime();
    if (begin == end) {
      const DAY_MSEC = 86400000;
      end = begin + DAY_MSEC;
    }    
    var newQueries = [];
    for (var i = 0; i < queries.length; ++i) {
      var query = queries[i].clone();
      query.beginTime = begin * 1000;
      query.endTime = end * 1000;
      newQueries.push(query);
    }
    
    this._content.load(newQueries, result.queryOptions);
    
    return true;
  },

  /**
   * Fill the header with information about what view is being displayed.
   */
  _setHeader: function(type, text) {
    var bundle = document.getElementById("placeBundle");
    var key = null;
    var isSearch = false;
    switch(type) {
      case "showing":
        key = "headerTextShowing";
        break;
      case "results":
        isSearch = true;
        key = "headerTextResultsFor";
        break;
      case "advanced":
        isSearch = true;
        key = "headerTextAdvancedSearch";
        break;
    }
    var showingPrefix = document.getElementById("showingPrefix");
    showingPrefix.setAttribute("value", bundle.getString(key));
    
    var contentTitle = document.getElementById("contentTitle");
    contentTitle.setAttribute("value", text);
    
    var searchModifiers = document.getElementById("searchModifiers");
    searchModifiers.hidden = !isSearch;
  },  
  
  /**
   * Run a search for the specified text, over the collection specified by
   * the dropdown arrow. The default is all bookmarks and history, but can be
   * localized to the active collection. 
   * @param   filterString
   *          The text to search for. 
   */
  search: function PP_applyFilter(filterString) {
    switch (PlacesSearchBox.filterCollection) {
    case "collection":
      var folder = this._content.getResult().folderId;
      this._content.applyFilter(filterString, true, folder);
      this._setHeader("results", filterString);
      break;
    case "all":
      this._content.filterString = filterString;
      this._setHeader("results", filterString);
      break;
    }
  },
  
  /**
   * Called when a place folder is selected in the left pane.
   */
  placeSelected: function PP_placeSelected(event) {
    var node = this._places.selectedNode;
    if (!node || this._places.suppressSelection)
      return;
    var queries = node.getQueries({});
    var newQueries = [];
    for (var i = 0; i < queries.length; ++i) {
      var query = queries[i].clone();
      query.itemTypes |= this._content.filterOptions;
      newQueries.push(query);
    }
    var newOptions = node.queryOptions.clone();

    var groupings = PlacesController.groupers.generic.value;
    var isBookmark = PlacesController.nodeIsFolder(node);
    if (isBookmark)
      groupings = PlacesController.groupers.bookmark.value;

    newOptions.setGroupingMode(groupings, groupings.length);
    this._content.load(newQueries, newOptions);

    this._setHeader("showing", node.title);
  },
  
  /**
   * Updates the calendar widget to show the range of dates selected in the
   * current result. 
   */
  _updateCalendar: function PP__updateCalendar() {
    // Make sure that by updating the calendar widget we don't fire selection
    // events and cause the UI to infinitely reload.
    var calendar = document.getElementById("historyCalendar");
    calendar.suppressRangeEvents = true;

    var result = this._content.getResult();
    var queries = result.getQueries({ });
    if (!queries.length)
      return;
    
    // Query values are OR'ed together, so just use the first. 
    var query = queries[0];
    
    const NOW = new Date();
    var begin = Math.floor(query.beginTime / 1000);
    var end = Math.floor(query.endTime / 1000);
    if (query.beginTimeReference == Ci.nsINavHistoryQuery.TIME_RELATIVE_TODAY) {
      if (query.beginTime == 0) {
        var d = new Date();
        d.setFullYear(NOW.getFullYear(), NOW.getMonth(), NOW.getDate());
        d.setHours(0, 0, 0, 0);
        begin += d.getTime();
      }
      else
        begin += NOW.getTime();
      end += NOW.getTime();
    }
    calendar.beginrange = new Date(begin);
    calendar.endrange = new Date(end);
    
    // Allow user selection events once again. 
    calendar.suppressRangeEvents = false;
  },
  
  /**
   * Update the Places UI when the content of the right tree changes. 
   */
  onContentChanged: function PP_onContentChanged() {
    var panelID = "commands_history";
    var filterButtonID = "filterList_history";
    var isBookmarks = this._content.isBookmarks;
    if (isBookmarks) {
      // if (query.annotation == "feed") {
      panelID = "commands_bookmark";
      filterButtonID = "filterList_bookmark";
    }
    var commandBar = document.getElementById("commandBar");
    commandBar.selectedPanel = document.getElementById(panelID);

    // Hide the Calendar for Bookmark queries. 
    document.getElementById("historyCalendar").setAttribute("hidden", isBookmarks);
    
    // Update the calendar with the current date range, if applicable. 
    if (!isBookmarks)
      this._updateCalendar();
  },
};

/**
 * A set of utilities relating to search within Bookmarks and History. 
 */
var PlacesSearchBox = {
  /**
   * Gets/sets the active collection from the dropdown menu.
   */
  get filterCollection() {
    var searchFilter = document.getElementById("searchFilter");
    return searchFilter.getAttribute("collection");
  },
  set filterCollection(collectionName) {
    LOG("SET COLN: " + collectionName);
    var searchFilter = document.getElementById("searchFilter");
    searchFilter.setAttribute("collection", collectionName);
    return collectionName;
  },
  
  /**
   * When the field is activated, if the contents are the gray text, clear
   * the field, otherwise select the contents. 
   */
  onFocus: function PS_onFocus() {
    var searchFilter = document.getElementById("searchFilter");
    var placeBundle = document.getElementById("placeBundle");
    
    var searchDefault = placeBundle.getString("searchDefault");
    if (searchFilter.value == searchDefault) {
      searchFilter.removeAttribute("empty");
      searchFilter.value = "";
    }
    else
      searchFilter.select();
  },
  
  /**
   * When the field is deactivated, reset the gray text if the value is
   * empty or has the gray text value. 
   */
  onBlur: function PS_onBlur() {
    var placeBundle = document.getElementById("placeBundle");
    var searchDefault = placeBundle.getString("searchDefault");
    var searchFilter = document.getElementById("searchFilter");
    
    if (searchFilter.value == searchDefault || !searchFilter.value) {
      searchFilter.setAttribute("empty", "true");
      searchFilter.value = searchDefault;
    }    
  },
  
  /** 
   * Set up the gray text in the search bar as the Places View loads. 
   */
  init: function PS_init() {
    var placeBundle = document.getElementById("placeBundle");
    var searchDefault = placeBundle.getString("searchDefault");
    var searchFilter = document.getElementById("searchFilter");
    searchFilter.value = searchDefault;
    searchFilter.setAttribute("empty", "true");
    searchFilter.focus();
  },

};

/**
 * Functions and data for advanced query builder
 */
var PlacesQueryBuilder = {

  _numRows: 1,
  _maxRows: 4,
  
  _keywordSearch: {
    advancedSearch_N_Subject: "advancedSearch_N_SubjectKeyword",
    advancedSearch_N_HostMenulist: false,
    advancedSearch_N_KeywordLabel: true,
    advancedSearch_N_UriMenulist: false,
    advancedSearch_N_TimeMenulist: false,
    advancedSearch_N_Textbox: "",
    advancedSearch_N_TimePicker: false,
    advancedSearch_N_TimeMenulist2: false,
  },
  _hostSearch: {
    advancedSearch_N_Subject: "advancedSearch_N_SubjectHost",
    advancedSearch_N_HostMenulist: "advancedSearch_N_HostMenuSelected",
    advancedSearch_N_KeywordLabel: false,
    advancedSearch_N_UriMenulist: false,
    advancedSearch_N_TimeMenulist: false,
    advancedSearch_N_Textbox: "",
    advancedSearch_N_TimePicker: false,
    advancedSearch_N_TimeMenulist2: false,
  },
  _uriSearch: {
    advancedSearch_N_Subject: "advancedSearch_N_SubjectUri",
    advancedSearch_N_HostMenulist: false,
    advancedSearch_N_KeywordLabel: false,
    advancedSearch_N_UriMenulist: "advancedSearch_N_UriMenuSelected",
    advancedSearch_N_TimeMenulist: false,
    advancedSearch_N_Textbox: "http://",
    advancedSearch_N_TimePicker: false,
    advancedSearch_N_TimeMenulist2: false,
  },
  _timeSearch: {
    advancedSearch_N_Subject: "advancedSearch_N_SubjectVisited",
    advancedSearch_N_HostMenulist: false,
    advancedSearch_N_KeywordLabel: false,
    advancedSearch_N_UriMenulist: false,
    advancedSearch_N_TimeMenulist: true,
    advancedSearch_N_Textbox: false,
    advancedSearch_N_TimePicker: "date",
    advancedSearch_N_TimeMenulist2: false,
  },
  _timeInLastSearch: {
    advancedSearch_N_Subject: "advancedSearch_N_SubjectVisited",
    advancedSearch_N_HostMenulist: false,
    advancedSearch_N_KeywordLabel: false,
    advancedSearch_N_UriMenulist: false,
    advancedSearch_N_TimeMenulist: true,
    advancedSearch_N_Textbox: "7",
    advancedSearch_N_TimePicker: false,
    advancedSearch_N_TimeMenulist2: true,
  },
  _nextSearch: null,
  _queryBuilders: null,
  
  init: function PQB_init() {
    // Initialize advanced search
    this._nextSearch = {
      "keyword": this._timeSearch,
      "visited": this._hostSearch,
      "host": this._uriSearch,
      "uri": this._keywordSearch,
    };
    
    this._queryBuilders = {
      "keyword": this.setKeywordQuery,
      "visited": this.setVisitedQuery,
      "host": this.setHostQuery,
      "uri": this.setUriQuery,
    };
    
    this._dateService = Cc["@mozilla.org/intl/scriptabledateformat;1"].
                          getService(Ci.nsIScriptableDateFormat);
  },

  toggle: function PQB_toggle() {
    var advancedSearch = document.getElementById("advancedSearch");
    if (advancedSearch.collapsed) {
      // Need to expand the advanced search box and initialize it.
      
      // Should have one row, containing a keyword search with
      // the keyword from the basic search box pre-filled.
      while (this._numRows > 1)
        this.removeRow();
      this.showSearch(1, this._keywordSearch);
      var searchbox = document.getElementById("searchFilter");
      var keywordbox = document.getElementById("advancedSearch1Textbox");
      keywordbox.value = searchbox.value;
      advancedSearch.collapsed = false;
      
      // Update the +/- button and the header.
      var button = document.getElementById("moreCriteria");
      var placeBundle = document.getElementById("placeBundle");
      button.label = placeBundle.getString("lessCriteria.label");
      PlacesPage._setHeader("advanced", "");
    }
    else {
      // Need to collapse the advanced search box.
      advancedSearch.collapsed = true;
      
      // Update the +/- button
      var button = document.getElementById("moreCriteria");
      var placeBundle = document.getElementById("placeBundle");
      button.label = placeBundle.getString("moreCriteria.label");
    }
  },
  
  setRowId: function PQB_setRowId(element, rowId) {
    if (element.id)
      element.id = element.id.replace("advancedSearch1", "advancedSearch" + rowId);
    if (element.hasAttribute('rowid'))
      element.setAttribute('rowid', rowId);
    for (var i = 0; i < element.childNodes.length; i++) {
      this.setRowId(element.childNodes[i], rowId);
    }
  },
  
  updateUIForRowChange: function PQB_updateUIForRowChange() {
    // Titlebar should show "match any/all" iff there are > 1 queries visible.
    var matchUI = document.getElementById("titlebarMatch");
    matchUI.hidden = (this._numRows <= 1);
    PlacesPage._setHeader("advanced", this._numRows <= 1 ? "" : ",");
    
    // Disable the + buttons if there are max advanced search rows
    // Disable the - button is there is only one advanced search row
    for (var i = 1; i <= this._numRows; i++) {
      var plus = document.getElementById("advancedSearch" + i + "Plus");
      plus.disabled = (this._numRows >= this._maxRows);
      var minus = document.getElementById("advancedSearch" + i + "Minus");
      minus.disabled = (this._numRows == 1);
    }
  },
  
  addRow: function PQB_addRow() {
    if (this._numRows >= this._maxRows)
      return;
    
    var gridRows = document.getElementById("advancedSearchRows");
    var newRow = gridRows.firstChild.cloneNode(true);

    var searchType = this._keywordSearch;
    var lastMenu = document.getElementById("advancedSearch" +
                                           this._numRows +
                                           "Subject");
    if (lastMenu && lastMenu.selectedItem) {
      searchType = this._nextSearch[lastMenu.selectedItem.value];
    }
    
    this._numRows++;
    this.setRowId(newRow, this._numRows);
    this.showSearch(this._numRows, searchType);
    gridRows.appendChild(newRow);
    this.updateUIForRowChange();
  },
  
  removeRow: function PQB_removeRow() {
    if (this._numRows <= 1)
      return;

    var row = document.getElementById("advancedSearch" + this._numRows + "Row");
    row.parentNode.removeChild(row);
    this._numRows--;
    
    this.updateUIForRowChange();
  },
  
  onDateTyped: function PQB_onDateTyped(event, row) {
    var textbox = document.getElementById("advancedSearch" + row + "TimePicker");
    var dateString = textbox.value;
    var dateArr = dateString.split("-");
    // The date can be split into a range by the '-' character, i.e.
    // 9/5/05 - 10/2/05.  Unfortunately, dates can also be written like
    // 9-5-05 - 10-2-05.  Try to parse the date based on how many hyphens
    // there are.
    var d0 = null;
    var d1 = null;
    // If there are an even number of elements in the date array, try to 
    // parse it as a range of two dates.
    if ((dateArr.length & 1) == 0) {
      var mid = dateArr.length / 2;
      var dateStr0 = dateArr[0];
      var dateStr1 = dateArr[mid];
      for (var i = 1; i < mid; i++) {
        dateStr0 += "-" + dateArr[i];
        dateStr1 += "-" + dateArr[i + mid];
      }
      d0 = new Date(dateStr0);
      d1 = new Date(dateStr1);
    }
    // If that didn't work, try to parse it as a single date.
    if (d0 == null || d0 == "Invalid Date") {
      d0 = new Date(dateString);
    }
    
    if (d0 != null && d0 != "Invalid Date") {
      // Parsing succeeded -- update the calendar.
      var calendar = document.getElementById("advancedSearch" + row + "Calendar");
      if (d0.getFullYear() < 2000)
        d0.setFullYear(2000 + (d0.getFullYear() % 100));
      if (d1 != null && d1 != "Invalid Date") {
        if (d1.getFullYear() < 2000)
          d1.setFullYear(2000 + (d1.getFullYear() % 100));
        calendar.updateSelection(d0, d1);
      }
      else {
        calendar.updateSelection(d0, d0);
      }
      
      // And update the search.
      this.doSearch();
    }
  },
  
  onCalendarChanged: function PQB_onCalendarChanged(event, row) {
    var calendar = document.getElementById("advancedSearch" + row + "Calendar");
    var begin = calendar.beginrange;
    var end = calendar.endrange;
    
    // If the calendar doesn't have a begin/end, don't change the textbox.
    if (begin == null || end == null)
      return true;
      
    // If the begin and end are the same day, only fill that into the textbox.
    var textbox = document.getElementById("advancedSearch" + row + "TimePicker");
    var beginDate = begin.getDate();
    var beginMonth = begin.getMonth() + 1;
    var beginYear = begin.getFullYear();
    var endDate = end.getDate();
    var endMonth = end.getMonth() + 1;
    var endYear = end.getFullYear();
    if (beginDate == endDate && beginMonth == endMonth && beginYear == endYear) {
      // Just one date.
      textbox.value = this._dateService.FormatDate("",
                                                   this._dateService.dateFormatShort,
                                                   beginYear,
                                                   beginMonth,
                                                   beginDate);
    }
    else
    {
      // Two dates.
      var beginStr = this._dateService.FormatDate("",
                                                   this._dateService.dateFormatShort,
                                                   beginYear,
                                                   beginMonth,
                                                   beginDate);
      var endStr = this._dateService.FormatDate("",
                                                this._dateService.dateFormatShort,
                                                endYear,
                                                endMonth,
                                                endDate);
      textbox.value = beginStr + " - " + endStr;
    }
    
    // Update the search.
    this.doSearch();
    
    return true;
  },
  
  handleTimePickerClick: function PQB_handleTimePickerClick(event, row) {
    var popup = document.getElementById("advancedSearch" + row + "DatePopup");
    if (popup.showing)
      popup.hidePopup();
    else {
      var textbox = document.getElementById("advancedSearch" + row + "TimePicker");
      popup.showPopup(textbox, -1, -1, "popup", "bottomleft", "topleft");
    }
  },
  
  showSearch: function PQB_showSearch(row, values) {
    for (val in values) {
      var id = val.replace("_N_", row);
      var element = document.getElementById(id);
      if (values[val] || typeof(values[val]) == "string") {
        if (typeof(values[val]) == "string") {
          if (values[val] == "date") {
            // "date" means that the current date should be filled into the
            // textbox, and the calendar for the row updated.
            var d = new Date();
            element.value = this._dateService.FormatDate("",
                                                         this._dateService.dateFormatShort,
                                                         d.getFullYear(),
                                                         d.getMonth() + 1,
                                                         d.getDate());
            var calendar = document.getElementById("advancedSearch" + row + "Calendar");
            calendar.updateSelection(d, d);
          }
          else if (element.nodeName == "textbox") {
            // values[val] is the initial value of the textbox.
            element.value = values[val];
          } else {
            // values[val] is the menuitem which should be selected.
            var itemId = values[val].replace("_N_", row);
            var item = document.getElementById(itemId);
            element.selectedItem = item;
          }
        }
        element.hidden = false;
      }
      else {
        element.hidden = true;
      }
    }
    
    this.doSearch();
  },
  
  setKeywordQuery: function PQB_setKeywordQuery(query, prefix) {
    query.searchTerms += document.getElementById(prefix + "Textbox").value + " ";
  },
  
  setUriQuery: function PQB_setUriQuery(query, prefix) {
    var matchType = document.getElementById(prefix + "UriMenulist").selectedItem.value;
    if (matchType == "startsWith")
      query.uriIsPrefix = true;
    else
      query.uriIsPrefix = false;

    var ios = Cc["@mozilla.org/network/io-service;1"].
                getService(Ci.nsIIOService);
    var spec = document.getElementById(prefix + "Textbox").value;
    query.uri = ios.newURI(spec, null, null);
  },
  
  setHostQuery: function PQB_setHostQuery(query, prefix) {
    if (document.getElementById(prefix + "HostMenulist").selectedItem.value == "is")
      query.domainIsHost = true;
    query.domain = document.getElementById(prefix + "Textbox").value;
  },
  
  setVisitedQuery: function PQB_setVisitedQuery(query, prefix) {
    var searchType = document.getElementById(prefix + "TimeMenulist").selectedItem.value;
    const DAY_MSEC = 86400000;
    switch (searchType) {
      case "on":
        var calendar = document.getElementById(prefix + "Calendar");
        var begin = calendar.beginrange.getTime();
        var end = calendar.endrange.getTime();
        if (begin == end) {
          end = begin + DAY_MSEC;
        }
        query.beginTime = begin * 1000;
        query.endTime = end * 1000;
        break;
      case "before":
        var calendar = document.getElementById(prefix + "Calendar");
        var time = calendar.beginrange.getTime();
        query.endTime = time * 1000;
        break;
      case "after":
        var calendar = document.getElementById(prefix + "Calendar");
        var time = calendar.endrange.getTime();
        query.beginTime = time * 1000;
        break;
      case "inLast":
        var textbox = document.getElementById(prefix + "Textbox");
        var amount = parseInt(textbox.value);
        amount = amount * DAY_MSEC;
        var menulist = document.getElementById(prefix + "TimeMenulist2");
        if (menulist.selectedItem.value == "weeks")
          amount = amount * 7;
        else if (menulist.selectedItem.value == "months")
          amount = amount * 30;
        var now = new Date();
        now = now - amount;
        query.beginTime = now * 1000;
        break;
    }
  },
  
  doSearch: function PQB_doSearch() {
    // Create the individual queries.
    var queryType = document.getElementById("advancedSearchType").selectedItem.value;
    var queries = [];
    if (queryType == "and")
      queries.push(PlacesPage._hist.getNewQuery());
    var onlyBookmarked = document.getElementById("advancedSearchOnlyBookmarked").checked;
    for (var i = 1; i <= this._numRows; i++) {
      var prefix = "advancedSearch" + i;
      
      // If the queries are being AND-ed, put all the rows in one query.
      // If they're being OR-ed, add a separate query for each row.
      var query;
      if (queryType == "and")
        query = queries[0];
      else
        query = PlacesPage._hist.getNewQuery();
      query.onlyBookmarked = onlyBookmarked;
      
      var querySubject = document.getElementById(prefix + "Subject").value;
      this._queryBuilders[querySubject](query, prefix);
      
      if (queryType == "or")
        queries.push(query);
    }
    
    // Set max results
    var result = PlacesPage._content.getResult();
    if (document.getElementById("advancedSearchHasMax").checked) {
      var max = parseInt(document.getElementById("advancedSearchMaxResults").value);
      if (isNaN(max))
        max = 100;
      result.queryOptions.maxResults = max;
      dump("Max results = " + result.queryOptions.maxResults + "(" + max + ")\n");
    }
    // Make sure we're getting url results, not visits
    result.queryOptions.resultType = result.queryOptions.RESULT_TYPE_URL;

    PlacesPage._content.load(queries, result.queryOptions);
  },
};

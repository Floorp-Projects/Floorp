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
 * The Original Code is Mozilla Places Organizer.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
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

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * Selects a place URI in the places list. 
 * This function is global so it can be easily accessed by openers. 
 * @param   placeURI
 *          A place: URI string to select
 */
function selectPlaceURI(placeURI) {
  PlacesOrganizer._places.selectPlaceURI(placeURI);
}

var PlacesOrganizer = {
  _places: null,
  _content: null,
  
  init: function PP_init() {
    this._places = document.getElementById("placesList");
    this._content = document.getElementById("placeContent");  
    
    Groupers.init();
    
    // Select the specified place in the places list. 
    var placeURI = "place:";
    if ("arguments" in window)
      placeURI = window.arguments[0];
    selectPlaceURI(placeURI);
    
    // Initialize the active view so that all commands work properly without
    // the user needing to explicitly click in a view (since the search box is
    // focused by default). 
    PlacesController.activeView = this._places;

    // Set up the search UI.
    PlacesSearchBox.init();
    
    // Set up the advanced query builder UI
    PlacesQueryBuilder.init();
  },
  
  
  /**
   * Fill the header with information about what view is being displayed.
   */
  _setHeader: function PP__setHeader(type, text) {
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
      var folder = this._content.getResult().root.QueryInterface(Ci.nsINavHistoryFolderResultNode).folderId;
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
  onPlaceSelected: function PP_onPlaceSelected(event) {
    if (!this._places.hasSelection)
      return;
    var node = asQuery(this._places.selectedNode);
    LOG("NODEURI: " + node.uri);
    this._content.place = node.uri;

    Groupers.setGroupingOptions(this._content.getResult(), true);

    this._setHeader("showing", node.title);
  },
  
  /**
   * Handle clicks on the tree. If the user middle clicks on a URL, load that 
   * URL according to rules. Single clicks or modified clicks do not result in 
   * any special action, since they're related to selection. 
   * @param   event
   *          The mouse event.
   */
  onTreeClick: function PP_onURLClicked(event) {
    var v = PlacesController.activeView;
    if (v.hasSingleSelection && event.button == 1) {
      if (PlacesController.nodeIsURI(v.selectedNode))
        PlacesController.mouseLoadURI(event);
      else if (PlacesController.nodeIsContainer(v.selectedNode)) {
        // The command execution function will take care of seeing the 
        // selection is a folder/container and loading its contents in 
        // tabs for us. 
        PlacesController.openLinksInTabs();
      }
    }
  },

  /**
   * Shows all subscribed feeds (Live Bookmarks) grouped under their parent 
   * feed.
   */
  groupByFeed: function PP_groupByFeed() {
    var groupings = [Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER];
    var sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;
    PlacesController.groupByAnnotation("livemark/feedURI", [], 0);
  },
  
  /**
   * Shows all subscribed feed (Live Bookmarks) content in a flat list
   */
  groupByPost: function PP_groupByPost() {
    var groupings = [Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER];
    var sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;
    PlacesController.groupByAnnotation("livemark/bookmarkFeedURI", [], 0);
  },
  
  /**
   * Returns the query array associated with the query currently loaded in
   * the main places pane.
   */
  getCurrentQueries: function PP_getCurrentQueries() {
    var result = this._content.getResult();
    return result.root.QueryInterface(Ci.nsINavHistoryQueryResultNode).getQueries({});
  },

  /**
   * Returns the options associated with the query currently loaded in the
   * main places pane.
   */
  getCurrentOptions: function PP_getCurrentOptions() {
    var result = this._content.getResult();
    return result.root.QueryInterface(Ci.nsINavHistoryQueryResultNode).queryOptions;
  }
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
    var searchFilter = document.getElementById("searchFilter");
    searchFilter.setAttribute("collection", collectionName);
    return collectionName;
  },
  
  /**
   * Focus the search box
   */
  focus: function PS_focus() {
    var searchFilter = document.getElementById("searchFilter");
    searchFilter.focus();
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
  }

};

/**
 * Functions and data for advanced query builder
 */
var PlacesQueryBuilder = {

  _numRows: 1,
  _maxRows: 4,
  
  _keywordSearch: {
    advancedSearch_N_Subject: "advancedSearch_N_SubjectKeyword",
    advancedSearch_N_LocationMenulist: false,
    advancedSearch_N_TimeMenulist: false,
    advancedSearch_N_Textbox: "",
    advancedSearch_N_TimePicker: false,
    advancedSearch_N_TimeMenulist2: false
  },
  _locationSearch: {
    advancedSearch_N_Subject: "advancedSearch_N_SubjectLocation",
    advancedSearch_N_LocationMenulist: "advancedSearch_N_LocationMenuSelected",
    advancedSearch_N_TimeMenulist: false,
    advancedSearch_N_Textbox: "",
    advancedSearch_N_TimePicker: false,
    advancedSearch_N_TimeMenulist2: false
  },
  _timeSearch: {
    advancedSearch_N_Subject: "advancedSearch_N_SubjectVisited",
    advancedSearch_N_LocationMenulist: false,
    advancedSearch_N_TimeMenulist: true,
    advancedSearch_N_Textbox: false,
    advancedSearch_N_TimePicker: "date",
    advancedSearch_N_TimeMenulist2: false
  },
  _timeInLastSearch: {
    advancedSearch_N_Subject: "advancedSearch_N_SubjectVisited",
    advancedSearch_N_LocationMenulist: false,
    advancedSearch_N_TimeMenulist: true,
    advancedSearch_N_Textbox: "7",
    advancedSearch_N_TimePicker: false,
    advancedSearch_N_TimeMenulist2: true
  },
  _nextSearch: null,
  _queryBuilders: null,
  
  init: function PQB_init() {
    // Initialize advanced search
    this._nextSearch = {
      "keyword": this._timeSearch,
      "visited": this._hostSearch,
      "location": this._locationSearch
    };
    
    this._queryBuilders = {
      "keyword": this.setKeywordQuery,
      "visited": this.setVisitedQuery,
      "location": this.setLocationQuery
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
      PlacesOrganizer._setHeader("advanced", "");
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
    PlacesOrganizer._setHeader("advanced", this._numRows <= 1 ? "" : ",");
    
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
  
  setLocationQuery: function PQB_setLocationQuery(query, prefix) {
    var type = document.getElementById(prefix + "LocationMenulist").selectedItem.value;
    if (type == "onsite") {
      query.domain = document.getElementById(prefix + "Textbox").value;
    }
    else {
      query.uriIsPrefix = (type == "startswith");
      var ios = Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService);
      var spec = document.getElementById(prefix + "Textbox").value;
      query.uri = ios.newURI(spec, null, null);
    }
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
      queries.push(PlacesController.history.getNewQuery());
    for (var i = 1; i <= this._numRows; i++) {
      var prefix = "advancedSearch" + i;
      
      // If the queries are being AND-ed, put all the rows in one query.
      // If they're being OR-ed, add a separate query for each row.
      var query;
      if (queryType == "and")
        query = queries[0];
      else
        query = PlacesController.history.getNewQuery();
      
      var querySubject = document.getElementById(prefix + "Subject").value;
      this._queryBuilders[querySubject](query, prefix);
      
      if (queryType == "or")
        queries.push(query);
    }
    
    // Make sure we're getting uri results, not visits
    var options = PlacesOrganizer.getCurrentOptions();
    options.resultType = options.RESULT_TYPE_URI;

    // XXXben - find some public way of doing this!
    PlacesOrganizer._content._load(queries, options);
  }
};

/**
 * Population and commands for the View Menu. 
 */
var ViewMenu = {
  /**
   * Removes content generated previously from a menupopup.
   * @param   popup
   *          The popup that contains the previously generated content.
   * @param   startID
   *          The id attribute of an element that is the start of the 
   *          dynamically generated region - remove elements after this
   *          item only. 
   *          Must be contained by popup. Can be null (in which case the
   *          contents of popup are removed). 
   * @param   endID
   *          The id attribute of an element that is the end of the
   *          dynamically generated region - remove elements up to this
   *          item only.
   *          Must be contained by popup. Can be null (in which case all
   *          items until the end of the popup will be removed). Ignored
   *          if startID is null. 
   * @returns The element for the caller to insert new items before, 
   *          null if the caller should just append to the popup.
   */
  _clean: function VM__clean(popup, startID, endID) {
    if (endID) 
      NS_ASSERT(startID, "meaningless to have valid endID and null startID");
    if (startID) {
      var startElement = document.getElementById(startID);
      NS_ASSERT(startElement.parentNode == 
                popup, "startElement is not in popup");
      NS_ASSERT(startElement, 
                "startID does not correspond to an existing element");
      var endElement = null;
      if (endID) {
        endElement = document.getElementById(endID);
        NS_ASSERT(endElement.parentNode == popup, 
                  "endElement is not in popup");
        NS_ASSERT(endElement, 
                  "endID does not correspond to an existing element");
      }
      while (startElement.nextSibling != endElement)
        popup.removeChild(startElement.nextSibling);
      return endElement;
    }
    else {
      while(popup.hasChildNodes())
        popup.removeChild(popup.firstChild);  
    }
    return null;
  },
  
  /**
   * Fills a menupopup with a list of columns
   * @param   event
   *          The popupshowing event that invoked this function.
   * @param   startID
   *          see _clean
   * @param   endID
   *          see _clean
   * @param   type
   *          the type of the menuitem, e.g. "radio" or "checkbox". 
   *          Can be null (no-type). 
   *          Checkboxes are checked if the column is visible.
   * @param   labelFormat
   *          A format string to be applied to item labels. If null, no format
   *          is used. 
   */
  fillWithColumns: function VM_fillWithColumns(event, startID, endID, type, labelFormat) {
    var popup = event.target;  
    var pivot = this._clean(popup, startID, endID);
    
    // If no column is "sort-active", the "Unsorted" item needs to be checked, 
    // so track whether or not we find a column that is sort-active. 
    var isSorted = false;
    var content = document.getElementById("placeContent");
    var strings = document.getElementById("placeBundle");
    var columns = content.columns;
    for (var i = 0; i < columns.count; ++i) {
      var column = columns.getColumnAt(i).element;
      var menuitem = document.createElementNS(XUL_NS, "menuitem");
      menuitem.id = "menucol_" + column.id;
      var label = column.getAttribute("label");
      if (labelFormat)
        label = strings.getFormattedString(labelFormat, [label]);
      menuitem.setAttribute("label", label);
      if (type == "radio") {
        menuitem.setAttribute("type", "radio");
        menuitem.setAttribute("name", "columns");
        // This column is the sort key. Its item is checked. 
        if (column.hasAttribute("sortDirection")) {
          menuitem.setAttribute("checked", "true");
          isSorted = true;
        }
      }
      else if (type == "checkbox") {
        menuitem.setAttribute("type", "checkbox");
        // Cannot uncheck the primary column. 
        if (column.getAttribute("primary") == "true")
          menuitem.setAttribute("disabled", "true");
        // Items for visible columns are checked. 
        if (column.getAttribute("hidden") != "true")
          menuitem.setAttribute("checked", "true");
      }
      if (pivot)
        popup.insertBefore(menuitem, pivot);
      else
        popup.appendChild(menuitem);      
    }
    event.stopPropagation();
  },
  
  /**
   * Set up the content of the view menu.
   */
  populate: function VM_populate(event) {
    this.fillWithColumns(event, "viewUnsorted", "directionSeparator", "radio", "sortByPrefix");
    
    var sortColumn = this._getSortColumn();
    var viewSortAscending = document.getElementById("viewSortAscending");
    var viewSortDescending = document.getElementById("viewSortDescending");
    // We need to remove an existing checked attribute because the unsorted 
    // menu item is not rebuilt every time we open the menu like the others.
    var viewUnsorted = document.getElementById("viewUnsorted");
    if (!sortColumn) {
      viewSortAscending.removeAttribute("checked");
      viewSortDescending.removeAttribute("checked");
      viewUnsorted.setAttribute("checked", "true");
    }
    else if (sortColumn.getAttribute("sortDirection") == "ascending") {
      viewSortAscending.setAttribute("checked", "true");
      viewSortDescending.removeAttribute("checked");
      viewUnsorted.removeAttribute("checked");
    }
    else if (sortColumn.getAttribute("sortDirection") == "descending") {
      viewSortDescending.setAttribute("checked", "true");
      viewSortAscending.removeAttribute("checked");
      viewUnsorted.removeAttribute("checked");
    }
  },
  
  /**
   * Shows/Hides a tree column.
   * @param   element
   *          The menuitem element for the column
   */
  showHideColumn: function VM_showHideColumn(element) {
    const PREFIX = "menucol_";
    var columnID = element.id.substr(PREFIX.length, element.id.length);
    var column = document.getElementById(columnID);
    NS_ASSERT(column, 
              "menu item for column that doesn't exist?! id = " + element.id);

    var splitter = column.nextSibling;
    if (splitter && splitter.localName != "splitter")
      splitter = null;
    
    if (element.getAttribute("checked") == "true") {
      column.removeAttribute("hidden");
      splitter.removeAttribute("hidden");
    }
    else {
      column.setAttribute("hidden", "true");
      splitter.setAttribute("hidden", "true");
    }    
  },
  
  /**
   * Gets the last column that was sorted. 
   * @returns  the currently sorted column, null if there is no sorted column. 
   */
  _getSortColumn: function VM__getSortColumn() {
    var content = document.getElementById("placeContent");
    var cols = content.columns;
    for (var i = 0; i < cols.count; ++i) {
      var column = cols.getColumnAt(i).element;
      var sortDirection = column.getAttribute("sortDirection");
      if (sortDirection == "ascending" || sortDirection == "descending")
        return column;
    }
    return null;
  },
  
  /**
   * Sorts the view by the specified key.
   * @param   element
   *          The menuitem element for the column that is the sort key. 
   *          Can be null - the primary column will be sorted. 
   * @param   direction
   *          The direction to sort - "ascending" or "descending". 
   *          Can be null - the last direction will be used.
   *          If both element and direction are null, the view will be
   *          unsorted. 
   */
  setSortColumn: function VM_setSortColumn(element, direction) {
    const PREFIX = "menucol_";
    // Validate the click - check to see if it was on a valid sort item.
    if (element && 
        (element.id.substring(0, PREFIX.length) != PREFIX &&
         element.id != "viewSortDescending" && 
         element.id != "viewSortAscending"))
      return;
    
    // If both element and direction are null, all will be unsorted.
    var unsorted = !element && !direction;
    // If only the element is null, the currently sorted column will be sorted 
    // with the specified direction, if there is no currently sorted column, 
    // the primary column will be sorted with the specified direction. 
    if (!element && direction) {
      element = this._getSortColumn();
      if (!element)
        element = document.getElementById("title");
    }
    else if (element && !direction) {
      var elementID = element.id.substr(PREFIX.length, element.id.length);
      element = document.getElementById(elementID);
      // If direction is null, use the default (ascending)
      direction = "ascending";
    }

    var content = document.getElementById("placeContent");      
    var columns = content.columns;
    for (var i = 0; i < columns.count; ++i) {
      var column = columns.getColumnAt(i).element;
      if (unsorted) {
        column.removeAttribute("sortActive");
        column.removeAttribute("sortDirection");
        content.getResult().sortAll(Ci.nsINavHistoryQueryOptions.SORT_BY_NONE);
      }
      else if (column.id == element.id) {
        // We need to do this to ensure the UI updates properly next time it is built.
        if (column.getAttribute("sortDirection") != direction) {
          column.setAttribute("sortActive", "true");
          column.setAttribute("sortDirection", direction);
        }
        var columnObj = content.columns.getColumnFor(column);
        content.view.cycleHeader(columnObj);
        break;
      }
    }
  }
};

function GroupingConfig(substr, onLabel, onAccesskey, offLabel, offAccesskey, 
                        onOncommand, offOncommand) {
  this.substr = substr;
  this.onLabel = onLabel;
  this.onAccesskey = onAccesskey;
  this.offLabel = offLabel;
  this.offAccesskey = offAccesskey;
  this.onOncommand = onOncommand;
  this.offOncommand = offOncommand;
}

/**
 * Handles Grouping within the Content View, and the commands that support it. 
 */
var Groupers = {
  defaultGrouper: null,
  annotationGroupers: [],
  
  /**
   * Initializes groupings for various vie types. 
   */
  init: function G_init() {
    var placeBundle = document.getElementById("placeBundle");
    this.defaultGrouper = 
      new GroupingConfig(null, placeBundle.getString("defaultGroupOnLabel"),
                         placeBundle.getString("defaultGroupOnAccesskey"),
                         placeBundle.getString("defaultGroupOffLabel"),
                         placeBundle.getString("defaultGroupOffAccesskey"),
                         "PlacesOrganizer.groupBySite()",
                         "PlacesOrganizer.groupByPage()");
    var subscriptionConfig = 
      new GroupingConfig("livemark/", placeBundle.getString("livemarkGroupOnLabel"),
                         placeBundle.getString("livemarkGroupOnAccesskey"),
                         placeBundle.getString("livemarkGroupOffLabel"),
                         placeBundle.getString("livemarkGroupOffAccesskey"),
                         "PlacesOrganizer.groupByFeed()",
                         "PlacesOrganizer.groupByPost()");
    this.annotationGroupers.push(subscriptionConfig);
  },
  
  /**
   * Updates the grouping broadcasters for the given result. 
   * @param   result
   *          
   * @param   on
   *
   */
  setGroupingOptions: function G_setGroupingOptions(result, on) {
    var node = asQuery(result.root);
    
    var separator = document.getElementById("placesBC_grouping:separator");
    var groupOff = document.getElementById("placesBC_grouping:off");
    var groupOn = document.getElementById("placesBC_grouping:on");
    separator.removeAttribute("hidden");
    groupOff.removeAttribute("hidden");
    groupOn.removeAttribute("hidden");
    
    // Walk the list of registered annotationGroupers, determining what are 
    // available and notifying the broadcaster 
    var disabled = false;
    var query = node.getQueries({})[0];
    var config = null;
    for (var i = 0; i < this.annotationGroupers.length; ++i) {
      config = this.annotationGroupers[i];
      var substr = config.substr;
      if (query.annotation.substr(0, substr.length) == substr) {
        if (config.disabled)
          disabled = true;
        break;
      }
      config = null;
    }
    
    if (disabled || PlacesController.nodeIsFolder(node)) {
      // Generic Bookmarks Folder or custom container that disables grouping.
      separator.setAttribute("hidden", "true");
      groupOff.setAttribute("hidden", "true");
      groupOn.setAttribute("hidden", "true");
    }
    else {
      if (!config)
        config = this.defaultGrouper;
      groupOn.setAttribute("label", config.onLabel);
      groupOn.setAttribute("accesskey", config.onAccesskey);
      groupOn.setAttribute("oncommand", config.onOncommand);
      groupOff.setAttribute("label", config.offLabel);
      groupOff.setAttribute("accesskey", config.offAccesskey);
      groupOff.setAttribute("oncommand", config.offOncommand);
      // Update the checked state of the UI
      if (on) {
        groupOn.setAttribute("checked", "true");
        groupOff.removeAttribute("checked");
      }
      else {
        groupOff.setAttribute("checked", "true");
        groupOn.removeAttribute("checked");
      }
    }
  }
};

#include ../../../../toolkit/content/debug.js

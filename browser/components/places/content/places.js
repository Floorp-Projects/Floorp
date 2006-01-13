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
const INDEX_HISTORY = 2;
const INDEX_BOOKMARKS = 4;

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
              "Browser:ShowPlaces", "View:PageInfo", "cmd_toggleTaskbar"],
  
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
    (tabURI == this._placesURI) ? this._showPlacesUI() : this._hidePlacesUI();
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

  init: function PP_init() {
    // Attach the Command Controller to the Places Views. 
    this._places = document.getElementById("placesList");
    this._content = document.getElementById("placeContent");  
    this._places.controllers.appendController(PlacesController);
    this._content.controllers.appendController(PlacesController);
    
    this._places.init(new ViewConfig([TYPE_X_MOZ_PLACE_CONTAINER],
                                     ViewConfig.GENERIC_DROP_TYPES,
                                     Ci.nsINavHistoryQuery.INCLUDE_QUERIES,
                                     ViewConfig.GENERIC_FILTER_OPTIONS, 4));
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
  },

  uninit: function PP_uninit() {
    PlacesUIHook.uninit();
  },

  showAdvancedOptions: function PP_showAdvancedOptions() {
    alert("Show advanced query builder.");
  },
  
  setFilterCollection: function PP_setFilterCollection(collectionName) {
    var searchFilter = document.getElementById("searchFilter");
    searchFilter.setAttribute("collection", collectionName);
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

  applyFilter: function PP_applyFilter(filterString) {
    var searchFilter = document.getElementById("searchFilter");
    var collectionName = searchFilter.getAttribute("collection");
    switch (collectionName) {
    case "collection":
      var folder = this._content.getResult().folderId;
      this._content.applyFilter(filterString, true, folder);
      break;
    case "bookmarks":
      this._content.applyFilter(filterString, true, 0);
      break;
    case "history":
      this._content.applyFilter(filterString, false, 0);
      break;
    case "all":
      this._content.filterString = filterString;
      break;
    }
  },
  
  /**
   * Fill the header with information about what view is being displayed.
   */
  _setHeader: function(isSearch, text) {
    var bundle = document.getElementById("placeBundle");
    var key = isSearch ? "headerTextResultsFor" : "headerTextShowing";
    var title = bundle.getFormattedString(key, [text]);
    
    var titlebarText = document.getElementById("titlebartext");
    titlebarText.setAttribute("value", title);
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

    this._setHeader(false, node.title);
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
    //var filterCollectionDeck = document.getElementById("filterCollectionDeck");
    //filterCollectionDeck.selectedPanel = document.getElementById(filterButtonID);

    // Hide the Calendar for Bookmark queries. 
    document.getElementById("historyCalendar").setAttribute("hidden", isBookmarks);
    
    // Update the calendar with the current date range, if applicable. 
    if (!isBookmarks)
      this._updateCalendar();
  },
};


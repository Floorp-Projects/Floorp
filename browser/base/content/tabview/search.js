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
 * The Original Code is search.js.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Aza Raskin <aza@mozilla.com>
 * Raymond Lee <raymond@raysquare.com>
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

/* ******************************
 *
 * This file incorporates work from:
 * Quicksilver Score (qs_score):
 * http://rails-oceania.googlecode.com/svn/lachiecox/qs_score/trunk/qs_score.js
 * This incorporated work is covered by the following copyright and
 * permission notice:
 * Copyright 2008 Lachie Cox
 * Licensed under the MIT license.
 * http://jquery.org/license
 *
 *  ***************************** */

// **********
// Title: search.js
// Implementation for the search functionality of Firefox Panorama.

// ----------
// Function: scorePatternMatch
// Given a pattern string, returns a score between 0 and 1 of how well
// that pattern matches the original string. It mimics the heuristics
// of the Mac application launcher Quicksilver.
function scorePatternMatch(pattern, matched, offset) {
  offset = offset || 0;
  pattern = pattern.toLowerCase();
  matched = matched.toLowerCase();
 
  if (pattern.length == 0) return 0.9;
  if (pattern.length > matched.length) return 0.0;

  for (var i = pattern.length; i > 0; i--) {
    var sub_pattern = pattern.substring(0,i);
    var index = matched.indexOf(sub_pattern);

    if (index < 0) continue;
    if (index + pattern.length > matched.length + offset) continue;

    var next_string = matched.substring(index+sub_pattern.length);
    var next_pattern = null;

    if (i >= pattern.length)
      next_pattern = '';
    else
      next_pattern = pattern.substring(i);
 
    var remaining_score = 
      scorePatternMatch(next_pattern, next_string, offset + index);
 
    if (remaining_score > 0) {
      var score = matched.length-next_string.length;

      if (index != 0) {
        var j = 0;

        var c = matched.charCodeAt(index-1);
        if (c == 32 || c == 9) {
          for (var j = (index - 2); j >= 0; j--) {
            c = matched.charCodeAt(j);
            score -= ((c == 32 || c == 9) ? 1 : 0.15);
          }
        } else {
          score -= index;
        }
      }
   
      score += remaining_score * next_string.length;
      score /= matched.length;
      return score;
    }
  }
  return 0.0;  
}

// ##########
// Class: TabUtils
// 
// A collection of helper functions for dealing with both
// <TabItem>s and <xul:tab>s without having to worry which
// one is which.
var TabUtils = {
  // ---------
  // Function: _nameOfTab
  // Given a <TabItem> or a <xul:tab> returns the tab's name.
  nameOf: function TabUtils_nameOfTab(tab) {
    // We can have two types of tabs: A <TabItem> or a <xul:tab>
    // because we have to deal with both tabs represented inside
    // of active Panoramas as well as for windows in which
    // Panorama has yet to be activated. We uses object sniffing to
    // determine the type of tab and then returns its name.     
    return tab.label != undefined ? tab.label : tab.nameEl.innerHTML;
  },
  
  // ---------
  // Function: URLOf
  // Given a <TabItem> or a <xul:tab> returns the URL of tab
  URLOf: function TabUtils_URLOf(tab) {
    // Convert a <TabItem> to <xul:tab>
    if(tab.tab != undefined)
      tab = tab.tab;
    return tab.linkedBrowser.currentURI.spec;
  },

  // ---------
  // Function: favURLOf
  // Given a <TabItem> or a <xul:tab> returns the URL of tab's favicon.
  faviconURLOf: function TabUtils_faviconURLOf(tab) {
    return tab.image != undefined ? tab.image : tab.favImgEl.src;
  },
  
  // ---------
  // Function: focus
  // Given a <TabItem> or a <xul:tab>, focuses it and it's window.
  focus: function TabUtils_focus(tab) {
    // Convert a <TabItem> to a <xul:tab>
    if (tab.tab != undefined) tab = tab.tab;
    tab.ownerDocument.defaultView.gBrowser.selectedTab = tab;
    tab.ownerDocument.defaultView.focus();    
  }
};

// ##########
// Class: TabMatcher
// 
// A singleton class that allows you to iterate over
// matching and not-matching tabs, given a case-insensitive
// search term.
function TabMatcher(term) { 
  this.term = term; 
}

TabMatcher.prototype = {  
  // ---------
  // Function: _filterAndSortMatches
  // Given an array of <TabItem>s and <xul:tab>s returns a new array
  // of tabs whose name matched the search term, sorted by lexical
  // closeness.  
  _filterAndSortForMatches: function TabMatcher__filterAndSortForMatches(tabs) {
    var self = this;
    tabs = tabs.filter(function(tab){
      let name = TabUtils.nameOf(tab);
      let url = TabUtils.URLOf(tab);
      return name.match(self.term, "i") || url.match(self.term, "i");
    });

    tabs.sort(function sorter(x, y){
      var yScore = scorePatternMatch(self.term, TabUtils.nameOf(y));
      var xScore = scorePatternMatch(self.term, TabUtils.nameOf(x));
      return yScore - xScore; 
    });
    
    return tabs;
  },
  
  // ---------
  // Function: _filterForUnmatches
  // Given an array of <TabItem>s returns an unsorted array of tabs whose name
  // does not match the the search term.
  _filterForUnmatches: function TabMatcher__filterForUnmatches(tabs) {
    var self = this;
    return tabs.filter(function(tab) {
      var name = tab.nameEl.innerHTML;
      let url = TabUtils.URLOf(tab);
      return !name.match(self.term, "i") && !url.match(self.term, "i");
    });
  },
  
  // ---------
  // Function: _getTabsForOtherWindows
  // Returns an array of <TabItem>s and <xul:tabs>s representing that
  // tabs from all windows but the currently focused window. <TabItem>s
  // will be returned for windows in which Panorama has been activated at
  // least once, while <xul:tab>s will be return for windows in which
  // Panorama has never been activated.
  _getTabsForOtherWindows: function TabMatcher__getTabsForOtherWindows(){
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    var enumerator = wm.getEnumerator("navigator:browser");    
    var currentWindow = wm.getMostRecentWindow("navigator:browser");
    
    var allTabs = [];    
    while (enumerator.hasMoreElements()) {
      var win = enumerator.getNext();
      // This function gets tabs from other windows: not the one you currently
      // have focused.
      if (win != currentWindow) {
        // If TabView is around iterate over all tabs, else get the currently
        // shown tabs...
        
        tvWindow = win.TabView.getContentWindow();
        if (tvWindow)
          allTabs = allTabs.concat( tvWindow.TabItems.getItems() );
        else
          // win.gBrowser.tabs isn't a proper array, so we can't use concat
          for (var i=0; i<win.gBrowser.tabs.length; i++) allTabs.push( win.gBrowser.tabs[i] );
      } 
    }
    return allTabs;    
  },
  
  // ----------
  // Function: matchedTabsFromOtherWindows
  // Returns an array of <TabItem>s and <xul:tab>s that match the search term
  // from all windows but the currently focused window. <TabItem>s will be
  // returned for windows in which Panorama has been activated at least once,
  // while <xul:tab>s will be return for windows in which Panorama has never
  // been activated.
  // (new TabMatcher("app")).matchedTabsFromOtherWindows();
  matchedTabsFromOtherWindows: function TabMatcher_matchedTabsFromOtherWindows(){
    if (this.term.length < 2)
      return [];
    
    var tabs = this._getTabsForOtherWindows();
    tabs = this._filterAndSortForMatches(tabs);
    return tabs;
  },
  
  // ----------
  // Function: matched
  // Returns an array of <TabItem>s which match the current search term.
  // If the term is less than 2 characters in length, it returns
  // nothing.
  matched: function TabMatcher_matched() {
    if (this.term.length < 2)
      return [];
      
    var tabs = TabItems.getItems();
    tabs = this._filterAndSortForMatches(tabs);
    return tabs;    
  },
  
  // ----------
  // Function: unmatched
  // Returns all of <TabItem>s that .matched() doesn't return.
  unmatched: function TabMatcher_unmatched() {
    var tabs = TabItems.getItems();
    if (this.term.length < 2)
      return tabs;
      
    return this._filterForUnmatches(tabs);
  },

  // ----------
  // Function: doSearch
  // Performs the search. Lets you provide three functions.
  // The first is on all matched tabs in the window, the second on all unmatched
  // tabs in the window, and the third on all matched tabs in other windows.
  // The first two functions take two parameters: A <TabItem> and its integer index
  // indicating the absolute rank of the <TabItem> in terms of match to
  // the search term. The last function also takes two paramaters, but can be
  // passed both <TabItem>s and <xul:tab>s and the index is offset by the
  // number of matched tabs inside the window.
  doSearch: function TabMatcher_doSearch(matchFunc, unmatchFunc, otherFunc) {
    var matches = this.matched();
    var unmatched = this.unmatched();
    var otherMatches = this.matchedTabsFromOtherWindows();
    
    matches.forEach(function(tab, i) {
      matchFunc(tab, i);
    });

    otherMatches.forEach(function(tab,i) {
      otherFunc(tab, i+matches.length);      
    });
    
    unmatched.forEach(function(tab, i) {
      unmatchFunc(tab, i);
    });    
  }
};

// ##########
// Class: SearchEventHandlerClass
// 
// A singleton class that handles all of the
// event handlers.
function SearchEventHandlerClass() { 
  this.init(); 
}

SearchEventHandlerClass.prototype = {
  // ----------
  // Function: init
  // Initializes the searchbox to be focused, and everything
  // else to be hidden, and to have everything have the appropriate
  // event handlers;
  init: function () {
    var self = this;
    iQ("#searchbox")[0].focus(); 
    iQ("#search").hide().click(function(event) {
      if ( event.target.id != "searchbox")
        hideSearch();
    });
    
    iQ("#searchbox").keyup(function() {
      performSearch();
    });
    
    iQ("#searchbutton").mousedown(function() {
      ensureSearchShown(null);
      self.switchToInMode();      
    });
    
    this.currentHandler = null;
    this.switchToBeforeMode();
  },
  
  // ----------
  // Function: beforeSearchKeyHandler
  // Handles all keypresses before the search interface is brought up.
  beforeSearchKeyHandler: function (event) {
    // Only match reasonable text-like characters for quick search.
    // TODO: Also include funky chars. Bug 593904
    if (!String.fromCharCode(event.which).match(/[a-zA-Z0-9]/) || event.altKey || 
        event.ctrlKey || event.metaKey)
      return;

    // If we are already in an input field, allow typing as normal.
    if (event.target.nodeName == "INPUT")
      return;

    this.switchToInMode();
    ensureSearchShown(event);
  },

  // ----------
  // Function: inSearchKeyHandler
  // Handles all keypresses while search mode.
  inSearchKeyHandler: function (event) {
    if ((event.keyCode == event.DOM_VK_ESCAPE) || 
        (event.keyCode == event.DOM_VK_BACK_SPACE && term.length <= 1)) {
      hideSearch(event);
      return;
    }

    let matcher = createSearchTabMacher();
    let matches = matcher.matched();
    let others =  matcher.matchedTabsFromOtherWindows();
    if ((event.keyCode == event.DOM_VK_RETURN || 
         event.keyCode == event.DOM_VK_ENTER) && 
         (matches.length > 0 || others.length > 0)) {
      hideSearch(event);
      if (matches.length > 0) 
        matches[0].zoomIn();
      else
        TabUtils.focus(others[0]);
    }
  },

  // ----------
  // Function: switchToBeforeMode
  // Make sure the event handlers are appropriate for
  // the before-search mode. 
  switchToBeforeMode: function switchToBeforeMode() {
    let self = this;
    if (this.currentHandler)
      iQ(window).unbind("keypress", this.currentHandler);
    this.currentHandler = function(event) self.beforeSearchKeyHandler(event);
    iQ(window).keypress(this.currentHandler);
  },
  
  // ----------
  // Function: switchToInMode
  // Make sure the event handlers are appropriate for
  // the in-search mode.   
  switchToInMode: function switchToInMode() {
    let self = this;
    if (this.currentHandler)
      iQ(window).unbind("keypress", this.currentHandler);
    this.currentHandler = function(event) self.inSearchKeyHandler(event);
    iQ(window).keypress(this.currentHandler);
  }
};

var TabHandlers = {
  onMatch: function(tab, index){
    tab.addClass("onTop");
    index != 0 ? tab.addClass("notMainMatch") : tab.removeClass("notMainMatch");

    // Remove any existing handlers before adding the new ones.
    // If we don't do this, then we may add more handlers than
    // we remove.
    iQ(tab.canvasEl)
    .unbind("mousedown", TabHandlers._hideHandler)
    .unbind("mouseup", TabHandlers._showHandler);

    iQ(tab.canvasEl)
    .mousedown(TabHandlers._hideHandler)
    .mouseup(TabHandlers._showHandler);
  },
  
  onUnmatch: function(tab, index){
    iQ(tab.container).removeClass("onTop");
    tab.removeClass("notMainMatch");

    iQ(tab.canvasEl)
     .unbind("mousedown", TabHandlers._hideHandler)
     .unbind("mouseup", TabHandlers._showHandler);
  },
  
  onOther: function(tab, index){
    // Unlike the other on* functions, in this function tab can
    // either be a <TabItem> or a <xul:tab>. In other functions
    // it is always a <TabItem>. Also note that index is offset
    // by the number of matches within the window.
    let item = iQ("<div/>")
      .addClass("inlineMatch")
      .click(function(event){
        hideSearch(event);
        TabUtils.focus(tab);
      });
    
    iQ("<img/>")
      .attr("src", TabUtils.faviconURLOf(tab) )
      .appendTo(item);
    
    iQ("<span/>")
      .text( TabUtils.nameOf(tab) )
      .appendTo(item);
      
    index != 0 ? item.addClass("notMainMatch") : item.removeClass("notMainMatch");      
    item.appendTo("#results");
    iQ("#otherresults").show();    
  },
  
  _hideHandler: function(event){
    iQ("#search").fadeOut();
    TabHandlers._mouseDownLocation = {x:event.clientX, y:event.clientY};
  },
  
  _showHandler: function(event){
    // If the user clicks on a tab without moving the mouse then
    // they are zooming into the tab and we need to exit search
    // mode.
    if (TabHandlers._mouseDownLocation.x == event.clientX &&
        TabHandlers._mouseDownLocation.y == event.clientY){
      hideSearch();
      return;
    }

    iQ("#search").show();
    iQ("#searchbox")[0].focus();
    // Marshal the search.
    setTimeout(performSearch, 0);
  },
  
  _mouseDownLocation: null
};

function createSearchTabMacher() {
  return new TabMatcher(iQ("#searchbox").val());
}

function hideSearch(event){
  iQ("#searchbox").val("");
  iQ("#search").hide();

  iQ("#searchbutton").css({ opacity:.8 });

  let mainWindow = gWindow.document.getElementById("main-window");
  mainWindow.setAttribute("activetitlebarcolor", "#C4C4C4");

  performSearch();
  SearchEventHandler.switchToBeforeMode();

  if (event){
    event.preventDefault();
    event.stopPropagation();
  }

  // Return focus to the tab window
  UI.blurAll();
  gTabViewFrame.contentWindow.focus();

  let newEvent = document.createEvent("Events");
  newEvent.initEvent("tabviewsearchdisabled", false, false);
  dispatchEvent(newEvent);
}

function performSearch() {
  let matcher = new TabMatcher(iQ("#searchbox").val());

  // Remove any previous other-window search results and
  // hide the display area.
  iQ("#results").empty();
  iQ("#otherresults").hide();
  iQ("#otherresults>.label").text(tabviewString("search.otherWindowTabs"));

  matcher.doSearch(TabHandlers.onMatch, TabHandlers.onUnmatch, TabHandlers.onOther);
}

function ensureSearchShown(event){
  var $search = iQ("#search");
  var $searchbox = iQ("#searchbox");
  iQ("#searchbutton").css({ opacity: 1 });

  if (!isSearchEnabled()) {
    $search.show();
    var mainWindow = gWindow.document.getElementById("main-window");
    mainWindow.setAttribute("activetitlebarcolor", "#717171");       

    // Marshal the focusing, otherwise you end up with
    // a race condition where only sometimes would the
    // first keystroke be registered by the search box.
    // When you marshal it never gets registered, so we
    // manually 
    setTimeout(function focusSearch() {
      $searchbox[0].focus();
      $searchbox[0].val = '0';
      $searchbox.css({"z-index":"1015"});
      if (event != null)
        $searchbox.val(String.fromCharCode(event.charCode));        

      let newEvent = document.createEvent("Events");
      newEvent.initEvent("tabviewsearchenabled", false, false);
      dispatchEvent(newEvent);
    }, 0);
  }
}

function isSearchEnabled() {
  return iQ("#search").css("display") != "none";
}

var SearchEventHandler = new SearchEventHandlerClass();

// Features to add:
// (1) Make sure this looks good on Windows. Bug 594429
// (2) Make sure that we don't put the matched tab over the search box. Bug 594433
// (3) Group all of the highlighted tabs into a group? Bug 594434

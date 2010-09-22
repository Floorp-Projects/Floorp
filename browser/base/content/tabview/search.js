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
// Class: TabMatcher
// 
// A singleton class that allows you to iterate over
// matching and not-matching tabs, given a case-insensitive
// search term.
function TabMatcher(term) { 
  this.term = term; 
}

TabMatcher.prototype = {
  // ----------
  // Function: matched
  // Returns an array of <TabItem>s which match the current search term.
  // If the term is less than 2 characters in length, it returns
  // nothing.
  matched: function matched() {
    var self = this;
    if (this.term.length < 2)
      return [];
    
    var tabs = TabItems.getItems();
    tabs = tabs.filter(function(tab){
      var name = tab.nameEl.innerHTML;      
      return name.match(self.term, "i");
    });
    
    tabs.sort(function sorter(x, y){
      var yScore = scorePatternMatch(self.term, y.nameEl.innerHTML);
      var xScore = scorePatternMatch(self.term, x.nameEl.innerHTML);
      return yScore - xScore; 
    });
     
    return tabs;    
  },
  
  // ----------
  // Function: unmatched
  // Returns all of <TabItem>s that .matched() doesn't return.
  unmatched: function unmatched() {
    var self = this;    
    var tabs = TabItems.getItems();
    
    if ( this.term.length < 2 )
      return tabs;
    
    return tabs.filter(function(tab) {
      var name = tab.nameEl.innerHTML;
      return !name.match(self.term, "i");
    });
  },

  // ----------
  // Function: doSearch
  // Performs the search. Lets you provide two functions, one that is called
  // on all matched tabs, and one that is called on all unmatched tabs.
  // Both functions take two parameters: A <TabItem> and its integer index
  // indicating the absolute rank of the <TabItem> in terms of match to
  // the search term. 
  doSearch: function(matchFunc, unmatchFunc) {
    var matches = this.matched();
    var unmatched = this.unmatched();
    
    matches.forEach(function(tab, i) {
      matchFunc(tab, i);
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
    var key = String.fromCharCode(event.which);
    // TODO: Also include funky chars. Bug 593904
    if (!key.match(/[A-Z0-9]/) || event.altKey || event.ctrlKey || event.metaKey)
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
    var term = iQ("#searchbox").val();
    
    if (event.which == event.DOM_VK_ESCAPE) 
      hideSearch(event);
    if (event.which == event.DOM_VK_BACK_SPACE && term.length <= 1) 
      hideSearch(event);

    var matches = (new TabMatcher(term)).matched();
    if (event.which == event.DOM_VK_RETURN && matches.length > 0) {
      matches[0].zoomIn();
      hideSearch(event);    
    }
  },

  // ----------
  // Function: switchToBeforeMode
  // Make sure the event handlers are appropriate for
  // the before-search mode. 
  switchToBeforeMode: function switchToBeforeMode() {
    var self = this;
    if (this.currentHandler)
      iQ(document).unbind("keydown", this.currentHandler);
    this.currentHandler = function(event) self.beforeSearchKeyHandler(event);
    iQ(document).keydown(self.currentHandler);
  },
  
  // ----------
  // Function: switchToInMode
  // Make sure the event handlers are appropriate for
  // the in-search mode.   
  switchToInMode: function switchToInMode() {
    var self = this;
    if (this.currentHandler)
      iQ(document).unbind("keydown", this.currentHandler);
    this.currentHandler = function(event) self.inSearchKeyHandler(event);
    iQ(document).keydown(self.currentHandler);
  }
};

var TabHandlers = {
  onMatch: function(tab, index){
   tab.setZ(1010);   
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
    // TODO: Set back as value per tab. bug 593902
    tab.setZ(500);
    tab.removeClass("notMainMatch");

    iQ(tab.canvasEl)
     .unbind("mousedown", TabHandlers._hideHandler)
     .unbind("mouseup", TabHandlers._showHandler);
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

function hideSearch(event){
  iQ("#searchbox").val("");
  iQ("#search").hide();
  
  iQ("#searchbutton").css({ opacity:.8 });
  
  var mainWindow = gWindow.document.getElementById("main-window");    
  mainWindow.setAttribute("activetitlebarcolor", "#C4C4C4");

  performSearch();
  SearchEventHandler.switchToBeforeMode();

  if (event){
    event.preventDefault();
    event.stopPropagation();
  }

  let newEvent = document.createEvent("Events");
  newEvent.initEvent("tabviewsearchdisabled", false, false);
  dispatchEvent(newEvent);

  // Return focus to the tab window
  UI.blurAll();
  gTabViewFrame.contentWindow.focus();
}

function performSearch() {
  var matcher = new TabMatcher(iQ("#searchbox").val());
  matcher.doSearch(TabHandlers.onMatch, TabHandlers.onUnmatch);
}

function ensureSearchShown(event){
  var $search = iQ("#search");
  var $searchbox = iQ("#searchbox");
  iQ("#searchbutton").css({ opacity: 1 });
  
  
  if ($search.css("display") == "none") {
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
      if (event != null){
        var keyCode = event.which + (event.shiftKey ? 0 : 32);
        $searchbox.val(String.fromCharCode(keyCode));        
      }
    }, 0);

    let newEvent = document.createEvent("Events");
    newEvent.initEvent("tabviewsearchenabled", false, false);
    dispatchEvent(newEvent);
  }
}

var SearchEventHandler = new SearchEventHandlerClass();

// Features to add:
// (1) Make sure this looks good on Windows. Bug 594429
// (2) Make sure that we don't put the matched tab over the search box. Bug 594433
// (3) Group all of the highlighted tabs into a group? Bug 594434

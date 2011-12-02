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

// ##########
// Class: TabUtils
//
// A collection of helper functions for dealing with both <TabItem>s and
// <xul:tab>s without having to worry which one is which.
let TabUtils = {
  // ----------
  // Function: toString
  // Prints [TabUtils] for debug use.
  toString: function TabUtils_toString() {
    return "[TabUtils]";
  },

  // ---------
  // Function: nameOfTab
  // Given a <TabItem> or a <xul:tab> returns the tab's name.
  nameOf: function TabUtils_nameOf(tab) {
    // We can have two types of tabs: A <TabItem> or a <xul:tab>
    // because we have to deal with both tabs represented inside
    // of active Panoramas as well as for windows in which
    // Panorama has yet to be activated. We uses object sniffing to
    // determine the type of tab and then returns its name.     
    return tab.label != undefined ? tab.label : tab.$tabTitle[0].textContent;
  },

  // ---------
  // Function: URLOf
  // Given a <TabItem> or a <xul:tab> returns the URL of tab.
  URLOf: function TabUtils_URLOf(tab) {
    // Convert a <TabItem> to <xul:tab>
    if ("tab" in tab)
      tab = tab.tab;
    return tab.linkedBrowser.currentURI.spec;
  },

  // ---------
  // Function: faviconURLOf
  // Given a <TabItem> or a <xul:tab> returns the URL of tab's favicon.
  faviconURLOf: function TabUtils_faviconURLOf(tab) {
    return tab.image != undefined ? tab.image : tab.$favImage[0].src;
  },

  // ---------
  // Function: focus
  // Given a <TabItem> or a <xul:tab>, focuses it and it's window.
  focus: function TabUtils_focus(tab) {
    // Convert a <TabItem> to a <xul:tab>
    if ("tab" in tab)
      tab = tab.tab;
    tab.ownerDocument.defaultView.gBrowser.selectedTab = tab;
    tab.ownerDocument.defaultView.focus();
  }
};

// ##########
// Class: TabMatcher
//
// A class that allows you to iterate over matching and not-matching tabs, 
// given a case-insensitive search term.
function TabMatcher(term) {
  this.term = term;
}

TabMatcher.prototype = {
  // ----------
  // Function: toString
  // Prints [TabMatcher (term)] for debug use.
  toString: function TabMatcher_toString() {
    return "[TabMatcher (" + this.term + ")]";
  },

  // ---------
  // Function: _filterAndSortForMatches
  // Given an array of <TabItem>s and <xul:tab>s returns a new array
  // of tabs whose name matched the search term, sorted by lexical
  // closeness.
  _filterAndSortForMatches: function TabMatcher__filterAndSortForMatches(tabs) {
    let self = this;
    tabs = tabs.filter(function TabMatcher__filterAndSortForMatches_filter(tab) {
      let name = TabUtils.nameOf(tab);
      let url = TabUtils.URLOf(tab);
      return name.match(self.term, "i") || url.match(self.term, "i");
    });

    tabs.sort(function TabMatcher__filterAndSortForMatches_sort(x, y) {
      let yScore = self._scorePatternMatch(self.term, TabUtils.nameOf(y));
      let xScore = self._scorePatternMatch(self.term, TabUtils.nameOf(x));
      return yScore - xScore;
    });

    return tabs;
  },

  // ---------
  // Function: _filterForUnmatches
  // Given an array of <TabItem>s returns an unsorted array of tabs whose name
  // does not match the the search term.
  _filterForUnmatches: function TabMatcher__filterForUnmatches(tabs) {
    let self = this;
    return tabs.filter(function TabMatcher__filterForUnmatches_filter(tab) {
      let name = tab.$tabTitle[0].textContent;
      let url = TabUtils.URLOf(tab);
      return !name.match(self.term, "i") && !url.match(self.term, "i");
    });
  },

  // ---------
  // Function: _getTabsForOtherWindows
  // Returns an array of <TabItem>s and <xul:tabs>s representing tabs
  // from all windows but the current window. <TabItem>s will be returned
  // for windows in which Panorama has been activated at least once, while
  // <xul:tab>s will be returned for windows in which Panorama has never
  // been activated.
  _getTabsForOtherWindows: function TabMatcher__getTabsForOtherWindows() {
    let enumerator = Services.wm.getEnumerator("navigator:browser");
    let allTabs = [];

    while (enumerator.hasMoreElements()) {
      let win = enumerator.getNext();
      // This function gets tabs from other windows, not from the current window
      if (win != gWindow)
        allTabs.push.apply(allTabs, win.gBrowser.tabs);
    }
    return allTabs;
  },

  // ----------
  // Function: matchedTabsFromOtherWindows
  // Returns an array of <TabItem>s and <xul:tab>s that match the search term
  // from all windows but the current window. <TabItem>s will be returned for
  // windows in which Panorama has been activated at least once, while
  // <xul:tab>s will be returned for windows in which Panorama has never
  // been activated.
  // (new TabMatcher("app")).matchedTabsFromOtherWindows();
  matchedTabsFromOtherWindows: function TabMatcher_matchedTabsFromOtherWindows() {
    if (this.term.length < 2)
      return [];

    let tabs = this._getTabsForOtherWindows();
    return this._filterAndSortForMatches(tabs);
  },

  // ----------
  // Function: matched
  // Returns an array of <TabItem>s which match the current search term.
  // If the term is less than 2 characters in length, it returns nothing.
  matched: function TabMatcher_matched() {
    if (this.term.length < 2)
      return [];

    let tabs = TabItems.getItems();
    return this._filterAndSortForMatches(tabs);
  },

  // ----------
  // Function: unmatched
  // Returns all of <TabItem>s that .matched() doesn't return.
  unmatched: function TabMatcher_unmatched() {
    let tabs = TabItems.getItems();
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
    let matches = this.matched();
    let unmatched = this.unmatched();
    let otherMatches = this.matchedTabsFromOtherWindows();
    
    matches.forEach(function(tab, i) {
      matchFunc(tab, i);
    });

    otherMatches.forEach(function(tab,i) {
      otherFunc(tab, i+matches.length);
    });

    unmatched.forEach(function(tab, i) {
      unmatchFunc(tab, i);
    });
  },

  // ----------
  // Function: _scorePatternMatch
  // Given a pattern string, returns a score between 0 and 1 of how well
  // that pattern matches the original string. It mimics the heuristics
  // of the Mac application launcher Quicksilver.
  _scorePatternMatch: function TabMatcher__scorePatternMatch(pattern, matched, offset) {
    offset = offset || 0;
    pattern = pattern.toLowerCase();
    matched = matched.toLowerCase();

    if (pattern.length == 0)
      return 0.9;
    if (pattern.length > matched.length)
      return 0.0;

    for (let i = pattern.length; i > 0; i--) {
      let sub_pattern = pattern.substring(0,i);
      let index = matched.indexOf(sub_pattern);

      if (index < 0)
        continue;
      if (index + pattern.length > matched.length + offset)
        continue;

      let next_string = matched.substring(index+sub_pattern.length);
      let next_pattern = null;

      if (i >= pattern.length)
        next_pattern = '';
      else
        next_pattern = pattern.substring(i);

      let remaining_score = this._scorePatternMatch(next_pattern, next_string, offset + index);

      if (remaining_score > 0) {
        let score = matched.length-next_string.length;

        if (index != 0) {
          let c = matched.charCodeAt(index-1);
          if (c == 32 || c == 9) {
            for (let j = (index - 2); j >= 0; j--) {
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
};

// ##########
// Class: TabHandlers
// 
// A object that handles all of the event handlers.
let TabHandlers = {
  _mouseDownLocation: null,

  // ---------
  // Function: onMatch
  // Adds styles and event listeners to the matched tab items.
  onMatch: function TabHandlers_onMatch(tab, index) {
    tab.addClass("onTop");
    index != 0 ? tab.addClass("notMainMatch") : tab.removeClass("notMainMatch");

    // Remove any existing handlers before adding the new ones.
    // If we don't do this, then we may add more handlers than
    // we remove.
    tab.$canvas
      .unbind("mousedown", TabHandlers._hideHandler)
      .unbind("mouseup", TabHandlers._showHandler);

    tab.$canvas
      .mousedown(TabHandlers._hideHandler)
      .mouseup(TabHandlers._showHandler);
  },

  // ---------
  // Function: onUnmatch
  // Removes styles and event listeners from the unmatched tab items.
  onUnmatch: function TabHandlers_onUnmatch(tab, index) {
    tab.$container.removeClass("onTop");
    tab.removeClass("notMainMatch");

    tab.$canvas
      .unbind("mousedown", TabHandlers._hideHandler)
      .unbind("mouseup", TabHandlers._showHandler);
  },

  // ---------
  // Function: onOther
  // Removes styles and event listeners from the unmatched tabs.
  onOther: function TabHandlers_onOther(tab, index) {
    // Unlike the other on* functions, in this function tab can
    // either be a <TabItem> or a <xul:tab>. In other functions
    // it is always a <TabItem>. Also note that index is offset
    // by the number of matches within the window.
    let item = iQ("<div/>")
      .addClass("inlineMatch")
      .click(function TabHandlers_onOther_click(event) {
        Search.hide(event);
        TabUtils.focus(tab);
      });

    iQ("<img/>")
      .attr("src", TabUtils.faviconURLOf(tab))
      .appendTo(item);

    iQ("<span/>")
      .text(TabUtils.nameOf(tab))
      .appendTo(item);

    index != 0 ? item.addClass("notMainMatch") : item.removeClass("notMainMatch");
    item.appendTo("#results");
    iQ("#otherresults").show();
  },

  // ---------
  // Function: _hideHandler
  // Performs when mouse down on a canvas of tab item.
  _hideHandler: function TabHandlers_hideHandler(event) {
    iQ("#search").fadeOut();
    iQ("#searchshade").fadeOut();
    TabHandlers._mouseDownLocation = {x:event.clientX, y:event.clientY};
  },

  // ---------
  // Function: _showHandler
  // Performs when mouse up on a canvas of tab item.
  _showHandler: function TabHandlers_showHandler(event) {
    // If the user clicks on a tab without moving the mouse then
    // they are zooming into the tab and we need to exit search
    // mode.
    if (TabHandlers._mouseDownLocation.x == event.clientX &&
        TabHandlers._mouseDownLocation.y == event.clientY) {
      Search.hide();
      return;
    }

    iQ("#searchshade").show();
    iQ("#search").show();
    iQ("#searchbox")[0].focus();
    // Marshal the search.
    setTimeout(Search.perform, 0);
  }
};

// ##########
// Class: Search
// 
// A object that handles the search feature.
let Search = {
  _initiatedBy: "",
  _blockClick: false,
  _currentHandler: null,

  // ----------
  // Function: toString
  // Prints [Search] for debug use.
  toString: function Search_toString() {
    return "[Search]";
  },

  // ----------
  // Function: init
  // Initializes the searchbox to be focused, and everything else to be hidden,
  // and to have everything have the appropriate event handlers.
  init: function Search_init() {
    let self = this;

    iQ("#search").hide();
    iQ("#searchshade").hide().mousedown(function Search_init_shade_mousedown(event) {
      if (event.target.id != "searchbox" && !self._blockClick)
        self.hide();
    });

    iQ("#searchbox").keyup(function Search_init_box_keyup() {
      self.perform();
    })
    .attr("title", tabviewString("button.searchTabs"));

    iQ("#searchbutton").mousedown(function Search_init_button_mousedown() {
      self._initiatedBy = "buttonclick";
      self.ensureShown();
      self.switchToInMode();
    })
    .attr("title", tabviewString("button.searchTabs"));

    window.addEventListener("focus", function Search_init_window_focus() {
      if (self.isEnabled()) {
        self._blockClick = true;
        setTimeout(function() {
          self._blockClick = false;
        }, 0);
      }
    }, false);

    this.switchToBeforeMode();
  },

  // ----------
  // Function: _beforeSearchKeyHandler
  // Handles all keydown before the search interface is brought up.
  _beforeSearchKeyHandler: function Search__beforeSearchKeyHandler(event) {
    // Only match reasonable text-like characters for quick search.
    if (event.altKey || event.ctrlKey || event.metaKey)
      return;

    if ((event.keyCode > 0 && event.keyCode <= event.DOM_VK_DELETE) ||
        event.keyCode == event.DOM_VK_CONTEXT_MENU ||
        event.keyCode == event.DOM_VK_SLEEP ||
        (event.keyCode >= event.DOM_VK_F1 &&
         event.keyCode <= event.DOM_VK_SCROLL_LOCK) ||
        event.keyCode == event.DOM_VK_META ||
        event.keyCode == 91 || // 91 = left windows key
        event.keyCode == 92 || // 92 = right windows key
        (!event.keyCode && !event.charCode)) {
      return;
    }

    // If we are already in an input field, allow typing as normal.
    if (event.target.nodeName == "INPUT")
      return;

    // / is used to activate the search feature so the key shouldn't be entered 
    // into the search box.
    if (event.keyCode == KeyEvent.DOM_VK_SLASH) {
      event.stopPropagation();
      event.preventDefault();
    }

    this.switchToInMode();
    this._initiatedBy = "keydown";
    this.ensureShown(true);
  },

  // ----------
  // Function: _inSearchKeyHandler
  // Handles all keydown while search mode.
  _inSearchKeyHandler: function Search__inSearchKeyHandler(event) {
    let term = iQ("#searchbox").val();
    if ((event.keyCode == event.DOM_VK_ESCAPE) ||
        (event.keyCode == event.DOM_VK_BACK_SPACE && term.length <= 1 &&
         this._initiatedBy == "keydown")) {
      this.hide(event);
      return;
    }

    let matcher = this.createSearchTabMatcher();
    let matches = matcher.matched();
    let others =  matcher.matchedTabsFromOtherWindows();
    if ((event.keyCode == event.DOM_VK_RETURN ||
         event.keyCode == event.DOM_VK_ENTER) &&
         (matches.length > 0 || others.length > 0)) {
      this.hide(event);
      if (matches.length > 0) 
        matches[0].zoomIn();
      else
        TabUtils.focus(others[0]);
    }
  },

  // ----------
  // Function: switchToBeforeMode
  // Make sure the event handlers are appropriate for the before-search mode.
  switchToBeforeMode: function Search_switchToBeforeMode() {
    let self = this;
    if (this._currentHandler)
      iQ(window).unbind("keydown", this._currentHandler);
    this._currentHandler = function Search_switchToBeforeMode_handler(event) {
      self._beforeSearchKeyHandler(event);
    }
    iQ(window).keydown(this._currentHandler);
  },

  // ----------
  // Function: switchToInMode
  // Make sure the event handlers are appropriate for the in-search mode.
  switchToInMode: function Search_switchToInMode() {
    let self = this;
    if (this._currentHandler)
      iQ(window).unbind("keydown", this._currentHandler);
    this._currentHandler = function Search_switchToInMode_handler(event) {
      self._inSearchKeyHandler(event);
    }
    iQ(window).keydown(this._currentHandler);
  },

  createSearchTabMatcher: function Search_createSearchTabMatcher() {
    return new TabMatcher(iQ("#searchbox").val());
  },

  // ----------
  // Function: isEnabled
  // Checks whether search mode is enabled or not.
  isEnabled: function Search_isEnabled() {
    return iQ("#search").css("display") != "none";
  },

  // ----------
  // Function: hide
  // Hides search mode.
  hide: function Search_hide(event) {
    if (!this.isEnabled())
      return;

    iQ("#searchbox").val("");
    iQ("#searchshade").hide();
    iQ("#search").hide();

    iQ("#searchbutton").css({ opacity:.8 });

#ifdef XP_MACOSX
    UI.setTitlebarColors(true);
#endif

    this.perform();
    this.switchToBeforeMode();

    if (event) {
      // when hiding the search mode, we need to prevent the keypress handler
      // in UI__setTabViewFrameKeyHandlers to handle the key press again. e.g. Esc
      // which is already handled by the key down in this class.
      if (event.type == "keydown")
        UI.ignoreKeypressForSearch = true;
      event.preventDefault();
      event.stopPropagation();
    }

    // Return focus to the tab window
    UI.blurAll();
    gTabViewFrame.contentWindow.focus();

    let newEvent = document.createEvent("Events");
    newEvent.initEvent("tabviewsearchdisabled", false, false);
    dispatchEvent(newEvent);
  },

  // ----------
  // Function: perform
  // Performs a search.
  perform: function Search_perform() {
    let matcher =  this.createSearchTabMatcher();

    // Remove any previous other-window search results and
    // hide the display area.
    iQ("#results").empty();
    iQ("#otherresults").hide();
    iQ("#otherresults>.label").text(tabviewString("search.otherWindowTabs"));

    matcher.doSearch(TabHandlers.onMatch, TabHandlers.onUnmatch, TabHandlers.onOther);
  },

  // ----------
  // Function: ensureShown
  // Ensures the search feature is displayed.  If not, display it.
  // Parameters:
  //  - a boolean indicates whether this is triggered by a keypress or not
  ensureShown: function Search_ensureShown(activatedByKeypress) {
    let $search = iQ("#search");
    let $searchShade = iQ("#searchshade");
    let $searchbox = iQ("#searchbox");
    iQ("#searchbutton").css({ opacity: 1 });

    // NOTE: when this function is called by keydown handler, next keypress
    // event or composition events of IME will be fired on the focused editor.
    function dispatchTabViewSearchEnabledEvent() {
      let newEvent = document.createEvent("Events");
      newEvent.initEvent("tabviewsearchenabled", false, false);
      dispatchEvent(newEvent);
    };

    if (!this.isEnabled()) {
      $searchShade.show();
      $search.show();

#ifdef XP_MACOSX
      UI.setTitlebarColors({active: "#717171", inactive: "#EDEDED"});
#endif

      if (activatedByKeypress) {
        // set the focus so key strokes are entered into the textbox.
        $searchbox[0].focus();
        dispatchTabViewSearchEnabledEvent();
      } else {
        // marshal the focusing, otherwise it ends up with searchbox[0].focus gets
        // called before the search button gets the focus after being pressed.
        setTimeout(function setFocusAndDispatchSearchEnabledEvent() {
          $searchbox[0].focus();
          dispatchTabViewSearchEnabledEvent();
        }, 0);
      }
    }
  }
};


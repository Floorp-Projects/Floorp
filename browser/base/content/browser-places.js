/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Places Browser Integration
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
 *   Annie Sullivan <annie.sullivan@gmail.com>
 *   Joe Hughes <joe@retrovirus.com>
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

/**
 * A BookmarkAllTabs command for the BrowserController in browser.js
 */
function BookmarkAllTabsCommand() {
}
BookmarkAllTabsCommand.prototype = {
  /**
   * true if the command is enabled, false otherwise.
   */
  get enabled() {
    return getBrowser().tabContainer.childNodes.length > 1;
  },
  
  /**
   * Performs the command (bookmarking all tabs).
   */
  execute: function BATC_execute() {
    var tabURIs = this._getUniqueTabInfo(getBrowser());
    PlacesController.showAddMultiBookmarkUI(tabURIs);
  },

  /**
   * This function returns a list of nsIURI objects characterizing the
   * tabs currently open in the given browser.  The URIs will appear in the
   * list in the order in which their corresponding tabs appeared.  However,
   * only the first instance of each URI will be returned.
   *
   * @param   tabBrowser
   *          the tabBrowser to get the contents of
   * @returns a list of nsIURI objects representing unique locations open
   */
  _getUniqueTabInfo: function BATC__getUniqueTabInfo(tabBrowser) {
    var tabList = [];
    var seenURIs = [];

    const activeBrowser = tabBrowser.selectedBrowser;
    const browsers = tabBrowser.browsers;
    for (var i = 0; i < browsers.length; ++i) {
      var webNav = browsers[i].webNavigation;
       var uri = webNav.currentURI;

       // skip redundant entries
       if (uri.spec in seenURIs)
         continue;

       // add to the set of seen URIs
       seenURIs[uri.spec] = true;

       tabList.push(uri);
    }
    return tabList;
  }
};
BookmarkAllTabsCommand.NAME = "Browser:BookmarkAllTabs";

// Tell the BrowserController about this command.
BrowserController.commands[BookmarkAllTabsCommand.NAME] = 
  new BookmarkAllTabsCommand();
BrowserController.events[BrowserController.EVENT_TABCHANGE] = 
  [BookmarkAllTabsCommand.NAME];

var PlacesCommandHook = {

  /**
   * Keeps a reference to the places popup window when it is open
   */
  _placesPopup: null,
  
  /**
   * Shows the places popup
   * @param event
   *        DOMEvent for the button command that opens the popup.
   */
  showPlacesPopup: function PCH_showPlacesPopup(event) {
    if (!this._placesPopup) {
      // XXX - the button might not exist, need to handle this case.
      var button = document.getElementById("bookmarksBarShowPlaces");
      var top = button.boxObject.screenY;
#ifndef XP_MACOSX
      top += button.boxObject.height;
#endif
      var left = button.boxObject.screenX;
      var openArguments = "chrome,dependent,";
      openArguments += "top=" + top + ",left=" + left;
      openArguments += ",titlebar=no"
      this._placesPopup = window.openDialog("chrome://browser/content/places/placesPopup.xul", "placesPopup", openArguments, ORGANIZER_ROOT_BOOKMARKS);
    }
  },
  
  /**
   * Hides the places popup
   * @param event
   *        DOMEvent for the button command that closes the popup.
   */
  hidePlacesPopup: function PCH_hidePlacesPopup(event) {
    if (this._placesPopup) {
      this._placesPopup.close();
      this._placesPopup = null;
    }
  },
  
  /**
   * Toggles the places popup
   * @param event
   *        DOMEvent for the button command that opens the popup.
   */
  togglePlacesPopup: function PCH_togglePlacesPopup(event) {
    if (this._placesPopup)
      this.hidePlacesPopup();
    else
      this.showPlacesPopup();
  },
  
  /**
   * Adds a bookmark to the page loaded in the current tab. 
   */
  bookmarkCurrentPage: function PCH_bookmarkCurrentPage() {
    var selectedBrowser = getBrowser().selectedBrowser;
    PlacesController.showAddBookmarkUI(selectedBrowser.currentURI);
  },
  
  /**
   * Adds a folder with bookmarks to all of the currently open tabs in this 
   * window.
   */
  bookmarkCurrentPages: function PCH_bookmarkCurrentPages() {
  },

  /**
   * Get the description associated with a document, as specified in a <META> 
   * element.
   * @param   doc
   *          A DOM Document to get a description for
   * @returns A description string if a META element was discovered with a 
   *          "description" or "httpequiv" attribute, empty string otherwise.
   */
  _getDescriptionFromDocument: function PCH_getDescriptionFromDocument(doc) {
    var metaElements = doc.getElementsByTagName("META");
    for (var i = 0; i < metaElements.length; ++i) {
      if (metaElements[i].localName.toLowerCase() == "description" || 
          metaElements[i].httpEquiv.toLowerCase() == "description") {
        return metaElements[i].content;
        break;
      }
    }
    return "";
  },
  
  /**
   * Adds a Live Bookmark to a feed associated with the current page. 
   * @param   url
   *          The nsIURI of the page the feed was attached to
   */
  addLiveBookmark: function PCH_addLiveBookmark(url) {
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    var feedURI = ios.newURI(url, null, null);
    
    var browser = gBrowser.selectedBrowser;
    
    // TODO: implement description annotation
    //var description = this._getDescriptionFromDocument(doc);
    var title = browser.contentDocument.title;

    // TODO: add dialog for filing/confirmation
    var bms = PlacesController.bookmarks;
    var livemarks = PlacesController.livemarks;
    livemarks.createLivemark(bms.toolbarRoot, title, browser.currentURI, 
                             feedURI, -1);
  },

  /**
   * Opens the Places Organizer. 
   * @param   place
   *          The place to select in the organizer window (a place: URI) 
   */
  showPlacesOrganizer: function PCH_showPlacesOrganizer(place) {
    var wm = 
        Cc["@mozilla.org/appshell/window-mediator;1"].
        getService(Ci.nsIWindowMediator);
    var organizer = wm.getMostRecentWindow("Places:Organizer");
    if (!organizer) {
      // No currently open places window, so open one with the specified mode.
      openDialog("chrome://browser/content/places/places.xul", 
                 "", "chrome,toolbar=yes,dialog=no,resizable", place);
    }
    else {
      // Set the mode on an existing places window. 
      organizer.selectPlaceURI(place);
      organizer.focus();
    }
  },
  
  /**
   * Update the state of the tagging icon, depending on whether or not the 
   * current page is bookmarked. 
   */
  updateTagButton: function PCH_updateTagButton() {
    var bookmarkButton = document.getElementById("places-bookmark");
    if (!bookmarkButton) 
      return;
      
    var strings = document.getElementById("placeBundle");
    var currentLocation = getBrowser().selectedBrowser.webNavigation.currentURI;
    if (PlacesController.bookmarks.isBookmarked(currentLocation)) {
      bookmarkButton.label = strings.getString("locationStatusBookmarked");
      bookmarkButton.setAttribute("bookmarked", "true");
    } else {
      bookmarkButton.label = strings.getString("locationStatusNotBookmarked");
      bookmarkButton.removeAttribute("bookmarked");
    }
  },

  /**
   * This method should be called when the bookmark button is clicked.
   */
  onBookmarkButtonClick: function PCH_onBookmarkButtonClick() {
    var currentURI = getBrowser().selectedBrowser.webNavigation.currentURI;
    PlacesController.showAddBookmarkUI(currentURI);
  }
};

// Functions for the history menu.
var HistoryMenu = {

  /**
   * Updates the history menu with the session history of the current tab.
   * This function is called every time the history menu is shown.
   * @param menu 
   *        XULNode for the history menu
   */
  update: function PHM_update(menu) {
    FillHistoryMenu(menu, "history", document.getElementById("endTabHistorySeparator"));
  },

  /**
   * Shows the places search page.
   * (Will be fully implemented when there is a places search page.)
   */
  showPlacesSearch: function PHM_showPlacesSearch() {
    // XXX The places view needs to be updated before this
    // does something different than show history.
    PlacesCommandHook.showPlacesOrganizer(ORGANIZER_ROOT_HISTORY);
  }
};

/**
 * Functions for handling events in the Bookmarks Toolbar and menu.
 */
var BookmarksEventHandler = {  
  /**
   * Handler for click event for an item in the bookmarks toolbar or menu.
   * Menus and submenus from the folder buttons bubble up to this handler.
   * Only handle middle-click; left-click is handled in the onCommand function.
   * When items are middle-clicked, open them in tabs.
   * If the click came through a menu, close the menu.
   * @param event DOMEvent for the click
   */
  onClick: function BT_onClick(event) {
    // Only handle middle-clicks.
    if (event.button != 1)
      return;
    
    PlacesController.openLinksInTabs();
    
    // If this event bubbled up from a menu or menuitem,
    // close the menus.
    if (event.target.localName == "menu" ||
        event.target.localName == "menuitem") {
      var node = event.target.parentNode;
      while (node && 
             (node.localName == "menu" || 
              node.localName == "menupopup")) {
        if (node.localName == "menupopup")
          node.hidePopup();
        
        node = node.parentNode;
      }
    }
  },
  
  /**
   * Handler for command event for an item in the bookmarks toolbar.
   * Menus and submenus from the folder buttons bubble up to this handler.
   * Opens the item.
   * @param event 
   *        DOMEvent for the command
   */
  onCommand: function BM_onCommand(event) {
    // If this is the special "Open in Tabs" menuitem, load all the menuitems in tabs.
    if (event.target.hasAttribute("openInTabs"))
      PlacesController.openLinksInTabs();
    else if (event.target.hasAttribute("siteURI"))
      openUILink(event.target.getAttribute("siteURI"), event);
    // If this is a normal bookmark, just load the bookmark's URI.
    else
      PlacesController.mouseLoadURI(event);
  },
  
  /**
   * Handler for popupshowing event for an item in bookmarks toolbar or menu.
   * If the item isn't the main bookmarks menu, add an "Open in Tabs" menuitem
   * to the bottom of the popup.
   * @param event 
   *        DOMEvent for popupshowing
   */
  onPopupShowing: function BM_onPopupShowing(event) {
    var target = event.target;

    if (target.localName == "menupopup" && target.id != "bookmarksMenuPopup") {
      // Show "Open in Tabs" menuitem if there are at least
      // two menuitems with places result nodes, and "Open (Feed Name)"
      // if it's a livemark with a siteURI.
      var numNodes = 0;
      var hasMultipleEntries = false;
      var hasFeedHomePage = false;
      var currentChild = target.firstChild;
      while (currentChild && numNodes < 2) {
        if (currentChild.node && currentChild.localName == "menuitem")
          numNodes++;
        currentChild = currentChild.nextSibling;
      }
      if (numNodes > 1)
        hasMultipleEntries = true;
      var button = target.parentNode;
      if (button.getAttribute("livemark") == "true" &&
          button.hasAttribute("siteURI"))
        hasFeedHomePage = true;

      if (hasMultipleEntries || hasFeedHomePage) {
        var separator = document.createElement("menuseparator");
        target.appendChild(separator);

        var strings = document.getElementById("placeBundle");

        if (hasFeedHomePage) {
          var openHomePage = document.createElement("menuitem");
          openHomePage.setAttribute(
            "siteURI", button.getAttribute("siteURI"));
          openHomePage.setAttribute(
            "label",
            strings.getFormattedString("menuOpenLivemarkOrigin.label",
                                       [button.getAttribute("label")]));
          target.appendChild(openHomePage);
        }

        if (hasMultipleEntries) {
          var openInTabs = document.createElement("menuitem");
          openInTabs.setAttribute("openInTabs", "true");
          openInTabs.setAttribute("label",
                                  strings.getString("menuOpenInTabs.label"));
          target.appendChild(openInTabs);
        }
      }
    }
  }
};

/**
 * Drag and Drop handling specifically for the Bookmarks Menu item in the
 * top level menu bar
 */
var BookmarksMenuDropHandler = {
  /**
   * Need to tell the session to update the state of the cursor as we drag
   * over the Bookmarks Menu to show the "can drop" state vs. the "no drop"
   * state.
   */
  onDragOver: function BMDH_onDragOver(event, flavor, session) {
    session.canDrop = this.canDrop(event, session);
  },

  /**
   * Advertises the set of data types that can be dropped on the Bookmarks
   * Menu
   * @returns a FlavourSet object per nsDragAndDrop parlance.
   */
  getSupportedFlavours: function BMDH_getSupportedFlavours() {
    var flavorSet = new FlavourSet();
    var view = document.getElementById("bookmarksMenuPopup");
    for (var i = 0; i < view.peerDropTypes.length; ++i)
      flavorSet.appendFlavour(view.peerDropTypes[i]);
    return flavorSet;
  }, 

  /**
   * Determine whether or not the user can drop on the Bookmarks Menu.
   * @param   event
   *          A dragover event
   * @param   session
   *          The active DragSession
   * @returns true if the user can drop onto the Bookmarks Menu item, false 
   *          otherwise.
   */
  canDrop: function BMDH_canDrop(event, session) {
    var view = document.getElementById("bookmarksMenuPopup");
    return PlacesControllerDragHelper.canDrop(view, -1);
  },
  
  /**
   * Called when the user drops onto the top level Bookmarks Menu item.
   * @param   event
   *          A drop event
   * @param   data
   *          Data that was dropped
   * @param   session
   *          The active DragSession
   */
  onDrop: function BMDH_onDrop(event, data, session) {
    var view = document.getElementById("bookmarksMenuPopup");
    PlacesController.activeView = view;
    // The insertion point for a menupopup view should be -1 during a drag
    // & drop operation.
    NS_ASSERT(view.insertionPoint.index == -1, "Insertion point for an menupopup view during a drag must be -1!");
    PlacesControllerDragHelper.onDrop(null, view, view.insertionPoint, 1);
    view._rebuild();
  }
};

/**
 * Handles special drag and drop functionality for menus on the Bookmarks 
 * Toolbar and Bookmarks Menu.
 */
var PlacesMenuDNDController = {
  
  /**
   * Attach a special context menu hiding listener that ensures that popups 
   * are properly closed after a context menu is hidden. See bug 332845 for 
   * why we have to do this.
   */
  init: function PMDC_init() {
    var placesContext = document.getElementById("placesContext");
    var self = this;
    placesContext.addEventListener("popuphidden", function () { self._closePopups() }, false);
  },

  _springLoadDelay: 350, // milliseconds

  /**
   * All Drag Timers set for the Places UI
   */
  _timers: { },
  
  /**
   * Called when the user drags over the Bookmarks top level <menu> element.
   * @param   event
   *          The DragEnter event that spawned the opening. 
   */
  onBookmarksMenuDragEnter: function PMDC_onDragEnter(event) {
    if ("loadTime" in this._timers) 
      return;
    
    this._setDragTimer("loadTime", this._openBookmarksMenu, 
                       this._springLoadDelay, [event]);
  },
  
  /**
   * When the user drags out of the Bookmarks Menu or Toolbar, set a timer to 
   * manually close the popup chain that was dragged out of. We need to do this
   * since the global popup dismissal listener does not handle multiple extant
   * popup chains well. See bug 332845 for more information.
   */
  onDragExit: function PMDC_onDragExit(event) {
    // Ensure that we don't set multiple timers if there's one already set.
    if ("closeTime" in this._timers)
      return;
      
    this._setDragTimer("closeTime", this._closePopups, 
                       this._springLoadDelay, [event.target]);
  },
  
  /**
   * Creates a timer that will fire during a drag and drop operation.
   * @param   id
   *          The identifier of the timer being set
   * @param   callback
   *          The function to call when the timer "fires"
   * @param   delay
   *          The time to wait before calling the callback function
   * @param   args
   *          An array of arguments to pass to the callback function
   */
  _setDragTimer: function PMDC__setDragTimer(id, callback, delay, args) {
    if (!this._dragSupported)
      return;

    // Cancel this timer if it's already running.
    if (id in this._timers)
      this._timers[id].cancel();
      
    /**
     * An object implementing nsITimerCallback that calls a user-supplied
     * method with the specified args in the context of the supplied object.
     */
    function Callback(object, method, args) {
      this._method = method;
      this._args = args;
      this._object = object;
    }
    Callback.prototype = {
      notify: function C_notify(timer) {
        this._method.apply(this._object, this._args);
      }
    };
    
    var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(new Callback(this, callback, args), delay, 
                           timer.TYPE_ONE_SHOT);
    this._timers[id] = timer;
  },
  
  /**
   * Determines if a XUL element represents a container in the Bookmarks system
   * @returns true if the element is a container element (menu or 
   *`         menu-toolbarbutton), false otherwise.
   */
  _isContainer: function PMDC__isContainer(node) {
    return node.localName == "menu" || 
           node.localName == "toolbarbutton" && node.getAttribute("type") == "menu";
  },
  
  /**
   * Close all pieces of toplevel menu UI in the browser window. Called in 
   * circumstances where there may be multiple chains of menupopups, e.g. 
   * during drag and drop operations between menus, and in context menu-on-
   * menu situations.
   */
  _closePopups: function PMDC__closePopups(target) {
    if (PlacesControllerDragHelper.draggingOverChildNode(target))
      return;

    if ("closeTime" in this._timers)
      delete this._timers.closeTime;
    
    // Close the bookmarks menu
    var bookmarksMenu = document.getElementById("bookmarksMenu");
    bookmarksMenu.firstChild.hidePopupAndChildPopups();

    var bookmarksBar = document.getElementById("bookmarksBarContent");
    // Close the overflow chevron menu and all its children
    bookmarksBar._chevron.firstChild.hidePopupAndChildPopups();
    
    // Close all popups on the bookmarks toolbar
    var toolbarItems = bookmarksBar.childNodes;
    for (var i = 0; i < toolbarItems.length; ++i) {
      var item = toolbarItems[i]
      if (this._isContainer(item))
        item.firstChild.hidePopupAndChildPopups();
    }
  },
  
  /**
   * Opens the Bookmarks Menu when it is dragged over. (This is special-cased, 
   * since the toplevel Bookmarks <menu> is not a member of an existing places
   * container, as folders on the personal toolbar or submenus are. 
   * @param   event
   *          The DragEnter event that spawned the opening. 
   */
  _openBookmarksMenu: function PMDC__openBookmarksMenu(event) {
    if ("loadTime" in this._timers)
      delete this._timers.loadTime;
    if (event.target.id == "bookmarksMenu") {
      // If this is the bookmarks menu, tell its menupopup child to show.
      event.target.lastChild.showPopup(event.target.lastChild);
    }  
  },

  // Whether or not drag and drop to menus is supported on this platform
  // Dragging in menus is disabled on OS X due to various repainting issues.
#ifdef XP_MACOSX
  _dragSupported: false,
#else
  _dragSupported: true,
#endif
};

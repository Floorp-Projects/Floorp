# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Places Browser Integration
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <beng@google.com>
#   Annie Sullivan <annie.sullivan@gmail.com>
#   Joe Hughes <joe@retrovirus.com>
#   Asaf Romano <mano@mozilla.com>
#   Ehsan Akhgari <ehsan.akhgari@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****


var StarUI = {
  _itemId: -1,
  uri: null,
  _batching: false,

  // nsISupports
  QueryInterface: function SU_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIDOMEventListener) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_NOINTERFACE;
  },

  _element: function(aID) {
    return document.getElementById(aID);
  },

  // Edit-bookmark panel
  get panel() {
    delete this.panel;
    var element = this._element("editBookmarkPanel");
    // initially the panel is hidden
    // to avoid impacting startup / new window performance
    element.hidden = false;
    element.addEventListener("popuphidden", this, false);
    element.addEventListener("keypress", this, true);
    return this.panel = element;
  },

  // list of command elements (by id) to disable when the panel is opened
  _blockedCommands: ["cmd_close", "cmd_closeWindow"],
  _blockCommands: function SU__blockCommands() {
    for each(var key in this._blockedCommands) {
      var elt = this._element(key);
      // make sure not to permanently disable this item (see bug 409155)
      if (elt.hasAttribute("wasDisabled"))
        continue;
      if (elt.getAttribute("disabled") == "true")
        elt.setAttribute("wasDisabled", "true");
      else {
        elt.setAttribute("wasDisabled", "false");
        elt.setAttribute("disabled", "true");
      }
    }
  },

  _restoreCommandsState: function SU__restoreCommandsState() {
    for each(var key in this._blockedCommands) {
      var elt = this._element(key);
      if (elt.getAttribute("wasDisabled") != "true")
        elt.removeAttribute("disabled");
      elt.removeAttribute("wasDisabled");
    }
  },

  // nsIDOMEventListener
  handleEvent: function SU_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "popuphidden":
        if (aEvent.originalTarget == this.panel) {
          if (!this._element("editBookmarkPanelContent").hidden)
            this.quitEditMode();
          this._restoreCommandsState();
          this._itemId = -1;
          this._uri = null;
          if (this._batching) {
            PlacesUIUtils.ptm.endBatch();
            this._batching = false;
          }
        }
        break;
      case "keypress":
        if (aEvent.keyCode == KeyEvent.DOM_VK_ESCAPE) {
          // In edit mode, if we're not editing a folder, the ESC key is mapped
          // to the cancel button
          if (!this._element("editBookmarkPanelContent").hidden) {
            var elt = aEvent.target;
            if (elt.localName != "tree" ||
                (elt.localName == "tree" && !elt.hasAttribute("editing")))
              this.cancelButtonOnCommand();
          }
        }
        else if (aEvent.keyCode == KeyEvent.DOM_VK_RETURN) {
          // hide the panel unless the folder tree is focused
          // or the tag autocomplete popup is open
          if (aEvent.target.localName != "tree" &&
              (aEvent.target.id != "editBMPanel_tagsField" ||
               !aEvent.target.popupOpen))
            this.panel.hidePopup();
        }
        break;
    }
  },

  _overlayLoaded: false,
  _overlayLoading: false,
  showEditBookmarkPopup:
  function SU_showEditBookmarkPopup(aItemId, aAnchorElement, aPosition) {
    // Performance: load the overlay the first time the panel is opened
    // (see bug 392443).
    if (this._overlayLoading)
      return;

    if (this._overlayLoaded) {
      this._doShowEditBookmarkPanel(aItemId, aAnchorElement, aPosition);
      return;
    }

    var loadObserver = {
      _self: this,
      _itemId: aItemId,
      _anchorElement: aAnchorElement,
      _position: aPosition,
      observe: function (aSubject, aTopic, aData) {
        this._self._overlayLoading = false;
        this._self._overlayLoaded = true;
        this._self._doShowEditBookmarkPanel(this._itemId, this._anchorElement,
                                            this._position);
      }
    };
    this._overlayLoading = true;
    document.loadOverlay("chrome://browser/content/places/editBookmarkOverlay.xul",
                         loadObserver);
  },

  _doShowEditBookmarkPanel:
  function SU__doShowEditBookmarkPanel(aItemId, aAnchorElement, aPosition) {
    if (this.panel.state != "closed")
      return;

    this._blockCommands(); // un-done in the popuphiding handler

    var bundle = this._element("bundle_browser");

    // Set panel title:
    // if we are batching, i.e. the bookmark has been added now,
    // then show Page Bookmarked, else if the bookmark did already exist,
    // we are about editing it, then use Edit This Bookmark.
    this._element("editBookmarkPanelTitle").value =
      this._batching ?
        bundle.getString("editBookmarkPanel.pageBookmarkedTitle") :
        bundle.getString("editBookmarkPanel.editBookmarkTitle");

    // No description; show the Done, Cancel;
    // hide the Edit, Undo buttons
    this._element("editBookmarkPanelDescription").textContent = "";
    this._element("editBookmarkPanelBottomButtons").hidden = false;
    this._element("editBookmarkPanelContent").hidden = false;
    this._element("editBookmarkPanelEditButton").hidden = true;
    this._element("editBookmarkPanelUndoRemoveButton").hidden = true;

    // The remove button is shown only if we're not already batching, i.e.
    // if the cancel button/ESC does not remove the bookmark.
    this._element("editBookmarkPanelRemoveButton").hidden = this._batching;

    // The label of the remove button differs if the URI is bookmarked
    // multiple times.
    var bookmarks = PlacesUtils.getBookmarksForURI(gBrowser.currentURI);
    var forms = bundle.getString("editBookmark.removeBookmarks.label");
    Cu.import("resource://gre/modules/PluralForm.jsm");
    var label = PluralForm.get(bookmarks.length, forms).replace("#1", bookmarks.length);
    this._element("editBookmarkPanelRemoveButton").label = label;

    // unset the unstarred state, if set
    this._element("editBookmarkPanelStarIcon").removeAttribute("unstarred");

    this._itemId = aItemId !== undefined ? aItemId : this._itemId;
    this.beginBatch();

    // XXXmano hack: We push a no-op transaction on the stack so it's always
    // safe for the Cancel button to call undoTransaction after endBatch.
    // Otherwise, if no changes were done in the edit-item panel, the last
    // transaction on the undo stack may be the initial createItem transaction,
    // or worse, the batched editing of some other item.
    PlacesUIUtils.ptm.doTransaction({ doTransaction: function() { },
                                      undoTransaction: function() { },
                                      redoTransaction: function() { },
                                      isTransient: false,
                                      merge: function() { return false; } });

    // Consume dismiss clicks, see bug 400924
    this.panel.popupBoxObject
        .setConsumeRollupEvent(Ci.nsIPopupBoxObject.ROLLUP_CONSUME);
    this.panel.openPopup(aAnchorElement, aPosition, -1, -1);

    gEditItemOverlay.initPanel(this._itemId,
                               { hiddenRows: ["description", "location",
                                              "loadInSidebar", "keyword"] });
  },

  panelShown:
  function SU_panelShown(aEvent) {
    if (aEvent.target == this.panel) {
      if (!this._element("editBookmarkPanelContent").hidden) {
        var namePicker = this._element("editBMPanel_namePicker");
        namePicker.focus();
        namePicker.editor.selectAll();
      }
      else
        this.panel.focus();
    }
  },

  showPageBookmarkedNotification:
  function PCH_showPageBookmarkedNotification(aItemId, aAnchorElement, aPosition) {
    this._blockCommands(); // un-done in the popuphiding handler

    var bundle = this._element("bundle_browser");
    var brandBundle = this._element("bundle_brand");
    var brandShortName = brandBundle.getString("brandShortName");

    // "Page Bookmarked" title
    this._element("editBookmarkPanelTitle").value =
      bundle.getString("editBookmarkPanel.pageBookmarkedTitle");

    // description
    this._element("editBookmarkPanelDescription").textContent =
      bundle.getFormattedString("editBookmarkPanel.pageBookmarkedDescription",
                                [brandShortName]);

    // show the "Edit.." button and the Remove Bookmark button, hide the
    // undo-remove-bookmark button.
    this._element("editBookmarkPanelEditButton").hidden = false;
    this._element("editBookmarkPanelRemoveButton").hidden = false;
    this._element("editBookmarkPanelUndoRemoveButton").hidden = true;

    // unset the unstarred state, if set
    this._element("editBookmarkPanelStarIcon").removeAttribute("unstarred");

    this._itemId = aItemId !== undefined ? aItemId : this._itemId;
    if (this.panel.state == "closed") {
      // Consume dismiss clicks, see bug 400924
      this.panel.popupBoxObject
          .setConsumeRollupEvent(Ci.nsIPopupBoxObject.ROLLUP_CONSUME);
      this.panel.openPopup(aAnchorElement, aPosition, -1, -1);
    }
    else
      this.panel.focus();
  },

  quitEditMode: function SU_quitEditMode() {
    this._element("editBookmarkPanelContent").hidden = true;
    this._element("editBookmarkPanelBottomButtons").hidden = true;
    gEditItemOverlay.uninitPanel(true);
  },

  editButtonCommand: function SU_editButtonCommand() {
    this.showEditBookmarkPopup();
  },

  cancelButtonOnCommand: function SU_cancelButtonOnCommand() {
    // The order here is important! We have to hide the panel first, otherwise
    // changes done as part of Undo may change the panel contents and by
    // that force it to commit more transactions
    this.panel.hidePopup();
    this.endBatch();
    PlacesUIUtils.ptm.undoTransaction();
  },

  removeBookmarkButtonCommand: function SU_removeBookmarkButtonCommand() {
#ifdef ADVANCED_STARRING_UI
    // In minimal mode ("page bookmarked" notification), the bookmark
    // is removed and the panel is hidden immediately. In full edit mode,
    // a "Bookmark Removed" notification along with an Undo button is
    // shown
    if (this._batching) {
      PlacesUIUtils.ptm.endBatch();
      PlacesUIUtils.ptm.beginBatch(); // allow undo from within the notification
      var bundle = this._element("bundle_browser");

      // "Bookmark Removed" title (the description field is already empty in
      // this mode)
      this._element("editBookmarkPanelTitle").value =
        bundle.getString("editBookmarkPanel.bookmarkedRemovedTitle");

      // hide the edit panel
      this.quitEditMode();

      // Hide the remove bookmark button, show the undo-remove-bookmark
      // button.
      this._element("editBookmarkPanelUndoRemoveButton").hidden = false;
      this._element("editBookmarkPanelRemoveButton").hidden = true;
      this._element("editBookmarkPanelStarIcon").setAttribute("unstarred", "true");
      this.panel.focus();
    }
#endif

    // cache its uri so we can get the new itemId in the case of undo
    this._uri = PlacesUtils.bookmarks.getBookmarkURI(this._itemId);

    // remove all bookmarks for the bookmark's url, this also removes
    // the tags for the url
    var itemIds = PlacesUtils.getBookmarksForURI(this._uri);
    for (var i=0; i < itemIds.length; i++) {
      var txn = PlacesUIUtils.ptm.removeItem(itemIds[i]);
      PlacesUIUtils.ptm.doTransaction(txn);
    }

#ifdef ADVANCED_STARRING_UI
    // hidePopup resets our itemId, thus we call it only after removing
    // the bookmark
    if (!this._batching)
#endif
      this.panel.hidePopup();
  },

  undoRemoveBookmarkCommand: function SU_undoRemoveBookmarkCommand() {
    // restore the bookmark by undoing the last transaction and go back
    // to the edit state
    this.endBatch();
    PlacesUIUtils.ptm.undoTransaction();
    this._itemId = PlacesUtils.getMostRecentBookmarkForURI(this._uri);
    this.showEditBookmarkPopup();
  },

  beginBatch: function SU_beginBatch() {
    if (!this._batching) {
      PlacesUIUtils.ptm.beginBatch();
      this._batching = true;
    }
  },

  endBatch: function SU_endBatch() {
    if (this._batching) {
      PlacesUIUtils.ptm.endBatch();
      this._batching = false;
    }
  }
}

var PlacesCommandHook = {
  /**
   * Adds a bookmark to the page loaded in the given browser.
   *
   * @param aBrowser
   *        a <browser> element.
   * @param [optional] aParent
   *        The folder in which to create a new bookmark if the page loaded in
   *        aBrowser isn't bookmarked yet, defaults to the unfiled root.
   * @param [optional] aShowEditUI
   *        whether or not to show the edit-bookmark UI for the bookmark item
   */  
  bookmarkPage: function PCH_bookmarkPage(aBrowser, aParent, aShowEditUI) {
    var uri = aBrowser.currentURI;
    var itemId = PlacesUtils.getMostRecentBookmarkForURI(uri);
    if (itemId == -1) {
      // Copied over from addBookmarkForBrowser:
      // Bug 52536: We obtain the URL and title from the nsIWebNavigation
      // associated with a <browser/> rather than from a DOMWindow.
      // This is because when a full page plugin is loaded, there is
      // no DOMWindow (?) but information about the loaded document
      // may still be obtained from the webNavigation.
      var webNav = aBrowser.webNavigation;
      var url = webNav.currentURI;
      var title;
      var description;
      var charset;
      try {
        title = webNav.document.title || url.spec;
        description = PlacesUIUtils.getDescriptionFromDocument(webNav.document);
        charset = webNav.document.characterSet;
      }
      catch (e) { }

      if (aShowEditUI) {
        // If we bookmark the page here (i.e. page was not "starred" already)
        // but open right into the "edit" state, start batching here, so
        // "Cancel" in that state removes the bookmark.
        StarUI.beginBatch();
      }

      var parent = aParent != undefined ?
                   aParent : PlacesUtils.unfiledBookmarksFolderId;
      var descAnno = { name: DESCRIPTION_ANNO, value: description };
      var txn = PlacesUIUtils.ptm.createItem(uri, parent, -1,
                                             title, null, [descAnno]);
      PlacesUIUtils.ptm.doTransaction(txn);
      // Set the character-set
      if (charset)
        PlacesUtils.history.setCharsetForURI(uri, charset);
      itemId = PlacesUtils.getMostRecentBookmarkForURI(uri);
    }

    // Revert the contents of the location bar
    if (gURLBar)
      gURLBar.handleRevert();

    // dock the panel to the star icon when possible, otherwise dock
    // it to the content area
    if (aBrowser.contentWindow == window.content) {
      var starIcon = aBrowser.ownerDocument.getElementById("star-button");
      if (starIcon && isElementVisible(starIcon)) {
        // Make sure the bookmark properties dialog hangs toward the middle of
        // the location bar in RTL builds
        var position = "after_end";
        if (gURLBar.getAttribute("chromedir") == "rtl")
          position = "after_start";
        if (aShowEditUI)
          StarUI.showEditBookmarkPopup(itemId, starIcon, position);
#ifdef ADVANCED_STARRING_UI
        else
          StarUI.showPageBookmarkedNotification(itemId, starIcon, position);
#endif
        return;
      }
    }

    StarUI.showEditBookmarkPopup(itemId, aBrowser, "overlap");
  },

  /**
   * Adds a bookmark to the page loaded in the current tab. 
   */
  bookmarkCurrentPage: function PCH_bookmarkCurrentPage(aShowEditUI, aParent) {
    this.bookmarkPage(getBrowser().selectedBrowser, aParent, aShowEditUI);
  },

  /**
   * Adds a bookmark to the page targeted by a link.
   * @param aParent
   *        The folder in which to create a new bookmark if aURL isn't
   *        bookmarked.
   * @param aURL (string)
   *        the address of the link target
   * @param aTitle
   *        The link text
   */
  bookmarkLink: function PCH_bookmarkLink(aParent, aURL, aTitle) {
    var linkURI = makeURI(aURL);
    var itemId = PlacesUtils.getMostRecentBookmarkForURI(linkURI);
    if (itemId == -1)
      PlacesUIUtils.showMinimalAddBookmarkUI(linkURI, aTitle);
    else {
      PlacesUIUtils.showItemProperties(itemId,
                                       PlacesUtils.bookmarks.TYPE_BOOKMARK);
    }
  },

  /**
   * This function returns a list of nsIURI objects characterizing the
   * tabs currently open in the browser.  The URIs will appear in the
   * list in the order in which their corresponding tabs appeared.  However,
   * only the first instance of each URI will be returned.
   *
   * @returns a list of nsIURI objects representing unique locations open
   */
  _getUniqueTabInfo: function BATC__getUniqueTabInfo() {
    var tabList = [];
    var seenURIs = [];

    var browsers = getBrowser().browsers;
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
  },

  /**
   * Adds a folder with bookmarks to all of the currently open tabs in this 
   * window.
   */
  bookmarkCurrentPages: function PCH_bookmarkCurrentPages() {
    var tabURIs = this._getUniqueTabInfo();
    PlacesUIUtils.showMinimalAddMultiBookmarkUI(tabURIs);
  },

  
  /**
   * Adds a Live Bookmark to a feed associated with the current page. 
   * @param     url
   *            The nsIURI of the page the feed was attached to
   * @title     title
   *            The title of the feed. Optional.
   * @subtitle  subtitle
   *            A short description of the feed. Optional.
   */
  addLiveBookmark: function PCH_addLiveBookmark(url, feedTitle, feedSubtitle) {
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    var feedURI = ios.newURI(url, null, null);
    
    var doc = gBrowser.contentDocument;
    var title = (arguments.length > 1) ? feedTitle : doc.title;
 
    var description;
    if (arguments.length > 2)
      description = feedSubtitle;
    else
      description = PlacesUIUtils.getDescriptionFromDocument(doc);

    var toolbarIP =
      new InsertionPoint(PlacesUtils.bookmarks.toolbarFolder, -1);
    PlacesUIUtils.showMinimalAddLivemarkUI(feedURI, gBrowser.currentURI,
                                           title, description, toolbarIP, true);
  },

  /**
   * Opens the Places Organizer. 
   * @param   aLeftPaneRoot
   *          The query to select in the organizer window - options
   *          are: History, AllBookmarks, BookmarksMenu, BookmarksToolbar,
   *          UnfiledBookmarks and Tags.
   */
  showPlacesOrganizer: function PCH_showPlacesOrganizer(aLeftPaneRoot) {
    var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
             getService(Ci.nsIWindowMediator);
    var organizer = wm.getMostRecentWindow("Places:Organizer");
    if (!organizer) {
      // No currently open places window, so open one with the specified mode.
      openDialog("chrome://browser/content/places/places.xul", 
                 "", "chrome,toolbar=yes,dialog=no,resizable", aLeftPaneRoot);
    }
    else {
      organizer.PlacesOrganizer.selectLeftPaneQuery(aLeftPaneRoot);
      organizer.focus();
    }
  },

  deleteButtonOnCommand: function PCH_deleteButtonCommand() {
    PlacesUtils.bookmarks.removeItem(gEditItemOverlay.itemId);

    // remove all tags for the associated url
    PlacesUtils.tagging.untagURI(gEditItemOverlay._uri, null);

    this.panel.hidePopup();
  }
};

// Functions for the history menu.
var HistoryMenu = {
  /**
   * popupshowing handler for the history menu.
   * @param aMenuPopup
   *        XULNode for the history menupopup
   */
  onPopupShowing: function PHM_onPopupShowing(aMenuPopup) {
    var resultNode = aMenuPopup.getResultNode();
    var wasOpen = resultNode.containerOpen;
    resultNode.containerOpen = true;
    document.getElementById("endHistorySeparator").hidden =
      resultNode.childCount == 0;

    if (!wasOpen)
      resultNode.containerOpen = false;

    // HistoryMenu.toggleRecentlyClosedTabs is defined in browser.js
    this.toggleRecentlyClosedTabs();
  }
};

/**
 * Functions for handling events in the Bookmarks Toolbar and menu.
 */
var BookmarksEventHandler = {  
  /**
   * Handler for click event for an item in the bookmarks toolbar or menu.
   * Menus and submenus from the folder buttons bubble up to this handler.
   * Left-click is handled in the onCommand function.
   * When items are middle-clicked (or clicked with modifier), open in tabs.
   * If the click came through a menu, close the menu.
   * @param aEvent
   *        DOMEvent for the click
   */
  onClick: function BT_onClick(aEvent) {
    // Only handle middle-click or left-click with modifiers.
#ifdef XP_MACOSX
    var modifKey = aEvent.metaKey || aEvent.shiftKey;
#else
    var modifKey = aEvent.ctrlKey || aEvent.shiftKey;
#endif
    if (aEvent.button == 2 || (aEvent.button == 0 && !modifKey))
      return;

    var target = aEvent.originalTarget;
    // If this event bubbled up from a menu or menuitem, close the menus.
    // Do this before opening tabs, to avoid hiding the open tabs confirm-dialog.
    if (target.localName == "menu" || target.localName == "menuitem") {
      for (node = target.parentNode; node; node = node.parentNode) {
        if (node.localName == "menupopup")
          node.hidePopup();
        else if (node.localName != "menu")
          break;
      }
    }

    if (target.node && PlacesUtils.nodeIsContainer(target.node)) {
      // Don't open the root folder in tabs when the empty area on the toolbar
      // is middle-clicked or when a non-bookmark item except for Open in Tabs)
      // in a bookmarks menupopup is middle-clicked.
      if (target.localName == "menu" || target.localName == "toolbarbutton")
        PlacesUIUtils.openContainerNodeInTabs(target.node, aEvent);
    }
    else if (aEvent.button == 1) {
      // left-clicks with modifier are already served by onCommand
      this.onCommand(aEvent);
    }
  },

  /**
   * Handler for command event for an item in the bookmarks toolbar.
   * Menus and submenus from the folder buttons bubble up to this handler.
   * Opens the item.
   * @param aEvent 
   *        DOMEvent for the command
   */
  onCommand: function BM_onCommand(aEvent) {
    var target = aEvent.originalTarget;
    if (target.node)
      PlacesUIUtils.openNodeWithEvent(target.node, aEvent);
  },

  /**
   * Handler for popupshowing event for an item in bookmarks toolbar or menu.
   * If the item isn't the main bookmarks menu, add an "Open All in Tabs"
   * menuitem to the bottom of the popup.
   * @param event 
   *        DOMEvent for popupshowing
   */
  onPopupShowing: function BM_onPopupShowing(event) {
    var target = event.originalTarget;
    if (!target.hasAttribute("placespopup"))
      return;

    // Check if the popup contains at least 2 menuitems with places nodes
    var numNodes = 0;
    var hasMultipleURIs = false;
    var currentChild = target.firstChild;
    while (currentChild) {
      if (currentChild.localName == "menuitem" && currentChild.node) {
        if (++numNodes == 2) {
          hasMultipleURIs = true;
          break;
        }
      }
      currentChild = currentChild.nextSibling;
    }

    var itemId = target._resultNode.itemId;
    var siteURIString = "";
    if (itemId != -1 && PlacesUtils.livemarks.isLivemark(itemId)) {
      var siteURI = PlacesUtils.livemarks.getSiteURI(itemId);
      if (siteURI)
        siteURIString = siteURI.spec;
    }

    if (!siteURIString && target._endOptOpenSiteURI) {
        target.removeChild(target._endOptOpenSiteURI);
        target._endOptOpenSiteURI = null;
    }

    if (!hasMultipleURIs && target._endOptOpenAllInTabs) {
      target.removeChild(target._endOptOpenAllInTabs);
      target._endOptOpenAllInTabs = null;
    }

    if (!(hasMultipleURIs || siteURIString)) {
      // we don't have to show any option
      if (target._endOptSeparator) {
        target.removeChild(target._endOptSeparator);
        target._endOptSeparator = null;
        target._endMarker = -1;
      }
      return;
    }

    if (!target._endOptSeparator) {
      // create a separator before options
      target._endOptSeparator = document.createElement("menuseparator");
      target._endMarker = target.childNodes.length;
      target.appendChild(target._endOptSeparator);
    }

    if (siteURIString && !target._endOptOpenSiteURI) {
      // Add "Open (Feed Name)" menuitem if it's a livemark with a siteURI
      target._endOptOpenSiteURI = document.createElement("menuitem");
      target._endOptOpenSiteURI.setAttribute("siteURI", siteURIString);
      target._endOptOpenSiteURI.setAttribute("oncommand",
          "openUILink(this.getAttribute('siteURI'), event);");
      // If a user middle-clicks this item we serve the oncommand event
      // We are using checkForMiddleClick because of Bug 246720
      // Note: stopPropagation is needed to avoid serving middle-click 
      // with BT_onClick that would open all items in tabs
      target._endOptOpenSiteURI.setAttribute("onclick",
          "checkForMiddleClick(this, event); event.stopPropagation();");
      target._endOptOpenSiteURI.setAttribute("label",
          PlacesUIUtils.getFormattedString("menuOpenLivemarkOrigin.label",
          [target.parentNode.getAttribute("label")]));
      target.appendChild(target._endOptOpenSiteURI);
    }

    if (hasMultipleURIs && !target._endOptOpenAllInTabs) {
        // Add the "Open All in Tabs" menuitem if there are
        // at least two menuitems with places result nodes.
        target._endOptOpenAllInTabs = document.createElement("menuitem");
        target._endOptOpenAllInTabs.setAttribute("oncommand",
            "PlacesUIUtils.openContainerNodeInTabs(this.parentNode._resultNode, event);");
        target._endOptOpenAllInTabs.setAttribute("onclick",
            "checkForMiddleClick(this, event); event.stopPropagation();");
        target._endOptOpenAllInTabs.setAttribute("label",
            gNavigatorBundle.getString("menuOpenAllInTabs.label"));
        target.appendChild(target._endOptOpenAllInTabs);
    }
  },

  fillInBTTooltip: function(aTipElement) {
    if (!aTipElement.node)
      return false;

    //Show tooltips only for URL items
    if (!PlacesUtils.nodeIsURI(aTipElement.node))
      return false;

    var title = aTipElement.node.title;
    var url = aTipElement.node.uri;

    var tooltipTitle = document.getElementById("btTitleText");
    tooltipTitle.hidden = !title || (title == url);
    if (!tooltipTitle.hidden)
      tooltipTitle.textContent = title;

    var tooltipUrl = document.getElementById("btUrlText");
    tooltipUrl.value = url;

    //Show tooltip
    return true;
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
    if (!this.canDrop(event, session))
      event.dataTransfer.effectAllowed = "none";
  },

  /**
   * Advertises the set of data types that can be dropped on the Bookmarks
   * Menu
   * @returns a FlavourSet object per nsDragAndDrop parlance.
   */
  getSupportedFlavours: function BMDH_getSupportedFlavours() {
    var view = document.getElementById("bookmarksMenuPopup");
    return view.getSupportedFlavours();
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
    PlacesControllerDragHelper.currentDataTransfer = event.dataTransfer;

    var ip = new InsertionPoint(PlacesUtils.bookmarksMenuFolderId, -1);  
    return ip && PlacesControllerDragHelper.canDrop(ip);
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
    PlacesControllerDragHelper.currentDataTransfer = event.dataTransfer;

  // Put the item at the end of bookmark menu
    var ip = new InsertionPoint(PlacesUtils.bookmarksMenuFolderId, -1,
                                Ci.nsITreeView.DROP_ON);
    PlacesControllerDragHelper.onDrop(ip);
  },

  /**
   * Called when drop target leaves the menu or after a drop.
   * @param   aEvent
   *          A drop event
   */
  onDragExit: function BMDH_onDragExit(event, session) {
    PlacesControllerDragHelper.currentDataTransfer = null;
  }
};

/**
 * Handles special drag and drop functionality for menus on the Bookmarks 
 * Toolbar and Bookmarks Menu.
 */
var PlacesMenuDNDController = {
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
           (node.localName == "toolbarbutton" &&
            node.getAttribute("type") == "menu");
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
      event.target.lastChild.setAttribute("autoopened", "true");
      event.target.lastChild.showPopup(event.target.lastChild);
    }  
  },

  // Whether or not drag and drop to menus is supported on this platform
  // Dragging in menus is disabled on OS X due to various repainting issues.
#ifdef XP_MACOSX
  _dragSupported: false
#else
  _dragSupported: true
#endif
};

var PlacesStarButton = {
  init: function PSB_init() {
    PlacesUtils.bookmarks.addObserver(this, false);
  },

  uninit: function PSB_uninit() {
    PlacesUtils.bookmarks.removeObserver(this);
  },

  QueryInterface: function PSB_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsINavBookmarkObserver) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_NOINTERFACE;
  },

  _starred: false,
  _batching: false,

  updateState: function PSB_updateState() {
    var starIcon = document.getElementById("star-button");
    if (!starIcon)
      return;

    var browserBundle = document.getElementById("bundle_browser");
    var uri = getBrowser().currentURI;
    this._starred = uri && (PlacesUtils.getMostRecentBookmarkForURI(uri) != -1 ||
                            PlacesUtils.getMostRecentFolderForFeedURI(uri) != -1);
    if (this._starred) {
      starIcon.setAttribute("starred", "true");
      starIcon.setAttribute("tooltiptext", browserBundle.getString("starButtonOn.tooltip"));
    }
    else {
      starIcon.removeAttribute("starred");
      starIcon.setAttribute("tooltiptext", browserBundle.getString("starButtonOff.tooltip"));
    }
  },

  onClick: function PSB_onClick(aEvent) {
    if (aEvent.button == 0)
      PlacesCommandHook.bookmarkCurrentPage(this._starred);

    // don't bubble to the textbox so that the address won't be selected
    aEvent.stopPropagation();
  },

  // nsINavBookmarkObserver  
  onBeginUpdateBatch: function PSB_onBeginUpdateBatch() {
    this._batching = true;
  },

  onEndUpdateBatch: function PSB_onEndUpdateBatch() {
    this.updateState();
    this._batching = false;
  },
  
  onItemAdded: function PSB_onItemAdded(aItemId, aFolder, aIndex) {
    if (!this._batching && !this._starred)
      this.updateState();
  },

  onItemRemoved: function PSB_onItemRemoved(aItemId, aFolder, aIndex) {
    if (!this._batching)
      this.updateState();
  },

  onItemChanged: function PSB_onItemChanged(aItemId, aProperty,
                                            aIsAnnotationProperty, aValue) {
    if (!this._batching && aProperty == "uri")
      this.updateState();
  },

  onItemVisited: function() { },
  onItemMoved: function() { }
};

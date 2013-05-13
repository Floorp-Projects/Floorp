# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

////////////////////////////////////////////////////////////////////////////////
//// StarUI

var StarUI = {
  _itemId: -1,
  uri: null,
  _batching: false,

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
    element.addEventListener("keypress", this, false);
    return this.panel = element;
  },

  // Array of command elements to disable when the panel is opened.
  get _blockedCommands() {
    delete this._blockedCommands;
    return this._blockedCommands =
      ["cmd_close", "cmd_closeWindow"].map(function (id) this._element(id), this);
  },

  _blockCommands: function SU__blockCommands() {
    this._blockedCommands.forEach(function (elt) {
      // make sure not to permanently disable this item (see bug 409155)
      if (elt.hasAttribute("wasDisabled"))
        return;
      if (elt.getAttribute("disabled") == "true") {
        elt.setAttribute("wasDisabled", "true");
      } else {
        elt.setAttribute("wasDisabled", "false");
        elt.setAttribute("disabled", "true");
      }
    });
  },

  _restoreCommandsState: function SU__restoreCommandsState() {
    this._blockedCommands.forEach(function (elt) {
      if (elt.getAttribute("wasDisabled") != "true")
        elt.removeAttribute("disabled");
      elt.removeAttribute("wasDisabled");
    });
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
          if (this._batching) {
            PlacesUtils.transactionManager.endBatch(false);
            this._batching = false;
          }

          switch (this._actionOnHide) {
            case "cancel": {
              PlacesUtils.transactionManager.undoTransaction();
              break;
            }
            case "remove": {
              // Remove all bookmarks for the bookmark's url, this also removes
              // the tags for the url.
              PlacesUtils.transactionManager.beginBatch(null);
              let itemIds = PlacesUtils.getBookmarksForURI(this._uriForRemoval);
              for (let i = 0; i < itemIds.length; i++) {
                let txn = new PlacesRemoveItemTransaction(itemIds[i]);
                PlacesUtils.transactionManager.doTransaction(txn);
              }
              PlacesUtils.transactionManager.endBatch(false);
              break;
            }
          }
          this._actionOnHide = "";
        }
        break;
      case "keypress":
        if (aEvent.defaultPrevented) {
          // The event has already been consumed inside of the panel.
          break;
        }
        switch (aEvent.keyCode) {
          case KeyEvent.DOM_VK_ESCAPE:
            if (!this._element("editBookmarkPanelContent").hidden)
              this.cancelButtonOnCommand();
            break;
          case KeyEvent.DOM_VK_RETURN:
            if (aEvent.target.className == "expander-up" ||
                aEvent.target.className == "expander-down" ||
                aEvent.target.id == "editBMPanel_newFolderButton") {
              //XXX Why is this necessary? The defaultPrevented check should
              //    be enough.
              break;
            }
            this.panel.hidePopup();
            break;
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

    this._overlayLoading = true;
    document.loadOverlay(
      "chrome://browser/content/places/editBookmarkOverlay.xul",
      (function (aSubject, aTopic, aData) {
        //XXX We just caused localstore.rdf to be re-applied (bug 640158)
        retrieveToolbarIconsizesFromTheme();

        // Move the header (star, title, button) into the grid,
        // so that it aligns nicely with the other items (bug 484022).
        let header = this._element("editBookmarkPanelHeader");
        let rows = this._element("editBookmarkPanelGrid").lastChild;
        rows.insertBefore(header, rows.firstChild);
        header.hidden = false;

        this._overlayLoading = false;
        this._overlayLoaded = true;
        this._doShowEditBookmarkPanel(aItemId, aAnchorElement, aPosition);
      }).bind(this)
    );
  },

  _doShowEditBookmarkPanel:
  function SU__doShowEditBookmarkPanel(aItemId, aAnchorElement, aPosition) {
    if (this.panel.state != "closed")
      return;

    this._blockCommands(); // un-done in the popuphiding handler

    // Set panel title:
    // if we are batching, i.e. the bookmark has been added now,
    // then show Page Bookmarked, else if the bookmark did already exist,
    // we are about editing it, then use Edit This Bookmark.
    this._element("editBookmarkPanelTitle").value =
      this._batching ?
        gNavigatorBundle.getString("editBookmarkPanel.pageBookmarkedTitle") :
        gNavigatorBundle.getString("editBookmarkPanel.editBookmarkTitle");

    // No description; show the Done, Cancel;
    this._element("editBookmarkPanelDescription").textContent = "";
    this._element("editBookmarkPanelBottomButtons").hidden = false;
    this._element("editBookmarkPanelContent").hidden = false;

    // The remove button is shown only if we're not already batching, i.e.
    // if the cancel button/ESC does not remove the bookmark.
    this._element("editBookmarkPanelRemoveButton").hidden = this._batching;

    // The label of the remove button differs if the URI is bookmarked
    // multiple times.
    var bookmarks = PlacesUtils.getBookmarksForURI(gBrowser.currentURI);
    var forms = gNavigatorBundle.getString("editBookmark.removeBookmarks.label");
    var label = PluralForm.get(bookmarks.length, forms).replace("#1", bookmarks.length);
    this._element("editBookmarkPanelRemoveButton").label = label;

    // unset the unstarred state, if set
    this._element("editBookmarkPanelStarIcon").removeAttribute("unstarred");

    this._itemId = aItemId !== undefined ? aItemId : this._itemId;
    this.beginBatch();

    this.panel.openPopup(aAnchorElement, aPosition);

    gEditItemOverlay.initPanel(this._itemId,
                               { hiddenRows: ["description", "location",
                                              "loadInSidebar", "keyword"] });
  },

  panelShown:
  function SU_panelShown(aEvent) {
    if (aEvent.target == this.panel) {
      if (!this._element("editBookmarkPanelContent").hidden) {
        let fieldToFocus = "editBMPanel_" +
          gPrefService.getCharPref("browser.bookmarks.editDialog.firstEditField");
        var elt = this._element(fieldToFocus);
        elt.focus();
        elt.select();
      }
      else {
        // Note this isn't actually used anymore, we should remove this
        // once we decide not to bring back the page bookmarked notification
        this.panel.focus();
      }
    }
  },

  quitEditMode: function SU_quitEditMode() {
    this._element("editBookmarkPanelContent").hidden = true;
    this._element("editBookmarkPanelBottomButtons").hidden = true;
    gEditItemOverlay.uninitPanel(true);
  },

  cancelButtonOnCommand: function SU_cancelButtonOnCommand() {
    this._actionOnHide = "cancel";
    this.panel.hidePopup();
  },

  removeBookmarkButtonCommand: function SU_removeBookmarkButtonCommand() {
    this._uriForRemoval = PlacesUtils.bookmarks.getBookmarkURI(this._itemId);
    this._actionOnHide = "remove";
    this.panel.hidePopup();
  },

  beginBatch: function SU_beginBatch() {
    if (!this._batching) {
      PlacesUtils.transactionManager.beginBatch(null);
      this._batching = true;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//// PlacesCommandHook

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
        let isErrorPage = /^about:(neterror|certerror|blocked)/
                          .test(webNav.document.documentURI);
        title = isErrorPage ? PlacesUtils.history.getPageTitle(url)
                            : webNav.document.title;
        title = title || url.spec;
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
      var descAnno = { name: PlacesUIUtils.DESCRIPTION_ANNO, value: description };
      var txn = new PlacesCreateBookmarkTransaction(uri, parent, 
                                                    PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                    title, null, [descAnno]);
      PlacesUtils.transactionManager.doTransaction(txn);
      itemId = txn.item.id;
      // Set the character-set
      if (charset && !PrivateBrowsingUtils.isWindowPrivate(aBrowser.contentWindow))
        PlacesUtils.setCharsetForURI(uri, charset);
    }

    // Revert the contents of the location bar
    if (gURLBar)
      gURLBar.handleRevert();

    // If it was not requested to open directly in "edit" mode, we are done.
    if (!aShowEditUI)
      return;

    // Try to dock the panel to:
    // 1. the bookmarks menu button
    // 2. the page-proxy-favicon
    // 3. the content area
    if (BookmarksMenuButton.anchor) {
      StarUI.showEditBookmarkPopup(itemId, BookmarksMenuButton.anchor,
                                   "bottomcenter topright");
      return;
    }

    let pageProxyFavicon = document.getElementById("page-proxy-favicon");
    if (isElementVisible(pageProxyFavicon)) {
      StarUI.showEditBookmarkPopup(itemId, pageProxyFavicon,
                                   "bottomcenter topright");
    } else {
      StarUI.showEditBookmarkPopup(itemId, aBrowser, "overlap");
    }
  },

  /**
   * Adds a bookmark to the page loaded in the current tab. 
   */
  bookmarkCurrentPage: function PCH_bookmarkCurrentPage(aShowEditUI, aParent) {
    this.bookmarkPage(gBrowser.selectedBrowser, aParent, aShowEditUI);
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
    if (itemId == -1) {
      PlacesUIUtils.showBookmarkDialog({ action: "add"
                                       , type: "bookmark"
                                       , uri: linkURI
                                       , title: aTitle
                                       , hiddenRows: [ "description"
                                                     , "location"
                                                     , "loadInSidebar"
                                                     , "keyword" ]
                                       }, window);
    }
    else {
      PlacesUIUtils.showBookmarkDialog({ action: "edit"
                                       , type: "bookmark"
                                       , itemId: itemId
                                       }, window);
    }
  },

  /**
   * List of nsIURI objects characterizing the tabs currently open in the
   * browser, modulo pinned tabs.  The URIs will be in the order in which their
   * corresponding tabs appeared and duplicates are discarded.
   */
  get uniqueCurrentPages() {
    let uniquePages = {};
    let URIs = [];
    gBrowser.visibleTabs.forEach(function (tab) {
      let spec = tab.linkedBrowser.currentURI.spec;
      if (!tab.pinned && !(spec in uniquePages)) {
        uniquePages[spec] = null;
        URIs.push(tab.linkedBrowser.currentURI);
      }
    });
    return URIs;
  },

  /**
   * Adds a folder with bookmarks to all of the currently open tabs in this 
   * window.
   */
  bookmarkCurrentPages: function PCH_bookmarkCurrentPages() {
    let pages = this.uniqueCurrentPages;
    if (pages.length > 1) {
    PlacesUIUtils.showBookmarkDialog({ action: "add"
                                     , type: "folder"
                                     , URIList: pages
                                     , hiddenRows: [ "description" ]
                                     }, window);
    }
  },

  /**
   * Updates disabled state for the "Bookmark All Tabs" command.
   */
  updateBookmarkAllTabsCommand:
  function PCH_updateBookmarkAllTabsCommand() {
    // There's nothing to do in non-browser windows.
    if (window.location.href != getBrowserURL())
      return;

    // Disable "Bookmark All Tabs" if there are less than two
    // "unique current pages".
    goSetCommandEnabled("Browser:BookmarkAllTabs",
                        this.uniqueCurrentPages.length >= 2);
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
    var feedURI = makeURI(url);
    
    var doc = gBrowser.contentDocument;
    var title = (arguments.length > 1) ? feedTitle : doc.title;
 
    var description;
    if (arguments.length > 2)
      description = feedSubtitle;
    else
      description = PlacesUIUtils.getDescriptionFromDocument(doc);

    var toolbarIP = new InsertionPoint(PlacesUtils.toolbarFolderId, -1);
    PlacesUIUtils.showBookmarkDialog({ action: "add"
                                     , type: "livemark"
                                     , feedURI: feedURI
                                     , siteURI: gBrowser.currentURI
                                     , title: title
                                     , description: description
                                     , defaultInsertionPoint: toolbarIP
                                     , hiddenRows: [ "feedLocation"
                                                   , "siteLocation"
                                                   , "description" ]
                                     }, window);
  },

  /**
   * Opens the Places Organizer. 
   * @param   aLeftPaneRoot
   *          The query to select in the organizer window - options
   *          are: History, AllBookmarks, BookmarksMenu, BookmarksToolbar,
   *          UnfiledBookmarks, Tags and Downloads.
   */
  showPlacesOrganizer: function PCH_showPlacesOrganizer(aLeftPaneRoot) {
    var organizer = Services.wm.getMostRecentWindow("Places:Organizer");
    if (!organizer) {
      // No currently open places window, so open one with the specified mode.
      openDialog("chrome://browser/content/places/places.xul", 
                 "", "chrome,toolbar=yes,dialog=no,resizable", aLeftPaneRoot);
    }
    else {
      organizer.PlacesOrganizer.selectLeftPaneQuery(aLeftPaneRoot);
      organizer.focus();
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
//// HistoryMenu

// View for the history menu.
function HistoryMenu(aPopupShowingEvent) {
  // Workaround for Bug 610187.  The sidebar does not include all the Places
  // views definitions, and we don't need them there.
  // Defining the prototype inheritance in the prototype itself would cause
  // browser.js to halt on "PlacesMenu is not defined" error.
  this.__proto__.__proto__ = PlacesMenu.prototype;
  XPCOMUtils.defineLazyServiceGetter(this, "_ss",
                                     "@mozilla.org/browser/sessionstore;1",
                                     "nsISessionStore");
  PlacesMenu.call(this, aPopupShowingEvent,
                  "place:sort=4&maxResults=15");
}

HistoryMenu.prototype = {
  toggleRecentlyClosedTabs: function HM_toggleRecentlyClosedTabs() {
    // enable/disable the Recently Closed Tabs sub menu
    var undoMenu = this._rootElt.getElementsByClassName("recentlyClosedTabsMenu")[0];

    // no restorable tabs, so disable menu
    if (this._ss.getClosedTabCount(window) == 0)
      undoMenu.setAttribute("disabled", true);
    else
      undoMenu.removeAttribute("disabled");
  },

  /**
    * Re-open a closed tab and put it to the end of the tab strip.
    * Used for a middle click.
    * @param aEvent
    *        The event when the user clicks the menu item
    */
  _undoCloseMiddleClick: function PHM__undoCloseMiddleClick(aEvent) {
    if (aEvent.button != 1)
      return;

    undoCloseTab(aEvent.originalTarget.value);
    gBrowser.moveTabToEnd();
  },

  /**
   * Populate when the history menu is opened
   */
  populateUndoSubmenu: function PHM_populateUndoSubmenu() {
    var undoMenu = this._rootElt.getElementsByClassName("recentlyClosedTabsMenu")[0];
    var undoPopup = undoMenu.firstChild;

    // remove existing menu items
    while (undoPopup.hasChildNodes())
      undoPopup.removeChild(undoPopup.firstChild);

    // no restorable tabs, so make sure menu is disabled, and return
    if (this._ss.getClosedTabCount(window) == 0) {
      undoMenu.setAttribute("disabled", true);
      return;
    }

    // enable menu
    undoMenu.removeAttribute("disabled");

    // populate menu
    var undoItems = JSON.parse(this._ss.getClosedTabData(window));
    for (var i = 0; i < undoItems.length; i++) {
      var m = document.createElement("menuitem");
      m.setAttribute("label", undoItems[i].title);
      if (undoItems[i].image) {
        let iconURL = undoItems[i].image;
        // don't initiate a connection just to fetch a favicon (see bug 467828)
        if (/^https?:/.test(iconURL))
          iconURL = "moz-anno:favicon:" + iconURL;
        m.setAttribute("image", iconURL);
      }
      m.setAttribute("class", "menuitem-iconic bookmark-item menuitem-with-favicon");
      m.setAttribute("value", i);
      m.setAttribute("oncommand", "undoCloseTab(" + i + ");");

      // Set the targetURI attribute so it will be shown in tooltip and trigger
      // onLinkHovered. SessionStore uses one-based indexes, so we need to
      // normalize them.
      let tabData = undoItems[i].state;
      let activeIndex = (tabData.index || tabData.entries.length) - 1;
      if (activeIndex >= 0 && tabData.entries[activeIndex])
        m.setAttribute("targetURI", tabData.entries[activeIndex].url);

      m.addEventListener("click", this._undoCloseMiddleClick, false);
      if (i == 0)
        m.setAttribute("key", "key_undoCloseTab");
      undoPopup.appendChild(m);
    }

    // "Restore All Tabs"
    var strings = gNavigatorBundle;
    undoPopup.appendChild(document.createElement("menuseparator"));
    m = undoPopup.appendChild(document.createElement("menuitem"));
    m.id = "menu_restoreAllTabs";
    m.setAttribute("label", strings.getString("menuRestoreAllTabs.label"));
    m.addEventListener("command", function() {
      for (var i = 0; i < undoItems.length; i++)
        undoCloseTab();
    }, false);
  },

  toggleRecentlyClosedWindows: function PHM_toggleRecentlyClosedWindows() {
    // enable/disable the Recently Closed Windows sub menu
    var undoMenu = this._rootElt.getElementsByClassName("recentlyClosedWindowsMenu")[0];

    // no restorable windows, so disable menu
    if (this._ss.getClosedWindowCount() == 0)
      undoMenu.setAttribute("disabled", true);
    else
      undoMenu.removeAttribute("disabled");
  },

  /**
   * Populate when the history menu is opened
   */
  populateUndoWindowSubmenu: function PHM_populateUndoWindowSubmenu() {
    let undoMenu = this._rootElt.getElementsByClassName("recentlyClosedWindowsMenu")[0];
    let undoPopup = undoMenu.firstChild;
    let menuLabelString = gNavigatorBundle.getString("menuUndoCloseWindowLabel");
    let menuLabelStringSingleTab =
      gNavigatorBundle.getString("menuUndoCloseWindowSingleTabLabel");

    // remove existing menu items
    while (undoPopup.hasChildNodes())
      undoPopup.removeChild(undoPopup.firstChild);

    // no restorable windows, so make sure menu is disabled, and return
    if (this._ss.getClosedWindowCount() == 0) {
      undoMenu.setAttribute("disabled", true);
      return;
    }

    // enable menu
    undoMenu.removeAttribute("disabled");

    // populate menu
    let undoItems = JSON.parse(this._ss.getClosedWindowData());
    for (let i = 0; i < undoItems.length; i++) {
      let undoItem = undoItems[i];
      let otherTabsCount = undoItem.tabs.length - 1;
      let label = (otherTabsCount == 0) ? menuLabelStringSingleTab
                                        : PluralForm.get(otherTabsCount, menuLabelString);
      let menuLabel = label.replace("#1", undoItem.title)
                           .replace("#2", otherTabsCount);
      let m = document.createElement("menuitem");
      m.setAttribute("label", menuLabel);
      let selectedTab = undoItem.tabs[undoItem.selected - 1];
      if (selectedTab.image) {
        let iconURL = selectedTab.image;
        // don't initiate a connection just to fetch a favicon (see bug 467828)
        if (/^https?:/.test(iconURL))
          iconURL = "moz-anno:favicon:" + iconURL;
        m.setAttribute("image", iconURL);
      }
      m.setAttribute("class", "menuitem-iconic bookmark-item menuitem-with-favicon");
      m.setAttribute("oncommand", "undoCloseWindow(" + i + ");");

      // Set the targetURI attribute so it will be shown in tooltip.
      // SessionStore uses one-based indexes, so we need to normalize them.
      let activeIndex = (selectedTab.index || selectedTab.entries.length) - 1;
      if (activeIndex >= 0 && selectedTab.entries[activeIndex])
        m.setAttribute("targetURI", selectedTab.entries[activeIndex].url);

      if (i == 0)
        m.setAttribute("key", "key_undoCloseWindow");
      undoPopup.appendChild(m);
    }

    // "Open All in Windows"
    undoPopup.appendChild(document.createElement("menuseparator"));
    let m = undoPopup.appendChild(document.createElement("menuitem"));
    m.id = "menu_restoreAllWindows";
    m.setAttribute("label", gNavigatorBundle.getString("menuRestoreAllWindows.label"));
    m.setAttribute("oncommand",
      "for (var i = 0; i < " + undoItems.length + "; i++) undoCloseWindow();");
  },

  toggleTabsFromOtherComputers: function PHM_toggleTabsFromOtherComputers() {
    // This is a no-op if MOZ_SERVICES_SYNC isn't defined
#ifdef MOZ_SERVICES_SYNC
    // Enable/disable the Tabs From Other Computers menu. Some of the menus handled
    // by HistoryMenu do not have this menuitem.
    let menuitem = this._rootElt.getElementsByClassName("syncTabsMenuItem")[0];
    if (!menuitem)
      return;

    // If Sync isn't configured yet, then don't show the menuitem.
    if (Weave.Status.checkSetup() == Weave.CLIENT_NOT_CONFIGURED ||
        Weave.Svc.Prefs.get("firstSync", "") == "notReady") {
      menuitem.setAttribute("hidden", true);
      return;
    }

    // The tabs engine might never be inited (if services.sync.registerEngines
    // is modified), so make sure we avoid undefined errors.
    let enabled = Weave.Service.isLoggedIn &&
                  Weave.Service.engineManager.get("tabs") &&
                  Weave.Service.engineManager.get("tabs").enabled;
    menuitem.setAttribute("disabled", !enabled);
    menuitem.setAttribute("hidden", false);
#endif
  },

  _onPopupShowing: function HM__onPopupShowing(aEvent) {
    PlacesMenu.prototype._onPopupShowing.apply(this, arguments);

    // Don't handle events for submenus.
    if (aEvent.target != aEvent.currentTarget)
      return;

    this.toggleRecentlyClosedTabs();
    this.toggleRecentlyClosedWindows();
    this.toggleTabsFromOtherComputers();
  },

  _onCommand: function HM__onCommand(aEvent) {
    let placesNode = aEvent.target._placesNode;
    if (placesNode) {
      if (!PrivateBrowsingUtils.isWindowPrivate(window))
        PlacesUIUtils.markPageAsTyped(placesNode.uri);
      openUILink(placesNode.uri, aEvent, { ignoreAlt: true });
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
//// BookmarksEventHandler

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
   * @param aView
   *        The places view which aEvent should be associated with.
   */
  onClick: function BEH_onClick(aEvent, aView) {
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
        else if (node.localName != "menu" &&
                 node.localName != "splitmenu" &&
                 node.localName != "hbox" &&
                 node.localName != "vbox" )
          break;
      }
    }

    if (target._placesNode && PlacesUtils.nodeIsContainer(target._placesNode)) {
      // Don't open the root folder in tabs when the empty area on the toolbar
      // is middle-clicked or when a non-bookmark item except for Open in Tabs)
      // in a bookmarks menupopup is middle-clicked.
      if (target.localName == "menu" || target.localName == "toolbarbutton")
        PlacesUIUtils.openContainerNodeInTabs(target._placesNode, aEvent, aView);
    }
    else if (aEvent.button == 1) {
      // left-clicks with modifier are already served by onCommand
      this.onCommand(aEvent, aView);
    }
  },

  /**
   * Handler for command event for an item in the bookmarks toolbar.
   * Menus and submenus from the folder buttons bubble up to this handler.
   * Opens the item.
   * @param aEvent 
   *        DOMEvent for the command
   * @param aView
   *        The places view which aEvent should be associated with.
   */
  onCommand: function BEH_onCommand(aEvent, aView) {
    var target = aEvent.originalTarget;
    if (target._placesNode)
      PlacesUIUtils.openNodeWithEvent(target._placesNode, aEvent, aView);
  },

  fillInBHTooltip: function BEH_fillInBHTooltip(aDocument, aEvent) {
    var node;
    var cropped = false;
    var targetURI;

    if (aDocument.tooltipNode.localName == "treechildren") {
      var tree = aDocument.tooltipNode.parentNode;
      var row = {}, column = {};
      var tbo = tree.treeBoxObject;
      tbo.getCellAt(aEvent.clientX, aEvent.clientY, row, column, {});
      if (row.value == -1)
        return false;
      node = tree.view.nodeForTreeIndex(row.value);
      cropped = tbo.isCellCropped(row.value, column.value);
    }
    else {
      // Check whether the tooltipNode is a Places node.
      // In such a case use it, otherwise check for targetURI attribute.
      var tooltipNode = aDocument.tooltipNode;
      if (tooltipNode._placesNode)
        node = tooltipNode._placesNode;
      else {
        // This is a static non-Places node.
        targetURI = tooltipNode.getAttribute("targetURI");
      }
    }

    if (!node && !targetURI)
      return false;

    // Show node.label as tooltip's title for non-Places nodes.
    var title = node ? node.title : tooltipNode.label;

    // Show URL only for Places URI-nodes or nodes with a targetURI attribute.
    var url;
    if (targetURI || PlacesUtils.nodeIsURI(node))
      url = targetURI || node.uri;

    // Show tooltip for containers only if their title is cropped.
    if (!cropped && !url)
      return false;

    var tooltipTitle = aDocument.getElementById("bhtTitleText");
    tooltipTitle.hidden = (!title || (title == url));
    if (!tooltipTitle.hidden)
      tooltipTitle.textContent = title;

    var tooltipUrl = aDocument.getElementById("bhtUrlText");
    tooltipUrl.hidden = !url;
    if (!tooltipUrl.hidden)
      tooltipUrl.value = url;

    // Show tooltip.
    return true;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// PlacesMenuDNDHandler

// Handles special drag and drop functionality for Places menus that are not
// part of a Places view (e.g. the bookmarks menu in the menubar).
var PlacesMenuDNDHandler = {
  _springLoadDelay: 350, // milliseconds
  _loadTimer: null,
  _closerTimer: null,

  /**
   * Called when the user enters the <menu> element during a drag.
   * @param   event
   *          The DragEnter event that spawned the opening. 
   */
  onDragEnter: function PMDH_onDragEnter(event) {
    // Opening menus in a Places popup is handled by the view itself.
    if (!this._isStaticContainer(event.target))
      return;

    let popup = event.target.lastChild;
    if (this._loadTimer || popup.state === "showing" || popup.state === "open")
      return;

    this._loadTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._loadTimer.initWithCallback(() => {
      this._loadTimer = null;
      popup.setAttribute("autoopened", "true");
      popup.showPopup(popup);
    }, this._springLoadDelay, Ci.nsITimer.TYPE_ONE_SHOT);
    event.preventDefault();
    event.stopPropagation();
  },

  /**
   * Handles dragleave on the <menu> element.
   * @returns true if the element is a container element (menu or 
   *          menu-toolbarbutton), false otherwise.
   */
  onDragLeave: function PMDH_onDragLeave(event) {
    // Handle menu-button separate targets.
    if (event.relatedTarget === event.currentTarget ||
        event.relatedTarget.parentNode === event.currentTarget)
      return;

    // Closing menus in a Places popup is handled by the view itself.
    if (!this._isStaticContainer(event.target))
      return;

    let popup = event.target.lastChild;

    if (this._loadTimer) {
      this._loadTimer.cancel();
      this._loadTimer = null;
    }
    this._closeTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._closeTimer.initWithCallback(function() {
      this._closeTimer = null;
      let node = PlacesControllerDragHelper.currentDropTarget;
      let inHierarchy = false;
      while (node && !inHierarchy) {
        inHierarchy = node == event.target;
        node = node.parentNode;
      }
      if (!inHierarchy && popup && popup.hasAttribute("autoopened")) {
        popup.removeAttribute("autoopened");
        popup.hidePopup();
      }
    }, this._springLoadDelay, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /**
   * Determines if a XUL element represents a static container.
   * @returns true if the element is a container element (menu or 
   *`         menu-toolbarbutton), false otherwise.
   */
  _isStaticContainer: function PMDH__isContainer(node) {
    let isMenu = node.localName == "menu" ||
                 (node.localName == "toolbarbutton" &&
                  (node.getAttribute("type") == "menu" ||
                   node.getAttribute("type") == "menu-button"));
    let isStatic = !("_placesNode" in node) && node.lastChild &&
                   node.lastChild.hasAttribute("placespopup") &&
                   !node.parentNode.hasAttribute("placespopup");
    return isMenu && isStatic;
  },

  /**
   * Called when the user drags over the <menu> element.
   * @param   event
   *          The DragOver event. 
   */
  onDragOver: function PMDH_onDragOver(event) {
    let ip = new InsertionPoint(PlacesUtils.bookmarksMenuFolderId,
                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                Ci.nsITreeView.DROP_ON);
    if (ip && PlacesControllerDragHelper.canDrop(ip, event.dataTransfer))
      event.preventDefault();

    event.stopPropagation();
  },

  /**
   * Called when the user drops on the <menu> element.
   * @param   event
   *          The Drop event. 
   */
  onDrop: function PMDH_onDrop(event) {
    // Put the item at the end of bookmark menu.
    let ip = new InsertionPoint(PlacesUtils.bookmarksMenuFolderId,
                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                Ci.nsITreeView.DROP_ON);
    PlacesControllerDragHelper.onDrop(ip, event.dataTransfer);
    event.stopPropagation();
  }
};

////////////////////////////////////////////////////////////////////////////////
//// PlacesToolbarHelper

/**
 * This object handles the initialization and uninitialization of the bookmarks
 * toolbar.
 */
let PlacesToolbarHelper = {
  _place: "place:folder=TOOLBAR",

  get _viewElt() {
    return document.getElementById("PlacesToolbar");
  },

  init: function PTH_init() {
    let viewElt = this._viewElt;
    if (!viewElt || viewElt._placesView)
      return;

    // If the bookmarks toolbar item is hidden because the parent toolbar is
    // collapsed or hidden (i.e. in a popup), spare the initialization.  Also,
    // there is no need to initialize the toolbar if customizing because
    // init() will be called when the customization is done.
    let toolbar = viewElt.parentNode.parentNode;
    if (toolbar.collapsed ||
        getComputedStyle(toolbar, "").display == "none" ||
        this._isCustomizing)
      return;

    new PlacesToolbar(this._place);
  },

  customizeStart: function PTH_customizeStart() {
    let viewElt = this._viewElt;
    if (viewElt && viewElt._placesView)
      viewElt._placesView.uninit();

    this._isCustomizing = true;
  },

  customizeDone: function PTH_customizeDone() {
    this._isCustomizing = false;
    this.init();
  }
};

////////////////////////////////////////////////////////////////////////////////
//// BookmarksMenuButton

/**
 * Handles the bookmarks menu-button in the toolbar.
 */
let BookmarksMenuButton = {
  get button() {
    if (!this._button) {
      this._button = document.getElementById("bookmarks-menu-button");
    }
    return this._button;
  },

  get star() {
    if (!this._star && this.button) {
      this._star = document.getAnonymousElementByAttribute(this.button,
                                                           "anonid",
                                                           "button");
    }
    return this._star;
  },

  get anchor() {
    if (!this._anchor && this.star && isElementVisible(this.star)) {
      // Anchor to the icon, so the panel looks more natural.
      this._anchor = document.getAnonymousElementByAttribute(this.star,
                                                             "class",
                                                             "toolbarbutton-icon");
    }
    return this._anchor;
  },

  STATUS_UPDATING: -1,
  STATUS_UNSTARRED: 0,
  STATUS_STARRED: 1,
  get status() {
    if (this._pendingStmt)
      return this.STATUS_UPDATING;
    return this.button &&
           this.button.hasAttribute("starred") ? this.STATUS_STARRED
                                               : this.STATUS_UNSTARRED;
  },

  get _starredTooltip()
  {
    delete this._starredTooltip;
    return this._starredTooltip =
      gNavigatorBundle.getString("starButtonOn.tooltip");
  },

  get _unstarredTooltip()
  {
    delete this._unstarredTooltip;
    return this._unstarredTooltip =
      gNavigatorBundle.getString("starButtonOff.tooltip");
  },

  /**
   * The popup contents must be updated when the user customizes the UI, or
   * changes the personal toolbar collapsed status.  In such a case, any needed
   * change should be handled in the popupshowing helper, for performance
   * reasons.
   */
  _popupNeedsUpdate: true,
  onToolbarVisibilityChange: function BMB_onToolbarVisibilityChange() {
    this._popupNeedsUpdate = true;
  },

  onPopupShowing: function BMB_onPopupShowing(event) {
    // Don't handle events for submenus.
    if (event.target != event.currentTarget)
      return;

    if (!this._popupNeedsUpdate)
      return;
    this._popupNeedsUpdate = false;

    let popup = event.target;
    let getPlacesAnonymousElement =
      aAnonId => document.getAnonymousElementByAttribute(popup.parentNode,
                                                         "placesanonid",
                                                         aAnonId);

    let viewToolbarMenuitem = getPlacesAnonymousElement("view-toolbar");
    if (viewToolbarMenuitem) {
      // Update View bookmarks toolbar checkbox menuitem.
      let personalToolbar = document.getElementById("PersonalToolbar");
      viewToolbarMenuitem.setAttribute("checked", !personalToolbar.collapsed);
    }

    let toolbarMenuitem = getPlacesAnonymousElement("toolbar-autohide");
    if (toolbarMenuitem) {
      // If bookmarks items are visible, hide Bookmarks Toolbar menu and the
      // separator after it.
      toolbarMenuitem.collapsed = toolbarMenuitem.nextSibling.collapsed =
        isElementVisible(document.getElementById("personal-bookmarks"));
    }
  },

  /**
   * Handles star styling based on page proxy state changes.
   */
  onPageProxyStateChanged: function BMB_onPageProxyStateChanged(aState) {
    if (!this.star) {
      return;
    }

    if (aState == "invalid") {
      this.star.setAttribute("disabled", "true");
      this.button.removeAttribute("starred");
    }
    else {
      this.star.removeAttribute("disabled");
    }
    this._updateStyle();
  },

  _updateStyle: function BMB__updateStyle() {
    if (!this.star) {
      return;
    }

    let personalToolbar = document.getElementById("PersonalToolbar");
    let onPersonalToolbar = this.button.parentNode == personalToolbar ||
                            this.button.parentNode.parentNode == personalToolbar;

    if (onPersonalToolbar) {
      this.button.classList.add("bookmark-item");
      this.button.classList.remove("toolbarbutton-1");
    }
    else {
      this.button.classList.remove("bookmark-item");
      this.button.classList.add("toolbarbutton-1");
    }
  },

  _uninitView: function BMB__uninitView() {
    // When an element with a placesView attached is removed and re-inserted,
    // XBL reapplies the binding causing any kind of issues and possible leaks,
    // so kill current view and let popupshowing generate a new one.
    if (this.button && this.button._placesView) {
      this.button._placesView.uninit();
    }
  },

  customizeStart: function BMB_customizeStart() {
    this._uninitView();
  },

  customizeChange: function BMB_customizeChange() {
    this._updateStyle();
  },

  customizeDone: function BMB_customizeDone() {
    delete this._button;
    delete this._star;
    delete this._anchor;
    this.onToolbarVisibilityChange();
    this._updateStyle();
  },

  _hasBookmarksObserver: false,
  uninit: function BMB_uninit() {
    this._uninitView();

    if (this._hasBookmarksObserver) {
      PlacesUtils.removeLazyBookmarkObserver(this);
    }

    if (this._pendingStmt) {
      this._pendingStmt.cancel();
      delete this._pendingStmt;
    }
  },

  updateStarState: function BMB_updateStarState() {
    if (!this.button || (this._uri && gBrowser.currentURI.equals(this._uri))) {
      return;
    }

    // Reset tracked values.
    this._uri = gBrowser.currentURI;
    this._itemIds = [];

    if (this._pendingStmt) {
      this._pendingStmt.cancel();
      delete this._pendingStmt;
    }

    // We can load about:blank before the actual page, but there is no point in handling that page.
    if (isBlankPageURL(this._uri.spec)) {
      return;
    }

    this._pendingStmt = PlacesUtils.asyncGetBookmarkIds(this._uri, function (aItemIds, aURI) {
      // Safety check that the bookmarked URI equals the tracked one.
      if (!aURI.equals(this._uri)) {
        Components.utils.reportError("BookmarksMenuButton did not receive current URI");
        return;
      }

      // It's possible that onItemAdded gets called before the async statement
      // calls back.  For such an edge case, retain all unique entries from both
      // arrays.
      this._itemIds = this._itemIds.filter(
        function (id) aItemIds.indexOf(id) == -1
      ).concat(aItemIds);

      this._updateStar();

      // Start observing bookmarks if needed.
      if (!this._hasBookmarksObserver) {
        try {
          PlacesUtils.addLazyBookmarkObserver(this);
          this._hasBookmarksObserver = true;
        } catch(ex) {
          Components.utils.reportError("BookmarksMenuButton failed adding a bookmarks observer: " + ex);
        }
      }

      delete this._pendingStmt;
    }, this);
  },

  _updateStar: function BMB__updateStar() {
    if (!this.button) {
      return;
    }

    if (this._itemIds.length > 0) {
      this.button.setAttribute("starred", "true");
      this.button.setAttribute("tooltiptext", this._starredTooltip);
    }
    else {
      this.button.removeAttribute("starred");
      this.button.setAttribute("tooltiptext", this._unstarredTooltip);
    }
  },

  onCommand: function BMB_onCommand(aEvent) {
    if (aEvent.target != aEvent.currentTarget) {
      return;
    }
    // Ignore clicks on the star if we are updating its state.
    if (!this._pendingStmt) {
      PlacesCommandHook.bookmarkCurrentPage(this._itemIds.length > 0);
    }
  },

  // nsINavBookmarkObserver
  onItemAdded: function BMB_onItemAdded(aItemId, aParentId, aIndex, aItemType,
                                        aURI) {
    if (!this.button) {
      return;
    }

    if (aURI && aURI.equals(this._uri)) {
      // If a new bookmark has been added to the tracked uri, register it.
      if (this._itemIds.indexOf(aItemId) == -1) {
        this._itemIds.push(aItemId);
        this._updateStar();
      }
    }
  },

  onItemRemoved: function BMB_onItemRemoved(aItemId) {
    if (!this.button) {
      return;
    }

    let index = this._itemIds.indexOf(aItemId);
    // If one of the tracked bookmarks has been removed, unregister it.
    if (index != -1) {
      this._itemIds.splice(index, 1);
      this._updateStar();
    }
  },

  onItemChanged: function BMB_onItemChanged(aItemId, aProperty,
                                            aIsAnnotationProperty, aNewValue) {
    if (!this.button) {
      return;
    }

    if (aProperty == "uri") {
      let index = this._itemIds.indexOf(aItemId);
      // If the changed bookmark was tracked, check if it is now pointing to
      // a different uri and unregister it.
      if (index != -1 && aNewValue != this._uri.spec) {
        this._itemIds.splice(index, 1);
        this._updateStar();
      }
      // If another bookmark is now pointing to the tracked uri, register it.
      else if (index == -1 && aNewValue == this._uri.spec) {
        this._itemIds.push(aItemId);
        this._updateStar();
      }
    }
  },

  onBeginUpdateBatch: function () {},
  onEndUpdateBatch: function () {},
  onBeforeItemRemoved: function () {},
  onItemVisited: function () {},
  onItemMoved: function () {},

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver
  ]),
};

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var StarUI = {
  _itemId: -1,
  uri: null,
  _batching: false,
  _isNewBookmark: false,
  _isComposing: false,
  _autoCloseTimer: 0,
  // The autoclose timer is diasbled if the user interacts with the
  // popup, such as making a change through typing or clicking on
  // the popup.
  _autoCloseTimerEnabled: true,

  _element(aID) {
    return document.getElementById(aID);
  },

  // Edit-bookmark panel
  get panel() {
    delete this.panel;
    var element = this._element("editBookmarkPanel");
    // initially the panel is hidden
    // to avoid impacting startup / new window performance
    element.hidden = false;
    element.addEventListener("keypress", this);
    element.addEventListener("mousedown", this);
    element.addEventListener("mouseout", this);
    element.addEventListener("mousemove", this);
    element.addEventListener("compositionstart", this);
    element.addEventListener("compositionend", this);
    element.addEventListener("input", this);
    element.addEventListener("popuphidden", this);
    element.addEventListener("popupshown", this);
    return this.panel = element;
  },

  // Array of command elements to disable when the panel is opened.
  get _blockedCommands() {
    delete this._blockedCommands;
    return this._blockedCommands =
      ["cmd_close", "cmd_closeWindow"].map(id => this._element(id));
  },

  _blockCommands: function SU__blockCommands() {
    this._blockedCommands.forEach(function(elt) {
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
    this._blockedCommands.forEach(function(elt) {
      if (elt.getAttribute("wasDisabled") != "true")
        elt.removeAttribute("disabled");
      elt.removeAttribute("wasDisabled");
    });
  },

  // nsIDOMEventListener
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "mousemove":
        clearTimeout(this._autoCloseTimer);
        // The autoclose timer is not disabled on generic mouseout
        // because the user may not have actually interacted with the popup.
        break;
      case "popuphidden":
        clearTimeout(this._autoCloseTimer);
        if (aEvent.originalTarget == this.panel) {
          if (!this._element("editBookmarkPanelContent").hidden)
            this.quitEditMode();

          if (this._anchorToolbarButton) {
            this._anchorToolbarButton.removeAttribute("open");
            this._anchorToolbarButton = null;
          }
          this._restoreCommandsState();
          this._itemId = -1;
          if (this._batching)
            this.endBatch();

          if (this._uriForRemoval) {
            if (this._isNewBookmark) {
              if (!PlacesUtils.useAsyncTransactions) {
                PlacesUtils.transactionManager.undoTransaction();
                break;
              }
              PlacesTransactions().undo().catch(Cu.reportError);
              break;
            }
            // Remove all bookmarks for the bookmark's url, this also removes
            // the tags for the url.
            if (!PlacesUIUtils.useAsyncTransactions) {
              let itemIds = PlacesUtils.getBookmarksForURI(this._uriForRemoval);
              for (let itemId of itemIds) {
                let txn = new PlacesRemoveItemTransaction(itemId);
                PlacesUtils.transactionManager.doTransaction(txn);
              }
              break;
            }

            PlacesTransactions.RemoveBookmarksForUrls([this._uriForRemoval])
                              .transact().catch(Cu.reportError);
          }
        }
        break;
      case "keypress":
        clearTimeout(this._autoCloseTimer);
        this._autoCloseTimerEnabled = false;

        if (aEvent.defaultPrevented) {
          // The event has already been consumed inside of the panel.
          break;
        }

        switch (aEvent.keyCode) {
          case KeyEvent.DOM_VK_ESCAPE:
            this.panel.hidePopup();
            break;
          case KeyEvent.DOM_VK_RETURN:
            if (aEvent.target.classList.contains("expander-up") ||
                aEvent.target.classList.contains("expander-down") ||
                aEvent.target.id == "editBMPanel_newFolderButton" ||
                aEvent.target.id == "editBookmarkPanelRemoveButton") {
              // XXX Why is this necessary? The defaultPrevented check should
              //    be enough.
              break;
            }
            this.panel.hidePopup();
            break;
          // This case is for catching character-generating keypresses
          case 0:
            let accessKey = document.getElementById("key_close");
            if (eventMatchesKey(aEvent, accessKey)) {
                this.panel.hidePopup();
            }
            break;
        }
        break;
      case "compositionend":
        // After composition is committed, "mouseout" or something can set
        // auto close timer.
        this._isComposing = false;
        break;
      case "compositionstart":
        if (aEvent.defaultPrevented) {
          // If the composition was canceled, nothing to do here.
          break;
        }
        this._isComposing = true;
        // Explicit fall-through, during composition, panel shouldn't be
        // hidden automatically.
      case "input":
        // Might have edited some text without keyboard events nor composition
        // events. Fall-through to cancel auto close in such case.
      case "mousedown":
        clearTimeout(this._autoCloseTimer);
        this._autoCloseTimerEnabled = false;
        break;
      case "mouseout":
        if (!this._autoCloseTimerEnabled) {
          // Don't autoclose the popup if the user has made a selection
          // or keypress and then subsequently mouseout.
          break;
        }
        // Explicit fall-through
      case "popupshown":
        // Don't handle events for descendent elements.
        if (aEvent.target != aEvent.currentTarget) {
          break;
        }
        // auto-close if new and not interacted with
        if (this._isNewBookmark && !this._isComposing) {
          // 3500ms matches the timeout that Pocket uses in
          // browser/extensions/pocket/content/panels/js/saved.js
          let delay = 3500;
          if (this._closePanelQuickForTesting) {
            delay /= 10;
          }
          clearTimeout(this._autoCloseTimer);
          this._autoCloseTimer = setTimeout(() => {
            if (!this.panel.mozMatchesSelector(":hover")) {
              this.panel.hidePopup();
            }
          }, delay);
          this._autoCloseTimerEnabled = true;
        }
        break;
    }
  },

  _overlayLoaded: false,
  _overlayLoading: false,
  showEditBookmarkPopup: Task.async(function* (aNode, aAnchorElement, aPosition, aIsNewBookmark) {
    // Slow double-clicks (not true double-clicks) shouldn't
    // cause the panel to flicker.
    if (this.panel.state == "showing" ||
        this.panel.state == "open") {
      return;
    }

    this._isNewBookmark = aIsNewBookmark;
    this._uriForRemoval = "";
    // TODO: Deprecate this once async transactions are enabled and the legacy
    // transactions code is gone (bug 1131491) - we don't want addons to to use
    // the  completeNodeLikeObjectForItemId, so it's better if they keep passing
    // the item-id for now).
    if (typeof(aNode) == "number") {
      let itemId = aNode;
      if (PlacesUIUtils.useAsyncTransactions) {
        let guid = yield PlacesUtils.promiseItemGuid(itemId);
        aNode = yield PlacesUIUtils.promiseNodeLike(guid);
      } else {
        aNode = { itemId };
        yield PlacesUIUtils.completeNodeLikeObjectForItemId(aNode);
      }
    }

    // Performance: load the overlay the first time the panel is opened
    // (see bug 392443).
    if (this._overlayLoading)
      return;

    if (this._overlayLoaded) {
      this._doShowEditBookmarkPanel(aNode, aAnchorElement, aPosition);
      return;
    }

    this._overlayLoading = true;
    document.loadOverlay(
      "chrome://browser/content/places/editBookmarkOverlay.xul",
      (function(aSubject, aTopic, aData) {
        // Move the header (star, title, button) into the grid,
        // so that it aligns nicely with the other items (bug 484022).
        let header = this._element("editBookmarkPanelHeader");
        let rows = this._element("editBookmarkPanelGrid").lastChild;
        rows.insertBefore(header, rows.firstChild);
        header.hidden = false;

        this._overlayLoading = false;
        this._overlayLoaded = true;
        this._doShowEditBookmarkPanel(aNode, aAnchorElement, aPosition);
      }).bind(this)
    );
  }),

  _doShowEditBookmarkPanel: Task.async(function* (aNode, aAnchorElement, aPosition) {
    if (this.panel.state != "closed")
      return;

    this._blockCommands(); // un-done in the popuphidden handler

    this._element("editBookmarkPanelTitle").value =
      this._isNewBookmark ?
        gNavigatorBundle.getString("editBookmarkPanel.pageBookmarkedTitle") :
        gNavigatorBundle.getString("editBookmarkPanel.editBookmarkTitle");

    // No description; show the Done, Remove;
    this._element("editBookmarkPanelDescription").textContent = "";
    this._element("editBookmarkPanelBottomButtons").hidden = false;
    this._element("editBookmarkPanelContent").hidden = false;

    // The label of the remove button differs if the URI is bookmarked
    // multiple times.
    let bookmarks = PlacesUtils.getBookmarksForURI(gBrowser.currentURI);
    let forms = gNavigatorBundle.getString("editBookmark.removeBookmarks.label");
    let label = PluralForm.get(bookmarks.length, forms).replace("#1", bookmarks.length);
    this._element("editBookmarkPanelRemoveButton").label = label;

    // unset the unstarred state, if set
    this._element("editBookmarkPanelStarIcon").removeAttribute("unstarred");

    this._itemId = aNode.itemId;
    this.beginBatch();

    if (aAnchorElement) {
      // Set the open=true attribute if the anchor is a
      // descendent of a toolbarbutton.
      let parent = aAnchorElement.parentNode;
      while (parent) {
        if (parent.localName == "toolbarbutton") {
          break;
        }
        parent = parent.parentNode;
      }
      if (parent) {
        this._anchorToolbarButton = parent;
        parent.setAttribute("open", "true");
      }
    }
    let onPanelReady = fn => {
      let target = this.panel;
      if (target.parentNode) {
        // By targeting the panel's parent and using a capturing listener, we
        // can have our listener called before others waiting for the panel to
        // be shown (which probably expect the panel to be fully initialized)
        target = target.parentNode;
      }
      target.addEventListener("popupshown", function(event) {
        fn();
      }, {"capture": true, "once": true});
    };
    gEditItemOverlay.initPanel({ node: aNode
                               , onPanelReady
                               , hiddenRows: ["description", "location",
                                              "loadInSidebar", "keyword"]
                               , focusedElement: "preferred"});

    this.panel.openPopup(aAnchorElement, aPosition);
  }),

  panelShown:
  function SU_panelShown(aEvent) {
    if (aEvent.target == this.panel) {
      if (this._element("editBookmarkPanelContent").hidden) {
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

  removeBookmarkButtonCommand: function SU_removeBookmarkButtonCommand() {
    this._uriForRemoval = PlacesUtils.bookmarks.getBookmarkURI(this._itemId);
    this.panel.hidePopup();
  },

  // Matching the way it is used in the Library, editBookmarkOverlay implements
  // an instant-apply UI, having no batched-Undo/Redo support.
  // However, in this context (the Star UI) we have a Cancel button whose
  // expected behavior is to undo all the operations done in the panel.
  // Sometime in the future this needs to be reimplemented using a
  // non-instant apply code path, but for the time being, we patch-around
  // editBookmarkOverlay so that all of the actions done in the panel
  // are treated by PlacesTransactions as a single batch.  To do so,
  // we start a PlacesTransactions batch when the star UI panel is shown, and
  // we keep the batch ongoing until the panel is hidden.
  _batchBlockingDeferred: null,
  beginBatch() {
    if (this._batching)
      return;
    if (PlacesUIUtils.useAsyncTransactions) {
      this._batchBlockingDeferred = PromiseUtils.defer();
      PlacesTransactions.batch(function* () {
        yield this._batchBlockingDeferred.promise;
      }.bind(this));
    } else {
      PlacesUtils.transactionManager.beginBatch(null);
    }
    this._batching = true;
  },

  endBatch() {
    if (!this._batching)
      return;

    if (PlacesUIUtils.useAsyncTransactions) {
      this._batchBlockingDeferred.resolve();
      this._batchBlockingDeferred = null;
    } else {
      PlacesUtils.transactionManager.endBatch(false);
    }
    this._batching = false;
  }
};

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
  bookmarkPage: Task.async(function* (aBrowser, aParent, aShowEditUI) {
    if (PlacesUIUtils.useAsyncTransactions) {
      yield this._bookmarkPagePT(aBrowser, aParent, aShowEditUI);
      return;
    }

    var uri = aBrowser.currentURI;
    var itemId = PlacesUtils.getMostRecentBookmarkForURI(uri);
    let isNewBookmark = itemId == -1;
    if (isNewBookmark) {
      // Bug 1148838 - Make this code work for full page plugins.
      var title;
      var description;
      var charset;

      let docInfo = yield this._getPageDetails(aBrowser);

      try {
        title = docInfo.isErrorPage ? PlacesUtils.history.getPageTitle(uri)
                                    : aBrowser.contentTitle;
        title = title || uri.spec;
        description = docInfo.description;
        charset = aBrowser.characterSet;
      } catch (e) { }

      if (aShowEditUI && isNewBookmark) {
        // If we bookmark the page here but open right into a cancelable
        // state (i.e. new bookmark in Library), start batching here so
        // all of the actions can be undone in a single undo step.
        StarUI.beginBatch();
      }

      var parent = aParent !== undefined ?
                   aParent : PlacesUtils.unfiledBookmarksFolderId;
      var descAnno = { name: PlacesUIUtils.DESCRIPTION_ANNO, value: description };
      var txn = new PlacesCreateBookmarkTransaction(uri, parent,
                                                    PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                    title, null, [descAnno]);
      PlacesUtils.transactionManager.doTransaction(txn);
      itemId = txn.item.id;
      // Set the character-set.
      if (charset && !PrivateBrowsingUtils.isBrowserPrivate(aBrowser))
        PlacesUtils.setCharsetForURI(uri, charset);
    }

    // Revert the contents of the location bar
    gURLBar.handleRevert();

    // If it was not requested to open directly in "edit" mode, we are done.
    if (!aShowEditUI)
      return;

    // Try to dock the panel to:
    // 1. the bookmarks menu button
    // 2. the identity icon
    // 3. the content area
    if (BookmarkingUI.anchor) {
      StarUI.showEditBookmarkPopup(itemId, BookmarkingUI.anchor,
                                   "bottomcenter topright", isNewBookmark);
      return;
    }

    let identityIcon = document.getElementById("identity-icon");
    if (isElementVisible(identityIcon)) {
      StarUI.showEditBookmarkPopup(itemId, identityIcon,
                                   "bottomcenter topright", isNewBookmark);
    } else {
      StarUI.showEditBookmarkPopup(itemId, aBrowser, "overlap", isNewBookmark);
    }
  }),

  // TODO: Replace bookmarkPage code with this function once legacy
  // transactions are removed.
  _bookmarkPagePT: Task.async(function* (aBrowser, aParentId, aShowEditUI) {
    let url = new URL(aBrowser.currentURI.spec);
    let info = yield PlacesUtils.bookmarks.fetch({ url });
    let isNewBookmark = !info;
    if (!info) {
      let parentGuid = aParentId !== undefined ?
                         yield PlacesUtils.promiseItemGuid(aParentId) :
                         PlacesUtils.bookmarks.unfiledGuid;
      info = { url, parentGuid };
      // Bug 1148838 - Make this code work for full page plugins.
      let description = null;
      let charset = null;

      let docInfo = yield this._getPageDetails(aBrowser);

      try {
        info.title = docInfo.isErrorPage ?
          (yield PlacesUtils.promisePlaceInfo(aBrowser.currentURI)).title :
          aBrowser.contentTitle;
        info.title = info.title || url.href;
        description = docInfo.description;
        charset = aBrowser.characterSet;
      } catch (e) {
        Components.utils.reportError(e);
      }

      if (aShowEditUI && isNewBookmark) {
        // If we bookmark the page here but open right into a cancelable
        // state (i.e. new bookmark in Library), start batching here so
        // all of the actions can be undone in a single undo step.
        StarUI.beginBatch();
      }

      if (description) {
        info.annotations = [{ name: PlacesUIUtils.DESCRIPTION_ANNO
                            , value: description }];
      }

      info.guid = yield PlacesTransactions.NewBookmark(info).transact();

      // Set the character-set
      if (charset && !PrivateBrowsingUtils.isBrowserPrivate(aBrowser))
         PlacesUtils.setCharsetForURI(makeURI(url.href), charset);
    }

    // Revert the contents of the location bar
    gURLBar.handleRevert();

    // If it was not requested to open directly in "edit" mode, we are done.
    if (!aShowEditUI)
      return;

    let node = yield PlacesUIUtils.promiseNodeLikeFromFetchInfo(info);

    // Try to dock the panel to:
    // 1. the bookmarks menu button
    // 2. the identity icon
    // 3. the content area
    if (BookmarkingUI.anchor) {
      StarUI.showEditBookmarkPopup(node, BookmarkingUI.anchor,
                                   "bottomcenter topright", isNewBookmark);
      return;
    }

    let identityIcon = document.getElementById("identity-icon");
    if (isElementVisible(identityIcon)) {
      StarUI.showEditBookmarkPopup(node, identityIcon,
                                   "bottomcenter topright", isNewBookmark);
    } else {
      StarUI.showEditBookmarkPopup(node, aBrowser, "overlap", isNewBookmark);
    }
  }),

  _getPageDetails(browser) {
    return new Promise(resolve => {
      let mm = browser.messageManager;
      mm.addMessageListener("Bookmarks:GetPageDetails:Result", function listener(msg) {
        mm.removeMessageListener("Bookmarks:GetPageDetails:Result", listener);
        resolve(msg.data);
      });

      mm.sendAsyncMessage("Bookmarks:GetPageDetails", { })
    });
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
   * @param [optional] aDescription
   *        The linked page description, if available
   */
  bookmarkLink: Task.async(function* (aParentId, aURL, aTitle, aDescription = "") {
    let node = yield PlacesUIUtils.fetchNodeLike({ url: aURL });
    if (node) {
      PlacesUIUtils.showBookmarkDialog({ action: "edit"
                                       , node
                                       }, window.top);
      return;
    }

    let ip = new InsertionPoint(aParentId,
                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                Components.interfaces.nsITreeView.DROP_ON);
    PlacesUIUtils.showBookmarkDialog({ action: "add"
                                     , type: "bookmark"
                                     , uri: makeURI(aURL)
                                     , title: aTitle
                                     , description: aDescription
                                     , defaultInsertionPoint: ip
                                     , hiddenRows: [ "description"
                                                   , "location"
                                                   , "loadInSidebar"
                                                   , "keyword" ]
                                     }, window.top);
  }),

  /**
   * List of nsIURI objects characterizing the tabs currently open in the
   * browser, modulo pinned tabs.  The URIs will be in the order in which their
   * corresponding tabs appeared and duplicates are discarded.
   */
  get uniqueCurrentPages() {
    let uniquePages = {};
    let URIs = [];

    gBrowser.visibleTabs.forEach(tab => {
      let browser = tab.linkedBrowser;
      let uri = browser.currentURI;
      let title = browser.contentTitle || tab.label;
      let spec = uri.spec;
      if (!tab.pinned && !(spec in uniquePages)) {
        uniquePages[spec] = null;
        URIs.push({ uri, title });
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
  addLiveBookmark: Task.async(function *(url, feedTitle, feedSubtitle) {
    let toolbarIP = new InsertionPoint(PlacesUtils.toolbarFolderId,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       Components.interfaces.nsITreeView.DROP_ON);

    let feedURI = makeURI(url);
    let title = feedTitle || gBrowser.contentTitle;
    let description = feedSubtitle;
    if (!description) {
      description = (yield this._getPageDetails(gBrowser.selectedBrowser)).description;
    }

    PlacesUIUtils.showBookmarkDialog({ action: "add"
                                     , type: "livemark"
                                     , feedURI
                                     , siteURI: gBrowser.currentURI
                                     , title
                                     , description
                                     , defaultInsertionPoint: toolbarIP
                                     , hiddenRows: [ "feedLocation"
                                                   , "siteLocation"
                                                   , "description" ]
                                     }, window);
  }),

  /**
   * Opens the Places Organizer.
   * @param   aLeftPaneRoot
   *          The query to select in the organizer window - options
   *          are: History, AllBookmarks, BookmarksMenu, BookmarksToolbar,
   *          UnfiledBookmarks, Tags and Downloads.
   */
  showPlacesOrganizer: function PCH_showPlacesOrganizer(aLeftPaneRoot) {
    var organizer = Services.wm.getMostRecentWindow("Places:Organizer");
    // Due to bug 528706, getMostRecentWindow can return closed windows.
    if (!organizer || organizer.closed) {
      // No currently open places window, so open one with the specified mode.
      openDialog("chrome://browser/content/places/places.xul",
                 "", "chrome,toolbar=yes,dialog=no,resizable", aLeftPaneRoot);
    } else {
      organizer.PlacesOrganizer.selectLeftPaneContainerByHierarchy(aLeftPaneRoot);
      organizer.focus();
    }
  }
};

XPCOMUtils.defineLazyModuleGetter(this, "RecentlyClosedTabsAndWindowsMenuUtils",
  "resource:///modules/sessionstore/RecentlyClosedTabsAndWindowsMenuUtils.jsm");

// View for the history menu.
function HistoryMenu(aPopupShowingEvent) {
  // Workaround for Bug 610187.  The sidebar does not include all the Places
  // views definitions, and we don't need them there.
  // Defining the prototype inheritance in the prototype itself would cause
  // browser.js to halt on "PlacesMenu is not defined" error.
  this.__proto__.__proto__ = PlacesMenu.prototype;
  PlacesMenu.call(this, aPopupShowingEvent,
                  "place:sort=4&maxResults=15");
}

HistoryMenu.prototype = {
  _getClosedTabCount() {
    // SessionStore doesn't track the hidden window, so just return zero then.
    if (window == Services.appShell.hiddenDOMWindow) {
      return 0;
    }

    return SessionStore.getClosedTabCount(window);
  },

  toggleRecentlyClosedTabs: function HM_toggleRecentlyClosedTabs() {
    // enable/disable the Recently Closed Tabs sub menu
    var undoMenu = this._rootElt.getElementsByClassName("recentlyClosedTabsMenu")[0];

    // no restorable tabs, so disable menu
    if (this._getClosedTabCount() == 0)
      undoMenu.setAttribute("disabled", true);
    else
      undoMenu.removeAttribute("disabled");
  },

  /**
   * Populate when the history menu is opened
   */
  populateUndoSubmenu: function PHM_populateUndoSubmenu() {
    var undoMenu = this._rootElt.getElementsByClassName("recentlyClosedTabsMenu")[0];
    var undoPopup = undoMenu.firstChild;

    // remove existing menu items
    while (undoPopup.hasChildNodes())
      undoPopup.firstChild.remove();

    // no restorable tabs, so make sure menu is disabled, and return
    if (this._getClosedTabCount() == 0) {
      undoMenu.setAttribute("disabled", true);
      return;
    }

    // enable menu
    undoMenu.removeAttribute("disabled");

    // populate menu
    let tabsFragment = RecentlyClosedTabsAndWindowsMenuUtils.getTabsFragment(window, "menuitem");
    undoPopup.appendChild(tabsFragment);
  },

  toggleRecentlyClosedWindows: function PHM_toggleRecentlyClosedWindows() {
    // enable/disable the Recently Closed Windows sub menu
    var undoMenu = this._rootElt.getElementsByClassName("recentlyClosedWindowsMenu")[0];

    // no restorable windows, so disable menu
    if (SessionStore.getClosedWindowCount() == 0)
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

    // remove existing menu items
    while (undoPopup.hasChildNodes())
      undoPopup.firstChild.remove();

    // no restorable windows, so make sure menu is disabled, and return
    if (SessionStore.getClosedWindowCount() == 0) {
      undoMenu.setAttribute("disabled", true);
      return;
    }

    // enable menu
    undoMenu.removeAttribute("disabled");

    // populate menu
    let windowsFragment = RecentlyClosedTabsAndWindowsMenuUtils.getWindowsFragment(window, "menuitem");
    undoPopup.appendChild(windowsFragment);
  },

  toggleTabsFromOtherComputers: function PHM_toggleTabsFromOtherComputers() {
    // Enable/disable the Tabs From Other Computers menu. Some of the menus handled
    // by HistoryMenu do not have this menuitem.
    let menuitem = this._rootElt.getElementsByClassName("syncTabsMenuItem")[0];
    if (!menuitem)
      return;

    if (!PlacesUIUtils.shouldShowTabsFromOtherComputersMenuitem()) {
      menuitem.setAttribute("hidden", true);
      return;
    }

    menuitem.setAttribute("hidden", false);
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
    let modifKey;
    if (AppConstants.platform == "macosx") {
      modifKey = aEvent.metaKey || aEvent.shiftKey;
    } else {
      modifKey = aEvent.ctrlKey || aEvent.shiftKey;
    }

    if (aEvent.button == 2 || (aEvent.button == 0 && !modifKey))
      return;

    var target = aEvent.originalTarget;
    // If this event bubbled up from a menu or menuitem, close the menus.
    // Do this before opening tabs, to avoid hiding the open tabs confirm-dialog.
    if (target.localName == "menu" || target.localName == "menuitem") {
      for (let node = target.parentNode; node; node = node.parentNode) {
        if (node.localName == "menupopup")
          node.hidePopup();
        else if (node.localName != "menu" &&
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
    } else if (aEvent.button == 1) {
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
      var tbo = tree.treeBoxObject;
      var cell = tbo.getCellAt(aEvent.clientX, aEvent.clientY);
      if (cell.row == -1)
        return false;
      node = tree.view.nodeForTreeIndex(cell.row);
      cropped = tbo.isCellCropped(cell.row, cell.col);
    } else {
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

// Handles special drag and drop functionality for Places menus that are not
// part of a Places view (e.g. the bookmarks menu in the menubar).
var PlacesMenuDNDHandler = {
  _springLoadDelayMs: 350,
  _closeDelayMs: 500,
  _loadTimer: null,
  _closeTimer: null,
  _closingTimerNode: null,

  /**
   * Called when the user enters the <menu> element during a drag.
   * @param   event
   *          The DragEnter event that spawned the opening.
   */
  onDragEnter: function PMDH_onDragEnter(event) {
    // Opening menus in a Places popup is handled by the view itself.
    if (!this._isStaticContainer(event.target))
      return;

    // If we re-enter the same menu or anchor before the close timer runs out,
    // we should ensure that we do not close:
    if (this._closeTimer && this._closingTimerNode === event.currentTarget) {
      this._closeTimer.cancel();
      this._closingTimerNode = null;
      this._closeTimer = null;
    }

    PlacesControllerDragHelper.currentDropTarget = event.target;
    let popup = event.target.lastChild;
    if (this._loadTimer || popup.state === "showing" || popup.state === "open")
      return;

    this._loadTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._loadTimer.initWithCallback(() => {
      this._loadTimer = null;
      popup.setAttribute("autoopened", "true");
      popup.showPopup(popup);
    }, this._springLoadDelayMs, Ci.nsITimer.TYPE_ONE_SHOT);
    event.preventDefault();
    event.stopPropagation();
  },

  /**
   * Handles dragleave on the <menu> element.
   */
  onDragLeave: function PMDH_onDragLeave(event) {
    // Handle menu-button separate targets.
    if (event.relatedTarget === event.currentTarget ||
        (event.relatedTarget &&
         event.relatedTarget.parentNode === event.currentTarget))
      return;

    // Closing menus in a Places popup is handled by the view itself.
    if (!this._isStaticContainer(event.target))
      return;

    PlacesControllerDragHelper.currentDropTarget = null;
    let popup = event.target.lastChild;

    if (this._loadTimer) {
      this._loadTimer.cancel();
      this._loadTimer = null;
    }
    this._closeTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._closingTimerNode = event.currentTarget;
    this._closeTimer.initWithCallback(function() {
      this._closeTimer = null;
      this._closingTimerNode = null;
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
    }, this._closeDelayMs, Ci.nsITimer.TYPE_ONE_SHOT);
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
                                Components.interfaces.nsITreeView.DROP_ON);
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
                                Components.interfaces.nsITreeView.DROP_ON);
    PlacesControllerDragHelper.onDrop(ip, event.dataTransfer);
    PlacesControllerDragHelper.currentDropTarget = null;
    event.stopPropagation();
  }
};

/**
 * This object handles the initialization and uninitialization of the bookmarks
 * toolbar.
 */
var PlacesToolbarHelper = {
  _place: "place:folder=TOOLBAR",

  get _viewElt() {
    return document.getElementById("PlacesToolbar");
  },

  get _placeholder() {
    return document.getElementById("bookmarks-toolbar-placeholder");
  },

  init: function PTH_init(forceToolbarOverflowCheck) {
    let viewElt = this._viewElt;
    if (!viewElt || viewElt._placesView)
      return;

    // CustomizableUI.addListener is idempotent, so we can safely
    // call this multiple times.
    CustomizableUI.addListener(this);

    // If the bookmarks toolbar item is:
    // - not in a toolbar, or;
    // - the toolbar is collapsed, or;
    // - the toolbar is hidden some other way:
    // don't initialize.  Also, there is no need to initialize the toolbar if
    // customizing, because that will happen when the customization is done.
    let toolbar = this._getParentToolbar(viewElt);
    if (!toolbar || toolbar.collapsed || this._isCustomizing ||
        getComputedStyle(toolbar, "").display == "none")
      return;

    new PlacesToolbar(this._place);
    if (forceToolbarOverflowCheck) {
      viewElt._placesView.updateOverflowStatus();
    }
    this._shouldWrap = false;
    this._setupPlaceholder();
  },

  uninit: function PTH_uninit() {
    CustomizableUI.removeListener(this);
  },

  customizeStart: function PTH_customizeStart() {
    try {
      let viewElt = this._viewElt;
      if (viewElt && viewElt._placesView)
        viewElt._placesView.uninit();
    } finally {
      this._isCustomizing = true;
    }
    this._shouldWrap = this._getShouldWrap();
  },

  customizeChange: function PTH_customizeChange() {
    this._setupPlaceholder();
  },

  _setupPlaceholder: function PTH_setupPlaceholder() {
    let placeholder = this._placeholder;
    if (!placeholder) {
      return;
    }

    let shouldWrapNow = this._getShouldWrap();
    if (this._shouldWrap != shouldWrapNow) {
      if (shouldWrapNow) {
        placeholder.setAttribute("wrap", "true");
      } else {
        placeholder.removeAttribute("wrap");
      }
      this._shouldWrap = shouldWrapNow;
    }
  },

  customizeDone: function PTH_customizeDone() {
    this._isCustomizing = false;
    this.init(true);
  },

  _getShouldWrap: function PTH_getShouldWrap() {
    let placement = CustomizableUI.getPlacementOfWidget("personal-bookmarks");
    let area = placement && placement.area;
    let areaType = area && CustomizableUI.getAreaType(area);
    return !area || CustomizableUI.TYPE_MENU_PANEL == areaType;
  },

  onPlaceholderCommand() {
    let widgetGroup = CustomizableUI.getWidget("personal-bookmarks");
    let widget = widgetGroup.forWindow(window);
    if (widget.overflowed ||
        widgetGroup.areaType == CustomizableUI.TYPE_MENU_PANEL) {
      PlacesCommandHook.showPlacesOrganizer("BookmarksToolbar");
    }
  },

  _getParentToolbar(element) {
    while (element) {
      if (element.localName == "toolbar") {
        return element;
      }
      element = element.parentNode;
    }
    return null;
  },

  onWidgetUnderflow(aNode, aContainer) {
    // The view gets broken by being removed and reinserted by the overflowable
    // toolbar, so we have to force an uninit and reinit.
    let win = aNode.ownerGlobal;
    if (aNode.id == "personal-bookmarks" && win == window) {
      this._resetView();
    }
  },

  onWidgetAdded(aWidgetId, aArea, aPosition) {
    if (aWidgetId == "personal-bookmarks" && !this._isCustomizing) {
      // It's possible (with the "Add to Menu", "Add to Toolbar" context
      // options) that the Places Toolbar Items have been moved without
      // letting us prepare and handle it with with customizeStart and
      // customizeDone. If that's the case, we need to reset the views
      // since they're probably broken from the DOM reparenting.
      this._resetView();
    }
  },

  _resetView() {
    if (this._viewElt) {
      // It's possible that the placesView might not exist, and we need to
      // do a full init. This could happen if the Bookmarks Toolbar Items are
      // moved to the Menu Panel, and then to the toolbar with the "Add to Toolbar"
      // context menu option, outside of customize mode.
      if (this._viewElt._placesView) {
        this._viewElt._placesView.uninit();
      }
      this.init(true);
    }
  },
};

/**
 * Handles the bookmarks menu-button in the toolbar.
 */

var BookmarkingUI = {
  BOOKMARK_BUTTON_ID: "bookmarks-menu-button",
  BOOKMARK_BUTTON_SHORTCUT: "addBookmarkAsKb",
  get button() {
    delete this.button;
    let widgetGroup = CustomizableUI.getWidget(this.BOOKMARK_BUTTON_ID);
    return this.button = widgetGroup.forWindow(window).node;
  },

  /* Can't make this a self-deleting getter because it's anonymous content
   * and might lose/regain bindings at some point. */
  get star() {
    return document.getAnonymousElementByAttribute(this.button, "anonid",
                                                   "button");
  },

  get anchor() {
    if (!this._shouldUpdateStarState()) {
      return null;
    }
    let widget = CustomizableUI.getWidget(this.BOOKMARK_BUTTON_ID)
                               .forWindow(window);
    if (widget.overflowed)
      return widget.anchor;

    let star = this.star;
    return star ? document.getAnonymousElementByAttribute(star, "class",
                                                          "toolbarbutton-icon")
                : null;
  },

  get notifier() {
    delete this.notifier;
    return this.notifier = document.getElementById("bookmarked-notification-anchor");
  },

  get dropmarkerNotifier() {
    delete this.dropmarkerNotifier;
    return this.dropmarkerNotifier = document.getElementById("bookmarked-notification-dropmarker-anchor");
  },

  get broadcaster() {
    delete this.broadcaster;
    let broadcaster = document.getElementById("bookmarkThisPageBroadcaster");
    return this.broadcaster = broadcaster;
  },

  STATUS_UPDATING: -1,
  STATUS_UNSTARRED: 0,
  STATUS_STARRED: 1,
  get status() {
    if (!this._shouldUpdateStarState()) {
      return this.STATUS_UNSTARRED;
    }
    if (this._pendingStmt)
      return this.STATUS_UPDATING;
    return this.button.hasAttribute("starred") ? this.STATUS_STARRED
                                               : this.STATUS_UNSTARRED;
  },

  get _starredTooltip() {
    delete this._starredTooltip;
    return this._starredTooltip =
      this._getFormattedTooltip("starButtonOn.tooltip2");
  },

  get _unstarredTooltip() {
    delete this._unstarredTooltip;
    return this._unstarredTooltip =
      this._getFormattedTooltip("starButtonOff.tooltip2");
  },

  _getFormattedTooltip(strId) {
    let args = [];
    let shortcut = document.getElementById(this.BOOKMARK_BUTTON_SHORTCUT);
    if (shortcut)
      args.push(ShortcutUtils.prettifyShortcut(shortcut));
    return gNavigatorBundle.getFormattedString(strId, args);
  },

  /**
   * The type of the area in which the button is currently located.
   * When in the panel, we don't update the button's icon.
   */
  _currentAreaType: null,
  _shouldUpdateStarState() {
    return this._currentAreaType == CustomizableUI.TYPE_TOOLBAR;
  },

  /**
   * The popup contents must be updated when the user customizes the UI, or
   * changes the personal toolbar collapsed status.  In such a case, any needed
   * change should be handled in the popupshowing helper, for performance
   * reasons.
   */
  _popupNeedsUpdate: true,
  onToolbarVisibilityChange: function BUI_onToolbarVisibilityChange() {
    this._popupNeedsUpdate = true;
  },

  onPopupShowing: function BUI_onPopupShowing(event) {
    // Don't handle events for submenus.
    if (event.target != event.currentTarget)
      return;

    // Ideally this code would never be reached, but if you click the outer
    // button's border, some cpp code for the menu button's so-called XBL binding
    // decides to open the popup even though the dropmarker is invisible.
    if (this._currentAreaType == CustomizableUI.TYPE_MENU_PANEL) {
      this._showSubview();
      event.preventDefault();
      event.stopPropagation();
      return;
    }

    let widget = CustomizableUI.getWidget(this.BOOKMARK_BUTTON_ID)
                               .forWindow(window);
    if (widget.overflowed) {
      // Don't open a popup in the overflow popup, rather just open the Library.
      event.preventDefault();
      widget.node.removeAttribute("closemenu");
      PlacesCommandHook.showPlacesOrganizer("BookmarksMenu");
      return;
    }

    this._initMobileBookmarks(document.getElementById("BMB_mobileBookmarks"));
    this._initRecentBookmarks(document.getElementById("BMB_recentBookmarks"),
                              "subviewbutton");

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
      viewToolbarMenuitem.classList.add("subviewbutton");
      let personalToolbar = document.getElementById("PersonalToolbar");
      viewToolbarMenuitem.setAttribute("checked", !personalToolbar.collapsed);
    }
  },

  attachPlacesView(event, node) {
    // If the view is already there, bail out early.
    if (node.parentNode._placesView)
      return;

    new PlacesMenu(event, "place:folder=BOOKMARKS_MENU", {
      extraClasses: {
        entry: "subviewbutton",
        footer: "panel-subview-footer"
      },
      insertionPoint: ".panel-subview-footer"
    });
  },

  RECENTLY_BOOKMARKED_PREF: "browser.bookmarks.showRecentlyBookmarked",

  // Set by sync after syncing bookmarks successfully once.
  MOBILE_BOOKMARKS_PREF: "browser.bookmarks.showMobileBookmarks",

  _shouldShowMobileBookmarks() {
    try {
      return Services.prefs.getBoolPref(this.MOBILE_BOOKMARKS_PREF);
    } catch (e) {}
    // No pref set (or invalid pref set), look for a mobile bookmarks left pane query.
    const organizerQueryAnno = "PlacesOrganizer/OrganizerQuery";
    const mobileBookmarksAnno = "MobileBookmarks";
    let shouldShow = PlacesUtils.annotations.getItemsWithAnnotation(organizerQueryAnno, {}).filter(
      id => PlacesUtils.annotations.getItemAnnotation(id, organizerQueryAnno) == mobileBookmarksAnno
    ).length > 0;
    // Sync will change this pref if/when it adds a mobile bookmarks query.
    Services.prefs.setBoolPref(this.MOBILE_BOOKMARKS_PREF, shouldShow);
    return shouldShow;
  },

  _initMobileBookmarks(mobileMenuItem) {
    mobileMenuItem.hidden = !this._shouldShowMobileBookmarks();
  },

  _initRecentBookmarks(aHeaderItem, aExtraCSSClass) {
    this._populateRecentBookmarks(aHeaderItem, aExtraCSSClass);

    // Add observers and listeners and remove them again when the menupopup closes.

    let bookmarksMenu = aHeaderItem.parentNode;
    let placesContextMenu = document.getElementById("placesContext");

    let prefObserver = () => {
      this._populateRecentBookmarks(aHeaderItem, aExtraCSSClass);
    };

    this._recentlyBookmarkedObserver = {
      QueryInterface: XPCOMUtils.generateQI([
        Ci.nsINavBookmarkObserver,
        Ci.nsISupportsWeakReference
      ])
    };
    this._recentlyBookmarkedObserver.onItemRemoved = () => {
      // Update the menu when a bookmark has been removed.
      // The native menubar on Mac doesn't support live update, so this won't
      // work there.
      this._populateRecentBookmarks(aHeaderItem, aExtraCSSClass);
    };

    let updatePlacesContextMenu = (shouldHidePrefUI = false) => {
      let prefEnabled = !shouldHidePrefUI && Services.prefs.getBoolPref(this.RECENTLY_BOOKMARKED_PREF);
      let showItem = document.getElementById("placesContext_showRecentlyBookmarked");
      let hideItem = document.getElementById("placesContext_hideRecentlyBookmarked");
      let separator = document.getElementById("placesContext_recentlyBookmarkedSeparator");
      showItem.hidden = shouldHidePrefUI || prefEnabled;
      hideItem.hidden = shouldHidePrefUI || !prefEnabled;
      separator.hidden = shouldHidePrefUI;
      if (!shouldHidePrefUI) {
        // Move to the bottom of the menu.
        separator.parentNode.appendChild(separator);
        showItem.parentNode.appendChild(showItem);
        hideItem.parentNode.appendChild(hideItem);
      }
    };

    let onPlacesContextMenuShowing = event => {
      if (event.target == event.currentTarget) {
        let triggerPopup = event.target.triggerNode;
        while (triggerPopup && triggerPopup.localName != "menupopup") {
          triggerPopup = triggerPopup.parentNode;
        }
        let shouldHidePrefUI = triggerPopup != bookmarksMenu;
        updatePlacesContextMenu(shouldHidePrefUI);
      }
    };

    let onBookmarksMenuHidden = event => {
      if (event.target == event.currentTarget) {
        updatePlacesContextMenu(true);

        Services.prefs.removeObserver(this.RECENTLY_BOOKMARKED_PREF, prefObserver);
        PlacesUtils.bookmarks.removeObserver(this._recentlyBookmarkedObserver);
        this._recentlyBookmarkedObserver = null;
        if (placesContextMenu) {
          placesContextMenu.removeEventListener("popupshowing", onPlacesContextMenuShowing);
        }
        bookmarksMenu.removeEventListener("popuphidden", onBookmarksMenuHidden);
      }
    };

    Services.prefs.addObserver(this.RECENTLY_BOOKMARKED_PREF, prefObserver, false);
    PlacesUtils.bookmarks.addObserver(this._recentlyBookmarkedObserver, true);

    // The context menu doesn't exist in non-browser windows on Mac
    if (placesContextMenu) {
      placesContextMenu.addEventListener("popupshowing", onPlacesContextMenuShowing);
    }

    bookmarksMenu.addEventListener("popuphidden", onBookmarksMenuHidden);
  },

  _populateRecentBookmarks(aHeaderItem, aExtraCSSClass = "") {
    while (aHeaderItem.nextSibling &&
           aHeaderItem.nextSibling.localName == "menuitem") {
      aHeaderItem.nextSibling.remove();
    }

    let shouldShow = Services.prefs.getBoolPref(this.RECENTLY_BOOKMARKED_PREF);
    let separator = aHeaderItem.previousSibling;
    aHeaderItem.hidden = !shouldShow;
    separator.hidden = !shouldShow;

    if (!shouldShow) {
      return;
    }

    const kMaxResults = 5;

    let options = PlacesUtils.history.getNewQueryOptions();
    options.excludeQueries = true;
    options.queryType = options.QUERY_TYPE_BOOKMARKS;
    options.sortingMode = options.SORT_BY_DATEADDED_DESCENDING;
    options.maxResults = kMaxResults;
    let query = PlacesUtils.history.getNewQuery();

    let sh = Cc["@mozilla.org/network/serialization-helper;1"]
               .getService(Ci.nsISerializationHelper);
    let loadingPrincipal = sh.serializeToString(document.nodePrincipal);

    let fragment = document.createDocumentFragment();
    let root = PlacesUtils.history.executeQuery(query, options).root;
    root.containerOpen = true;
    for (let i = 0; i < root.childCount; i++) {
      let node = root.getChild(i);
      let uri = node.uri;
      let title = node.title;
      let icon = node.icon;

      let item =
        document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                 "menuitem");
      item.setAttribute("label", title || uri);
      item.setAttribute("targetURI", uri);
      item.setAttribute("simulated-places-node", true);
      item.setAttribute("class", "menuitem-iconic menuitem-with-favicon bookmark-item " +
                                 aExtraCSSClass);
      if (icon) {
        item.setAttribute("image", icon);
        item.setAttribute("loadingprincipal", loadingPrincipal);
      }
      item._placesNode = node;
      fragment.appendChild(item);
    }
    root.containerOpen = false;
    aHeaderItem.parentNode.insertBefore(fragment, aHeaderItem.nextSibling);
  },

  showRecentlyBookmarked() {
    Services.prefs.setBoolPref(this.RECENTLY_BOOKMARKED_PREF, true);
  },

  hideRecentlyBookmarked() {
    Services.prefs.setBoolPref(this.RECENTLY_BOOKMARKED_PREF, false);
  },

  _updateCustomizationState: function BUI__updateCustomizationState() {
    let placement = CustomizableUI.getPlacementOfWidget(this.BOOKMARK_BUTTON_ID);
    this._currentAreaType = placement && CustomizableUI.getAreaType(placement.area);
  },

  _uninitView: function BUI__uninitView() {
    // When an element with a placesView attached is removed and re-inserted,
    // XBL reapplies the binding causing any kind of issues and possible leaks,
    // so kill current view and let popupshowing generate a new one.
    if (this.button._placesView)
      this.button._placesView.uninit();

    // We have to do the same thing for the "special" views underneath the
    // the bookmarks menu.
    const kSpecialViewNodeIDs = ["BMB_bookmarksToolbar", "BMB_unsortedBookmarks"];
    for (let viewNodeID of kSpecialViewNodeIDs) {
      let elem = document.getElementById(viewNodeID);
      if (elem && elem._placesView) {
        elem._placesView.uninit();
      }
    }
  },

  onCustomizeStart: function BUI_customizeStart(aWindow) {
    if (aWindow == window) {
      this._uninitView();
      this._isCustomizing = true;
    }
  },

  onWidgetAdded: function BUI_widgetAdded(aWidgetId) {
    if (aWidgetId == this.BOOKMARK_BUTTON_ID) {
      this._onWidgetWasMoved();
    }
  },

  onWidgetRemoved: function BUI_widgetRemoved(aWidgetId) {
    if (aWidgetId == this.BOOKMARK_BUTTON_ID) {
      this._onWidgetWasMoved();
    }
  },

  onWidgetReset: function BUI_widgetReset(aNode, aContainer) {
    if (aNode == this.button) {
      this._onWidgetWasMoved();
    }
  },

  onWidgetUndoMove: function BUI_undoWidgetUndoMove(aNode, aContainer) {
    if (aNode == this.button) {
      this._onWidgetWasMoved();
    }
  },

  _onWidgetWasMoved: function BUI_widgetWasMoved() {
    let usedToUpdateStarState = this._shouldUpdateStarState();
    this._updateCustomizationState();
    if (!usedToUpdateStarState && this._shouldUpdateStarState()) {
      this.updateStarState();
    } else if (usedToUpdateStarState && !this._shouldUpdateStarState()) {
      this._updateStar();
    }
    // If we're moved outside of customize mode, we need to uninit
    // our view so it gets reconstructed.
    if (!this._isCustomizing) {
      this._uninitView();
    }
  },

  onCustomizeEnd: function BUI_customizeEnd(aWindow) {
    if (aWindow == window) {
      this._isCustomizing = false;
      this.onToolbarVisibilityChange();
    }
  },

  init() {
    CustomizableUI.addListener(this);
    this._updateCustomizationState();
  },

  _hasBookmarksObserver: false,
  _itemIds: [],
  uninit: function BUI_uninit() {
    this._updateBookmarkPageMenuItem(true);
    CustomizableUI.removeListener(this);

    this._uninitView();

    if (this._hasBookmarksObserver) {
      PlacesUtils.removeLazyBookmarkObserver(this);
    }

    if (this._pendingStmt) {
      this._pendingStmt.cancel();
      delete this._pendingStmt;
    }
  },

  onLocationChange: function BUI_onLocationChange() {
    if (this._uri && gBrowser.currentURI.equals(this._uri)) {
      return;
    }
    this.updateStarState();
  },

  updateStarState: function BUI_updateStarState() {
    // Reset tracked values.
    this._uri = gBrowser.currentURI;
    this._itemIds = [];

    if (this._pendingStmt) {
      this._pendingStmt.cancel();
      delete this._pendingStmt;
    }

    this._pendingStmt = PlacesUtils.asyncGetBookmarkIds(this._uri, (aItemIds, aURI) => {
      // Safety check that the bookmarked URI equals the tracked one.
      if (!aURI.equals(this._uri)) {
        Components.utils.reportError("BookmarkingUI did not receive current URI");
        return;
      }

      // It's possible that onItemAdded gets called before the async statement
      // calls back.  For such an edge case, retain all unique entries from both
      // arrays.
      this._itemIds = this._itemIds.filter(
        id => !aItemIds.includes(id)
      ).concat(aItemIds);

      this._updateStar();

      // Start observing bookmarks if needed.
      if (!this._hasBookmarksObserver) {
        try {
          PlacesUtils.addLazyBookmarkObserver(this);
          this._hasBookmarksObserver = true;
        } catch (ex) {
          Components.utils.reportError("BookmarkingUI failed adding a bookmarks observer: " + ex);
        }
      }

      delete this._pendingStmt;
    });
  },

  _updateStar: function BUI__updateStar() {
    if (!this._shouldUpdateStarState()) {
      if (this.broadcaster.hasAttribute("starred")) {
        this.broadcaster.removeAttribute("starred");
        this.broadcaster.removeAttribute("buttontooltiptext");
      }
      return;
    }

    if (this._itemIds.length > 0) {
      this.broadcaster.setAttribute("starred", "true");
      this.broadcaster.setAttribute("buttontooltiptext", this._starredTooltip);
      if (this.button.getAttribute("overflowedItem") == "true") {
        this.button.setAttribute("label", this._starButtonOverflowedStarredLabel);
      }
    } else {
      this.broadcaster.removeAttribute("starred");
      this.broadcaster.setAttribute("buttontooltiptext", this._unstarredTooltip);
      if (this.button.getAttribute("overflowedItem") == "true") {
        this.button.setAttribute("label", this._starButtonOverflowedLabel);
      }
    }
  },

  /**
   * forceReset is passed when we're destroyed and the label should go back
   * to the default (Bookmark This Page) for OS X.
   */
  _updateBookmarkPageMenuItem: function BUI__updateBookmarkPageMenuItem(forceReset) {
    let isStarred = !forceReset && this._itemIds.length > 0;
    let label = isStarred ? "editlabel" : "bookmarklabel";
    if (this.broadcaster) {
      this.broadcaster.setAttribute("label", this.broadcaster.getAttribute(label));
    }
  },

  onMainMenuPopupShowing: function BUI_onMainMenuPopupShowing(event) {
    // Don't handle events for submenus.
    if (event.target != event.currentTarget)
      return;

    this._updateBookmarkPageMenuItem();
    PlacesCommandHook.updateBookmarkAllTabsCommand();
    this._initMobileBookmarks(document.getElementById("menu_mobileBookmarks"));
    this._initRecentBookmarks(document.getElementById("menu_recentBookmarks"));
  },

  _showBookmarkedNotification: function BUI_showBookmarkedNotification() {
    function getCenteringTransformForRects(rectToPosition, referenceRect) {
      let topDiff = referenceRect.top - rectToPosition.top;
      let leftDiff = referenceRect.left - rectToPosition.left;
      let heightDiff = referenceRect.height - rectToPosition.height;
      let widthDiff = referenceRect.width - rectToPosition.width;
      return [(leftDiff + .5 * widthDiff) + "px", (topDiff + .5 * heightDiff) + "px"];
    }

    if (this._notificationTimeout) {
      clearTimeout(this._notificationTimeout);
    }

    if (this.notifier.style.transform == "") {
      // Get all the relevant nodes and computed style objects
      let dropmarker = document.getAnonymousElementByAttribute(this.button, "anonid", "dropmarker");
      let dropmarkerIcon = document.getAnonymousElementByAttribute(dropmarker, "class", "dropmarker-icon");
      let dropmarkerStyle = getComputedStyle(dropmarkerIcon);

      // Check for RTL and get bounds
      let isRTL = getComputedStyle(this.button).direction == "rtl";
      let buttonRect = this.button.getBoundingClientRect();
      let notifierRect = this.notifier.getBoundingClientRect();
      let dropmarkerRect = dropmarkerIcon.getBoundingClientRect();
      let dropmarkerNotifierRect = this.dropmarkerNotifier.getBoundingClientRect();

      // Compute, but do not set, transform for star icon
      let [translateX, translateY] = getCenteringTransformForRects(notifierRect, buttonRect);
      let starIconTransform = "translate(" + translateX + ", " + translateY + ")";
      if (isRTL) {
        starIconTransform += " scaleX(-1)";
      }

      // Compute, but do not set, transform for dropmarker
      [translateX, translateY] = getCenteringTransformForRects(dropmarkerNotifierRect, dropmarkerRect);
      let dropmarkerTransform = "translate(" + translateX + ", " + translateY + ")";

      // Do all layout invalidation in one go:
      this.notifier.style.transform = starIconTransform;
      this.dropmarkerNotifier.style.transform = dropmarkerTransform;

      let dropmarkerAnimationNode = this.dropmarkerNotifier.firstChild;
      dropmarkerAnimationNode.style.MozImageRegion = dropmarkerStyle.MozImageRegion;
      dropmarkerAnimationNode.style.listStyleImage = dropmarkerStyle.listStyleImage;
    }

    let isInOverflowPanel = this.button.getAttribute("overflowedItem") == "true";
    if (!isInOverflowPanel) {
      this.notifier.setAttribute("notification", "finish");
      this.button.setAttribute("notification", "finish");
      this.dropmarkerNotifier.setAttribute("notification", "finish");
    }

    this._notificationTimeout = setTimeout( () => {
      this.notifier.removeAttribute("notification");
      this.dropmarkerNotifier.removeAttribute("notification");
      this.button.removeAttribute("notification");

      this.dropmarkerNotifier.style.transform = "";
      this.notifier.style.transform = "";
    }, 1000);
  },

  _showSubview() {
    let view = document.getElementById("PanelUI-bookmarks");
    view.addEventListener("ViewShowing", this);
    view.addEventListener("ViewHiding", this);
    let anchor = document.getElementById(this.BOOKMARK_BUTTON_ID);
    anchor.setAttribute("closemenu", "none");
    PanelUI.showSubView("PanelUI-bookmarks", anchor,
                        CustomizableUI.AREA_PANEL);
  },

  onCommand: function BUI_onCommand(aEvent) {
    if (aEvent.target != aEvent.currentTarget) {
      return;
    }

    // Handle special case when the button is in the panel.
    let isBookmarked = this._itemIds.length > 0;

    if (this._currentAreaType == CustomizableUI.TYPE_MENU_PANEL) {
      this._showSubview();
      return;
    }
    let widget = CustomizableUI.getWidget(this.BOOKMARK_BUTTON_ID)
                               .forWindow(window);
    if (widget.overflowed) {
      // Close the overflow panel because the Edit Bookmark panel will appear.
      widget.node.removeAttribute("closemenu");
    }

    // Ignore clicks on the star if we are updating its state.
    if (!this._pendingStmt) {
      if (!isBookmarked)
        this._showBookmarkedNotification();
      PlacesCommandHook.bookmarkCurrentPage(true);
    }
  },

  onCurrentPageContextPopupShowing() {
    this._updateBookmarkPageMenuItem();
  },

  handleEvent: function BUI_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "ViewShowing":
        this.onPanelMenuViewShowing(aEvent);
        break;
      case "ViewHiding":
        this.onPanelMenuViewHiding(aEvent);
        break;
    }
  },

  onPanelMenuViewShowing: function BUI_onViewShowing(aEvent) {
    this._updateBookmarkPageMenuItem();
    // Update checked status of the toolbar toggle.
    let viewToolbar = document.getElementById("panelMenu_viewBookmarksToolbar");
    let personalToolbar = document.getElementById("PersonalToolbar");
    if (personalToolbar.collapsed)
      viewToolbar.removeAttribute("checked");
    else
      viewToolbar.setAttribute("checked", "true");
    // Get all statically placed buttons to supply them with keyboard shortcuts.
    let staticButtons = viewToolbar.parentNode.getElementsByTagName("toolbarbutton");
    for (let i = 0, l = staticButtons.length; i < l; ++i)
      CustomizableUI.addShortcut(staticButtons[i]);
    // Setup the Places view.
    this._panelMenuView = new PlacesPanelMenuView("place:folder=BOOKMARKS_MENU",
                                                  "panelMenu_bookmarksMenu",
                                                  "panelMenu_bookmarksMenu", {
                                                    extraClasses: {
                                                      entry: "subviewbutton",
                                                      footer: "panel-subview-footer"
                                                    }
                                                  });
    aEvent.target.removeEventListener("ViewShowing", this);
  },

  onPanelMenuViewHiding: function BUI_onViewHiding(aEvent) {
    this._panelMenuView.uninit();
    delete this._panelMenuView;
    aEvent.target.removeEventListener("ViewHiding", this);
  },

  onPanelMenuViewCommand: function BUI_onPanelMenuViewCommand(aEvent, aView) {
    let target = aEvent.originalTarget;
    if (!target._placesNode)
      return;
    if (PlacesUtils.nodeIsContainer(target._placesNode))
      PlacesCommandHook.showPlacesOrganizer([ "BookmarksMenu", target._placesNode.itemId ]);
    else
      PlacesUIUtils.openNodeWithEvent(target._placesNode, aEvent, aView);
    PanelUI.hide();
  },

  // nsINavBookmarkObserver
  onItemAdded: function BUI_onItemAdded(aItemId, aParentId, aIndex, aItemType,
                                        aURI) {
    if (aURI && aURI.equals(this._uri)) {
      // If a new bookmark has been added to the tracked uri, register it.
      if (!this._itemIds.includes(aItemId)) {
        this._itemIds.push(aItemId);
        // Only need to update the UI if it wasn't marked as starred before:
        if (this._itemIds.length == 1) {
          this._updateStar();
        }
      }
    }
  },

  onItemRemoved: function BUI_onItemRemoved(aItemId) {
    let index = this._itemIds.indexOf(aItemId);
    // If one of the tracked bookmarks has been removed, unregister it.
    if (index != -1) {
      this._itemIds.splice(index, 1);
      // Only need to update the UI if the page is no longer starred
      if (this._itemIds.length == 0) {
        this._updateStar();
      }
    }
  },

  onItemChanged: function BUI_onItemChanged(aItemId, aProperty,
                                            aIsAnnotationProperty, aNewValue) {
    if (aProperty == "uri") {
      let index = this._itemIds.indexOf(aItemId);
      // If the changed bookmark was tracked, check if it is now pointing to
      // a different uri and unregister it.
      if (index != -1 && aNewValue != this._uri.spec) {
        this._itemIds.splice(index, 1);
        // Only need to update the UI if the page is no longer starred
        if (this._itemIds.length == 0) {
          this._updateStar();
        }
      } else if (index == -1 && aNewValue == this._uri.spec) {
        // If another bookmark is now pointing to the tracked uri, register it.
        this._itemIds.push(aItemId);
        // Only need to update the UI if it wasn't marked as starred before:
        if (this._itemIds.length == 1) {
          this._updateStar();
        }
      }
    }
  },

  onBeginUpdateBatch() {},
  onEndUpdateBatch() {},
  onBeforeItemRemoved() {},
  onItemVisited() {},
  onItemMoved() {},

  // CustomizableUI events:
  _starButtonLabel: null,
  get _starButtonOverflowedLabel() {
    delete this._starButtonOverflowedLabel;
    return this._starButtonOverflowedLabel =
      gNavigatorBundle.getString("starButtonOverflowed.label");
  },
  get _starButtonOverflowedStarredLabel() {
    delete this._starButtonOverflowedStarredLabel;
    return this._starButtonOverflowedStarredLabel =
      gNavigatorBundle.getString("starButtonOverflowedStarred.label");
  },
  onWidgetOverflow(aNode, aContainer) {
    let win = aNode.ownerGlobal;
    if (aNode.id != this.BOOKMARK_BUTTON_ID || win != window)
      return;

    let currentLabel = aNode.getAttribute("label");
    if (!this._starButtonLabel)
      this._starButtonLabel = currentLabel;

    if (currentLabel == this._starButtonLabel) {
      let desiredLabel = this._itemIds.length > 0 ? this._starButtonOverflowedStarredLabel
                                                 : this._starButtonOverflowedLabel;
      aNode.setAttribute("label", desiredLabel);
    }
  },

  onWidgetUnderflow(aNode, aContainer) {
    let win = aNode.ownerGlobal;
    if (aNode.id != this.BOOKMARK_BUTTON_ID || win != window)
      return;

    // The view gets broken by being removed and reinserted. Uninit
    // here so popupshowing will generate a new one:
    this._uninitView();

    if (aNode.getAttribute("label") != this._starButtonLabel)
      aNode.setAttribute("label", this._starButtonLabel);
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver
  ])
};

var AutoShowBookmarksToolbar = {
  init() {
    Services.obs.addObserver(this, "autoshow-bookmarks-toolbar", false);
  },

  uninit() {
    Services.obs.removeObserver(this, "autoshow-bookmarks-toolbar");
  },

  observe(subject, topic, data) {
    let toolbar = document.getElementById("PersonalToolbar");
    if (!toolbar.collapsed)
      return;

    let placement = CustomizableUI.getPlacementOfWidget("personal-bookmarks");
    let area = placement && placement.area;
    if (area != CustomizableUI.AREA_BOOKMARKS)
      return;

    setToolbarVisibility(toolbar, true);
  }
};

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

XPCOMUtils.defineLazyScriptGetter(this, ["PlacesToolbar", "PlacesMenu",
                                         "PlacesPanelview", "PlacesPanelMenuView"],
                                  "chrome://browser/content/places/browserPlacesViews.js");
XPCOMUtils.defineLazyModuleGetters(this, {
  BookmarkPanelHub: "resource://activity-stream/lib/BookmarkPanelHub.jsm",
});

var StarUI = {
  _itemGuids: null,
  _batching: false,
  _isNewBookmark: false,
  _isComposing: false,
  _autoCloseTimer: 0,
  // The autoclose timer is diasbled if the user interacts with the
  // popup, such as making a change through typing or clicking on
  // the popup.
  _autoCloseTimerEnabled: true,
  // The autoclose timeout length. 3500ms matches the timeout that Pocket uses
  // in browser/components/pocket/content/panels/js/saved.js.
  _autoCloseTimeout: 3500,
  _removeBookmarksOnPopupHidden: false,

  _element(aID) {
    return document.getElementById(aID);
  },

  get showForNewBookmarks() {
    return Services.prefs.getBoolPref("browser.bookmarks.editDialog.showForNewBookmarks");
  },

  // Edit-bookmark panel
  get panel() {
    delete this.panel;
    var element = this._element("editBookmarkPanel");
    // initially the panel is hidden
    // to avoid impacting startup / new window performance
    element.hidden = false;
    element.addEventListener("keypress", this, {mozSystemGroup: true});
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

  // nsIDOMEventListener
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "mousemove":
        clearTimeout(this._autoCloseTimer);
        // The autoclose timer is not disabled on generic mouseout
        // because the user may not have actually interacted with the popup.
        break;
      case "popuphidden": {
        clearTimeout(this._autoCloseTimer);
        if (aEvent.originalTarget == this.panel) {
          let selectedFolderGuid = gEditItemOverlay.selectedFolderGuid;
          gEditItemOverlay.uninitPanel(true);

          this._anchorElement.removeAttribute("open");
          this._anchorElement = null;

          let removeBookmarksOnPopupHidden = this._removeBookmarksOnPopupHidden;
          this._removeBookmarksOnPopupHidden = false;
          let guidsForRemoval = this._itemGuids;
          this._itemGuids = null;

          if (this._batching) {
            this.endBatch();
          }

          if (removeBookmarksOnPopupHidden && guidsForRemoval) {
            if (this._isNewBookmark) {
              PlacesTransactions.undo().catch(Cu.reportError);
              break;
            }
            // Remove all bookmarks for the bookmark's url, this also removes
            // the tags for the url.
            PlacesTransactions.Remove(guidsForRemoval)
                              .transact().catch(Cu.reportError);
          } else if (this._isNewBookmark) {
            this.showConfirmation();
          }

          if (!removeBookmarksOnPopupHidden) {
            this._storeRecentlyUsedFolder(selectedFolderGuid).catch(console.error);
          }
        }
        break;
      }
      case "keypress":
        clearTimeout(this._autoCloseTimer);
        this._autoCloseTimerEnabled = false;

        if (aEvent.defaultPrevented) {
          // The event has already been consumed inside of the panel.
          break;
        }

        switch (aEvent.keyCode) {
          case KeyEvent.DOM_VK_ESCAPE:
            if (this._isNewBookmark) {
              this._removeBookmarksOnPopupHidden = true;
            }
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
          let delay = this._autoCloseTimeout;
          if (this._closePanelQuickForTesting) {
            delay /= 10;
          }
          clearTimeout(this._autoCloseTimer);
          this._autoCloseTimer = setTimeout(() => {
            if (!this.panel.matches(":hover")) {
              this.panel.hidePopup(true);
            }
          }, delay);
          this._autoCloseTimerEnabled = true;
        }
        break;
    }
  },

  getRecommendation(data) {
    return BookmarkPanelHub.messageRequest(data, window);
  },

  toggleRecommendation() {
    BookmarkPanelHub.toggleRecommendation();
  },

  async showEditBookmarkPopup(aNode, aIsNewBookmark, aUrl) {
    // Slow double-clicks (not true double-clicks) shouldn't
    // cause the panel to flicker.
    if (this.panel.state != "closed") {
      return;
    }

    this._isNewBookmark = aIsNewBookmark;
    this._itemGuids = null;

    this._element("editBookmarkPanelTitle").value =
      this._isNewBookmark ?
        gNavigatorBundle.getString("editBookmarkPanel.newBookmarkTitle") :
        gNavigatorBundle.getString("editBookmarkPanel.editBookmarkTitle");

    this._element("editBookmarkPanel_showForNewBookmarks").checked =
      this.showForNewBookmarks;

    this._itemGuids = [];
    await PlacesUtils.bookmarks.fetch({url: aUrl},
      bookmark => this._itemGuids.push(bookmark.guid));

    let removeButton = this._element("editBookmarkPanelRemoveButton");
    if (this._isNewBookmark) {
      removeButton.label = gNavigatorBundle.getString("editBookmarkPanel.cancel.label");
      removeButton.setAttribute("accesskey",
        gNavigatorBundle.getString("editBookmarkPanel.cancel.accesskey"));
    } else {
      // The label of the remove button differs if the URI is bookmarked
      // multiple times.
      let bookmarksCount = this._itemGuids.length;
      let forms = gNavigatorBundle.getString("editBookmark.removeBookmarks.label");
      let label = PluralForm.get(bookmarksCount, forms)
                            .replace("#1", bookmarksCount);
      removeButton.label = label;
      removeButton.setAttribute("accesskey",
        gNavigatorBundle.getString("editBookmark.removeBookmarks.accesskey"));
    }

    this._setIconAndPreviewImage();

    await this.getRecommendation({
      container: this._element("editBookmarkPanelRecommendation"),
      infoButton: this._element("editBookmarkPanelInfoButton"),
      recommendationContainer: this._element("editBookmarkPanelRecommendation"),
      document,
      url: aUrl.href,
      close: e => {
        e.stopPropagation();
        BookmarkPanelHub.toggleRecommendation(false);
      },
      hidePopup: () => {
        this.panel.hidePopup();
      },
    });

    this.beginBatch();

    this._anchorElement = BookmarkingUI.anchor;
    this._anchorElement.setAttribute("open", "true");

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
      }, {capture: true, once: true});
    };
    gEditItemOverlay.initPanel({ node: aNode,
                                 onPanelReady,
                                 hiddenRows: ["location", "keyword"],
                                 focusedElement: "preferred"});

    this.panel.openPopup(this._anchorElement, "bottomcenter topright");
  },

  _setIconAndPreviewImage() {
    let faviconImage = this._element("editBookmarkPanelFavicon");
    faviconImage.removeAttribute("iconloadingprincipal");
    faviconImage.removeAttribute("src");

    let tab = gBrowser.selectedTab;
    if (tab.hasAttribute("image") && !tab.hasAttribute("busy")) {
      faviconImage.setAttribute("iconloadingprincipal",
                                tab.getAttribute("iconloadingprincipal"));
      faviconImage.setAttribute("src", tab.getAttribute("image"));
    }

    let canvas = PageThumbs.createCanvas(window);
    PageThumbs.captureToCanvas(gBrowser.selectedBrowser, canvas);
    document.mozSetImageElement("editBookmarkPanelImageCanvas", canvas);
  },

  removeBookmarkButtonCommand: function SU_removeBookmarkButtonCommand() {
    this._removeBookmarksOnPopupHidden = true;
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
    this._batchBlockingDeferred = PromiseUtils.defer();
    PlacesTransactions.batch(async () => {
      // First await for the batch to be concluded.
      await this._batchBlockingDeferred.promise;
      // And then for any pending promises added in the meanwhile.
      await Promise.all(gEditItemOverlay.transactionPromises);
    });
    this._batching = true;
  },

  endBatch() {
    if (!this._batching)
      return;

    this._batchBlockingDeferred.resolve();
    this._batchBlockingDeferred = null;
    this._batching = false;
  },

  async _storeRecentlyUsedFolder(selectedFolderGuid) {
    // These are displayed by default, so don't save the folder for them.
    if (!selectedFolderGuid ||
        PlacesUtils.bookmarks.userContentRoots.includes(selectedFolderGuid)) {
      return;
    }

    // List of recently used folders:
    let lastUsedFolderGuids =
      await PlacesUtils.metadata.get(PlacesUIUtils.LAST_USED_FOLDERS_META_KEY, []);

    let index = lastUsedFolderGuids.indexOf(selectedFolderGuid);
    if (index > 1) {
      // The guid is in the array but not the most recent.
      lastUsedFolderGuids.splice(index, 1);
      lastUsedFolderGuids.unshift(selectedFolderGuid);
    } else if (index == -1) {
      lastUsedFolderGuids.unshift(selectedFolderGuid);
    }
    if (lastUsedFolderGuids.length > 5) {
      lastUsedFolderGuids.pop();
    }

    await PlacesUtils.metadata.set(PlacesUIUtils.LAST_USED_FOLDERS_META_KEY,
                                   lastUsedFolderGuids);
  },

  onShowForNewBookmarksCheckboxCommand() {
    Services.prefs.setBoolPref("browser.bookmarks.editDialog.showForNewBookmarks",
      this._element("editBookmarkPanel_showForNewBookmarks").checked);
  },

  showConfirmation() {
    let animationTriggered = LibraryUI.triggerLibraryAnimation("bookmark");

    // Show the "Saved to Library!" hint in addition to the library button
    // animation for the first three times, or when the animation was skipped
    // e.g. because the library button has been customized away.
    const HINT_COUNT_PREF =
      "browser.bookmarks.editDialog.confirmationHintShowCount";
    const HINT_COUNT = Services.prefs.getIntPref(HINT_COUNT_PREF, 0);
    if (animationTriggered && HINT_COUNT >= 3) {
      return;
    }
    Services.prefs.setIntPref(HINT_COUNT_PREF, HINT_COUNT + 1);

    let anchor;
    if (window.toolbar.visible) {
      for (let id of ["library-button", "bookmarks-menu-button"]) {
        let element = document.getElementById(id);
        if (element &&
            element.getAttribute("cui-areatype") != "menu-panel" &&
            element.getAttribute("overflowedItem") != "true") {
          anchor = element;
          break;
        }
      }
    }
    if (!anchor) {
      anchor = document.getElementById("PanelUI-menu-button");
    }
    ConfirmationHint.show(anchor, "pageBookmarked");
  },
};

var PlacesCommandHook = {
  /**
   * Adds a bookmark to the page loaded in the current browser.
   */
  async bookmarkPage() {
    let browser = gBrowser.selectedBrowser;
    let url = new URL(browser.currentURI.spec);
    let info = await PlacesUtils.bookmarks.fetch({ url });
    let isNewBookmark = !info;
    let showEditUI = !isNewBookmark || StarUI.showForNewBookmarks;
    if (isNewBookmark) {
      let parentGuid = PlacesUtils.bookmarks.unfiledGuid;
      info = { url, parentGuid };
      // Bug 1148838 - Make this code work for full page plugins.
      let charset = null;

      let isErrorPage = false;
      if (browser.documentURI) {
        isErrorPage = /^about:(neterror|certerror|blocked)/.test(browser.documentURI.spec);
      }

      try {
        if (isErrorPage) {
          let entry = await PlacesUtils.history.fetch(browser.currentURI);
          if (entry) {
            info.title = entry.title;
          }
        } else {
          info.title = browser.contentTitle;
        }
        info.title = info.title || url.href;
        charset = browser.characterSet;
      } catch (e) {
        Cu.reportError(e);
      }

      if (showEditUI) {
        // If we bookmark the page here but open right into a cancelable
        // state (i.e. new bookmark in Library), start batching here so
        // all of the actions can be undone in a single undo step.
        StarUI.beginBatch();
      }

      info.guid = await PlacesTransactions.NewBookmark(info).transact();

      if (charset) {
        PlacesUIUtils.setCharsetForPage(url, charset, window).catch(Cu.reportError);
      }
    }

    // Revert the contents of the location bar
    gURLBar.handleRevert();

    // If it was not requested to open directly in "edit" mode, we are done.
    if (!showEditUI) {
      StarUI.showConfirmation();
      return;
    }

    let node = await PlacesUIUtils.promiseNodeLikeFromFetchInfo(info);

    await StarUI.showEditBookmarkPopup(node, isNewBookmark, url);
  },

  /**
   * Adds a bookmark to the page targeted by a link.
   * @param url (string)
   *        the address of the link target
   * @param title
   *        The link text
   */
  async bookmarkLink(url, title) {
    let bm = await PlacesUtils.bookmarks.fetch({url});
    if (bm) {
      let node = await PlacesUIUtils.promiseNodeLikeFromFetchInfo(bm);
      PlacesUIUtils.showBookmarkDialog({ action: "edit", node }, window.top);
      return;
    }

    let defaultInsertionPoint = new PlacesInsertionPoint({
      parentId: PlacesUtils.bookmarksMenuFolderId,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
    });
    PlacesUIUtils.showBookmarkDialog({ action: "add",
                                       type: "bookmark",
                                       uri: Services.io.newURI(url),
                                       title,
                                       defaultInsertionPoint,
                                       hiddenRows: [ "location", "keyword" ],
                                     }, window.top);
  },

  /**
   * List of nsIURI objects characterizing tabs given in param.
   * Duplicates are discarded.
  */
  getUniquePages(tabs) {
    let uniquePages = {};
    let URIs = [];

    tabs.forEach(tab => {
      let browser = tab.linkedBrowser;
      let uri = browser.currentURI;
      let title = browser.contentTitle || tab.label;
      let spec = uri.spec;
      if (!(spec in uniquePages)) {
        uniquePages[spec] = null;
        URIs.push({ uri, title });
      }
    });
    return URIs;
  },

  /**
   * List of nsIURI objects characterizing the tabs currently open in the
   * browser, modulo pinned tabs. The URIs will be in the order in which their
   * corresponding tabs appeared and duplicates are discarded.
   */
  get uniqueCurrentPages() {
    let visibleUnpinnedTabs = gBrowser.visibleTabs
                                      .filter(tab => !tab.pinned);
    return this.getUniquePages(visibleUnpinnedTabs);
  },

   /**
   * List of nsIURI objects characterizing the tabs currently
   * selected in the window. Duplicates are discarded.
   */
  get uniqueSelectedPages() {
    return this.getUniquePages(gBrowser.selectedTabs);
  },

  /**
   * Adds a folder with bookmarks to URIList given in param.
   */
  bookmarkPages(URIList) {
    if (!URIList.length) {
      return;
    }

    let bookmarkDialogInfo = {action: "add"};
    if (URIList.length > 1) {
      bookmarkDialogInfo.type = "folder";
      bookmarkDialogInfo.URIList = URIList;
    } else {
      bookmarkDialogInfo.type = "bookmark";
      bookmarkDialogInfo.title = URIList[0].title;
      bookmarkDialogInfo.uri = URIList[0].uri;
    }

    PlacesUIUtils.showBookmarkDialog(bookmarkDialogInfo, window);
  },

  /**
   * Opens the Places Organizer.
   * @param {String} item The item to select in the organizer window,
   *                      options are (case sensitive):
   *                      BookmarksMenu, BookmarksToolbar, UnfiledBookmarks,
   *                      AllBookmarks, History, Downloads.
   */
  showPlacesOrganizer(item) {
    var organizer = Services.wm.getMostRecentWindow("Places:Organizer");
    // Due to bug 528706, getMostRecentWindow can return closed windows.
    if (!organizer || organizer.closed) {
      // No currently open places window, so open one with the specified mode.
      openDialog("chrome://browser/content/places/places.xul",
                 "", "chrome,toolbar=yes,dialog=no,resizable", item);
    } else {
      organizer.PlacesOrganizer.selectLeftPaneContainerByHierarchy(item);
      organizer.focus();
    }
  },

  searchBookmarks() {
    gURLBar.search(UrlbarTokenizer.RESTRICT.BOOKMARK);
  },
};

ChromeUtils.defineModuleGetter(this, "RecentlyClosedTabsAndWindowsMenuUtils",
  "resource:///modules/sessionstore/RecentlyClosedTabsAndWindowsMenuUtils.jsm");

// View for the history menu.
function HistoryMenu(aPopupShowingEvent) {
  // Workaround for Bug 610187.  The sidebar does not include all the Places
  // views definitions, and we don't need them there.
  // Defining the prototype inheritance in the prototype itself would cause
  // browser.js to halt on "PlacesMenu is not defined" error.
  this.__proto__.__proto__ = PlacesMenu.prototype;
  Object.keys(this._elements).forEach(name => {
    this[name] = document.getElementById(this._elements[name]);
  });
  PlacesMenu.call(this, aPopupShowingEvent,
                  "place:sort=4&maxResults=15");
}

HistoryMenu.prototype = {
  _elements: {
    undoTabMenu: "historyUndoMenu",
    hiddenTabsMenu: "hiddenTabsMenu",
    undoWindowMenu: "historyUndoWindowMenu",
    syncTabsMenuitem: "sync-tabs-menuitem",
  },

  _getClosedTabCount() {
    try {
      return SessionStore.getClosedTabCount(window);
    } catch (ex) {
      // SessionStore doesn't track the hidden window, so just return zero then.
      return 0;
    }
  },

  toggleHiddenTabs() {
    if (window.gBrowser &&
        gBrowser.visibleTabs.length < gBrowser.tabs.length) {
      this.hiddenTabsMenu.removeAttribute("hidden");
    } else {
      this.hiddenTabsMenu.setAttribute("hidden", "true");
    }
  },

  toggleRecentlyClosedTabs: function HM_toggleRecentlyClosedTabs() {
    // enable/disable the Recently Closed Tabs sub menu
    // no restorable tabs, so disable menu
    if (this._getClosedTabCount() == 0)
      this.undoTabMenu.setAttribute("disabled", true);
    else
      this.undoTabMenu.removeAttribute("disabled");
  },

  /**
   * Populate when the history menu is opened
   */
  populateUndoSubmenu: function PHM_populateUndoSubmenu() {
    var undoPopup = this.undoTabMenu.menupopup;

    // remove existing menu items
    while (undoPopup.hasChildNodes())
      undoPopup.firstChild.remove();

    // no restorable tabs, so make sure menu is disabled, and return
    if (this._getClosedTabCount() == 0) {
      this.undoTabMenu.setAttribute("disabled", true);
      return;
    }

    // enable menu
    this.undoTabMenu.removeAttribute("disabled");

    // populate menu
    let tabsFragment = RecentlyClosedTabsAndWindowsMenuUtils.getTabsFragment(window, "menuitem");
    undoPopup.appendChild(tabsFragment);
  },

  toggleRecentlyClosedWindows: function PHM_toggleRecentlyClosedWindows() {
    // enable/disable the Recently Closed Windows sub menu
    // no restorable windows, so disable menu
    if (SessionStore.getClosedWindowCount() == 0)
      this.undoWindowMenu.setAttribute("disabled", true);
    else
      this.undoWindowMenu.removeAttribute("disabled");
  },

  /**
   * Populate when the history menu is opened
   */
  populateUndoWindowSubmenu: function PHM_populateUndoWindowSubmenu() {
    let undoPopup = this.undoWindowMenu.menupopup;

    // remove existing menu items
    while (undoPopup.hasChildNodes())
      undoPopup.firstChild.remove();

    // no restorable windows, so make sure menu is disabled, and return
    if (SessionStore.getClosedWindowCount() == 0) {
      this.undoWindowMenu.setAttribute("disabled", true);
      return;
    }

    // enable menu
    this.undoWindowMenu.removeAttribute("disabled");

    // populate menu
    let windowsFragment = RecentlyClosedTabsAndWindowsMenuUtils.getWindowsFragment(window, "menuitem");
    undoPopup.appendChild(windowsFragment);
  },

  toggleTabsFromOtherComputers: function PHM_toggleTabsFromOtherComputers() {
    // Enable/disable the Tabs From Other Computers menu. Some of the menus handled
    // by HistoryMenu do not have this menuitem.
    if (!this.syncTabsMenuitem)
      return;

    if (!PlacesUIUtils.shouldShowTabsFromOtherComputersMenuitem()) {
      this.syncTabsMenuitem.setAttribute("hidden", true);
      return;
    }

    this.syncTabsMenuitem.setAttribute("hidden", false);
  },

  _onPopupShowing: function HM__onPopupShowing(aEvent) {
    PlacesMenu.prototype._onPopupShowing.apply(this, arguments);

    // Don't handle events for submenus.
    if (aEvent.target != aEvent.currentTarget)
      return;

    this.toggleHiddenTabs();
    this.toggleRecentlyClosedTabs();
    this.toggleRecentlyClosedWindows();
    this.toggleTabsFromOtherComputers();
  },

  _onCommand: function HM__onCommand(aEvent) {
    aEvent = getRootEvent(aEvent);
    let placesNode = aEvent.target._placesNode;
    if (placesNode) {
      if (!PrivateBrowsingUtils.isWindowPrivate(window))
        PlacesUIUtils.markPageAsTyped(placesNode.uri);
      openUILink(placesNode.uri, aEvent, {
        ignoreAlt: true,
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    }
  },
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

  onMouseUp(aEvent) {
    // Handles left-click with modifier if not browser.bookmarks.openInTabClosesMenu.
    if (aEvent.button != 0 || PlacesUIUtils.openInTabClosesMenu)
      return;
    let target = aEvent.originalTarget;
    if (target.tagName != "menuitem")
      return;
    let modifKey = AppConstants.platform === "macosx" ? aEvent.metaKey
                                                      : aEvent.ctrlKey;
    if (modifKey) {
      target.setAttribute("closemenu", "none");
      var menupopup = target.parentNode;
      menupopup.addEventListener("popuphidden", () => {
        target.removeAttribute("closemenu");
      }, {once: true});
    } else {
      // Handles edge case where same menuitem was opened previously
      // while menu was kept open, but now menu should close.
      target.removeAttribute("closemenu");
    }
  },

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
    // If this event bubbled up from a menu or menuitem,
    // close the menus if browser.bookmarks.openInTabClosesMenu.
    var tag = target.tagName;
    if (PlacesUIUtils.openInTabClosesMenu && (tag == "menuitem" || tag == "menu")) {
      closeMenus(aEvent.target);
    }

    if (target._placesNode && PlacesUtils.nodeIsContainer(target._placesNode)) {
      // Don't open the root folder in tabs when the empty area on the toolbar
      // is middle-clicked or when a non-bookmark item (except for Open in Tabs)
      // in a bookmarks menupopup is middle-clicked.
      if (target.localName == "menu" || target.localName == "toolbarbutton")
        PlacesUIUtils.openMultipleLinksInTabs(target._placesNode, aEvent, aView);
    } else if (aEvent.button == 1) {
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
  onCommand: function BEH_onCommand(aEvent) {
    var target = aEvent.originalTarget;
    if (target._placesNode)
      PlacesUIUtils.openNodeWithEvent(target._placesNode, aEvent);
  },

  fillInBHTooltip: function BEH_fillInBHTooltip(aDocument, aEvent) {
    var node;
    var cropped = false;
    var targetURI;

    if (aDocument.tooltipNode.localName == "treechildren") {
      var tree = aDocument.tooltipNode.parentNode;
      var cell = tree.getCellAt(aEvent.clientX, aEvent.clientY);
      if (cell.row == -1)
        return false;
      node = tree.view.nodeForTreeIndex(cell.row);
      cropped = tree.isCellCropped(cell.row, cell.col);
    } else {
      // Check whether the tooltipNode is a Places node.
      // In such a case use it, otherwise check for targetURI attribute.
      var tooltipNode = aDocument.tooltipNode;
      if (tooltipNode._placesNode) {
        node = tooltipNode._placesNode;
      } else {
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
  },
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
      popup.openPopup();
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
                  node.getAttribute("type") == "menu");
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
    let ip = new PlacesInsertionPoint({
      parentId: PlacesUtils.bookmarksMenuFolderId,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
    });
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
    let ip = new PlacesInsertionPoint({
      parentId: PlacesUtils.bookmarksMenuFolderId,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
    });
    PlacesControllerDragHelper.onDrop(ip, event.dataTransfer);
    PlacesControllerDragHelper.currentDropTarget = null;
    event.stopPropagation();
  },
};

/**
 * This object handles the initialization and uninitialization of the bookmarks
 * toolbar.
 */
var PlacesToolbarHelper = {
  get _viewElt() {
    return document.getElementById("PlacesToolbar");
  },

  init: function PTH_init() {
    let viewElt = this._viewElt;
    if (!viewElt || viewElt._placesView)
      return;

    // CustomizableUI.addListener is idempotent, so we can safely
    // call this multiple times.
    CustomizableUI.addListener(this);

    if (!this._isObservingToolbars) {
      this._isObservingToolbars = true;
      window.addEventListener("toolbarvisibilitychange", this);
    }

    // If the bookmarks toolbar item is:
    // - not in a toolbar, or;
    // - the toolbar is collapsed, or;
    // - the toolbar is hidden some other way:
    // don't initialize.  Also, there is no need to initialize the toolbar if
    // customizing, because that will happen when the customization is done.
    let toolbar = this._getParentToolbar(viewElt);
    if (!toolbar || toolbar.collapsed || this._isCustomizing ||
        getComputedStyle(toolbar, "").display == "none") {
      return;
    }

    new PlacesToolbar(`place:parent=${PlacesUtils.bookmarks.toolbarGuid}`);
  },

  handleEvent(event) {
    switch (event.type) {
      case "toolbarvisibilitychange":
        if (event.target == this._getParentToolbar(this._viewElt))
          this._resetView();
        break;
    }
  },

  uninit: function PTH_uninit() {
    if (this._isObservingToolbars) {
      delete this._isObservingToolbars;
      window.removeEventListener("toolbarvisibilitychange", this);
    }
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
  },

  customizeDone: function PTH_customizeDone() {
    this._isCustomizing = false;
    this.init();
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
      this.init();
    }
  },
};

/**
 * Handles the Library button in the toolbar.
 */
var LibraryUI = {
  /**
   * @returns true if the animation could be triggered, false otherwise.
   */
  triggerLibraryAnimation(animation) {
    if (!this.hasOwnProperty("COSMETIC_ANIMATIONS_ENABLED")) {
      XPCOMUtils.defineLazyPreferenceGetter(this, "COSMETIC_ANIMATIONS_ENABLED",
        "toolkit.cosmeticAnimations.enabled", true);
    }

    let libraryButton = document.getElementById("library-button");
    if (!libraryButton ||
        libraryButton.getAttribute("cui-areatype") == "menu-panel" ||
        libraryButton.getAttribute("overflowedItem") == "true" ||
        !libraryButton.closest("#nav-bar") ||
        !window.toolbar.visible ||
        !this.COSMETIC_ANIMATIONS_ENABLED) {
      return false;
    }

    let animatableBox = document.getElementById("library-animatable-box");
    let navBar = document.getElementById("nav-bar");
    let libraryIcon = document.getAnonymousElementByAttribute(libraryButton, "class", "toolbarbutton-icon");
    let iconBounds = window.windowUtils.getBoundsWithoutFlushing(libraryIcon);
    let libraryBounds = window.windowUtils.getBoundsWithoutFlushing(libraryButton);

    animatableBox.style.setProperty("--library-button-height", libraryBounds.height + "px");
    animatableBox.style.setProperty("--library-icon-x", iconBounds.x + "px");
    if (navBar.hasAttribute("brighttext")) {
      animatableBox.setAttribute("brighttext", "true");
    } else {
      animatableBox.removeAttribute("brighttext");
    }
    animatableBox.removeAttribute("fade");
    libraryButton.setAttribute("animate", animation);
    animatableBox.setAttribute("animate", animation);
    if (!this._libraryButtonAnimationEndListeners[animation]) {
      this._libraryButtonAnimationEndListeners[animation] = event => {
        this._libraryButtonAnimationEndListener(event, animation);
      };
    }
    animatableBox.addEventListener("animationend", this._libraryButtonAnimationEndListeners[animation]);

    window.addEventListener("resize", this._onWindowResize);

    return true;
  },

  _libraryButtonAnimationEndListeners: {},
  _libraryButtonAnimationEndListener(aEvent, animation) {
    let animatableBox = document.getElementById("library-animatable-box");
    if (aEvent.animationName.startsWith(`library-${animation}-animation`)) {
      animatableBox.setAttribute("fade", "true");
    } else if (aEvent.animationName == `library-${animation}-fade`) {
      animatableBox.removeEventListener("animationend", LibraryUI._libraryButtonAnimationEndListeners[animation]);
      animatableBox.removeAttribute("animate");
      animatableBox.removeAttribute("fade");
      window.removeEventListener("resize", this._onWindowResize);
      let libraryButton = document.getElementById("library-button");
      // Put the 'fill' back in the normal icon.
      libraryButton.removeAttribute("animate");
    }
  },

  _windowResizeRunning: false,
  _onWindowResize(aEvent) {
    if (LibraryUI._windowResizeRunning) {
      return;
    }
    LibraryUI._windowResizeRunning = true;

    requestAnimationFrame(() => {
      let libraryButton = document.getElementById("library-button");
      // Only update the position if the library button remains in the
      // navbar (not moved to the palette or elsewhere).
      if (!libraryButton ||
          libraryButton.getAttribute("cui-areatype") == "menu-panel" ||
          libraryButton.getAttribute("overflowedItem") == "true" ||
          !libraryButton.closest("#nav-bar")) {
        return;
      }

      let animatableBox = document.getElementById("library-animatable-box");
      let libraryIcon = document.getAnonymousElementByAttribute(libraryButton, "class", "toolbarbutton-icon");
      let iconBounds = window.windowUtils.getBoundsWithoutFlushing(libraryIcon);

      // Resizing the window will only have the ability to change the X offset of the
      // library button.
      animatableBox.style.setProperty("--library-icon-x", iconBounds.x + "px");

      LibraryUI._windowResizeRunning = false;
    });
  },
};

/**
 * Handles the bookmarks menu-button in the toolbar.
 */

var BookmarkingUI = {
  STAR_ID: "star-button",
  STAR_BOX_ID: "star-button-box",
  BOOKMARK_BUTTON_ID: "bookmarks-menu-button",
  BOOKMARK_BUTTON_SHORTCUT: "addBookmarkAsKb",
  get button() {
    delete this.button;
    let widgetGroup = CustomizableUI.getWidget(this.BOOKMARK_BUTTON_ID);
    return this.button = widgetGroup.forWindow(window).node;
  },

  get star() {
    delete this.star;
    return this.star = document.getElementById(this.STAR_ID);
  },

  get starBox() {
    delete this.starBox;
    return this.starBox = document.getElementById(this.STAR_BOX_ID);
  },

  get anchor() {
    let action = PageActions.actionForID(PageActions.ACTION_ID_BOOKMARK);
    return BrowserPageActions.panelAnchorNodeForAction(action);
  },

  get stringbundleset() {
    delete this.stringbundleset;
    return this.stringbundleset = document.getElementById("stringbundleset");
  },

  STATUS_UPDATING: -1,
  STATUS_UNSTARRED: 0,
  STATUS_STARRED: 1,
  get status() {
    if (this._pendingUpdate)
      return this.STATUS_UPDATING;
    return this.star.hasAttribute("starred") ? this.STATUS_STARRED
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

  onPopupShowing: function BUI_onPopupShowing(event) {
    // Don't handle events for submenus.
    if (event.target != event.currentTarget)
      return;

    // On non-photon, this code should never be reached. However, if you click
    // the outer button's border, some cpp code for the menu button's XBL
    // binding decides to open the popup even though the dropmarker is invisible.
    //
    // Separately, in Photon, if the button is in the dynamic portion of the
    // overflow panel, we want to show a subview instead.
    if (this.button.getAttribute("cui-areatype") == CustomizableUI.TYPE_MENU_PANEL ||
        this.button.hasAttribute("overflowedItem")) {
      this._showSubView();
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

    this.selectLabel("BMB_viewBookmarksSidebar",
                     SidebarUI.currentID == "viewBookmarksSidebar");
    this.selectLabel("BMB_viewBookmarksToolbar",
                     !document.getElementById("PersonalToolbar").collapsed);
  },

  selectLabel(elementId, visible) {
    let element = document.getElementById(elementId);
    element.setAttribute("label", element.getAttribute(visible ? "label-hide"
                                                               : "label-show"));
  },

  toggleBookmarksToolbar() {
    CustomizableUI.setToolbarVisibility("PersonalToolbar",
      document.getElementById("PersonalToolbar").collapsed);
  },

  attachPlacesView(event, node) {
    // If the view is already there, bail out early.
    if (node.parentNode._placesView)
      return;

    new PlacesMenu(event, `place:parent=${PlacesUtils.bookmarks.menuGuid}`, {
      extraClasses: {
        entry: "subviewbutton",
        footer: "panel-subview-footer",
      },
      insertionPoint: ".panel-subview-footer",
    });
  },

  // Set by sync after syncing bookmarks successfully once.
  MOBILE_BOOKMARKS_PREF: "browser.bookmarks.showMobileBookmarks",

  _shouldShowMobileBookmarks() {
    return Services.prefs.getBoolPref(this.MOBILE_BOOKMARKS_PREF, false);
  },

  _initMobileBookmarks(mobileMenuItem) {
    mobileMenuItem.hidden = !this._shouldShowMobileBookmarks();
  },

  _uninitView: function BUI__uninitView() {
    // When an element with a placesView attached is removed and re-inserted,
    // XBL reapplies the binding causing any kind of issues and possible leaks,
    // so kill current view and let popupshowing generate a new one.
    if (this.button._placesView)
      this.button._placesView.uninit();
    // Also uninit the main menubar placesView, since it would have the same
    // issues.
    let menubar = document.getElementById("bookmarksMenu");
    if (menubar && menubar._placesView)
      menubar._placesView.uninit();

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
    // If we're moved outside of customize mode, we need to uninit
    // our view so it gets reconstructed.
    if (!this._isCustomizing) {
      this._uninitView();
    }
  },

  onCustomizeEnd: function BUI_customizeEnd(aWindow) {
    if (aWindow == window) {
      this._isCustomizing = false;
    }
  },

  init() {
    CustomizableUI.addListener(this);

    if (Services.prefs.getBoolPref("toolkit.cosmeticAnimations.enabled")) {
      let starButtonBox = document.getElementById("star-button-box");
      starButtonBox.setAttribute("animationsenabled", "true");
      this.star.addEventListener("mouseover", this, {once: true});
    }
  },

  _hasBookmarksObserver: false,
  _itemGuids: new Set(),
  uninit: function BUI_uninit() {
    this.updateBookmarkPageMenuItem(true);
    CustomizableUI.removeListener(this);

    this.star.removeEventListener("mouseover", this);

    this._uninitView();

    if (this._hasBookmarksObserver) {
      PlacesUtils.bookmarks.removeObserver(this);
      PlacesUtils.observers.removeListener(["bookmark-added"], this.handlePlacesEvents);
    }

    if (this._pendingUpdate) {
      delete this._pendingUpdate;
    }
  },

  onLocationChange: function BUI_onLocationChange() {
    if (this._uri && gBrowser.currentURI.equals(this._uri)) {
      return;
    }
    this.updateStarState();
  },

  updateStarState: function BUI_updateStarState() {
    this._uri = gBrowser.currentURI;
    this._itemGuids.clear();
    let guids = new Set();

    // those objects are use to check if we are in the current iteration before
    // returning any result.
    let pendingUpdate = this._pendingUpdate = {};

    PlacesUtils.bookmarks.fetch({url: this._uri}, b => guids.add(b.guid), { concurrent: true })
      .catch(Cu.reportError)
      .then(() => {
         if (pendingUpdate != this._pendingUpdate) {
           return;
         }

         // It's possible that "bookmark-added" gets called before the async statement
         // calls back.  For such an edge case, retain all unique entries from the
         // array.
         if (this._itemGuids.size > 0) {
           this._itemGuids = new Set(...this._itemGuids, ...guids);
         } else {
           this._itemGuids = guids;
         }

         this._updateStar();

         // Start observing bookmarks if needed.
         if (!this._hasBookmarksObserver) {
           try {
             PlacesUtils.bookmarks.addObserver(this);
            this.handlePlacesEvents = this.handlePlacesEvents.bind(this);
            PlacesUtils.observers.addListener(["bookmark-added"], this.handlePlacesEvents);
             this._hasBookmarksObserver = true;
           } catch (ex) {
             Cu.reportError("BookmarkingUI failed adding a bookmarks observer: " + ex);
           }
         }

         delete this._pendingUpdate;
       });
  },

  _updateStar: function BUI__updateStar() {
    let starred = this._itemGuids.size > 0;
    if (!starred) {
      this.star.removeAttribute("animate");
    }

    // Update the image for all elements.
    for (let element of [
      this.star,
      document.getElementById("context-bookmarkpage"),
      document.getElementById("panelMenuBookmarkThisPage"),
      document.getElementById("pageAction-panel-bookmark"),
    ]) {
      if (!element) {
        // The page action panel element may not have been created yet.
        continue;
      }
      if (starred) {
        element.setAttribute("starred", "true");
        Services.obs.notifyObservers(null, "bookmark-icon-updated", "starred");
      } else {
        element.removeAttribute("starred");
        Services.obs.notifyObservers(null, "bookmark-icon-updated", "unstarred");
      }
    }

    // Update the tooltip for elements that require it.
    for (let element of [
      this.star,
      document.getElementById("context-bookmarkpage"),
    ]) {
      element.setAttribute("tooltiptext", starred ? this._starredTooltip
                                                  : this._unstarredTooltip);
    }
  },

  /**
   * forceReset is passed when we're destroyed and the label should go back
   * to the default (Bookmark This Page) for OS X.
   */
  updateBookmarkPageMenuItem: function BUI_updateBookmarkPageMenuItem(forceReset) {
    if (!this.stringbundleset) {
      // We are loaded in a non-browser context, like the sidebar.
      return;
    }
    let isStarred = !forceReset && this._itemGuids.size > 0;
    let label = this.stringbundleset.getAttribute(
      isStarred ? "string-editthisbookmark" : "string-bookmarkthispage");

    let panelMenuToolbarButton =
      document.getElementById("panelMenuBookmarkThisPage");
    if (!panelMenuToolbarButton) {
      // We don't have the star UI or context menu (e.g. we're the hidden
      // window). So we just set the bookmarks menu item label and exit.
      document.getElementById("menu_bookmarkThisPage")
              .setAttribute("label", label);
      return;
    }

    for (let element of [
      document.getElementById("menu_bookmarkThisPage"),
      document.getElementById("context-bookmarkpage"),
      panelMenuToolbarButton,
    ]) {
      element.setAttribute("label", label);
    }

    // Update the title and the starred state for the page action panel.
    PageActions.actionForID(PageActions.ACTION_ID_BOOKMARK)
               .setTitle(label, window);
    this._updateStar();
  },

  onMainMenuPopupShowing: function BUI_onMainMenuPopupShowing(event) {
    // Don't handle events for submenus.
    if (event.target != event.currentTarget)
      return;

    this.updateBookmarkPageMenuItem();
    this._initMobileBookmarks(document.getElementById("menu_mobileBookmarks"));
  },

  showSubView(anchor) {
    this._showSubView(null, anchor);
  },

  _showSubView(event, anchor = document.getElementById(this.BOOKMARK_BUTTON_ID)) {
    let view = document.getElementById("PanelUI-bookmarks");
    view.addEventListener("ViewShowing", this);
    view.addEventListener("ViewHiding", this);
    anchor.setAttribute("closemenu", "none");
    PanelUI.showSubView("PanelUI-bookmarks", anchor, event);
  },

  onCommand: function BUI_onCommand(aEvent) {
    if (aEvent.target != aEvent.currentTarget) {
      return;
    }

    // Handle special case when the button is in the panel.
    if (this.button.getAttribute("cui-areatype") == CustomizableUI.TYPE_MENU_PANEL) {
      this._showSubView(aEvent);
      return;
    }
    let widget = CustomizableUI.getWidget(this.BOOKMARK_BUTTON_ID)
                               .forWindow(window);
    if (widget.overflowed) {
      // Close the overflow panel because the Edit Bookmark panel will appear.
      widget.node.removeAttribute("closemenu");
    }
    this.onStarCommand(aEvent);
  },

  onStarCommand(aEvent) {
    // Ignore non-left clicks on the star, or if we are updating its state.
    if (!this._pendingUpdate && (aEvent.type != "click" || aEvent.button == 0)) {
      let isBookmarked = this._itemGuids.size > 0;
      if (!isBookmarked) {
        BrowserUtils.setToolbarButtonHeightProperty(this.star);
        // there are no other animations on this element, so we can simply
        // listen for animationend with the "once" option to clean up
        let animatableBox = document.getElementById("star-button-animatable-box");
        animatableBox.addEventListener("animationend", event => {
          this.star.removeAttribute("animate");
        }, { once: true });
        this.star.setAttribute("animate", "true");
      }
      PlacesCommandHook.bookmarkPage();
    }
  },

  onCurrentPageContextPopupShowing() {
    this.updateBookmarkPageMenuItem();
  },

  handleEvent: function BUI_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "mouseover":
        this.star.setAttribute("preloadanimations", "true");
        break;
      case "ViewShowing":
        this.onPanelMenuViewShowing(aEvent);
        break;
      case "ViewHiding":
        this.onPanelMenuViewHiding(aEvent);
        break;
    }
  },

  onPanelMenuViewShowing: function BUI_onViewShowing(aEvent) {
    let panelview = aEvent.target;

    this.updateBookmarkPageMenuItem();

    // Get all statically placed buttons to supply them with keyboard shortcuts.
    let staticButtons = panelview.getElementsByTagName("toolbarbutton");
    for (let i = 0, l = staticButtons.length; i < l; ++i)
      CustomizableUI.addShortcut(staticButtons[i]);

    // Setup the Places view.
    // We restrict the amount of results to 42. Not 50, but 42. Why? Because 42.
    let query = "place:queryType=" + Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS +
      "&sort=" + Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING +
      "&maxResults=42&excludeQueries=1";

    this._panelMenuView = new PlacesPanelview(document.getElementById("panelMenu_bookmarksMenu"),
      panelview, query);
    panelview.removeEventListener("ViewShowing", this);
  },

  onPanelMenuViewHiding: function BUI_onViewHiding(aEvent) {
    this._panelMenuView.uninit();
    delete this._panelMenuView;
    aEvent.target.removeEventListener("ViewHiding", this);
  },

  showBookmarkingTools(triggerNode) {
    let placement = CustomizableUI.getPlacementOfWidget(this.BOOKMARK_BUTTON_ID);
    this.selectLabel("panelMenu_toggleBookmarksMenu",
                     placement && placement.area == CustomizableUI.AREA_NAVBAR);
    this.selectLabel("panelMenu_viewBookmarksSidebar",
                     SidebarUI.currentID == "viewBookmarksSidebar");
    this.selectLabel("panelMenu_viewBookmarksToolbar",
                     !document.getElementById("PersonalToolbar").collapsed);
    PanelUI.showSubView("PanelUI-bookmarkingTools", triggerNode);
  },

  toggleMenuButtonInToolbar(triggerNode) {
    let placement = CustomizableUI.getPlacementOfWidget(this.BOOKMARK_BUTTON_ID);
    const area = CustomizableUI.AREA_NAVBAR;
    if (!placement) {
      // Button is in the palette, so we can move it to the navbar.
      let pos;
      let widgetIDs = CustomizableUI.getWidgetIdsInArea(CustomizableUI.AREA_NAVBAR);
      // If there's a spring inside the navbar, find it and use that as the
      // placement marker.
      let lastSpringID = null;
      for (let i = widgetIDs.length - 1; i >= 0; --i) {
        let id = widgetIDs[i];
        if (CustomizableUI.isSpecialWidget(id) && /spring/.test(id)) {
          lastSpringID = id;
          break;
        }
      }
      if (lastSpringID) {
        pos = CustomizableUI.getPlacementOfWidget(lastSpringID).position + 1;
      } else {
        // Next alternative is to use the searchbar as the placement marker.
        const searchWidgetID = "search-container";
        if (widgetIDs.includes(searchWidgetID)) {
          pos = CustomizableUI.getPlacementOfWidget(searchWidgetID).position + 1;
        } else {
          // Last alternative is to use the navbar as the placement marker.
          pos = CustomizableUI.getPlacementOfWidget("urlbar-container").position + 1;
        }
      }

      CustomizableUI.addWidgetToArea(this.BOOKMARK_BUTTON_ID, area, pos);
    } else {
      // Move it back to the palette.
      CustomizableUI.removeWidgetFromArea(this.BOOKMARK_BUTTON_ID);
    }
    triggerNode.setAttribute("checked", !placement);
    updateToggleControlLabel(triggerNode);
  },

  handlePlacesEvents(aEvents) {
    // Only need to update the UI if it wasn't marked as starred before:
    if (this._itemGuids.size == 0) {
      for (let {url, guid} of aEvents) {
        if (url && url == this._uri.spec) {
          // If a new bookmark has been added to the tracked uri, register it.
          if (!this._itemGuids.has(guid)) {
            this._itemGuids.add(guid);
            this._updateStar();
          }
        }
      }
    }
  },

  // nsINavBookmarkObserver
  onItemRemoved(aItemId, aParentId, aIndex, aItemType, aURI, aGuid) {
    // If one of the tracked bookmarks has been removed, unregister it.
    if (this._itemGuids.has(aGuid)) {
      this._itemGuids.delete(aGuid);
      // Only need to update the UI if the page is no longer starred
      if (this._itemGuids.size == 0) {
        this._updateStar();
      }
    }
  },

  onItemChanged(aItemId, aProperty, aIsAnnotationProperty, aNewValue, aLastModified,
                aItemType, aParentId, aGuid) {
    if (aProperty == "uri") {
      // If the changed bookmark was tracked, check if it is now pointing to
      // a different uri and unregister it.
      if (this._itemGuids.has(aGuid) && aNewValue != this._uri.spec) {
        this._itemGuids.delete(aGuid);
        // Only need to update the UI if the page is no longer starred
        if (this._itemGuids.size == 0) {
          this._updateStar();
        }
      } else if (!this._itemGuids.has(aGuid) && aNewValue == this._uri.spec) {
        // If another bookmark is now pointing to the tracked uri, register it.
        this._itemGuids.add(aGuid);
        // Only need to update the UI if it wasn't marked as starred before:
        if (this._itemGuids.size == 1) {
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

  onWidgetUnderflow(aNode, aContainer) {
    let win = aNode.ownerGlobal;
    if (aNode.id != this.BOOKMARK_BUTTON_ID || win != window)
      return;

    // The view gets broken by being removed and reinserted. Uninit
    // here so popupshowing will generate a new one:
    this._uninitView();
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsINavBookmarkObserver,
  ]),
};

var AutoShowBookmarksToolbar = {
  init() {
    Services.obs.addObserver(this, "autoshow-bookmarks-toolbar");
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
  },
};

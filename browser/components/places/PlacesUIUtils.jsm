/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["PlacesUIUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { clearTimeout, setTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["Element"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  CustomizableUI: "resource:///modules/CustomizableUI.jsm",
  MigrationUtils: "resource:///modules/MigrationUtils.jsm",
  OpenInTabsUtils: "resource:///modules/OpenInTabsUtils.jsm",
  PlacesTransactions: "resource://gre/modules/PlacesTransactions.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  Weave: "resource://services-sync/main.js",
});

XPCOMUtils.defineLazyGetter(this, "bundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/places/places.properties"
  );
});

const gInContentProcess =
  Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;
const FAVICON_REQUEST_TIMEOUT = 60 * 1000;
// Map from windows to arrays of data about pending favicon loads.
let gFaviconLoadDataMap = new Map();

const ITEM_CHANGED_BATCH_NOTIFICATION_THRESHOLD = 10;

// copied from utilityOverlay.js
const TAB_DROP_TYPE = "application/x-moz-tabbrowser-tab";
const PREF_LOAD_BOOKMARKS_IN_BACKGROUND =
  "browser.tabs.loadBookmarksInBackground";
const PREF_LOAD_BOOKMARKS_IN_TABS = "browser.tabs.loadBookmarksInTabs";

let InternalFaviconLoader = {
  /**
   * Actually cancel the request, and clear the timeout for cancelling it.
   */
  _cancelRequest({ uri, innerWindowID, timerID, callback }, reason) {
    // Break cycle
    let request = callback.request;
    delete callback.request;
    // Ensure we don't time out.
    clearTimeout(timerID);
    try {
      request.cancel();
    } catch (ex) {
      Cu.reportError(
        "When cancelling a request for " +
          uri.spec +
          " because " +
          reason +
          ", it was already canceled!"
      );
    }
  },

  /**
   * Called for every inner that gets destroyed, only in the parent process.
   */
  removeRequestsForInner(innerID) {
    for (let [window, loadDataForWindow] of gFaviconLoadDataMap) {
      let newLoadDataForWindow = loadDataForWindow.filter(loadData => {
        let innerWasDestroyed = loadData.innerWindowID == innerID;
        if (innerWasDestroyed) {
          this._cancelRequest(
            loadData,
            "the inner window was destroyed or a new favicon was loaded for it"
          );
        }
        // Keep the items whose inner is still alive.
        return !innerWasDestroyed;
      });
      // Map iteration with for...of is safe against modification, so
      // now just replace the old value:
      gFaviconLoadDataMap.set(window, newLoadDataForWindow);
    }
  },

  /**
   * Called when a toplevel chrome window unloads. We use this to tidy up after ourselves,
   * avoid leaks, and cancel any remaining requests. The last part should in theory be
   * handled by the inner-window-destroyed handlers. We clean up just to be on the safe side.
   */
  onUnload(win) {
    let loadDataForWindow = gFaviconLoadDataMap.get(win);
    if (loadDataForWindow) {
      for (let loadData of loadDataForWindow) {
        this._cancelRequest(loadData, "the chrome window went away");
      }
    }
    gFaviconLoadDataMap.delete(win);
  },

  /**
   * Remove a particular favicon load's loading data from our map tracking
   * load data per chrome window.
   *
   * @param win
   *        the chrome window in which we should look for this load
   * @param filterData ({innerWindowID, uri, callback})
   *        the data we should use to find this particular load to remove.
   *
   * @return the loadData object we removed, or null if we didn't find any.
   */
  _removeLoadDataFromWindowMap(win, { innerWindowID, uri, callback }) {
    let loadDataForWindow = gFaviconLoadDataMap.get(win);
    if (loadDataForWindow) {
      let itemIndex = loadDataForWindow.findIndex(loadData => {
        return (
          loadData.innerWindowID == innerWindowID &&
          loadData.uri.equals(uri) &&
          loadData.callback.request == callback.request
        );
      });
      if (itemIndex != -1) {
        let loadData = loadDataForWindow[itemIndex];
        loadDataForWindow.splice(itemIndex, 1);
        return loadData;
      }
    }
    return null;
  },

  /**
   * Create a function to use as a nsIFaviconDataCallback, so we can remove cancelling
   * information when the request succeeds. Note that right now there are some edge-cases,
   * such as about: URIs with chrome:// favicons where the success callback is not invoked.
   * This is OK: we will 'cancel' the request after the timeout (or when the window goes
   * away) but that will be a no-op in such cases.
   */
  _makeCompletionCallback(win, id) {
    return {
      onComplete(uri) {
        let loadData = InternalFaviconLoader._removeLoadDataFromWindowMap(win, {
          uri,
          innerWindowID: id,
          callback: this,
        });
        if (loadData) {
          clearTimeout(loadData.timerID);
        }
        delete this.request;
      },
    };
  },

  ensureInitialized() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    Services.obs.addObserver(windowGlobal => {
      this.removeRequestsForInner(windowGlobal.innerWindowId);
    }, "window-global-destroyed");
  },

  loadFavicon(browser, principal, pageURI, uri, expiration, iconURI) {
    this.ensureInitialized();
    let { ownerGlobal: win, innerWindowID } = browser;
    if (!gFaviconLoadDataMap.has(win)) {
      gFaviconLoadDataMap.set(win, []);
      let unloadHandler = event => {
        let doc = event.target;
        let eventWin = doc.defaultView;
        if (eventWin == win) {
          win.removeEventListener("unload", unloadHandler);
          this.onUnload(win);
        }
      };
      win.addEventListener("unload", unloadHandler, true);
    }

    // First we do the actual setAndFetch call:
    let loadType = PrivateBrowsingUtils.isWindowPrivate(win)
      ? PlacesUtils.favicons.FAVICON_LOAD_PRIVATE
      : PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE;
    let callback = this._makeCompletionCallback(win, innerWindowID);

    if (iconURI && iconURI.schemeIs("data")) {
      expiration = PlacesUtils.toPRTime(expiration);
      PlacesUtils.favicons.replaceFaviconDataFromDataURL(
        uri,
        iconURI.spec,
        expiration,
        principal
      );
    }

    let request = PlacesUtils.favicons.setAndFetchFaviconForPage(
      pageURI,
      uri,
      false,
      loadType,
      callback,
      principal
    );

    // Now register the result so we can cancel it if/when necessary.
    if (!request) {
      // The favicon service can return with success but no-op (and leave request
      // as null) if the icon is the same as the page (e.g. for images) or if it is
      // the favicon for an error page. In this case, we do not need to do anything else.
      return;
    }
    callback.request = request;
    let loadData = { innerWindowID, uri, callback };
    loadData.timerID = setTimeout(() => {
      this._cancelRequest(loadData, "it timed out");
      this._removeLoadDataFromWindowMap(win, loadData);
    }, FAVICON_REQUEST_TIMEOUT);
    let loadDataForWindow = gFaviconLoadDataMap.get(win);
    loadDataForWindow.push(loadData);
  },
};

var PlacesUIUtils = {
  _bookmarkToolbarTelemetryListening: false,
  LAST_USED_FOLDERS_META_KEY: "bookmarks/lastusedfolders",

  getFormattedString: function PUIU_getFormattedString(key, params) {
    return bundle.formatStringFromName(key, params);
  },

  /**
   * Get a localized plural string for the specified key name and numeric value
   * substituting parameters.
   *
   * @param   aKey
   *          String, key for looking up the localized string in the bundle
   * @param   aNumber
   *          Number based on which the final localized form is looked up
   * @param   aParams
   *          Array whose items will substitute #1, #2,... #n parameters
   *          in the string.
   *
   * @see https://developer.mozilla.org/en/Localization_and_Plurals
   * @return The localized plural string.
   */
  getPluralString: function PUIU_getPluralString(aKey, aNumber, aParams) {
    let str = PluralForm.get(aNumber, bundle.GetStringFromName(aKey));

    // Replace #1 with aParams[0], #2 with aParams[1], and so on.
    return str.replace(/\#(\d+)/g, function(matchedId, matchedNumber) {
      let param = aParams[parseInt(matchedNumber, 10) - 1];
      return param !== undefined ? param : matchedId;
    });
  },

  getString: function PUIU_getString(key) {
    return bundle.GetStringFromName(key);
  },

  /**
   * Shows the bookmark dialog corresponding to the specified info.
   *
   * @param {object} aInfo
   *        Describes the item to be edited/added in the dialog.
   *        See documentation at the top of bookmarkProperties.js
   * @param {DOMWindow} [aParentWindow]
   *        Owner window for the new dialog.
   *
   * @see documentation at the top of bookmarkProperties.js
   * @return The guid of the item that was created or edited, undefined otherwise.
   */
  async showBookmarkDialog(aInfo, aParentWindow = null) {
    // Preserve size attributes differently based on the fact the dialog has
    // a folder picker or not, since it needs more horizontal space than the
    // other controls.
    let hasFolderPicker =
      !("hiddenRows" in aInfo) || !aInfo.hiddenRows.includes("folderPicker");
    // Use a different chrome url to persist different sizes.
    let dialogURL = hasFolderPicker
      ? "chrome://browser/content/places/bookmarkProperties2.xhtml"
      : "chrome://browser/content/places/bookmarkProperties.xhtml";

    let features = "centerscreen,chrome,modal,resizable=yes";

    let topUndoEntry;
    let batchBlockingDeferred;

    // Set the transaction manager into batching mode.
    topUndoEntry = PlacesTransactions.topUndoEntry;
    batchBlockingDeferred = PromiseUtils.defer();
    PlacesTransactions.batch(async () => {
      await batchBlockingDeferred.promise;
    });

    if (!aParentWindow) {
      aParentWindow = Services.wm.getMostRecentWindow(null);
    }

    if (
      Services.prefs.getBoolPref("browser.proton.modals.enabled", false) &&
      aParentWindow.gDialogBox
    ) {
      await aParentWindow.gDialogBox.open(dialogURL, aInfo);
    } else {
      aParentWindow.openDialog(dialogURL, "", features, aInfo);
    }

    let bookmarkGuid =
      ("bookmarkGuid" in aInfo && aInfo.bookmarkGuid) || undefined;

    batchBlockingDeferred.resolve();

    if (!bookmarkGuid && topUndoEntry != PlacesTransactions.topUndoEntry) {
      PlacesTransactions.undo().catch(Cu.reportError);
    }

    return bookmarkGuid;
  },

  /**
   * Bookmarks one or more pages. If there is more than one, this will create
   * the bookmarks in a new folder.
   *
   * @param {array.<nsIURI>} URIList
   *   The list of URIs to bookmark.
   * @param {array.<string>} [hiddenRows]
   *   An array of rows to be hidden.
   * @param {DOMWindow} [window]
   *   The window to use as the parent to display the bookmark dialog.
   */
  async showBookmarkPagesDialog(URIList, hiddenRows = [], win = null) {
    if (!URIList.length) {
      return;
    }

    const bookmarkDialogInfo = { action: "add", hiddenRows };
    if (URIList.length > 1) {
      bookmarkDialogInfo.type = "folder";
      bookmarkDialogInfo.URIList = URIList;
    } else {
      bookmarkDialogInfo.type = "bookmark";
      bookmarkDialogInfo.title = URIList[0].title;
      bookmarkDialogInfo.uri = URIList[0].uri;
    }

    await PlacesUIUtils.showBookmarkDialog(bookmarkDialogInfo, win);
  },

  /**
   * set and fetch a favicon. Can only be used from the parent process.
   * @param browser    {Browser}   The XUL browser element for which we're fetching a favicon.
   * @param principal  {Principal} The loading principal to use for the fetch.
   * @pram pageURI     {URI}       The page URI associated to this favicon load.
   * @param uri        {URI}       The URI to fetch.
   * @param expiration {Number}    An optional expiration time.
   * @param iconURI    {URI}       An optional data: URI holding the icon's data.
   */
  loadFavicon(
    browser,
    principal,
    pageURI,
    uri,
    expiration = 0,
    iconURI = null
  ) {
    if (gInContentProcess) {
      throw new Error("Can't track loads from within the child process!");
    }
    InternalFaviconLoader.loadFavicon(
      browser,
      principal,
      pageURI,
      uri,
      expiration,
      iconURI
    );
  },

  /**
   * Returns the closet ancestor places view for the given DOM node
   * @param aNode
   *        a DOM node
   * @return the closet ancestor places view if exists, null otherwsie.
   */
  getViewForNode: function PUIU_getViewForNode(aNode) {
    let node = aNode;

    if (Cu.isDeadWrapper(node)) {
      return null;
    }

    if (node.localName == "panelview" && node._placesView) {
      return node._placesView;
    }

    // The view for a <menu> of which its associated menupopup is a places
    // view, is the menupopup.
    if (
      node.localName == "menu" &&
      !node._placesNode &&
      node.menupopup._placesView
    ) {
      return node.menupopup._placesView;
    }

    while (Element.isInstance(node)) {
      if (node._placesView) {
        return node._placesView;
      }
      if (
        node.localName == "tree" &&
        node.getAttribute("is") == "places-tree"
      ) {
        return node;
      }

      node = node.parentNode;
    }

    return null;
  },

  /**
   * Returns the active PlacesController for a given command.
   *
   * @param win The window containing the affected view
   * @param command The command
   * @return a PlacesController
   */
  getControllerForCommand(win, command) {
    // A context menu may be built for non-focusable views.  Thus, we first try
    // to look for a view associated with document.popupNode
    let popupNode;
    try {
      popupNode = win.document.popupNode;
    } catch (e) {
      // The document went away (bug 797307).
      return null;
    }
    if (popupNode) {
      let isManaged = !!popupNode.closest("#managed-bookmarks");
      if (isManaged) {
        return this.managedBookmarksController;
      }
      let view = this.getViewForNode(popupNode);
      if (view && view._contextMenuShown) {
        return view.controllers.getControllerForCommand(command);
      }
    }

    // When we're not building a context menu, only focusable views
    // are possible.  Thus, we can safely use the command dispatcher.
    let controller = win.top.document.commandDispatcher.getControllerForCommand(
      command
    );
    return controller || null;
  },

  /**
   * Update all the Places commands for the given window.
   *
   * @param win The window to update.
   */
  updateCommands(win) {
    // Get the controller for one of the places commands.
    let controller = this.getControllerForCommand(win, "placesCmd_open");
    for (let command of [
      "placesCmd_open",
      "placesCmd_open:window",
      "placesCmd_open:privatewindow",
      "placesCmd_open:tab",
      "placesCmd_new:folder",
      "placesCmd_new:bookmark",
      "placesCmd_new:separator",
      "placesCmd_show:info",
      "placesCmd_reload",
      "placesCmd_sortBy:name",
      "placesCmd_cut",
      "placesCmd_copy",
      "placesCmd_paste",
      "placesCmd_delete",
    ]) {
      win.goSetCommandEnabled(
        command,
        controller && controller.isCommandEnabled(command)
      );
    }
  },

  /**
   * Executes the given command on the currently active controller.
   *
   * @param win The window containing the affected view
   * @param command The command to execute
   */
  doCommand(win, command) {
    let controller = this.getControllerForCommand(win, command);
    if (controller && controller.isCommandEnabled(command)) {
      controller.doCommand(command);
    }
  },

  /**
   * By calling this before visiting an URL, the visit will be associated to a
   * TRANSITION_TYPED transition (if there is no a referrer).
   * This is used when visiting pages from the history menu, history sidebar,
   * url bar, url autocomplete results, and history searches from the places
   * organizer.  If this is not called visits will be marked as
   * TRANSITION_LINK.
   */
  markPageAsTyped: function PUIU_markPageAsTyped(aURL) {
    PlacesUtils.history.markPageAsTyped(
      Services.uriFixup.getFixupURIInfo(aURL).preferredURI
    );
  },

  /**
   * By calling this before visiting an URL, the visit will be associated to a
   * TRANSITION_BOOKMARK transition.
   * This is used when visiting pages from the bookmarks menu,
   * personal toolbar, and bookmarks from within the places organizer.
   * If this is not called visits will be marked as TRANSITION_LINK.
   */
  markPageAsFollowedBookmark: function PUIU_markPageAsFollowedBookmark(aURL) {
    PlacesUtils.history.markPageAsFollowedBookmark(
      Services.uriFixup.getFixupURIInfo(aURL).preferredURI
    );
  },

  /**
   * By calling this before visiting an URL, any visit in frames will be
   * associated to a TRANSITION_FRAMED_LINK transition.
   * This is actually used to distinguish user-initiated visits in frames
   * so automatic visits can be correctly ignored.
   */
  markPageAsFollowedLink: function PUIU_markPageAsFollowedLink(aURL) {
    PlacesUtils.history.markPageAsFollowedLink(
      Services.uriFixup.getFixupURIInfo(aURL).preferredURI
    );
  },

  /**
   * Sets the character-set for a page. The character set will not be saved
   * if the window is determined to be a private browsing window.
   *
   * @param {string|URL|nsIURI} url The URL of the page to set the charset on.
   * @param {String} charset character-set value.
   * @param {window} window The window that the charset is being set from.
   * @return {Promise}
   */
  async setCharsetForPage(url, charset, window) {
    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }

    // UTF-8 is the default. If we are passed the value then set it to null,
    // to ensure any charset is removed from the database.
    if (charset.toLowerCase() == "utf-8") {
      charset = null;
    }

    await PlacesUtils.history.update({
      url,
      annotations: new Map([[PlacesUtils.CHARSET_ANNO, charset]]),
    });
  },

  /**
   * Allows opening of javascript/data URI only if the given node is
   * bookmarked (see bug 224521).
   * @param aURINode
   *        a URI node
   * @param aWindow
   *        a window on which a potential error alert is shown on.
   * @return true if it's safe to open the node in the browser, false otherwise.
   *
   */
  checkURLSecurity: function PUIU_checkURLSecurity(aURINode, aWindow) {
    if (PlacesUtils.nodeIsBookmark(aURINode)) {
      return true;
    }

    var uri = Services.io.newURI(aURINode.uri);
    if (uri.schemeIs("javascript") || uri.schemeIs("data")) {
      const BRANDING_BUNDLE_URI = "chrome://branding/locale/brand.properties";
      var brandShortName = Services.strings
        .createBundle(BRANDING_BUNDLE_URI)
        .GetStringFromName("brandShortName");

      var errorStr = this.getString("load-js-data-url-error");
      Services.prompt.alert(aWindow, brandShortName, errorStr);
      return false;
    }
    return true;
  },

  /**
   * Check whether or not the given node represents a removable entry (either in
   * history or in bookmarks).
   *
   * @param aNode
   *        a node, except the root node of a query.
   * @return true if the aNode represents a removable entry, false otherwise.
   */
  canUserRemove(aNode) {
    let parentNode = aNode.parent;
    if (!parentNode) {
      // canUserRemove doesn't accept root nodes.
      return false;
    }

    // Is it a query pointing to one of the special root folders?
    if (PlacesUtils.nodeIsQuery(parentNode)) {
      if (PlacesUtils.nodeIsFolder(aNode)) {
        let guid = PlacesUtils.getConcreteItemGuid(aNode);
        // If the parent folder is not a folder, it must be a query, and so this node
        // cannot be removed.
        if (PlacesUtils.isRootItem(guid)) {
          return false;
        }
      } else if (PlacesUtils.isVirtualLeftPaneItem(aNode.bookmarkGuid)) {
        // If the item is a left-pane top-level item, it can't be removed.
        return false;
      }
    }

    // If it's not a bookmark, or it's child of a query, we can remove it.
    if (aNode.itemId == -1 || PlacesUtils.nodeIsQuery(parentNode)) {
      return true;
    }

    // Otherwise it has to be a child of an editable folder.
    return !this.isFolderReadOnly(parentNode);
  },

  /**
   * DO NOT USE THIS API IN ADDONS. IT IS VERY LIKELY TO CHANGE WHEN THE SWITCH
   * TO GUIDS IS COMPLETE (BUG 1071511).
   *
   * Check whether or not the given Places node points to a folder which
   * should not be modified by the user (i.e. its children should be unremovable
   * and unmovable, new children should be disallowed, etc).
   * These semantics are not inherited, meaning that read-only folder may
   * contain editable items (for instance, the places root is read-only, but all
   * of its direct children aren't).
   *
   * You should only pass folder nodes.
   *
   * @param placesNode
   *        any folder result node.
   * @throws if placesNode is not a folder result node or views is invalid.
   * @return true if placesNode is a read-only folder, false otherwise.
   */
  isFolderReadOnly(placesNode) {
    if (
      typeof placesNode != "object" ||
      !PlacesUtils.nodeIsFolder(placesNode)
    ) {
      throw new Error("invalid value for placesNode");
    }

    return (
      PlacesUtils.getConcreteItemId(placesNode) == PlacesUtils.placesRootId
    );
  },

  /** aItemsToOpen needs to be an array of objects of the form:
   * {uri: string, isBookmark: boolean}
   */
  openTabset(aItemsToOpen, aEvent, aWindow) {
    if (!aItemsToOpen.length) {
      return;
    }

    let browserWindow = getBrowserWindow(aWindow);
    var urls = [];
    let skipMarking =
      browserWindow && PrivateBrowsingUtils.isWindowPrivate(browserWindow);
    for (let item of aItemsToOpen) {
      urls.push(item.uri);
      if (skipMarking) {
        continue;
      }

      if (item.isBookmark) {
        this.markPageAsFollowedBookmark(item.uri);
      } else {
        this.markPageAsTyped(item.uri);
      }
    }

    // whereToOpenLink doesn't return "window" when there's no browser window
    // open (Bug 630255).
    var where = browserWindow
      ? browserWindow.whereToOpenLink(aEvent, false, true)
      : "window";
    if (where == "window") {
      // There is no browser window open, thus open a new one.
      let args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
      let stringsToLoad = Cc["@mozilla.org/array;1"].createInstance(
        Ci.nsIMutableArray
      );
      urls.forEach(url =>
        stringsToLoad.appendElement(PlacesUtils.toISupportsString(url))
      );
      args.appendElement(stringsToLoad);

      browserWindow = Services.ww.openWindow(
        aWindow,
        AppConstants.BROWSER_CHROME_URL,
        null,
        "chrome,dialog=no,all",
        args
      );
      return;
    }

    var loadInBackground = where == "tabshifted";
    // For consistency, we want all the bookmarks to open in new tabs, instead
    // of having one of them replace the currently focused tab.  Hence we call
    // loadTabs with aReplace set to false.
    browserWindow.gBrowser.loadTabs(urls, {
      inBackground: loadInBackground,
      replace: false,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  },

  /**
   * Loads a selected node's or nodes' URLs in tabs,
   * warning the user when lots of URLs are being opened
   *
   * @param {object|array} nodeOrNodes
   *          Contains the node or nodes that we're opening in tabs
   * @param {event} event
   *          The DOM mouse/key event with modifier keys set that track the
   *          user's preferred destination window or tab.
   * @param {object} view
   *          The current view that contains the node or nodes selected for
   *          opening
   */
  openMultipleLinksInTabs(nodeOrNodes, event, view) {
    let window = view.ownerWindow;
    let urlsToOpen = [];

    if (PlacesUtils.nodeIsContainer(nodeOrNodes)) {
      urlsToOpen = PlacesUtils.getURLsForContainerNode(nodeOrNodes);
    } else {
      for (var i = 0; i < nodeOrNodes.length; i++) {
        // Skip over separators and folders.
        if (PlacesUtils.nodeIsURI(nodeOrNodes[i])) {
          urlsToOpen.push({
            uri: nodeOrNodes[i].uri,
            isBookmark: PlacesUtils.nodeIsBookmark(nodeOrNodes[i]),
          });
        }
      }
    }
    if (OpenInTabsUtils.confirmOpenInTabs(urlsToOpen.length, window)) {
      this.openTabset(urlsToOpen, event, window);
    }
  },

  /**
   * Loads the node's URL in the appropriate tab or window given the
   * user's preference specified by modifier keys tracked by a
   * DOM mouse/key event.
   * @param   aNode
   *          An uri result node.
   * @param   aEvent
   *          The DOM mouse/key event with modifier keys set that track the
   *          user's preferred destination window or tab.
   */
  openNodeWithEvent: function PUIU_openNodeWithEvent(aNode, aEvent) {
    let window = aEvent.target.ownerGlobal;

    let browserWindow = getBrowserWindow(window);

    let where = window.whereToOpenLink(aEvent, false, true);
    if (this.loadBookmarksInTabs && PlacesUtils.nodeIsBookmark(aNode)) {
      if (where == "current" && !aNode.uri.startsWith("javascript:")) {
        where = "tab";
      }
      if (where == "tab" && browserWindow.gBrowser.selectedTab.isEmpty) {
        where = "current";
      }
    }

    this._openNodeIn(aNode, where, window);
  },

  /**
   * Loads the node's URL in the appropriate tab or window.
   * see also openUILinkIn
   */
  openNodeIn: function PUIU_openNodeIn(aNode, aWhere, aView, aPrivate) {
    let window = aView.ownerWindow;
    this._openNodeIn(aNode, aWhere, window, aPrivate);
  },

  _openNodeIn: function PUIU__openNodeIn(
    aNode,
    aWhere,
    aWindow,
    aPrivate = false
  ) {
    if (
      aNode &&
      PlacesUtils.nodeIsURI(aNode) &&
      this.checkURLSecurity(aNode, aWindow)
    ) {
      let isBookmark = PlacesUtils.nodeIsBookmark(aNode);

      if (!PrivateBrowsingUtils.isWindowPrivate(aWindow)) {
        if (isBookmark) {
          this.markPageAsFollowedBookmark(aNode.uri);
        } else {
          this.markPageAsTyped(aNode.uri);
        }
      }

      const isJavaScriptURL = aNode.uri.startsWith("javascript:");
      aWindow.openTrustedLinkIn(aNode.uri, aWhere, {
        allowPopups: isJavaScriptURL,
        inBackground: this.loadBookmarksInBackground,
        allowInheritPrincipal: isJavaScriptURL,
        private: aPrivate,
      });
    }
  },

  /**
   * Helper for guessing scheme from an url string.
   * Used to avoid nsIURI overhead in frequently called UI functions.
   *
   * @param {string} href The url to guess the scheme from.
   * @return guessed scheme for this url string.
   * @note this is not supposed be perfect, so use it only for UI purposes.
   */
  guessUrlSchemeForUI(href) {
    return href.substr(0, href.indexOf(":"));
  },

  getBestTitle: function PUIU_getBestTitle(aNode, aDoNotCutTitle) {
    var title;
    if (!aNode.title && PlacesUtils.nodeIsURI(aNode)) {
      // if node title is empty, try to set the label using host and filename
      // Services.io.newURI will throw if aNode.uri is not a valid URI
      try {
        var uri = Services.io.newURI(aNode.uri);
        var host = uri.host;
        var fileName = uri.QueryInterface(Ci.nsIURL).fileName;
        // if fileName is empty, use path to distinguish labels
        if (aDoNotCutTitle) {
          title = host + uri.pathQueryRef;
        } else {
          title =
            host +
            (fileName
              ? (host ? "/" + this.ellipsis + "/" : "") + fileName
              : uri.pathQueryRef);
        }
      } catch (e) {
        // Use (no title) for non-standard URIs (data:, javascript:, ...)
        title = "";
      }
    } else {
      title = aNode.title;
    }

    return title || this.getString("noTitle");
  },

  shouldShowTabsFromOtherComputersMenuitem() {
    let weaveOK =
      Weave.Status.checkSetup() != Weave.CLIENT_NOT_CONFIGURED &&
      Weave.Svc.Prefs.get("firstSync", "") != "notReady";
    return weaveOK;
  },

  /**
   * WARNING TO ADDON AUTHORS: DO NOT USE THIS METHOD. IT'S LIKELY TO BE REMOVED IN A
   * FUTURE RELEASE.
   *
   * Checks if a place: href represents a folder shortcut.
   *
   * @param queryString
   *        the query string to check (a place: href)
   * @return whether or not queryString represents a folder shortcut.
   * @throws if queryString is malformed.
   */
  isFolderShortcutQueryString(queryString) {
    // Based on GetSimpleBookmarksQueryFolder in nsNavHistory.cpp.

    let query = {},
      options = {};
    PlacesUtils.history.queryStringToQuery(queryString, query, options);
    query = query.value;
    options = options.value;
    return (
      query.folderCount == 1 &&
      !query.hasBeginTime &&
      !query.hasEndTime &&
      !query.hasDomain &&
      !query.hasURI &&
      !query.hasSearchTerms &&
      !query.tags.length == 0 &&
      options.maxResults == 0
    );
  },

  /**
   * Helpers for consumers of editBookmarkOverlay which don't have a node as their input.
   *
   * Given a bookmark object for either a url bookmark or a folder, returned by
   * Bookmarks.fetch (see Bookmark.jsm), this creates a node-like object suitable for
   * initialising the edit overlay with it.
   *
   * @param aFetchInfo
   *        a bookmark object returned by Bookmarks.fetch.
   * @return a node-like object suitable for initialising editBookmarkOverlay.
   * @throws if aFetchInfo is representing a separator.
   */
  async promiseNodeLikeFromFetchInfo(aFetchInfo) {
    if (aFetchInfo.itemType == PlacesUtils.bookmarks.TYPE_SEPARATOR) {
      throw new Error("promiseNodeLike doesn't support separators");
    }

    let parent = {
      itemId: await PlacesUtils.promiseItemId(aFetchInfo.parentGuid),
      bookmarkGuid: aFetchInfo.parentGuid,
      type: Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER,
    };

    return Object.freeze({
      itemId: await PlacesUtils.promiseItemId(aFetchInfo.guid),
      bookmarkGuid: aFetchInfo.guid,
      title: aFetchInfo.title,
      uri: aFetchInfo.url !== undefined ? aFetchInfo.url.href : "",

      get type() {
        if (aFetchInfo.itemType == PlacesUtils.bookmarks.TYPE_FOLDER) {
          return Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER;
        }

        if (!this.uri.length) {
          throw new Error("Unexpected item type");
        }

        if (/^place:/.test(this.uri)) {
          if (this.isFolderShortcutQueryString(this.uri)) {
            return Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT;
          }

          return Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY;
        }

        return Ci.nsINavHistoryResultNode.RESULT_TYPE_URI;
      },

      get parent() {
        return parent;
      },
    });
  },

  /**
   * This function wraps potentially large places transaction operations
   * with batch notifications to the result node, hence switching the views
   * to batch mode.
   *
   * @param {nsINavHistoryResult} resultNode The result node to turn on batching.
   * @note If resultNode is not supplied, the function will pass-through to
   *       functionToWrap.
   * @param {Integer} itemsBeingChanged The count of items being changed. If the
   *                                    count is lower than a threshold, then
   *                                    batching won't be set.
   * @param {Function} functionToWrap The function to
   */
  async batchUpdatesForNode(resultNode, itemsBeingChanged, functionToWrap) {
    if (!resultNode) {
      await functionToWrap();
      return;
    }

    if (itemsBeingChanged > ITEM_CHANGED_BATCH_NOTIFICATION_THRESHOLD) {
      resultNode.onBeginUpdateBatch();
    }

    try {
      await functionToWrap();
    } finally {
      if (itemsBeingChanged > ITEM_CHANGED_BATCH_NOTIFICATION_THRESHOLD) {
        resultNode.onEndUpdateBatch();
      }
    }
  },

  /**
   * Processes a set of transfer items that have been dropped or pasted.
   * Batching will be applied where necessary.
   *
   * @param {Array} items A list of unwrapped nodes to process.
   * @param {Object} insertionPoint The requested point for insertion.
   * @param {Boolean} doCopy Set to true to copy the items, false will move them
   *                         if possible.
   * @paramt {Object} view The view that should be used for batching.
   * @return {Array} Returns an empty array when the insertion point is a tag, else
   *                 returns an array of copied or moved guids.
   */
  async handleTransferItems(items, insertionPoint, doCopy, view) {
    let transactions;
    let itemsCount;
    if (insertionPoint.isTag) {
      let urls = items.filter(item => "uri" in item).map(item => item.uri);
      itemsCount = urls.length;
      transactions = [
        PlacesTransactions.Tag({ urls, tag: insertionPoint.tagName }),
      ];
    } else {
      let insertionIndex = await insertionPoint.getIndex();
      itemsCount = items.length;
      transactions = getTransactionsForTransferItems(
        items,
        insertionIndex,
        insertionPoint.guid,
        !doCopy
      );
    }

    // Check if we actually have something to add, if we don't it probably wasn't
    // valid, or it was moving to the same location, so just ignore it.
    if (!transactions.length) {
      return [];
    }

    let guidsToSelect = [];
    let resultForBatching = getResultForBatching(view);

    // If we're inserting into a tag, we don't get the guid, so we'll just
    // pass the transactions direct to the batch function.
    let batchingItem = transactions;
    if (!insertionPoint.isTag) {
      // If we're not a tag, then we need to get the ids of the items to select.
      batchingItem = async () => {
        for (let transaction of transactions) {
          let result = await transaction.transact();
          guidsToSelect = guidsToSelect.concat(result);
        }
      };
    }

    await this.batchUpdatesForNode(resultForBatching, itemsCount, async () => {
      await PlacesTransactions.batch(batchingItem);
    });

    return guidsToSelect;
  },

  onSidebarTreeClick(event) {
    // right-clicks are not handled here
    if (event.button == 2) {
      return;
    }

    let tree = event.target.parentNode;
    let cell = tree.getCellAt(event.clientX, event.clientY);
    if (cell.row == -1 || cell.childElt == "twisty") {
      return;
    }

    // getCoordsForCellItem returns the x coordinate in logical coordinates
    // (i.e., starting from the left and right sides in LTR and RTL modes,
    // respectively.)  Therefore, we make sure to exclude the blank area
    // before the tree item icon (that is, to the left or right of it in
    // LTR and RTL modes, respectively) from the click target area.
    let win = tree.ownerGlobal;
    let rect = tree.getCoordsForCellItem(cell.row, cell.col, "image");
    let isRTL = win.getComputedStyle(tree).direction == "rtl";
    let mouseInGutter = isRTL ? event.clientX > rect.x : event.clientX < rect.x;

    let metaKey =
      AppConstants.platform === "macosx" ? event.metaKey : event.ctrlKey;
    let modifKey = metaKey || event.shiftKey;
    let isContainer = tree.view.isContainer(cell.row);
    let openInTabs =
      isContainer &&
      (event.button == 1 || (event.button == 0 && modifKey)) &&
      PlacesUtils.hasChildURIs(tree.view.nodeForTreeIndex(cell.row));

    if (event.button == 0 && isContainer && !openInTabs) {
      tree.view.toggleOpenState(cell.row);
    } else if (
      !mouseInGutter &&
      openInTabs &&
      event.originalTarget.localName == "treechildren"
    ) {
      tree.view.selection.select(cell.row);
      this.openMultipleLinksInTabs(tree.selectedNode, event, tree);
    } else if (
      !mouseInGutter &&
      !isContainer &&
      event.originalTarget.localName == "treechildren"
    ) {
      // Clear all other selection since we're loading a link now. We must
      // do this *before* attempting to load the link since openURL uses
      // selection as an indication of which link to load.
      tree.view.selection.select(cell.row);
      this.openNodeWithEvent(tree.selectedNode, event);
    }
  },

  onSidebarTreeKeyPress(event) {
    let node = event.target.selectedNode;
    if (node) {
      if (event.keyCode == event.DOM_VK_RETURN) {
        this.openNodeWithEvent(node, event);
      }
    }
  },

  /**
   * The following function displays the URL of a node that is being
   * hovered over.
   */
  onSidebarTreeMouseMove(event) {
    let treechildren = event.target;
    if (treechildren.localName != "treechildren") {
      return;
    }

    let tree = treechildren.parentNode;
    let cell = tree.getCellAt(event.clientX, event.clientY);

    // cell.row is -1 when the mouse is hovering an empty area within the tree.
    // To avoid showing a URL from a previously hovered node for a currently
    // hovered non-url node, we must clear the moused-over URL in these cases.
    if (cell.row != -1) {
      let node = tree.view.nodeForTreeIndex(cell.row);
      if (PlacesUtils.nodeIsURI(node)) {
        this.setMouseoverURL(node.uri, tree.ownerGlobal);
        return;
      }
    }
    this.setMouseoverURL("", tree.ownerGlobal);
  },

  setMouseoverURL(url, win) {
    // When the browser window is closed with an open sidebar, the sidebar
    // unload event happens after the browser's one.  In this case
    // top.XULBrowserWindow has been nullified already.
    if (win.top.XULBrowserWindow) {
      win.top.XULBrowserWindow.setOverLink(url);
    }
  },

  ensureBookmarkToolbarTelemetryListening() {
    if (this._bookmarkToolbarTelemetryListening) {
      return;
    }

    // This listener is for counting new bookmarks
    let placesUtilsObserversListener = events => {
      for (let event of events) {
        if (
          event.type == "bookmark-added" &&
          event.parentGuid == PlacesUtils.bookmarks.toolbarGuid
        ) {
          Services.telemetry.scalarAdd(
            "browser.engagement.bookmarks_toolbar_bookmark_added",
            1
          );
        }
      }
    };

    // This listener is for tracking bookmark moves
    let placesUtilsBookmarksObserver = {
      onItemChanged() {},
      onItemMoved(
        aItemId,
        aProperty,
        aIsAnnotationProperty,
        aNewValue,
        aLastModified,
        aItemType,
        aGuid,
        oldParentGuid,
        newParentGuid
      ) {
        let hasMovedToToolbar =
          newParentGuid == PlacesUtils.bookmarks.toolbarGuid &&
          oldParentGuid != PlacesUtils.bookmarks.toolbarGuid;
        if (hasMovedToToolbar) {
          Services.telemetry.scalarAdd(
            "browser.engagement.bookmarks_toolbar_bookmark_added",
            1
          );
        }
      },
    };

    this._bookmarkToolbarTelemetryListening = true;
    PlacesUtils.observers.addListener(
      ["bookmark-added"],
      placesUtilsObserversListener
    );
    PlacesUtils.bookmarks.addObserver(placesUtilsBookmarksObserver);
    PlacesUtils.registerShutdownFunction(() => {
      PlacesUtils.observers.removeListener(
        ["bookmark-added"],
        placesUtilsObserversListener
      );
      PlacesUtils.bookmarks.removeObserver(placesUtilsBookmarksObserver);
    });
  },

  /**
   * Uncollapses PersonalToolbar if its collapsed status is not
   * persisted, and user customized it or changed default bookmarks.
   *
   * If the user does not have a persisted value for the toolbar's
   * "collapsed" attribute, try to determine whether it's customized.
   *
   * @param {Boolean} aForceVisible Set to true to ignore if the user had
   * previously collapsed the toolbar manually.
   */
  NUM_TOOLBAR_BOOKMARKS_TO_UNHIDE: 3,
  maybeToggleBookmarkToolbarVisibility(aForceVisible = false) {
    const BROWSER_DOCURL = AppConstants.BROWSER_CHROME_URL;
    let xulStore = Services.xulStore;

    if (
      aForceVisible ||
      !xulStore.hasValue(BROWSER_DOCURL, "PersonalToolbar", "collapsed")
    ) {
      // We consider the toolbar customized if it has more than NUM_TOOLBAR_BOOKMARKS_TO_UNHIDE
      // children, or if it has a persisted currentset value.
      let toolbarIsCustomized = xulStore.hasValue(
        BROWSER_DOCURL,
        "PersonalToolbar",
        "currentset"
      );

      if (
        aForceVisible ||
        toolbarIsCustomized ||
        PlacesUtils.getChildCountForFolder(PlacesUtils.bookmarks.toolbarGuid) >
          this.NUM_TOOLBAR_BOOKMARKS_TO_UNHIDE
      ) {
        Services.obs.notifyObservers(
          null,
          "browser-set-toolbar-visibility",
          JSON.stringify([CustomizableUI.AREA_BOOKMARKS, "true"])
        );
      }
    }
  },

  maybeToggleBookmarkToolbarVisibilityAfterMigration() {
    if (
      Services.prefs.getBoolPref(
        "browser.migrate.showBookmarksToolbarAfterMigration"
      )
    ) {
      this.maybeToggleBookmarkToolbarVisibility(true);
    }
  },

  async managedPlacesContextShowing(event) {
    let menupopup = event.target;
    let document = menupopup.ownerDocument;
    let window = menupopup.ownerGlobal;
    // We need to populate the submenus in order to have information
    // to show the context menu.
    if (
      menupopup.triggerNode.id == "managed-bookmarks" &&
      !menupopup.triggerNode.menupopup.hasAttribute("hasbeenopened")
    ) {
      await window.PlacesToolbarHelper.populateManagedBookmarks(
        menupopup.triggerNode.menupopup
      );
    }
    let linkItems = [
      "placesContext_open:newtab",
      "placesContext_open:newwindow",
      "placesContext_open:newprivatewindow",
      "placesContext_openSeparator",
      "placesContext_copy",
    ];
    Array.from(menupopup.children).forEach(function(child) {
      if (!(child.id in linkItems)) {
        child.hidden = true;
      }
    });
    // Store triggerNode in controller for checking if commands are enabled
    this.managedBookmarksController.triggerNode = menupopup.triggerNode;
    // Container in this context means a folder.
    let isFolder = menupopup.triggerNode.hasAttribute("container");
    let openContainerInTabs_menuitem = document.getElementById(
      "placesContext_openContainer:tabs"
    );
    if (isFolder) {
      // Disable the openContainerInTabs menuitem if there
      // are no children of the menu that have links.
      let menuitems = menupopup.triggerNode.menupopup.children;
      let openContainerInTabs = Array.from(menuitems).some(
        menuitem => menuitem.link
      );
      openContainerInTabs_menuitem.disabled = !openContainerInTabs;
    } else {
      document.getElementById(
        "placesContext_open:newprivatewindow"
      ).hidden = PrivateBrowsingUtils.isWindowPrivate(window);
    }
    openContainerInTabs_menuitem.hidden = !isFolder;
    linkItems.forEach(id => (document.getElementById(id).hidden = isFolder));

    event.target.ownerGlobal.updateCommands("places");
  },

  placesContextShowing(event) {
    let menupopup = event.target;
    if (menupopup.id != "placesContext") {
      // Ignore any popupshowing events from submenus
      return true;
    }

    let isManaged = !!menupopup.triggerNode.closest("#managed-bookmarks");
    if (isManaged) {
      this.managedPlacesContextShowing(event);
      return true;
    }
    let document = menupopup.ownerDocument;
    // TODO: Ideally, we want to simply use menupopup.triggerNode instead of
    // document.popupNode here, but document.popupNode will be refferred while
    // building the context menu, set menupopup.triggerNode to document.popupNode
    // for now. So, if we make not refer to document.popupNode in Places in
    // bug 1706004, need to modify here as well.
    document.popupNode = menupopup.triggerNode;
    menupopup._view = this.getViewForNode(document.popupNode);
    if (!menupopup._view) {
      // This can happen if we try to invoke the context menu on
      // an uninitialized places toolbar. Just bail out:
      event.preventDefault();
      return false;
    }
    if (!this.openInTabClosesMenu) {
      document
        .getElementById("placesContext_open:newtab")
        .setAttribute("closemenu", "single");
    }
    return menupopup._view.buildContextMenu(menupopup);
  },

  placesContextHiding(event) {
    let menupopup = event.target;
    if (menupopup._view) {
      menupopup._view.destroyContextMenu();
    }
  },

  openSelectionInTabs(event) {
    let isManaged = !!event.target.parentNode.triggerNode.closest(
      "#managed-bookmarks"
    );
    let controller;
    if (isManaged) {
      controller = this.managedBookmarksController;
    } else {
      let document = event.target.ownerDocument;
      controller = PlacesUIUtils.getViewForNode(document.popupNode).controller;
    }
    controller.openSelectionInTabs(event);
  },

  managedBookmarksController: {
    triggerNode: null,

    openSelectionInTabs(event) {
      let window = event.target.ownerGlobal;
      let menuitems = event.target.parentNode.triggerNode.menupopup.children;
      let items = [];
      for (let i = 0; i < menuitems.length; i++) {
        if (menuitems[i].link) {
          let item = {};
          item.uri = menuitems[i].link;
          item.isBookmark = true;
          items.push(item);
        }
      }
      PlacesUIUtils.openTabset(items, event, window);
    },

    isCommandEnabled(command) {
      switch (command) {
        case "placesCmd_copy":
        case "placesCmd_open:window":
        case "placesCmd_open:privatewindow":
        case "placesCmd_open:tab": {
          return true;
        }
      }
      return false;
    },

    doCommand(command) {
      let window = this.triggerNode.ownerGlobal;
      switch (command) {
        case "placesCmd_copy":
          // This is a little hacky, but there is a lot of code in Places that handles
          // clipboard stuff, so it's easier to reuse.
          let node = {};
          node.type = 0;
          node.title = this.triggerNode.label;
          node.uri = this.triggerNode.link;

          // Copied from _populateClipboard in controller.js

          // This order is _important_! It controls how this and other applications
          // select data to be inserted based on type.
          let contents = [
            { type: PlacesUtils.TYPE_X_MOZ_URL, entries: [] },
            { type: PlacesUtils.TYPE_HTML, entries: [] },
            { type: PlacesUtils.TYPE_UNICODE, entries: [] },
          ];

          contents.forEach(function(content) {
            content.entries.push(PlacesUtils.wrapNode(node, content.type));
          });

          let xferable = Cc[
            "@mozilla.org/widget/transferable;1"
          ].createInstance(Ci.nsITransferable);
          xferable.init(null);

          function addData(type, data) {
            xferable.addDataFlavor(type);
            xferable.setTransferData(type, PlacesUtils.toISupportsString(data));
          }

          contents.forEach(function(content) {
            addData(content.type, content.entries.join(PlacesUtils.endl));
          });

          Services.clipboard.setData(
            xferable,
            null,
            Ci.nsIClipboard.kGlobalClipboard
          );
          break;
        case "placesCmd_open:privatewindow":
          window.openUILinkIn(this.triggerNode.link, "window", {
            triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
            private: true,
          });
          break;
        case "placesCmd_open:window":
          window.openUILinkIn(this.triggerNode.link, "window", {
            triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
            private: false,
          });
          break;
        case "placesCmd_open:tab": {
          window.openUILinkIn(this.triggerNode.link, "tab", {
            triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
          });
        }
      }
    },
  },

  async maybeAddImportButton() {
    if (!Services.policies.isAllowed("profileImport")) {
      return;
    }
    // Check if the experiment is running. If not, wait for it to run.
    const kPref = "browser.toolbars.bookmarks.2h2020";
    if (!Services.prefs.getBoolPref(kPref, false)) {
      Services.prefs.addObserver(kPref, function obs() {
        Services.prefs.removeObserver(kPref, obs);
        Services.tm.dispatchToMainThread(() =>
          PlacesUIUtils.maybeAddImportButton()
        );
      });
      return;
    }
    let numberOfBookmarks = await PlacesUtils.withConnectionWrapper(
      "PlacesUIUtils: maybeAddImportButton",
      async db => {
        let rows = await db.execute(
          `SELECT COUNT(*) as n FROM moz_bookmarks b
           WHERE b.parent = :parentId`,
          { parentId: PlacesUtils.toolbarFolderId }
        );
        return rows[0].getResultByName("n");
      }
    ).catch(e => {
      // We want to report errors, but we still want to add the button then:
      Cu.reportError(e);
      return 0;
    });

    if (numberOfBookmarks < 3) {
      CustomizableUI.addWidgetToArea(
        "import-button",
        CustomizableUI.AREA_BOOKMARKS,
        0
      );
      Services.prefs.setBoolPref("browser.bookmarks.addedImportButton", true);
      this.removeImportButtonWhenImportSucceeds();
    }
  },

  removeImportButtonWhenImportSucceeds() {
    // If the user (re)moved the button, clear the pref and stop worrying about
    // moving the item.
    let placement = CustomizableUI.getPlacementOfWidget("import-button");
    if (placement?.area != CustomizableUI.AREA_BOOKMARKS) {
      Services.prefs.clearUserPref("browser.bookmarks.addedImportButton");
      return;
    }
    // Otherwise, wait for a successful migration:
    let obs = (subject, topic, data) => {
      if (
        data == Ci.nsIBrowserProfileMigrator.BOOKMARKS &&
        MigrationUtils.getImportedCount("bookmarks") > 0
      ) {
        CustomizableUI.removeWidgetFromArea("import-button");
        Services.prefs.clearUserPref("browser.bookmarks.addedImportButton");
        Services.obs.removeObserver(obs, "Migration:ItemAfterMigrate");
        Services.obs.removeObserver(obs, "Migration:ItemError");
      }
    };
    Services.obs.addObserver(obs, "Migration:ItemAfterMigrate");
    Services.obs.addObserver(obs, "Migration:ItemError");
  },

  get _nonPrefDefaultParentGuid() {
    let { unfiledGuid, toolbarGuid } = PlacesUtils.bookmarks;
    return this._2020h2bookmarks ? toolbarGuid : unfiledGuid;
  },

  get defaultParentGuid() {
    if (!PlacesUIUtils._2020h2bookmarks) {
      return PlacesUtils.bookmarks.unfiledGuid;
    }
    // Defined via a lazy pref getter below, see the comment there about the
    // reason for this (temporary) setup.
    return this._defaultParentGuid;
  },
};

/**
 * Promise used by the toolbar view browser-places to determine whether we
 * can start loading its content (which involves IO, and so is postponed
 * during startup).
 * This promise's resolution value indicates whether toolbar initialization
 * waited on sessionstore-windows-restored and then an idle task, or happened
 * immediately as the window was opened. This is used for telemetry.
 */
PlacesUIUtils.canLoadToolbarContentPromise = new Promise(resolve => {
  PlacesUIUtils.unblockToolbars = () => {
    resolve("waited-for-session-idle");
    // Overwrite the property with the new promise, as the session has
    // now been restored:
    PlacesUIUtils.canLoadToolbarContentPromise = Promise.resolve("immediate");
  };
});

// These are lazy getters to avoid importing PlacesUtils immediately.
XPCOMUtils.defineLazyGetter(PlacesUIUtils, "PLACES_FLAVORS", () => {
  return [
    PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER,
    PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR,
    PlacesUtils.TYPE_X_MOZ_PLACE,
  ];
});
XPCOMUtils.defineLazyGetter(PlacesUIUtils, "URI_FLAVORS", () => {
  return [PlacesUtils.TYPE_X_MOZ_URL, TAB_DROP_TYPE, PlacesUtils.TYPE_UNICODE];
});
XPCOMUtils.defineLazyGetter(PlacesUIUtils, "SUPPORTED_FLAVORS", () => {
  return [...PlacesUIUtils.PLACES_FLAVORS, ...PlacesUIUtils.URI_FLAVORS];
});

XPCOMUtils.defineLazyGetter(PlacesUIUtils, "ellipsis", function() {
  return Services.prefs.getComplexValue(
    "intl.ellipsis",
    Ci.nsIPrefLocalizedString
  ).data;
});

XPCOMUtils.defineLazyPreferenceGetter(
  PlacesUIUtils,
  "loadBookmarksInBackground",
  PREF_LOAD_BOOKMARKS_IN_BACKGROUND,
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  PlacesUIUtils,
  "loadBookmarksInTabs",
  PREF_LOAD_BOOKMARKS_IN_TABS,
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  PlacesUIUtils,
  "openInTabClosesMenu",
  "browser.bookmarks.openInTabClosesMenu",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  PlacesUIUtils,
  "maxRecentFolders",
  "browser.bookmarks.editDialog.maxRecentFolders",
  7
);

XPCOMUtils.defineLazyPreferenceGetter(
  PlacesUIUtils,
  "_2020h2bookmarks",
  "browser.toolbars.bookmarks.2h2020",
  false
);

/**
 * This value should be accessed through the defaultParentGuid getter,
 * which will only access this pref if the browser.toolbars.bookmarks.2h2020
 * pref is true. We can't put that check directly in the pref transformation
 * callback below, because then the resulting value doesn't update if the
 * 2h2020 pref updates, breaking tests and potentially real-world behaviour
 * if the 2h2020 pref is flipped at runtime.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  PlacesUIUtils,
  "_defaultParentGuid",
  "browser.bookmarks.defaultLocation",
  "", // Avoid eagerly loading PlacesUtils.
  null,
  prefValue => {
    if (!prefValue) {
      return PlacesUIUtils._nonPrefDefaultParentGuid;
    }
    if (["toolbar", "menu", "unfiled"].includes(prefValue)) {
      return PlacesUtils.bookmarks[prefValue + "Guid"];
    }
    return PlacesUtils.bookmarks
      .fetch({ guid: prefValue })
      .then(bm => bm.guid)
      .catch(() => PlacesUIUtils._nonPrefDefaultParentGuid);
  }
);

/**
 * Determines if an unwrapped node can be moved.
 *
 * @param unwrappedNode
 *        A node unwrapped by PlacesUtils.unwrapNodes().
 * @return True if the node can be moved, false otherwise.
 */
function canMoveUnwrappedNode(unwrappedNode) {
  if (
    (unwrappedNode.concreteGuid &&
      PlacesUtils.isRootItem(unwrappedNode.concreteGuid)) ||
    (unwrappedNode.guid && PlacesUtils.isRootItem(unwrappedNode.guid))
  ) {
    return false;
  }

  let parentGuid = unwrappedNode.parentGuid;
  if (parentGuid == PlacesUtils.bookmarks.rootGuid) {
    return false;
  }

  return true;
}

/**
 * This gets the most appropriate item for using for batching. In the case of multiple
 * views being related, the method returns the most expensive result to batch.
 * For example, if it detects the left-hand library pane, then it will look for
 * and return the reference to the right-hand pane.
 *
 * @param {Object} viewOrElement The item to check.
 * @return {Object} Will return the best result node to batch, or null
 *                  if one could not be found.
 */
function getResultForBatching(viewOrElement) {
  if (
    viewOrElement &&
    Element.isInstance(viewOrElement) &&
    viewOrElement.id === "placesList"
  ) {
    // Note: fall back to the existing item if we can't find the right-hane pane.
    viewOrElement =
      viewOrElement.ownerDocument.getElementById("placeContent") ||
      viewOrElement;
  }

  if (viewOrElement && viewOrElement.result) {
    return viewOrElement.result;
  }

  return null;
}

/**
 * Processes a set of transfer items and returns transactions to insert or
 * move them.
 *
 * @param {Array} items A list of unwrapped nodes to get transactions for.
 * @param {Integer} insertionIndex The requested index for insertion.
 * @param {String} insertionParentGuid The guid of the parent folder to insert
 *                                     or move the items to.
 * @param {Boolean} doMove Set to true to MOVE the items if possible, false will
 *                         copy them.
 * @return {Array} Returns an array of created PlacesTransactions.
 */
function getTransactionsForTransferItems(
  items,
  insertionIndex,
  insertionParentGuid,
  doMove
) {
  let canMove = true;
  for (let item of items) {
    if (!PlacesUIUtils.SUPPORTED_FLAVORS.includes(item.type)) {
      throw new Error(`Unsupported '${item.type}' data type`);
    }

    // Work out if this is data from the same app session we're running in.
    if (!("instanceId" in item) || item.instanceId != PlacesUtils.instanceId) {
      if (item.type == PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER) {
        throw new Error(
          "Can't copy a container from a legacy-transactions build"
        );
      }
      // Only log if this is one of "our" types as external items, e.g. drag from
      // url bar to toolbar, shouldn't complain.
      if (PlacesUIUtils.PLACES_FLAVORS.includes(item.type)) {
        Cu.reportError(
          "Tried to move an unmovable Places " +
            "node, reverting to a copy operation."
        );
      }

      // We can never move from an external copy.
      canMove = false;
    }

    if (doMove && canMove) {
      canMove = canMoveUnwrappedNode(item);
    }
  }

  if (doMove && !canMove) {
    doMove = false;
  }

  if (doMove) {
    // Move is simple, we pass the transaction a list of GUIDs and where to move
    // them to.
    return [
      PlacesTransactions.Move({
        guids: items.map(item => item.itemGuid),
        newParentGuid: insertionParentGuid,
        newIndex: insertionIndex,
      }),
    ];
  }

  return getTransactionsForCopy(items, insertionIndex, insertionParentGuid);
}

/**
 * Processes a set of transfer items and returns an array of transactions.
 *
 * @param {Array} items A list of unwrapped nodes to get transactions for.
 * @param {Integer} insertionIndex The requested index for insertion.
 * @param {String} insertionParentGuid The guid of the parent folder to insert
 *                                     or move the items to.
 * @return {Array} Returns an array of created PlacesTransactions.
 */
function getTransactionsForCopy(items, insertionIndex, insertionParentGuid) {
  let transactions = [];
  let index = insertionIndex;

  for (let item of items) {
    let transaction;
    let guid = item.itemGuid;

    if (
      PlacesUIUtils.PLACES_FLAVORS.includes(item.type) &&
      // For anything that is comming from within this session, we do a
      // direct copy, otherwise we fallback and form a new item below.
      "instanceId" in item &&
      item.instanceId == PlacesUtils.instanceId &&
      // If the Item doesn't have a guid, this could be a virtual tag query or
      // other item, so fallback to inserting a new bookmark with the URI.
      guid &&
      // For virtual root items, we fallback to creating a new bookmark, as
      // we want a shortcut to be created, not a full tree copy.
      !PlacesUtils.bookmarks.isVirtualRootItem(guid) &&
      !PlacesUtils.isVirtualLeftPaneItem(guid)
    ) {
      transaction = PlacesTransactions.Copy({
        guid,
        newIndex: index,
        newParentGuid: insertionParentGuid,
      });
    } else if (item.type == PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR) {
      transaction = PlacesTransactions.NewSeparator({
        index,
        parentGuid: insertionParentGuid,
      });
    } else {
      let title = item.type != PlacesUtils.TYPE_UNICODE ? item.title : item.uri;
      transaction = PlacesTransactions.NewBookmark({
        index,
        parentGuid: insertionParentGuid,
        title,
        url: item.uri,
      });
    }

    transactions.push(transaction);

    if (index != -1) {
      index++;
    }
  }
  return transactions;
}

function getBrowserWindow(aWindow) {
  // Prefer the caller window if it's a browser window, otherwise use
  // the top browser window.
  return aWindow &&
    aWindow.document.documentElement.getAttribute("windowtype") ==
      "navigator:browser"
    ? aWindow
    : BrowserWindowTracker.getTopWindow();
}

/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["PlacesUIUtils"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["Element"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  OpenInTabsUtils: "resource:///modules/OpenInTabsUtils.jsm",
  PlacesTransactions: "resource://gre/modules/PlacesTransactions.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  Weave: "resource://services-sync/main.js",
});

XPCOMUtils.defineLazyGetter(this, "bundle", function() {
  return Services.strings.createBundle("chrome://browser/locale/places/places.properties");
});

const gInContentProcess = Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;
const FAVICON_REQUEST_TIMEOUT = 60 * 1000;
// Map from windows to arrays of data about pending favicon loads.
let gFaviconLoadDataMap = new Map();

const ITEM_CHANGED_BATCH_NOTIFICATION_THRESHOLD = 10;

// copied from utilityOverlay.js
const TAB_DROP_TYPE = "application/x-moz-tabbrowser-tab";
const PREF_LOAD_BOOKMARKS_IN_BACKGROUND = "browser.tabs.loadBookmarksInBackground";
const PREF_LOAD_BOOKMARKS_IN_TABS = "browser.tabs.loadBookmarksInTabs";

let InternalFaviconLoader = {
  /**
   * This gets called for every inner window that is destroyed.
   * In the parent process, we process the destruction ourselves. In the child process,
   * we notify the parent which will then process it based on that message.
   */
  observe(subject, topic, data) {
    let innerWindowID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    this.removeRequestsForInner(innerWindowID);
  },

  /**
   * Actually cancel the request, and clear the timeout for cancelling it.
   */
  _cancelRequest({uri, innerWindowID, timerID, callback}, reason) {
    // Break cycle
    let request = callback.request;
    delete callback.request;
    // Ensure we don't time out.
    clearTimeout(timerID);
    try {
      request.cancel();
    } catch (ex) {
      Cu.reportError("When cancelling a request for " + uri.spec + " because " + reason + ", it was already canceled!");
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
          this._cancelRequest(loadData, "the inner window was destroyed or a new favicon was loaded for it");
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
  _removeLoadDataFromWindowMap(win, {innerWindowID, uri, callback}) {
    let loadDataForWindow = gFaviconLoadDataMap.get(win);
    if (loadDataForWindow) {
      let itemIndex = loadDataForWindow.findIndex(loadData => {
        return loadData.innerWindowID == innerWindowID &&
               loadData.uri.equals(uri) &&
               loadData.callback.request == callback.request;
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

    Services.obs.addObserver(this, "inner-window-destroyed");
    Services.ppmm.addMessageListener("Toolkit:inner-window-destroyed", msg => {
      this.removeRequestsForInner(msg.data);
    });
  },

  loadFavicon(browser, principal, uri, expiration, iconURI) {
    this.ensureInitialized();
    let win = browser.ownerGlobal;
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

    let {innerWindowID, currentURI} = browser;

    // First we do the actual setAndFetch call:
    let loadType = PrivateBrowsingUtils.isWindowPrivate(win)
      ? PlacesUtils.favicons.FAVICON_LOAD_PRIVATE
      : PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE;
    let callback = this._makeCompletionCallback(win, innerWindowID);

    if (iconURI && iconURI.schemeIs("data")) {
      expiration = PlacesUtils.toPRTime(expiration);
      PlacesUtils.favicons.replaceFaviconDataFromDataURL(uri, iconURI.spec,
                                                         expiration, principal);
    }

    let request = PlacesUtils.favicons.setAndFetchFaviconForPage(currentURI, uri, false,
                                                                 loadType, callback, principal);

    // Now register the result so we can cancel it if/when necessary.
    if (!request) {
      // The favicon service can return with success but no-op (and leave request
      // as null) if the icon is the same as the page (e.g. for images) or if it is
      // the favicon for an error page. In this case, we do not need to do anything else.
      return;
    }
    callback.request = request;
    let loadData = {innerWindowID, uri, callback};
    loadData.timerID = setTimeout(() => {
      this._cancelRequest(loadData, "it timed out");
      this._removeLoadDataFromWindowMap(win, loadData);
    }, FAVICON_REQUEST_TIMEOUT);
    let loadDataForWindow = gFaviconLoadDataMap.get(win);
    loadDataForWindow.push(loadData);
  },
};

var PlacesUIUtils = {
  LAST_USED_FOLDERS_META_KEY: "bookmarks/lastusedfolders",

  /**
   * Makes a URI from a spec, and do fixup
   * @param   aSpec
   *          The string spec of the URI
   * @return A URI object for the spec.
   */
  createFixedURI: function PUIU_createFixedURI(aSpec) {
    return Services.uriFixup.createFixupURI(aSpec, Ci.nsIURIFixup.FIXUP_FLAG_NONE);
  },

  getFormattedString: function PUIU_getFormattedString(key, params) {
    return bundle.formatStringFromName(key, params, params.length);
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
   * @param aInfo
   *        Describes the item to be edited/added in the dialog.
   *        See documentation at the top of bookmarkProperties.js
   * @param aWindow
   *        Owner window for the new dialog.
   *
   * @see documentation at the top of bookmarkProperties.js
   * @return The guid of the item that was created or edited, undefined otherwise.
   */
  showBookmarkDialog(aInfo, aParentWindow) {
    // Preserve size attributes differently based on the fact the dialog has
    // a folder picker or not, since it needs more horizontal space than the
    // other controls.
    let hasFolderPicker = !("hiddenRows" in aInfo) ||
                          !aInfo.hiddenRows.includes("folderPicker");
    // Use a different chrome url to persist different sizes.
    let dialogURL = hasFolderPicker ?
                    "chrome://browser/content/places/bookmarkProperties2.xul" :
                    "chrome://browser/content/places/bookmarkProperties.xul";

    let features = "centerscreen,chrome,modal,resizable=yes";

    let topUndoEntry;
    let batchBlockingDeferred;

    // Set the transaction manager into batching mode.
    topUndoEntry = PlacesTransactions.topUndoEntry;
    batchBlockingDeferred = PromiseUtils.defer();
    PlacesTransactions.batch(async () => {
      await batchBlockingDeferred.promise;
    });

    aParentWindow.openDialog(dialogURL, "", features, aInfo);

    let bookmarkGuid = ("bookmarkGuid" in aInfo && aInfo.bookmarkGuid) || undefined;

    batchBlockingDeferred.resolve();

    if (!bookmarkGuid &&
        topUndoEntry != PlacesTransactions.topUndoEntry) {
      PlacesTransactions.undo().catch(Cu.reportError);
    }

    return bookmarkGuid;
  },

  /**
   * set and fetch a favicon. Can only be used from the parent process.
   * @param browser    {Browser}   The XUL browser element for which we're fetching a favicon.
   * @param principal  {Principal} The loading principal to use for the fetch.
   * @param uri        {URI}       The URI to fetch.
   * @param expiration {Number}    An optional expiration time.
   * @param iconURI    {URI}       An optional data: URI holding the icon's data.
   */
  loadFavicon(browser, principal, uri, expiration = 0, iconURI = null) {
    if (gInContentProcess) {
      throw new Error("Can't track loads from within the child process!");
    }
    InternalFaviconLoader.loadFavicon(browser, principal, uri, expiration, iconURI);
  },

  /**
   * Returns the closet ancestor places view for the given DOM node
   * @param aNode
   *        a DOM node
   * @return the closet ancestor places view if exists, null otherwsie.
   */
  getViewForNode: function PUIU_getViewForNode(aNode) {
    let node = aNode;

    if (node.localName == "panelview" && node._placesView) {
      return node._placesView;
    }

    // The view for a <menu> of which its associated menupopup is a places
    // view, is the menupopup.
    if (node.localName == "menu" && !node._placesNode &&
        node.lastChild._placesView)
      return node.lastChild._placesView;

    while (Element.isInstance(node)) {
      if (node._placesView)
        return node._placesView;
      if (node.localName == "tree" && node.getAttribute("type") == "places")
        return node;

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
      let view = this.getViewForNode(popupNode);
      if (view && view._contextMenuShown)
        return view.controllers.getControllerForCommand(command);
    }

    // When we're not building a context menu, only focusable views
    // are possible.  Thus, we can safely use the command dispatcher.
    let controller = win.top.document.commandDispatcher
                        .getControllerForCommand(command);
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
      win.goSetCommandEnabled(command,
                              controller && controller.isCommandEnabled(command));
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
    if (controller && controller.isCommandEnabled(command))
      controller.doCommand(command);
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
    PlacesUtils.history.markPageAsTyped(this.createFixedURI(aURL));
  },

  /**
   * By calling this before visiting an URL, the visit will be associated to a
   * TRANSITION_BOOKMARK transition.
   * This is used when visiting pages from the bookmarks menu,
   * personal toolbar, and bookmarks from within the places organizer.
   * If this is not called visits will be marked as TRANSITION_LINK.
   */
  markPageAsFollowedBookmark: function PUIU_markPageAsFollowedBookmark(aURL) {
    PlacesUtils.history.markPageAsFollowedBookmark(this.createFixedURI(aURL));
  },

  /**
   * By calling this before visiting an URL, any visit in frames will be
   * associated to a TRANSITION_FRAMED_LINK transition.
   * This is actually used to distinguish user-initiated visits in frames
   * so automatic visits can be correctly ignored.
   */
  markPageAsFollowedLink: function PUIU_markPageAsFollowedLink(aURL) {
    PlacesUtils.history.markPageAsFollowedLink(this.createFixedURI(aURL));
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
    if (PlacesUtils.nodeIsBookmark(aURINode))
      return true;

    var uri = Services.io.newURI(aURINode.uri);
    if (uri.schemeIs("javascript") || uri.schemeIs("data")) {
      const BRANDING_BUNDLE_URI = "chrome://branding/locale/brand.properties";
      var brandShortName = Services.strings.
                           createBundle(BRANDING_BUNDLE_URI).
                           GetStringFromName("brandShortName");

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
   * @param aView
   *        The view originating the request.
   * @return true if the aNode represents a removable entry, false otherwise.
   */
  canUserRemove(aNode, aView) {
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

    // If it's not a bookmark, we can remove it unless it's a child of a
    // livemark.
    if (aNode.itemId == -1) {
      // Rather than executing a db query, checking the existence of the feedURI
      // annotation, detect livemark children by the fact that they are the only
      // direct non-bookmark children of bookmark folders.
      return !PlacesUtils.nodeIsFolder(parentNode);
    }

    // Generally it's always possible to remove children of a query.
    if (PlacesUtils.nodeIsQuery(parentNode))
      return true;

    // Otherwise it has to be a child of an editable folder.
    return !this.isFolderReadOnly(parentNode, aView);
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
   * @param view
   *        The view originating the request.
   * @throws if placesNode is not a folder result node or views is invalid.
   * @note livemark "folders" are considered read-only (but see bug 1072833).
   * @return true if placesNode is a read-only folder, false otherwise.
   */
  isFolderReadOnly(placesNode, view) {
    if (typeof placesNode != "object" || !PlacesUtils.nodeIsFolder(placesNode)) {
      throw new Error("invalid value for placesNode");
    }
    if (!view || typeof view != "object") {
      throw new Error("invalid value for aView");
    }
    let itemId = PlacesUtils.getConcreteItemId(placesNode);
    if (itemId == PlacesUtils.placesRootId ||
        view.controller.hasCachedLivemarkInfo(placesNode))
      return true;

    return false;
  },

  /** aItemsToOpen needs to be an array of objects of the form:
    * {uri: string, isBookmark: boolean}
    */
  _openTabset: function PUIU__openTabset(aItemsToOpen, aEvent, aWindow) {
    if (!aItemsToOpen.length)
      return;

    let browserWindow = getBrowserWindow(aWindow);
    var urls = [];
    let skipMarking = browserWindow && PrivateBrowsingUtils.isWindowPrivate(browserWindow);
    for (let item of aItemsToOpen) {
      urls.push(item.uri);
      if (skipMarking) {
        continue;
      }

      if (item.isBookmark)
        this.markPageAsFollowedBookmark(item.uri);
      else
        this.markPageAsTyped(item.uri);
    }

    // whereToOpenLink doesn't return "window" when there's no browser window
    // open (Bug 630255).
    var where = browserWindow ?
                browserWindow.whereToOpenLink(aEvent, false, true) : "window";
    if (where == "window") {
      // There is no browser window open, thus open a new one.
      var uriList = PlacesUtils.toISupportsString(urls.join("|"));
      var args = Cc["@mozilla.org/array;1"].
                  createInstance(Ci.nsIMutableArray);
      args.appendElement(uriList);
      browserWindow = Services.ww.openWindow(aWindow,
                                             "chrome://browser/content/browser.xul",
                                             null, "chrome,dialog=no,all", args);
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

  openLiveMarkNodesInTabs:
  function PUIU_openLiveMarkNodesInTabs(aNode, aEvent, aView) {
    let window = aView.ownerWindow;

    PlacesUtils.livemarks.getLivemark({id: aNode.itemId})
      .then(aLivemark => {
        let urlsToOpen = [];

        let nodes = aLivemark.getNodesForContainer(aNode);
        for (let node of nodes) {
          urlsToOpen.push({uri: node.uri, isBookmark: false});
        }

        if (OpenInTabsUtils.confirmOpenInTabs(urlsToOpen.length, window)) {
          this._openTabset(urlsToOpen, aEvent, window);
        }
      }, Cu.reportError);
  },

  openContainerNodeInTabs:
  function PUIU_openContainerInTabs(aNode, aEvent, aView) {
    let window = aView.ownerWindow;

    let urlsToOpen = PlacesUtils.getURLsForContainerNode(aNode);
    if (OpenInTabsUtils.confirmOpenInTabs(urlsToOpen.length, window)) {
      this._openTabset(urlsToOpen, aEvent, window);
    }
  },

  openURINodesInTabs: function PUIU_openURINodesInTabs(aNodes, aEvent, aView) {
    let window = aView.ownerWindow;

    let urlsToOpen = [];
    for (var i = 0; i < aNodes.length; i++) {
      // Skip over separators and folders.
      if (PlacesUtils.nodeIsURI(aNodes[i]))
        urlsToOpen.push({uri: aNodes[i].uri, isBookmark: PlacesUtils.nodeIsBookmark(aNodes[i])});
    }
    this._openTabset(urlsToOpen, aEvent, window);
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
  openNodeWithEvent:
  function PUIU_openNodeWithEvent(aNode, aEvent) {
    let window = aEvent.target.ownerGlobal;

    let browserWindow = getBrowserWindow(window);

    let where = window.whereToOpenLink(aEvent, false, true);
    if (this.loadBookmarksInTabs && PlacesUtils.nodeIsBookmark(aNode)) {
      if (where == "current" && !aNode.uri.startsWith("javascript:")) {
        where = "tab";
      }
      if (where == "tab" && browserWindow.isTabEmpty(browserWindow.gBrowser.selectedTab)) {
        where = "current";
      }
    }

    this._openNodeIn(aNode, where, window);
    let view = this.getViewForNode(aEvent.target);
    if (view && view.controller.hasCachedLivemarkInfo(aNode.parent)) {
      Services.telemetry.scalarAdd("browser.feeds.livebookmark_item_opened", 1);
    }
  },

  /**
   * Loads the node's URL in the appropriate tab or window.
   * see also openUILinkIn
   */
  openNodeIn: function PUIU_openNodeIn(aNode, aWhere, aView, aPrivate) {
    let window = aView.ownerWindow;
    this._openNodeIn(aNode, aWhere, window, aPrivate);
  },

  _openNodeIn: function PUIU__openNodeIn(aNode, aWhere, aWindow, aPrivate = false) {
    if (aNode && PlacesUtils.nodeIsURI(aNode) &&
        this.checkURLSecurity(aNode, aWindow)) {
      let isBookmark = PlacesUtils.nodeIsBookmark(aNode);

      if (!PrivateBrowsingUtils.isWindowPrivate(aWindow)) {
        if (isBookmark)
          this.markPageAsFollowedBookmark(aNode.uri);
        else
          this.markPageAsTyped(aNode.uri);
      }

      aWindow.openTrustedLinkIn(aNode.uri, aWhere, {
        allowPopups: aNode.uri.startsWith("javascript:"),
        inBackground: this.loadBookmarksInBackground,
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
          title = host + (fileName ?
                           (host ? "/" + this.ellipsis + "/" : "") + fileName :
                           uri.pathQueryRef);
        }
      } catch (e) {
        // Use (no title) for non-standard URIs (data:, javascript:, ...)
        title = "";
      }
    } else
      title = aNode.title;

    return title || this.getString("noTitle");
  },

  shouldShowTabsFromOtherComputersMenuitem() {
    let weaveOK = Weave.Status.checkSetup() != Weave.CLIENT_NOT_CONFIGURED &&
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

    let query = {}, options = {};
    PlacesUtils.history.queryStringToQuery(queryString, query, options);
    query = query.value;
    options = options.value;
    return query.folderCount == 1 &&
           !query.hasBeginTime &&
           !query.hasEndTime &&
           !query.hasDomain &&
           !query.hasURI &&
           !query.hasSearchTerms &&
           !query.tags.length == 0 &&
           options.maxResults == 0;
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
    if (aFetchInfo.itemType == PlacesUtils.bookmarks.TYPE_SEPARATOR)
      throw new Error("promiseNodeLike doesn't support separators");

    let parent = {
      itemId: await PlacesUtils.promiseItemId(aFetchInfo.parentGuid),
      bookmarkGuid: aFetchInfo.parentGuid,
      type: Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER
    };

    return Object.freeze({
      itemId: await PlacesUtils.promiseItemId(aFetchInfo.guid),
      bookmarkGuid: aFetchInfo.guid,
      title: aFetchInfo.title,
      uri: aFetchInfo.url !== undefined ? aFetchInfo.url.href : "",

      get type() {
        if (aFetchInfo.itemType == PlacesUtils.bookmarks.TYPE_FOLDER)
          return Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER;

        if (this.uri.length == 0)
          throw new Error("Unexpected item type");

        if (/^place:/.test(this.uri)) {
          if (this.isFolderShortcutQueryString(this.uri))
            return Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT;

          return Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY;
        }

        return Ci.nsINavHistoryResultNode.RESULT_TYPE_URI;
      },

      get parent() {
        return parent;
      }
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

    resultNode = resultNode.QueryInterface(Ci.nsINavBookmarkObserver);

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
      transactions = [PlacesTransactions.Tag({ urls, tag: insertionPoint.tagName })];
    } else {
      let insertionIndex = await insertionPoint.getIndex();
      itemsCount = items.length;
      transactions = getTransactionsForTransferItems(
        items, insertionIndex, insertionPoint.guid, !doCopy);
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
    if (event.button == 2)
      return;

    let tree = event.target.parentNode;
    let tbo = tree.treeBoxObject;
    let cell = tbo.getCellAt(event.clientX, event.clientY);
    if (cell.row == -1 || cell.childElt == "twisty")
      return;

    // getCoordsForCellItem returns the x coordinate in logical coordinates
    // (i.e., starting from the left and right sides in LTR and RTL modes,
    // respectively.)  Therefore, we make sure to exclude the blank area
    // before the tree item icon (that is, to the left or right of it in
    // LTR and RTL modes, respectively) from the click target area.
    let win = tree.ownerGlobal;
    let rect = tbo.getCoordsForCellItem(cell.row, cell.col, "image");
    let isRTL = win.getComputedStyle(tree).direction == "rtl";
    let mouseInGutter = isRTL ? event.clientX > rect.x
                              : event.clientX < rect.x;

    let metaKey = AppConstants.platform === "macosx" ? event.metaKey
                                                     : event.ctrlKey;
    let modifKey = metaKey || event.shiftKey;
    let isContainer = tbo.view.isContainer(cell.row);
    let openInTabs = isContainer &&
                     (event.button == 1 || (event.button == 0 && modifKey)) &&
                     PlacesUtils.hasChildURIs(tree.view.nodeForTreeIndex(cell.row));

    if (event.button == 0 && isContainer && !openInTabs) {
      tbo.view.toggleOpenState(cell.row);
    } else if (!mouseInGutter && openInTabs &&
               event.originalTarget.localName == "treechildren") {
      tbo.view.selection.select(cell.row);
      this.openContainerNodeInTabs(tree.selectedNode, event, tree);
    } else if (!mouseInGutter && !isContainer &&
               event.originalTarget.localName == "treechildren") {
      // Clear all other selection since we're loading a link now. We must
      // do this *before* attempting to load the link since openURL uses
      // selection as an indication of which link to load.
      tbo.view.selection.select(cell.row);
      this.openNodeWithEvent(tree.selectedNode, event);
    }
  },

  onSidebarTreeKeyPress(event) {
    let node = event.target.selectedNode;
    if (node) {
      if (event.keyCode == event.DOM_VK_RETURN)
        this.openNodeWithEvent(node, event);
    }
  },

  /**
   * The following function displays the URL of a node that is being
   * hovered over.
   */
  onSidebarTreeMouseMove(event) {
    let treechildren = event.target;
    if (treechildren.localName != "treechildren")
      return;

    let tree = treechildren.parentNode;
    let cell = tree.treeBoxObject.getCellAt(event.clientX, event.clientY);

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
      win.top.XULBrowserWindow.setOverLink(url, null);
    }
  },
};

// These are lazy getters to avoid importing PlacesUtils immediately.
XPCOMUtils.defineLazyGetter(PlacesUIUtils, "PLACES_FLAVORS", () => {
  return [PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER,
          PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR,
          PlacesUtils.TYPE_X_MOZ_PLACE];
});
XPCOMUtils.defineLazyGetter(PlacesUIUtils, "URI_FLAVORS", () => {
  return [PlacesUtils.TYPE_X_MOZ_URL,
          TAB_DROP_TYPE,
          PlacesUtils.TYPE_UNICODE];
});
XPCOMUtils.defineLazyGetter(PlacesUIUtils, "SUPPORTED_FLAVORS", () => {
  return [...PlacesUIUtils.PLACES_FLAVORS,
          ...PlacesUIUtils.URI_FLAVORS];
});

XPCOMUtils.defineLazyGetter(PlacesUIUtils, "ellipsis", function() {
  return Services.prefs.getComplexValue("intl.ellipsis",
                                        Ci.nsIPrefLocalizedString).data;
});

XPCOMUtils.defineLazyPreferenceGetter(PlacesUIUtils, "loadBookmarksInBackground",
                                      PREF_LOAD_BOOKMARKS_IN_BACKGROUND, false);
XPCOMUtils.defineLazyPreferenceGetter(PlacesUIUtils, "loadBookmarksInTabs",
                                      PREF_LOAD_BOOKMARKS_IN_TABS, false);
XPCOMUtils.defineLazyPreferenceGetter(PlacesUIUtils, "openInTabClosesMenu",
  "browser.bookmarks.openInTabClosesMenu", false);

/**
 * Determines if an unwrapped node can be moved.
 *
 * @param unwrappedNode
 *        A node unwrapped by PlacesUtils.unwrapNodes().
 * @return True if the node can be moved, false otherwise.
 */
function canMoveUnwrappedNode(unwrappedNode) {
  if ((unwrappedNode.concreteGuid && PlacesUtils.isRootItem(unwrappedNode.concreteGuid)) ||
      unwrappedNode.id <= 0 || PlacesUtils.isRootItem(unwrappedNode.id)) {
    return false;
  }

  let parentGuid = unwrappedNode.parentGuid;
  // If there's no parent Guid, this was likely a virtual query that returns
  // bookmarks, such as a tags query.
  if (!parentGuid ||
      parentGuid == PlacesUtils.bookmarks.rootGuid) {
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
  if (viewOrElement && Element.isInstance(viewOrElement) &&
      viewOrElement.id === "placesList") {
    // Note: fall back to the existing item if we can't find the right-hane pane.
    viewOrElement = viewOrElement.ownerDocument.getElementById("placeContent") || viewOrElement;
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
function getTransactionsForTransferItems(items, insertionIndex,
                                         insertionParentGuid, doMove) {
  let canMove = true;
  for (let item of items) {
    if (!PlacesUIUtils.SUPPORTED_FLAVORS.includes(item.type)) {
      throw new Error(`Unsupported '${item.type}' data type`);
    }

    // Work out if this is data from the same app session we're running in.
    if (!("instanceId" in item) || item.instanceId != PlacesUtils.instanceId) {
      if (item.type == PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER) {
        throw new Error("Can't copy a container from a legacy-transactions build");
      }
      // Only log if this is one of "our" types as external items, e.g. drag from
      // url bar to toolbar, shouldn't complain.
      if (PlacesUIUtils.PLACES_FLAVORS.includes(item.type)) {
        Cu.reportError("Tried to move an unmovable Places " +
                       "node, reverting to a copy operation.");
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
    return [PlacesTransactions.Move({
      guids: items.map(item => item.itemGuid),
      newParentGuid: insertionParentGuid,
      newIndex: insertionIndex,
    })];
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
function getTransactionsForCopy(items, insertionIndex,
                                insertionParentGuid) {
  let transactions = [];
  let index = insertionIndex;

  for (let item of items) {
    let transaction;
    let guid = item.itemGuid;

    if (PlacesUIUtils.PLACES_FLAVORS.includes(item.type) &&
        // For anything that is comming from within this session, we do a
        // direct copy, otherwise we fallback and form a new item below.
        "instanceId" in item && item.instanceId == PlacesUtils.instanceId &&
        // If the Item doesn't have a guid, this could be a virtual tag query or
        // other item, so fallback to inserting a new bookmark with the URI.
        guid &&
        // For virtual root items, we fallback to creating a new bookmark, as
        // we want a shortcut to be created, not a full tree copy.
        !PlacesUtils.bookmarks.isVirtualRootItem(guid) &&
        !PlacesUtils.isVirtualLeftPaneItem(guid)) {
      transaction = PlacesTransactions.Copy({
        excludingAnnotation: "Places/SmartBookmark",
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
  return aWindow && aWindow.document.documentElement.getAttribute("windowtype") == "navigator:browser" ?
    aWindow : BrowserWindowTracker.getTopWindow();
}

/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["PlacesUIUtils"];

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

Cu.import("resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");

// PlacesUtils exposes multiple symbols, so we can't use defineLazyModuleGetter.
Cu.import("resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTransactions",
                                  "resource://gre/modules/PlacesTransactions.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CloudSync",
                                  "resource://gre/modules/CloudSync.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Weave",
                                  "resource://services-sync/main.js");

const gInContentProcess = Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;
const FAVICON_REQUEST_TIMEOUT = 60 * 1000;
// Map from windows to arrays of data about pending favicon loads.
let gFaviconLoadDataMap = new Map();

// copied from utilityOverlay.js
const TAB_DROP_TYPE = "application/x-moz-tabbrowser-tab";

// This function isn't public both because it's synchronous and because it is
// going to be removed in bug 1072833.
function IsLivemark(aItemId) {
  // Since this check may be done on each dragover event, it's worth maintaining
  // a cache.
  let self = IsLivemark;
  if (!("ids" in self)) {
    const LIVEMARK_ANNO = PlacesUtils.LMANNO_FEEDURI;

    let idsVec = PlacesUtils.annotations.getItemsWithAnnotation(LIVEMARK_ANNO);
    self.ids = new Set(idsVec);

    let obs = Object.freeze({
      QueryInterface: XPCOMUtils.generateQI(Ci.nsIAnnotationObserver),

      onItemAnnotationSet(itemId, annoName) {
        if (annoName == LIVEMARK_ANNO)
          self.ids.add(itemId);
      },

      onItemAnnotationRemoved(itemId, annoName) {
        // If annoName is set to an empty string, the item is gone.
        if (annoName == LIVEMARK_ANNO || annoName == "")
          self.ids.delete(itemId);
      },

      onPageAnnotationSet() { },
      onPageAnnotationRemoved() { },
    });
    PlacesUtils.annotations.addObserver(obs);
    PlacesUtils.registerShutdownFunction(() => {
      PlacesUtils.annotations.removeObserver(obs);
    });
  }
  return self.ids.has(aItemId);
}

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

    Services.obs.addObserver(this, "inner-window-destroyed", false);
    Services.ppmm.addMessageListener("Toolkit:inner-window-destroyed", msg => {
      this.removeRequestsForInner(msg.data);
    });
  },

  loadFavicon(browser, principal, uri) {
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

    // Immediately cancel any earlier requests
    this.removeRequestsForInner(innerWindowID);

    // First we do the actual setAndFetch call:
    let loadType = PrivateBrowsingUtils.isWindowPrivate(win)
      ? PlacesUtils.favicons.FAVICON_LOAD_PRIVATE
      : PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE;
    let callback = this._makeCompletionCallback(win, innerWindowID);
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

this.PlacesUIUtils = {
  ORGANIZER_LEFTPANE_VERSION: 7,
  ORGANIZER_FOLDER_ANNO: "PlacesOrganizer/OrganizerFolder",
  ORGANIZER_QUERY_ANNO: "PlacesOrganizer/OrganizerQuery",

  LOAD_IN_SIDEBAR_ANNO: "bookmarkProperties/loadInSidebar",
  DESCRIPTION_ANNO: "bookmarkProperties/description",

  /**
   * Makes a URI from a spec, and do fixup
   * @param   aSpec
   *          The string spec of the URI
   * @return A URI object for the spec.
   */
  createFixedURI: function PUIU_createFixedURI(aSpec) {
    return URIFixup.createFixupURI(aSpec, Ci.nsIURIFixup.FIXUP_FLAG_NONE);
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
    return str.replace(/\#(\d+)/g, function (matchedId, matchedNumber) {
      let param = aParams[parseInt(matchedNumber, 10) - 1];
      return param !== undefined ? param : matchedId;
    });
  },

  getString: function PUIU_getString(key) {
    return bundle.GetStringFromName(key);
  },

  get _copyableAnnotations() {
    return [
      this.DESCRIPTION_ANNO,
      this.LOAD_IN_SIDEBAR_ANNO,
      PlacesUtils.READ_ONLY_ANNO,
    ];
  },

  /**
   * Get a transaction for copying a uri item (either a bookmark or a history
   * entry) from one container to another.
   *
   * @param   aData
   *          JSON object of dropped or pasted item properties
   * @param   aContainer
   *          The container being copied into
   * @param   aIndex
   *          The index within the container the item is copied to
   * @return A nsITransaction object that performs the copy.
   *
   * @note Since a copy creates a completely new item, only some internal
   *       annotations are synced from the old one.
   * @see this._copyableAnnotations for the list of copyable annotations.
   */
  _getURIItemCopyTransaction:
  function PUIU__getURIItemCopyTransaction(aData, aContainer, aIndex)
  {
    let transactions = [];
    if (aData.dateAdded) {
      transactions.push(
        new PlacesEditItemDateAddedTransaction(null, aData.dateAdded)
      );
    }
    if (aData.lastModified) {
      transactions.push(
        new PlacesEditItemLastModifiedTransaction(null, aData.lastModified)
      );
    }

    let annos = [];
    if (aData.annos) {
      annos = aData.annos.filter(function (aAnno) {
        return this._copyableAnnotations.includes(aAnno.name);
      }, this);
    }

    // There's no need to copy the keyword since it's bound to the bookmark url.
    return new PlacesCreateBookmarkTransaction(PlacesUtils._uri(aData.uri),
                                               aContainer, aIndex, aData.title,
                                               null, annos, transactions);
  },

  /**
   * Gets a transaction for copying (recursively nesting to include children)
   * a folder (or container) and its contents from one folder to another.
   *
   * @param   aData
   *          Unwrapped dropped folder data - Obj containing folder and children
   * @param   aContainer
   *          The container we are copying into
   * @param   aIndex
   *          The index in the destination container to insert the new items
   * @return A nsITransaction object that will perform the copy.
   *
   * @note Since a copy creates a completely new item, only some internal
   *       annotations are synced from the old one.
   * @see this._copyableAnnotations for the list of copyable annotations.
   */
  _getFolderCopyTransaction(aData, aContainer, aIndex) {
    function getChildItemsTransactions(aRoot) {
      let transactions = [];
      let index = aIndex;
      for (let i = 0; i < aRoot.childCount; ++i) {
        let child = aRoot.getChild(i);
        // Temporary hacks until we switch to PlacesTransactions.jsm.
        let isLivemark =
          PlacesUtils.annotations.itemHasAnnotation(child.itemId,
                                                    PlacesUtils.LMANNO_FEEDURI);
        let [node] = PlacesUtils.unwrapNodes(
          PlacesUtils.wrapNode(child, PlacesUtils.TYPE_X_MOZ_PLACE, isLivemark),
          PlacesUtils.TYPE_X_MOZ_PLACE
        );

        // Make sure that items are given the correct index, this will be
        // passed by the transaction manager to the backend for the insertion.
        // Insertion behaves differently for DEFAULT_INDEX (append).
        if (aIndex != PlacesUtils.bookmarks.DEFAULT_INDEX) {
          index = i;
        }

        if (node.type == PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER) {
          if (node.livemark && node.annos) {
            transactions.push(
              PlacesUIUtils._getLivemarkCopyTransaction(node, aContainer, index)
            );
          }
          else {
            transactions.push(
              PlacesUIUtils._getFolderCopyTransaction(node, aContainer, index)
            );
          }
        }
        else if (node.type == PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR) {
          transactions.push(new PlacesCreateSeparatorTransaction(-1, index));
        }
        else if (node.type == PlacesUtils.TYPE_X_MOZ_PLACE) {
          transactions.push(
            PlacesUIUtils._getURIItemCopyTransaction(node, -1, index)
          );
        }
        else {
          throw new Error("Unexpected item under a bookmarks folder");
        }
      }
      return transactions;
    }

    if (aContainer == PlacesUtils.tagsFolderId) { // Copying into a tag folder.
      let transactions = [];
      if (!aData.livemark && aData.type == PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER) {
        let {root} = PlacesUtils.getFolderContents(aData.id, false, false);
        let urls = PlacesUtils.getURLsForContainerNode(root);
        root.containerOpen = false;
        for (let { uri } of urls) {
          transactions.push(
            new PlacesTagURITransaction(NetUtil.newURI(uri), [aData.title])
          );
        }
      }
      return new PlacesAggregatedTransaction("addTags", transactions);
    }

    if (aData.livemark && aData.annos) { // Copying a livemark.
      return this._getLivemarkCopyTransaction(aData, aContainer, aIndex);
    }

    let {root} = PlacesUtils.getFolderContents(aData.id, false, false);
    let transactions = getChildItemsTransactions(root);
    root.containerOpen = false;

    if (aData.dateAdded) {
      transactions.push(
        new PlacesEditItemDateAddedTransaction(null, aData.dateAdded)
      );
    }
    if (aData.lastModified) {
      transactions.push(
        new PlacesEditItemLastModifiedTransaction(null, aData.lastModified)
      );
    }

    let annos = [];
    if (aData.annos) {
      annos = aData.annos.filter(function (aAnno) {
        return this._copyableAnnotations.includes(aAnno.name);
      }, this);
    }

    return new PlacesCreateFolderTransaction(aData.title, aContainer, aIndex,
                                             annos, transactions);
  },

  /**
   * Gets a transaction for copying a live bookmark item from one container to
   * another.
   *
   * @param   aData
   *          Unwrapped live bookmarkmark data
   * @param   aContainer
   *          The container we are copying into
   * @param   aIndex
   *          The index in the destination container to insert the new items
   * @return A nsITransaction object that will perform the copy.
   *
   * @note Since a copy creates a completely new item, only some internal
   *       annotations are synced from the old one.
   * @see this._copyableAnnotations for the list of copyable annotations.
   */
  _getLivemarkCopyTransaction:
  function PUIU__getLivemarkCopyTransaction(aData, aContainer, aIndex)
  {
    if (!aData.livemark || !aData.annos) {
      throw new Error("node is not a livemark");
    }

    let feedURI, siteURI;
    let annos = [];
    if (aData.annos) {
      annos = aData.annos.filter(function (aAnno) {
        if (aAnno.name == PlacesUtils.LMANNO_FEEDURI) {
          feedURI = PlacesUtils._uri(aAnno.value);
        }
        else if (aAnno.name == PlacesUtils.LMANNO_SITEURI) {
          siteURI = PlacesUtils._uri(aAnno.value);
        }
        return this._copyableAnnotations.includes(aAnno.name)
      }, this);
    }

    return new PlacesCreateLivemarkTransaction(feedURI, siteURI, aData.title,
                                               aContainer, aIndex, annos);
  },

  /**
   * Constructs a Transaction for the drop or paste of a blob of data into
   * a container.
   * @param   data
   *          The unwrapped data blob of dropped or pasted data.
   * @param   type
   *          The content type of the data
   * @param   container
   *          The container the data was dropped or pasted into
   * @param   index
   *          The index within the container the item was dropped or pasted at
   * @param   copy
   *          The drag action was copy, so don't move folders or links.
   * @return An object implementing nsITransaction that can perform
   *         the move/insert.
   */
  makeTransaction:
  function PUIU_makeTransaction(data, type, container, index, copy)
  {
    switch (data.type) {
      case PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER:
        if (copy) {
          return this._getFolderCopyTransaction(data, container, index);
        }

        // Otherwise move the item.
        return new PlacesMoveItemTransaction(data.id, container, index);
        break;
      case PlacesUtils.TYPE_X_MOZ_PLACE:
        if (copy || data.id == -1) { // Id is -1 if the place is not bookmarked.
          return this._getURIItemCopyTransaction(data, container, index);
        }

        // Otherwise move the item.
        return new PlacesMoveItemTransaction(data.id, container, index);
        break;
      case PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR:
        if (copy) {
          // There is no data in a separator, so copying it just amounts to
          // inserting a new separator.
          return new PlacesCreateSeparatorTransaction(container, index);
        }

        // Otherwise move the item.
        return new PlacesMoveItemTransaction(data.id, container, index);
        break;
      default:
        if (type == PlacesUtils.TYPE_X_MOZ_URL ||
            type == PlacesUtils.TYPE_UNICODE ||
            type == TAB_DROP_TYPE) {
          let title = type != PlacesUtils.TYPE_UNICODE ? data.title
                                                       : data.uri;
          return new PlacesCreateBookmarkTransaction(PlacesUtils._uri(data.uri),
                                                     container, index, title);
        }
    }
    return null;
  },

  /**
   * ********* PlacesTransactions version of the function defined above ********
   *
   * Constructs a Places Transaction for the drop or paste of a blob of data
   * into a container.
   *
   * @param   aData
   *          The unwrapped data blob of dropped or pasted data.
   * @param   aType
   *          The content type of the data.
   * @param   aNewParentGuid
   *          GUID of the container the data was dropped or pasted into.
   * @param   aIndex
   *          The index within the container the item was dropped or pasted at.
   * @param   aCopy
   *          The drag action was copy, so don't move folders or links.
   *
   * @return  a Places Transaction that can be transacted for performing the
   *          move/insert command.
   */
  getTransactionForData: function(aData, aType, aNewParentGuid, aIndex, aCopy) {
    if (!this.SUPPORTED_FLAVORS.includes(aData.type))
      throw new Error(`Unsupported '${aData.type}' data type`);

    if ("itemGuid" in aData) {
      if (!this.PLACES_FLAVORS.includes(aData.type))
        throw new Error (`itemGuid unexpectedly set on ${aData.type} data`);

      let info = { guid: aData.itemGuid
                 , newParentGuid: aNewParentGuid
                 , newIndex: aIndex };
      if (aCopy) {
        info.excludingAnnotation = "Places/SmartBookmark";
        return PlacesTransactions.Copy(info);
      }
      return PlacesTransactions.Move(info);
    }

    // Since it's cheap and harmless, we allow the paste of separators and
    // bookmarks from builds that use legacy transactions (i.e. when itemGuid
    // was not set on PLACES_FLAVORS data). Containers are a different story,
    // and thus disallowed.
    if (aData.type == PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER)
      throw new Error("Can't copy a container from a legacy-transactions build");

    if (aData.type == PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR) {
      return PlacesTransactions.NewSeparator({ parentGuid: aNewParentGuid
                                             , index: aIndex });
    }

    let title = aData.type != PlacesUtils.TYPE_UNICODE ? aData.title
                                                       : aData.uri;
    return PlacesTransactions.NewBookmark({ uri: NetUtil.newURI(aData.uri)
                                          , title: title
                                          , parentGuid: aNewParentGuid
                                          , index: aIndex });
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
   * @return true if any transaction has been performed, false otherwise.
   */
  showBookmarkDialog:
  function PUIU_showBookmarkDialog(aInfo, aParentWindow) {
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
    aParentWindow.openDialog(dialogURL, "", features, aInfo);
    return ("performed" in aInfo && aInfo.performed);
  },

  _getTopBrowserWin: function PUIU__getTopBrowserWin() {
    return RecentWindow.getMostRecentBrowserWindow();
  },

  /**
   * set and fetch a favicon. Can only be used from the parent process.
   * @param browser   {Browser}   The XUL browser element for which we're fetching a favicon.
   * @param principal {Principal} The loading principal to use for the fetch.
   * @param uri       {URI}       The URI to fetch.
   */
  loadFavicon(browser, principal, uri) {
    if (gInContentProcess) {
      throw new Error("Can't track loads from within the child process!");
    }
    InternalFaviconLoader.loadFavicon(browser, principal, uri);
  },

  /**
   * Returns the closet ancestor places view for the given DOM node
   * @param aNode
   *        a DOM node
   * @return the closet ancestor places view if exists, null otherwsie.
   */
  getViewForNode: function PUIU_getViewForNode(aNode) {
    let node = aNode;

    // The view for a <menu> of which its associated menupopup is a places
    // view, is the menupopup.
    if (node.localName == "menu" && !node._placesNode &&
        node.lastChild._placesView)
      return node.lastChild._placesView;

    while (node instanceof Ci.nsIDOMElement) {
      if (node._placesView)
        return node._placesView;
      if (node.localName == "tree" && node.getAttribute("type") == "places")
        return node;

      node = node.parentNode;
    }

    return null;
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

    var uri = PlacesUtils._uri(aURINode.uri);
    if (uri.schemeIs("javascript") || uri.schemeIs("data")) {
      const BRANDING_BUNDLE_URI = "chrome://branding/locale/brand.properties";
      var brandShortName = Cc["@mozilla.org/intl/stringbundle;1"].
                           getService(Ci.nsIStringBundleService).
                           createBundle(BRANDING_BUNDLE_URI).
                           GetStringFromName("brandShortName");

      var errorStr = this.getString("load-js-data-url-error");
      Services.prompt.alert(aWindow, brandShortName, errorStr);
      return false;
    }
    return true;
  },

  /**
   * Get the description associated with a document, as specified in a <META>
   * element.
   * @param   doc
   *          A DOM Document to get a description for
   * @return A description string if a META element was discovered with a
   *         "description" or "httpequiv" attribute, empty string otherwise.
   */
  getDescriptionFromDocument: function PUIU_getDescriptionFromDocument(doc) {
    var metaElements = doc.getElementsByTagName("META");
    for (var i = 0; i < metaElements.length; ++i) {
      if (metaElements[i].name.toLowerCase() == "description" ||
          metaElements[i].httpEquiv.toLowerCase() == "description") {
        return metaElements[i].content;
      }
    }
    return "";
  },

  /**
   * Retrieve the description of an item
   * @param aItemId
   *        item identifier
   * @return the description of the given item, or an empty string if it is
   * not set.
   */
  getItemDescription: function PUIU_getItemDescription(aItemId) {
    if (PlacesUtils.annotations.itemHasAnnotation(aItemId, this.DESCRIPTION_ANNO))
      return PlacesUtils.annotations.getItemAnnotation(aItemId, this.DESCRIPTION_ANNO);
    return "";
  },

  /**
   * Check whether or not the given node represents a removable entry (either in
   * history or in bookmarks).
   *
   * @param aNode
   *        a node, except the root node of a query.
   * @return true if the aNode represents a removable entry, false otherwise.
   */
  canUserRemove: function (aNode) {
    let parentNode = aNode.parent;
    if (!parentNode) {
      // canUserRemove doesn't accept root nodes.
      return false;
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
    return !this.isContentsReadOnly(parentNode);
  },

  /**
   * DO NOT USE THIS API IN ADDONS. IT IS VERY LIKELY TO CHANGE WHEN THE SWITCH
   * TO GUIDS IS COMPLETE (BUG 1071511).
   *
   * Check whether or not the given node or item-id points to a folder which
   * should not be modified by the user (i.e. its children should be unremovable
   * and unmovable, new children should be disallowed, etc).
   * These semantics are not inherited, meaning that read-only folder may
   * contain editable items (for instance, the places root is read-only, but all
   * of its direct children aren't).
   *
   * You should only pass folder item ids or folder nodes for aNodeOrItemId.
   * While this is only enforced for the node case (if an item id of a separator
   * or a bookmark is passed, false is returned), it's considered the caller's
   * job to ensure that it checks a folder.
   * Also note that folder-shortcuts should only be passed as result nodes.
   * Otherwise they are just treated as bookmarks (i.e. false is returned).
   *
   * @param aNodeOrItemId
   *        any item id or result node.
   * @throws if aNodeOrItemId is neither an item id nor a folder result node.
   * @note livemark "folders" are considered read-only (but see bug 1072833).
   * @return true if aItemId points to a read-only folder, false otherwise.
   */
  isContentsReadOnly: function (aNodeOrItemId) {
    let itemId;
    if (typeof(aNodeOrItemId) == "number") {
      itemId = aNodeOrItemId;
    }
    else if (PlacesUtils.nodeIsFolder(aNodeOrItemId)) {
      itemId = PlacesUtils.getConcreteItemId(aNodeOrItemId);
    }
    else {
      throw new Error("invalid value for aNodeOrItemId");
    }

    if (itemId == PlacesUtils.placesRootId || IsLivemark(itemId))
      return true;

    // leftPaneFolderId, and as a result, allBookmarksFolderId, is a lazy getter
    // performing at least a synchronous DB query (and on its very first call
    // in a fresh profile, it also creates the entire structure).
    // Therefore we don't want to this function, which is called very often by
    // isCommandEnabled, to ever be the one that invokes it first, especially
    // because isCommandEnabled may be called way before the left pane folder is
    // even created (for example, if the user only uses the bookmarks menu or
    // toolbar for managing bookmarks).  To do so, we avoid comparing to those
    // special folder if the lazy getter is still in place.  This is safe merely
    // because the only way to access the left pane contents goes through
    // "resolving" the leftPaneFolderId getter.
    if ("get" in Object.getOwnPropertyDescriptor(this, "leftPaneFolderId"))
      return false;

    return itemId == this.leftPaneFolderId ||
           itemId == this.allBookmarksFolderId;
  },

  /**
   * Gives the user a chance to cancel loading lots of tabs at once
   */
  confirmOpenInTabs(numTabsToOpen, aWindow) {
    const WARN_ON_OPEN_PREF = "browser.tabs.warnOnOpen";
    var reallyOpen = true;

    if (Services.prefs.getBoolPref(WARN_ON_OPEN_PREF)) {
      if (numTabsToOpen >= Services.prefs.getIntPref("browser.tabs.maxOpenBeforeWarn")) {
        // default to true: if it were false, we wouldn't get this far
        var warnOnOpen = { value: true };

        var messageKey = "tabs.openWarningMultipleBranded";
        var openKey = "tabs.openButtonMultiple";
        const BRANDING_BUNDLE_URI = "chrome://branding/locale/brand.properties";
        var brandShortName = Cc["@mozilla.org/intl/stringbundle;1"].
                             getService(Ci.nsIStringBundleService).
                             createBundle(BRANDING_BUNDLE_URI).
                             GetStringFromName("brandShortName");

        var buttonPressed = Services.prompt.confirmEx(
          aWindow,
          this.getString("tabs.openWarningTitle"),
          this.getFormattedString(messageKey, [numTabsToOpen, brandShortName]),
          (Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0) +
            (Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1),
          this.getString(openKey), null, null,
          this.getFormattedString("tabs.openWarningPromptMeBranded",
                                  [brandShortName]),
          warnOnOpen
        );

        reallyOpen = (buttonPressed == 0);
        // don't set the pref unless they press OK and it's false
        if (reallyOpen && !warnOnOpen.value)
          Services.prefs.setBoolPref(WARN_ON_OPEN_PREF, false);
      }
    }

    return reallyOpen;
  },

  /** aItemsToOpen needs to be an array of objects of the form:
    * {uri: string, isBookmark: boolean}
    */
  _openTabset: function PUIU__openTabset(aItemsToOpen, aEvent, aWindow) {
    if (!aItemsToOpen.length)
      return;

    // Prefer the caller window if it's a browser window, otherwise use
    // the top browser window.
    var browserWindow = null;
    browserWindow =
      aWindow && aWindow.document.documentElement.getAttribute("windowtype") == "navigator:browser" ?
      aWindow : this._getTopBrowserWin();

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
      args.appendElement(uriList, /* weak =*/ false);
      browserWindow = Services.ww.openWindow(aWindow,
                                             "chrome://browser/content/browser.xul",
                                             null, "chrome,dialog=no,all", args);
      return;
    }

    var loadInBackground = where == "tabshifted" ? true : false;
    // For consistency, we want all the bookmarks to open in new tabs, instead
    // of having one of them replace the currently focused tab.  Hence we call
    // loadTabs with aReplace set to false.
    browserWindow.gBrowser.loadTabs(urls, loadInBackground, false);
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

        if (this.confirmOpenInTabs(urlsToOpen.length, window)) {
          this._openTabset(urlsToOpen, aEvent, window);
        }
      }, Cu.reportError);
  },

  openContainerNodeInTabs:
  function PUIU_openContainerInTabs(aNode, aEvent, aView) {
    let window = aView.ownerWindow;

    let urlsToOpen = PlacesUtils.getURLsForContainerNode(aNode);
    if (this.confirmOpenInTabs(urlsToOpen.length, window)) {
      this._openTabset(urlsToOpen, aEvent, window);
    }
  },

  openURINodesInTabs: function PUIU_openURINodesInTabs(aNodes, aEvent, aView) {
    let window = aView.ownerWindow;

    let urlsToOpen = [];
    for (var i=0; i < aNodes.length; i++) {
      // Skip over separators and folders.
      if (PlacesUtils.nodeIsURI(aNodes[i]))
        urlsToOpen.push({uri: aNodes[i].uri, isBookmark: PlacesUtils.nodeIsBookmark(aNodes[i])});
    }
    this._openTabset(urlsToOpen, aEvent, window);
  },

  /**
   * Loads the node's URL in the appropriate tab or window or as a web
   * panel given the user's preference specified by modifier keys tracked by a
   * DOM mouse/key event.
   * @param   aNode
   *          An uri result node.
   * @param   aEvent
   *          The DOM mouse/key event with modifier keys set that track the
   *          user's preferred destination window or tab.
   * @param   aView
   *          The controller associated with aNode.
   */
  openNodeWithEvent:
  function PUIU_openNodeWithEvent(aNode, aEvent, aView) {
    let window = aView.ownerWindow;
    this._openNodeIn(aNode, window.whereToOpenLink(aEvent, false, true), window);
  },

  /**
   * Loads the node's URL in the appropriate tab or window or as a
   * web panel.
   * see also openUILinkIn
   */
  openNodeIn: function PUIU_openNodeIn(aNode, aWhere, aView, aPrivate) {
    let window = aView.ownerWindow;
    this._openNodeIn(aNode, aWhere, window, aPrivate);
  },

  _openNodeIn: function PUIU_openNodeIn(aNode, aWhere, aWindow, aPrivate=false) {
    if (aNode && PlacesUtils.nodeIsURI(aNode) &&
        this.checkURLSecurity(aNode, aWindow)) {
      let isBookmark = PlacesUtils.nodeIsBookmark(aNode);

      if (!PrivateBrowsingUtils.isWindowPrivate(aWindow)) {
        if (isBookmark)
          this.markPageAsFollowedBookmark(aNode.uri);
        else
          this.markPageAsTyped(aNode.uri);
      }

      // Check whether the node is a bookmark which should be opened as
      // a web panel
      if (aWhere == "current" && isBookmark) {
        if (PlacesUtils.annotations
                       .itemHasAnnotation(aNode.itemId, this.LOAD_IN_SIDEBAR_ANNO)) {
          let browserWin = this._getTopBrowserWin();
          if (browserWin) {
            browserWin.openWebPanel(aNode.title, aNode.uri);
            return;
          }
        }
      }

      aWindow.openUILinkIn(aNode.uri, aWhere, {
        allowPopups: aNode.uri.startsWith("javascript:"),
        inBackground: Services.prefs.getBoolPref("browser.tabs.loadBookmarksInBackground"),
        private: aPrivate,
      });
    }
  },

  /**
   * Helper for guessing scheme from an url string.
   * Used to avoid nsIURI overhead in frequently called UI functions.
   *
   * @param aUrlString the url to guess the scheme from.
   *
   * @return guessed scheme for this url string.
   *
   * @note this is not supposed be perfect, so use it only for UI purposes.
   */
  guessUrlSchemeForUI: function PUIU_guessUrlSchemeForUI(aUrlString) {
    return aUrlString.substr(0, aUrlString.indexOf(":"));
  },

  getBestTitle: function PUIU_getBestTitle(aNode, aDoNotCutTitle) {
    var title;
    if (!aNode.title && PlacesUtils.nodeIsURI(aNode)) {
      // if node title is empty, try to set the label using host and filename
      // PlacesUtils._uri() will throw if aNode.uri is not a valid URI
      try {
        var uri = PlacesUtils._uri(aNode.uri);
        var host = uri.host;
        var fileName = uri.QueryInterface(Ci.nsIURL).fileName;
        // if fileName is empty, use path to distinguish labels
        if (aDoNotCutTitle) {
          title = host + uri.path;
        } else {
          title = host + (fileName ?
                           (host ? "/" + this.ellipsis + "/" : "") + fileName :
                           uri.path);
        }
      }
      catch (e) {
        // Use (no title) for non-standard URIs (data:, javascript:, ...)
        title = "";
      }
    }
    else
      title = aNode.title;

    return title || this.getString("noTitle");
  },

  get leftPaneQueries() {
    // build the map
    this.leftPaneFolderId;
    return this.leftPaneQueries;
  },

  // Get the folder id for the organizer left-pane folder.
  get leftPaneFolderId() {
    let leftPaneRoot = -1;
    let allBookmarksId;

    // Shortcuts to services.
    let bs = PlacesUtils.bookmarks;
    let as = PlacesUtils.annotations;

    // This is the list of the left pane queries.
    let queries = {
      "PlacesRoot": { title: "" },
      "History": { title: this.getString("OrganizerQueryHistory") },
      "Downloads": { title: this.getString("OrganizerQueryDownloads") },
      "Tags": { title: this.getString("OrganizerQueryTags") },
      "AllBookmarks": { title: this.getString("OrganizerQueryAllBookmarks") },
      "BookmarksToolbar":
        { title: null,
          concreteTitle: PlacesUtils.getString("BookmarksToolbarFolderTitle"),
          concreteId: PlacesUtils.toolbarFolderId },
      "BookmarksMenu":
        { title: null,
          concreteTitle: PlacesUtils.getString("BookmarksMenuFolderTitle"),
          concreteId: PlacesUtils.bookmarksMenuFolderId },
      "UnfiledBookmarks":
        { title: null,
          concreteTitle: PlacesUtils.getString("OtherBookmarksFolderTitle"),
          concreteId: PlacesUtils.unfiledBookmarksFolderId },
    };
    // All queries but PlacesRoot.
    const EXPECTED_QUERY_COUNT = 7;

    // Removes an item and associated annotations, ignoring eventual errors.
    function safeRemoveItem(aItemId) {
      try {
        if (as.itemHasAnnotation(aItemId, PlacesUIUtils.ORGANIZER_QUERY_ANNO) &&
            !(as.getItemAnnotation(aItemId, PlacesUIUtils.ORGANIZER_QUERY_ANNO) in queries)) {
          // Some extension annotated their roots with our query annotation,
          // so we should not delete them.
          return;
        }
        // removeItemAnnotation does not check if item exists, nor the anno,
        // so this is safe to do.
        as.removeItemAnnotation(aItemId, PlacesUIUtils.ORGANIZER_FOLDER_ANNO);
        as.removeItemAnnotation(aItemId, PlacesUIUtils.ORGANIZER_QUERY_ANNO);
        // This will throw if the annotation is an orphan.
        bs.removeItem(aItemId);
      }
      catch (e) { /* orphan anno */ }
    }

    // Returns true if item really exists, false otherwise.
    function itemExists(aItemId) {
      try {
        bs.getItemIndex(aItemId);
        return true;
      }
      catch (e) {
        return false;
      }
    }

    // Get all items marked as being the left pane folder.
    let items = as.getItemsWithAnnotation(this.ORGANIZER_FOLDER_ANNO);
    if (items.length > 1) {
      // Something went wrong, we cannot have more than one left pane folder,
      // remove all left pane folders and continue.  We will create a new one.
      items.forEach(safeRemoveItem);
    }
    else if (items.length == 1 && items[0] != -1) {
      leftPaneRoot = items[0];
      // Check that organizer left pane root is valid.
      let version = as.getItemAnnotation(leftPaneRoot, this.ORGANIZER_FOLDER_ANNO);
      if (version != this.ORGANIZER_LEFTPANE_VERSION ||
          !itemExists(leftPaneRoot)) {
        // Invalid root, we must rebuild the left pane.
        safeRemoveItem(leftPaneRoot);
        leftPaneRoot = -1;
      }
    }

    if (leftPaneRoot != -1) {
      // A valid left pane folder has been found.
      // Build the leftPaneQueries Map.  This is used to quickly access them,
      // associating a mnemonic name to the real item ids.
      delete this.leftPaneQueries;
      this.leftPaneQueries = {};

      let items = as.getItemsWithAnnotation(this.ORGANIZER_QUERY_ANNO);
      // While looping through queries we will also check for their validity.
      let queriesCount = 0;
      let corrupt = false;
      for (let i = 0; i < items.length; i++) {
        let queryName = as.getItemAnnotation(items[i], this.ORGANIZER_QUERY_ANNO);

        // Some extension did use our annotation to decorate their items
        // with icons, so we should check only our elements, to avoid dataloss.
        if (!(queryName in queries))
          continue;

        let query = queries[queryName];
        query.itemId = items[i];

        if (!itemExists(query.itemId)) {
          // Orphan annotation, bail out and create a new left pane root.
          corrupt = true;
          break;
        }

        // Check that all queries have valid parents.
        let parentId = bs.getFolderIdForItem(query.itemId);
        if (!items.includes(parentId) && parentId != leftPaneRoot) {
          // The parent is not part of the left pane, bail out and create a new
          // left pane root.
          corrupt = true;
          break;
        }

        // Titles could have been corrupted or the user could have changed his
        // locale.  Check title and eventually fix it.
        if (bs.getItemTitle(query.itemId) != query.title)
          bs.setItemTitle(query.itemId, query.title);
        if ("concreteId" in query) {
          if (bs.getItemTitle(query.concreteId) != query.concreteTitle)
            bs.setItemTitle(query.concreteId, query.concreteTitle);
        }

        // Add the query to our cache.
        this.leftPaneQueries[queryName] = query.itemId;
        queriesCount++;
      }

      // Note: it's not enough to just check for queriesCount, since we may
      // find an invalid query just after accounting for a sufficient number of
      // valid ones.  As well as we can't just rely on corrupt since we may find
      // less valid queries than expected.
      if (corrupt || queriesCount != EXPECTED_QUERY_COUNT) {
        // Queries number is wrong, so the left pane must be corrupt.
        // Note: we can't just remove the leftPaneRoot, because some query could
        // have a bad parent, so we have to remove all items one by one.
        items.forEach(safeRemoveItem);
        safeRemoveItem(leftPaneRoot);
      }
      else {
        // Everything is fine, return the current left pane folder.
        delete this.leftPaneFolderId;
        return this.leftPaneFolderId = leftPaneRoot;
      }
    }

    // Create a new left pane folder.
    var callback = {
      // Helper to create an organizer special query.
      create_query: function CB_create_query(aQueryName, aParentId, aQueryUrl) {
        let itemId = bs.insertBookmark(aParentId,
                                       PlacesUtils._uri(aQueryUrl),
                                       bs.DEFAULT_INDEX,
                                       queries[aQueryName].title);
        // Mark as special organizer query.
        as.setItemAnnotation(itemId, PlacesUIUtils.ORGANIZER_QUERY_ANNO, aQueryName,
                             0, as.EXPIRE_NEVER);
        // We should never backup this, since it changes between profiles.
        as.setItemAnnotation(itemId, PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO, 1,
                             0, as.EXPIRE_NEVER);
        // Add to the queries map.
        PlacesUIUtils.leftPaneQueries[aQueryName] = itemId;
        return itemId;
      },

      // Helper to create an organizer special folder.
      create_folder: function CB_create_folder(aFolderName, aParentId, aIsRoot) {
              // Left Pane Root Folder.
        let folderId = bs.createFolder(aParentId,
                                       queries[aFolderName].title,
                                       bs.DEFAULT_INDEX);
        // We should never backup this, since it changes between profiles.
        as.setItemAnnotation(folderId, PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO, 1,
                             0, as.EXPIRE_NEVER);

        if (aIsRoot) {
          // Mark as special left pane root.
          as.setItemAnnotation(folderId, PlacesUIUtils.ORGANIZER_FOLDER_ANNO,
                               PlacesUIUtils.ORGANIZER_LEFTPANE_VERSION,
                               0, as.EXPIRE_NEVER);
        }
        else {
          // Mark as special organizer folder.
          as.setItemAnnotation(folderId, PlacesUIUtils.ORGANIZER_QUERY_ANNO, aFolderName,
                           0, as.EXPIRE_NEVER);
          PlacesUIUtils.leftPaneQueries[aFolderName] = folderId;
        }
        return folderId;
      },

      runBatched: function CB_runBatched(aUserData) {
        delete PlacesUIUtils.leftPaneQueries;
        PlacesUIUtils.leftPaneQueries = { };

        // Left Pane Root Folder.
        leftPaneRoot = this.create_folder("PlacesRoot", bs.placesRoot, true);

        // History Query.
        this.create_query("History", leftPaneRoot,
                          "place:type=" +
                          Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY +
                          "&sort=" +
                          Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING);

        // Downloads.
        this.create_query("Downloads", leftPaneRoot,
                          "place:transition=" +
                          Ci.nsINavHistoryService.TRANSITION_DOWNLOAD +
                          "&sort=" +
                          Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING);

        // Tags Query.
        this.create_query("Tags", leftPaneRoot,
                          "place:type=" +
                          Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY +
                          "&sort=" +
                          Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING);

        // All Bookmarks Folder.
        allBookmarksId = this.create_folder("AllBookmarks", leftPaneRoot, false);

        // All Bookmarks->Bookmarks Toolbar Query.
        this.create_query("BookmarksToolbar", allBookmarksId,
                          "place:folder=TOOLBAR");

        // All Bookmarks->Bookmarks Menu Query.
        this.create_query("BookmarksMenu", allBookmarksId,
                          "place:folder=BOOKMARKS_MENU");

        // All Bookmarks->Unfiled Bookmarks Query.
        this.create_query("UnfiledBookmarks", allBookmarksId,
                          "place:folder=UNFILED_BOOKMARKS");
      }
    };
    bs.runInBatchMode(callback, null);

    delete this.leftPaneFolderId;
    return this.leftPaneFolderId = leftPaneRoot;
  },

  /**
   * Get the folder id for the organizer left-pane folder.
   */
  get allBookmarksFolderId() {
    // ensure the left-pane root is initialized;
    this.leftPaneFolderId;
    delete this.allBookmarksFolderId;
    return this.allBookmarksFolderId = this.leftPaneQueries["AllBookmarks"];
  },

  /**
   * If an item is a left-pane query, returns the name of the query
   * or an empty string if not.
   *
   * @param aItemId id of a container
   * @return the name of the query, or empty string if not a left-pane query
   */
  getLeftPaneQueryNameFromId: function PUIU_getLeftPaneQueryNameFromId(aItemId) {
    var queryName = "";
    // If the let pane hasn't been built, use the annotation service
    // directly, to avoid building the left pane too early.
    if (Object.getOwnPropertyDescriptor(this, "leftPaneFolderId").value === undefined) {
      try {
        queryName = PlacesUtils.annotations.
                                getItemAnnotation(aItemId, this.ORGANIZER_QUERY_ANNO);
      }
      catch (ex) {
        // doesn't have the annotation
        queryName = "";
      }
    }
    else {
      // If the left pane has already been built, use the name->id map
      // cached in PlacesUIUtils.
      for (let [name, id] of Object.entries(this.leftPaneQueries)) {
        if (aItemId == id)
          queryName = name;
      }
    }
    return queryName;
  },

  shouldShowTabsFromOtherComputersMenuitem: function() {
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

    let queriesParam = { }, optionsParam = { };
    PlacesUtils.history.queryStringToQueries(queryString,
                                             queriesParam,
                                             { },
                                             optionsParam);
    let queries = queries.value;
    if (queries.length == 0)
      throw new Error(`Invalid place: uri: ${queryString}`);
    return queries.length == 1 &&
           queries[0].folderCount == 1 &&
           !queries[0].hasBeginTime &&
           !queries[0].hasEndTime &&
           !queries[0].hasDomain &&
           !queries[0].hasURI &&
           !queries[0].hasSearchTerms &&
           !queries[0].tags.length == 0 &&
           optionsParam.value.maxResults == 0;
  },

  /**
   * WARNING TO ADDON AUTHORS: DO NOT USE THIS METHOD. IT"S LIKELY TO BE REMOVED IN A
   * FUTURE RELEASE.
   *
   * Helpers for consumers of editBookmarkOverlay which don't have a node as their input.
   * Given a partial node-like object, having at least the itemId property set, this
   * method completes the rest of the properties necessary for initialising the edit
   * overlay with it.
   *
   * @param aNodeLike
   *        an object having at least the itemId nsINavHistoryResultNode property set,
   *        along with any other properties available.
   */
  completeNodeLikeObjectForItemId(aNodeLike) {
    if (this.useAsyncTransactions) {
      // When async-transactions are enabled, node-likes must have
      // bookmarkGuid set, and we cannot set it synchronously.
      throw new Error("completeNodeLikeObjectForItemId cannot be used when " +
                      "async transactions are enabled");
    }
    if (!("itemId" in aNodeLike))
      throw new Error("itemId missing in aNodeLike");

    let itemId = aNodeLike.itemId;
    let defGetter = XPCOMUtils.defineLazyGetter.bind(XPCOMUtils, aNodeLike);

    if (!("title" in aNodeLike))
      defGetter("title", () => PlacesUtils.bookmarks.getItemTitle(itemId));

    if (!("uri" in aNodeLike)) {
      defGetter("uri", () => {
        let uri = null;
        try {
          uri = PlacesUtils.bookmarks.getBookmarkURI(itemId);
        }
        catch (ex) { }
        return uri ? uri.spec : "";
      });
    }

    if (!("type" in aNodeLike)) {
      defGetter("type", () => {
        if (aNodeLike.uri.length > 0) {
          if (/^place:/.test(aNodeLike.uri)) {
            if (this.isFolderShortcutQueryString(aNodeLike.uri))
              return Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT;

            return Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY;
          }

          return Ci.nsINavHistoryResultNode.RESULT_TYPE_URI;
        }

        let itemType = PlacesUtils.bookmarks.getItemType(itemId);
        if (itemType == PlacesUtils.bookmarks.TYPE_FOLDER)
          return Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER;

        throw new Error("Unexpected item type");
      });
    }
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
  promiseNodeLikeFromFetchInfo: Task.async(function* (aFetchInfo) {
    if (aFetchInfo.itemType == PlacesUtils.bookmarks.TYPE_SEPARATOR)
      throw new Error("promiseNodeLike doesn't support separators");

    return Object.freeze({
      itemId: yield PlacesUtils.promiseItemId(aFetchInfo.guid),
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
      }
    });
  }),

  /**
   * Shortcut for calling promiseNodeLikeFromFetchInfo on the result of
   * Bookmarks.fetch for the given guid/info object.
   *
   * @see promiseNodeLikeFromFetchInfo above and Bookmarks.fetch in Bookmarks.jsm.
   */
  fetchNodeLike: Task.async(function* (aGuidOrInfo) {
    let info = yield PlacesUtils.bookmarks.fetch(aGuidOrInfo);
    if (!info)
      return null;
    return (yield this.promiseNodeLikeFromFetchInfo(info));
  })
};


PlacesUIUtils.PLACES_FLAVORS = [PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER,
                                PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR,
                                PlacesUtils.TYPE_X_MOZ_PLACE];

PlacesUIUtils.URI_FLAVORS = [PlacesUtils.TYPE_X_MOZ_URL,
                             TAB_DROP_TYPE,
                             PlacesUtils.TYPE_UNICODE],

PlacesUIUtils.SUPPORTED_FLAVORS = [...PlacesUIUtils.PLACES_FLAVORS,
                                   ...PlacesUIUtils.URI_FLAVORS];

XPCOMUtils.defineLazyServiceGetter(PlacesUIUtils, "RDF",
                                   "@mozilla.org/rdf/rdf-service;1",
                                   "nsIRDFService");

XPCOMUtils.defineLazyGetter(PlacesUIUtils, "ellipsis", function() {
  return Services.prefs.getComplexValue("intl.ellipsis",
                                        Ci.nsIPrefLocalizedString).data;
});

XPCOMUtils.defineLazyGetter(PlacesUIUtils, "useAsyncTransactions", function() {
  try {
    return Services.prefs.getBoolPref("browser.places.useAsyncTransactions");
  }
  catch (ex) { }
  return false;
});

XPCOMUtils.defineLazyServiceGetter(this, "URIFixup",
                                   "@mozilla.org/docshell/urifixup;1",
                                   "nsIURIFixup");

XPCOMUtils.defineLazyGetter(this, "bundle", function() {
  const PLACES_STRING_BUNDLE_URI =
    "chrome://browser/locale/places/places.properties";
  return Cc["@mozilla.org/intl/stringbundle;1"].
         getService(Ci.nsIStringBundleService).
         createBundle(PLACES_STRING_BUNDLE_URI);
});

/**
 * This is a compatibility shim for old PUIU.ptm users.
 *
 * If you're looking for transactions and writing new code using them, directly
 * use the transactions objects exported by the PlacesUtils.jsm module.
 *
 * This object will be removed once enough users are converted to the new API.
 */
XPCOMUtils.defineLazyGetter(PlacesUIUtils, "ptm", function() {
  // Ensure PlacesUtils is imported in scope.
  PlacesUtils;

  return {
    aggregateTransactions: (aName, aTransactions) =>
      new PlacesAggregatedTransaction(aName, aTransactions),

    createFolder: (aName, aContainer, aIndex, aAnnotations,
                   aChildItemsTransactions) =>
      new PlacesCreateFolderTransaction(aName, aContainer, aIndex, aAnnotations,
                                        aChildItemsTransactions),

    createItem: (aURI, aContainer, aIndex, aTitle, aKeyword,
                 aAnnotations, aChildTransactions) =>
      new PlacesCreateBookmarkTransaction(aURI, aContainer, aIndex, aTitle,
                                          aKeyword, aAnnotations,
                                          aChildTransactions),

    createSeparator: (aContainer, aIndex) =>
      new PlacesCreateSeparatorTransaction(aContainer, aIndex),

    createLivemark: (aFeedURI, aSiteURI, aName, aContainer, aIndex,
                     aAnnotations) =>
      new PlacesCreateLivemarkTransaction(aFeedURI, aSiteURI, aName, aContainer,
                                          aIndex, aAnnotations),

    moveItem: (aItemId, aNewContainer, aNewIndex) =>
      new PlacesMoveItemTransaction(aItemId, aNewContainer, aNewIndex),

    removeItem: (aItemId) =>
      new PlacesRemoveItemTransaction(aItemId),

    editItemTitle: (aItemId, aNewTitle) =>
      new PlacesEditItemTitleTransaction(aItemId, aNewTitle),

    editBookmarkURI: (aItemId, aNewURI) =>
      new PlacesEditBookmarkURITransaction(aItemId, aNewURI),

    setItemAnnotation: (aItemId, aAnnotationObject) =>
      new PlacesSetItemAnnotationTransaction(aItemId, aAnnotationObject),

    setPageAnnotation: (aURI, aAnnotationObject) =>
      new PlacesSetPageAnnotationTransaction(aURI, aAnnotationObject),

    editBookmarkKeyword: (aItemId, aNewKeyword) =>
      new PlacesEditBookmarkKeywordTransaction(aItemId, aNewKeyword),

    editLivemarkSiteURI: (aLivemarkId, aSiteURI) =>
      new PlacesEditLivemarkSiteURITransaction(aLivemarkId, aSiteURI),

    editLivemarkFeedURI: (aLivemarkId, aFeedURI) =>
      new PlacesEditLivemarkFeedURITransaction(aLivemarkId, aFeedURI),

    editItemDateAdded: (aItemId, aNewDateAdded) =>
      new PlacesEditItemDateAddedTransaction(aItemId, aNewDateAdded),

    editItemLastModified: (aItemId, aNewLastModified) =>
      new PlacesEditItemLastModifiedTransaction(aItemId, aNewLastModified),

    sortFolderByName: (aFolderId) =>
      new PlacesSortFolderByNameTransaction(aFolderId),

    tagURI: (aURI, aTags) =>
      new PlacesTagURITransaction(aURI, aTags),

    untagURI: (aURI, aTags) =>
      new PlacesUntagURITransaction(aURI, aTags),

    /**
     * Transaction for setting/unsetting Load-in-sidebar annotation.
     *
     * @param aBookmarkId
     *        id of the bookmark where to set Load-in-sidebar annotation.
     * @param aLoadInSidebar
     *        boolean value.
     * @return nsITransaction object.
     */
    setLoadInSidebar: function(aItemId, aLoadInSidebar)
    {
      let annoObj = { name: PlacesUIUtils.LOAD_IN_SIDEBAR_ANNO,
                      type: Ci.nsIAnnotationService.TYPE_INT32,
                      flags: 0,
                      value: aLoadInSidebar,
                      expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
      return new PlacesSetItemAnnotationTransaction(aItemId, annoObj);
    },

   /**
    * Transaction for editing the description of a bookmark or a folder.
    *
    * @param aItemId
    *        id of the item to edit.
    * @param aDescription
    *        new description.
    * @return nsITransaction object.
    */
    editItemDescription: function(aItemId, aDescription)
    {
      let annoObj = { name: PlacesUIUtils.DESCRIPTION_ANNO,
                      type: Ci.nsIAnnotationService.TYPE_STRING,
                      flags: 0,
                      value: aDescription,
                      expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
      return new PlacesSetItemAnnotationTransaction(aItemId, annoObj);
    },

    // //////////////////////////////////////////////////////////////////////////
    // // nsITransactionManager forwarders.

    beginBatch: () =>
      PlacesUtils.transactionManager.beginBatch(null),

    endBatch: () =>
      PlacesUtils.transactionManager.endBatch(false),

    doTransaction: (txn) =>
      PlacesUtils.transactionManager.doTransaction(txn),

    undoTransaction: () =>
      PlacesUtils.transactionManager.undoTransaction(),

    redoTransaction: () =>
      PlacesUtils.transactionManager.redoTransaction(),

    get numberOfUndoItems() {
      return PlacesUtils.transactionManager.numberOfUndoItems;
    },
    get numberOfRedoItems() {
      return PlacesUtils.transactionManager.numberOfRedoItems;
    },
    get maxTransactionCount() {
      return PlacesUtils.transactionManager.maxTransactionCount;
    },
    set maxTransactionCount(val) {
      PlacesUtils.transactionManager.maxTransactionCount = val;
    },

    clear: () =>
      PlacesUtils.transactionManager.clear(),

    peekUndoStack: () =>
      PlacesUtils.transactionManager.peekUndoStack(),

    peekRedoStack: () =>
      PlacesUtils.transactionManager.peekRedoStack(),

    getUndoStack: () =>
      PlacesUtils.transactionManager.getUndoStack(),

    getRedoStack: () =>
      PlacesUtils.transactionManager.getRedoStack(),

    AddListener: (aListener) =>
      PlacesUtils.transactionManager.AddListener(aListener),

    RemoveListener: (aListener) =>
      PlacesUtils.transactionManager.RemoveListener(aListener)
  }
});

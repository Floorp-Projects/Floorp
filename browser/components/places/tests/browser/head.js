XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");

// We need to cache this before test runs...
var cachedLeftPaneFolderIdGetter;
var getter = PlacesUIUtils.__lookupGetter__("leftPaneFolderId");
if (!cachedLeftPaneFolderIdGetter && typeof(getter) == "function") {
  cachedLeftPaneFolderIdGetter = getter;
}

// ...And restore it when test ends.
registerCleanupFunction(function() {
  let getter = PlacesUIUtils.__lookupGetter__("leftPaneFolderId");
  if (cachedLeftPaneFolderIdGetter && typeof(getter) != "function") {
    PlacesUIUtils.__defineGetter__("leftPaneFolderId", cachedLeftPaneFolderIdGetter);
  }
});

function openLibrary(callback, aLeftPaneRoot) {
  let library = window.openDialog("chrome://browser/content/places/places.xul",
                                  "", "chrome,toolbar=yes,dialog=no,resizable",
                                  aLeftPaneRoot);
  waitForFocus(function () {
    callback(library);
  }, library);

  return library;
}

/**
 * Returns a handle to a Library window.
 * If one is opens returns itm otherwise it opens a new one.
 *
 * @param aLeftPaneRoot
 *        Hierarchy to open and select in the left pane.
 */
function promiseLibrary(aLeftPaneRoot) {
  return new Promise(resolve => {
    let library = Services.wm.getMostRecentWindow("Places:Organizer");
    if (library && !library.closed) {
      if (aLeftPaneRoot) {
        library.PlacesOrganizer.selectLeftPaneContainerByHierarchy(aLeftPaneRoot);
      }
      resolve(library);
    }
    else {
      openLibrary(resolve, aLeftPaneRoot);
    }
  });
}

function promiseLibraryClosed(organizer) {
  return new Promise(resolve => {
    // Wait for the Organizer window to actually be closed
    organizer.addEventListener("unload", function onUnload() {
      organizer.removeEventListener("unload", onUnload);
      resolve();
    });

    // Close Library window.
    organizer.close();
  });
}

/**
 * Waits for a clipboard operation to complete, looking for the expected type.
 *
 * @see waitForClipboard
 *
 * @param aPopulateClipboardFn
 *        Function to populate the clipboard.
 * @param aFlavor
 *        Data flavor to expect.
 */
function promiseClipboard(aPopulateClipboardFn, aFlavor) {
  return new Promise(resolve => {
    waitForClipboard(data => !!data, aPopulateClipboardFn, resolve, aFlavor);
  });
}

/**
 * Waits for all pending async statements on the default connection, before
 * proceeding with aCallback.
 *
 * @param aCallback
 *        Function to be called when done.
 * @param aScope
 *        Scope for the callback.
 * @param aArguments
 *        Arguments array for the callback.
 *
 * @note The result is achieved by asynchronously executing a query requiring
 *       a write lock.  Since all statements on the same connection are
 *       serialized, the end of this write operation means that all writes are
 *       complete.  Note that WAL makes so that writers don't block readers, but
 *       this is a problem only across different connections.
 */
function waitForAsyncUpdates(aCallback, aScope, aArguments)
{
  let scope = aScope || this;
  let args = aArguments || [];
  let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                              .DBConnection;
  let begin = db.createAsyncStatement("BEGIN EXCLUSIVE");
  begin.executeAsync();
  begin.finalize();

  let commit = db.createAsyncStatement("COMMIT");
  commit.executeAsync({
    handleResult: function() {},
    handleError: function() {},
    handleCompletion: function(aReason)
    {
      aCallback.apply(scope, args);
    }
  });
  commit.finalize();
}

function synthesizeClickOnSelectedTreeCell(aTree, aOptions) {
  let tbo = aTree.treeBoxObject;
  if (tbo.view.selection.count != 1)
     throw new Error("The test node should be successfully selected");
  // Get selection rowID.
  let min = {}, max = {};
  tbo.view.selection.getRangeAt(0, min, max);
  let rowID = min.value;
  tbo.ensureRowIsVisible(rowID);
  // Calculate the click coordinates.
  var rect = tbo.getCoordsForCellItem(rowID, aTree.columns[0], "text");
  var x = rect.x + rect.width / 2;
  var y = rect.y + rect.height / 2;
  // Simulate the click.
  EventUtils.synthesizeMouse(aTree.body, x, y, aOptions || {},
                             aTree.ownerGlobal);
}

/**
 * Asynchronously check a url is visited.
 *
 * @param aURI The URI.
 * @return {Promise}
 * @resolves When the check has been added successfully.
 * @rejects JavaScript exception.
 */
function promiseIsURIVisited(aURI) {
  let deferred = Promise.defer();

  PlacesUtils.asyncHistory.isURIVisited(aURI, function(aURI, aIsVisited) {
    deferred.resolve(aIsVisited);
  });

  return deferred.promise;
}

function promiseBookmarksNotification(notification, conditionFn) {
  info(`promiseBookmarksNotification: waiting for ${notification}`);
  return new Promise((resolve, reject) => {
    let proxifiedObserver = new Proxy({}, {
      get: (target, name) => {
        if (name == "QueryInterface")
          return XPCOMUtils.generateQI([ Ci.nsINavBookmarkObserver ]);
        info(`promiseBookmarksNotification: got ${name} notification`);
        if (name == notification)
          return (...args) => {
            if (conditionFn.apply(this, args)) {
              clearTimeout(timeout);
              PlacesUtils.bookmarks.removeObserver(proxifiedObserver, false);
              executeSoon(resolve);
            } else {
              info(`promiseBookmarksNotification: skip cause condition doesn't apply to ${JSON.stringify(args)}`);
            }
          }
        return () => {};
      }
    });
    PlacesUtils.bookmarks.addObserver(proxifiedObserver, false);
    let timeout = setTimeout(() => {
      PlacesUtils.bookmarks.removeObserver(proxifiedObserver, false);
      reject(new Error("Timed out while waiting for bookmarks notification"));
    }, 2000);
  });
}

function promiseHistoryNotification(notification, conditionFn) {
  info(`Waiting for ${notification}`);
  return new Promise((resolve, reject) => {
    let proxifiedObserver = new Proxy({}, {
      get: (target, name) => {
        if (name == "QueryInterface")
          return XPCOMUtils.generateQI([ Ci.nsINavHistoryObserver ]);
        if (name == notification)
          return (...args) => {
            if (conditionFn.apply(this, args)) {
              clearTimeout(timeout);
              PlacesUtils.history.removeObserver(proxifiedObserver, false);
              executeSoon(resolve);
            }
          }
        return () => {};
      }
    });
    PlacesUtils.history.addObserver(proxifiedObserver, false);
    let timeout = setTimeout(() => {
      PlacesUtils.history.removeObserver(proxifiedObserver, false);
      reject(new Error("Timed out while waiting for history notification"));
    }, 2000);
  });
}

/**
 * Makes the specified toolbar visible or invisible and returns a Promise object
 * that is resolved when the toolbar has completed any animations associated
 * with hiding or showing the toolbar.
 *
 * Note that this code assumes that changes to a toolbar's visibility trigger
 * a transition on the max-height property of the toolbar element.
 * Changes to this styling could cause the returned Promise object to be
 * resolved too early or not at all.
 *
 * @param aToolbar
 *        The toolbar to update.
 * @param aVisible
 *        True to make the toolbar visible, false to make it hidden.
 *
 * @return {Promise}
 * @resolves Any animation associated with updating the toolbar's visibility has
 *           finished.
 * @rejects Never.
 */
function promiseSetToolbarVisibility(aToolbar, aVisible, aCallback) {
  return new Promise((resolve, reject) => {
    function listener(event) {
      if (event.propertyName == "max-height") {
        aToolbar.removeEventListener("transitionend", listener);
        resolve();
      }
    }

    let transitionProperties =
      window.getComputedStyle(aToolbar).transitionProperty.split(", ");
    if (isToolbarVisible(aToolbar) != aVisible &&
        transitionProperties.some(
          prop => prop == "max-height" || prop == "all"
        )) {
      // Just because max-height is a transitionable property doesn't mean
      // a transition will be triggered, but it's more likely.
      aToolbar.addEventListener("transitionend", listener);
      setToolbarVisibility(aToolbar, aVisible);
      return;
    }

    // No animation to wait for
    setToolbarVisibility(aToolbar, aVisible);
    resolve();
  });
}

/**
 * Helper function to determine if the given toolbar is in the visible
 * state according to its autohide/collapsed attribute.
 *
 * @aToolbar The toolbar to query.
 *
 * @returns True if the relevant attribute on |aToolbar| indicates it is
 *          visible, false otherwise.
 */
function isToolbarVisible(aToolbar) {
  let hidingAttribute = aToolbar.getAttribute("type") == "menubar"
                        ? "autohide"
                        : "collapsed";
  let hidingValue = aToolbar.getAttribute(hidingAttribute).toLowerCase();
  // Check for both collapsed="true" and collapsed="collapsed"
  return hidingValue !== "true" && hidingValue !== hidingAttribute;
}

/**
 * Executes a task after opening the bookmarks dialog, then cancels the dialog.
 *
 * @param autoCancel
 *        whether to automatically cancel the dialog at the end of the task
 * @param openFn
 *        generator function causing the dialog to open
 * @param task
 *        the task to execute once the dialog is open
 */
var withBookmarksDialog = Task.async(function* (autoCancel, openFn, taskFn) {
  let closed = false;
  let dialogPromise = new Promise(resolve => {
    Services.ww.registerNotification(function winObserver(subject, topic, data) {
      if (topic == "domwindowopened") {
        let win = subject.QueryInterface(Ci.nsIDOMWindow);
        win.addEventListener("load", function load() {
          win.removeEventListener("load", load);
          ok(win.location.href.startsWith("chrome://browser/content/places/bookmarkProperties"),
             "The bookmark properties dialog is open");
          // This is needed for the overlay.
          waitForFocus(() => {
            resolve(win);
          }, win);
        });
      } else if (topic == "domwindowclosed") {
        Services.ww.unregisterNotification(winObserver);
        closed = true;
      }
    });
  });

  info("withBookmarksDialog: opening the dialog");
  // The dialog might be modal and could block our events loop, so executeSoon.
  executeSoon(openFn);

  info("withBookmarksDialog: waiting for the dialog");
  let dialogWin = yield dialogPromise;

  // Ensure overlay is loaded
  info("waiting for the overlay to be loaded");
  yield waitForCondition(() => dialogWin.gEditItemOverlay.initialized,
                         "EditItemOverlay should be initialized");

  // Check the first textbox is focused.
  let doc = dialogWin.document;
  let elt = doc.querySelector("textbox:not([collapsed=true])");
  if (elt) {
    info("waiting for focus on the first textfield");
    yield waitForCondition(() => doc.activeElement == elt.inputField,
                           "The first non collapsed textbox should have been focused");
  }

  info("withBookmarksDialog: executing the task");
  try {
    yield taskFn(dialogWin);
  } finally {
    if (!closed) {
      if (!autoCancel) {
        ok(false, "The test should have closed the dialog!");
      }
      info("withBookmarksDialog: canceling the dialog");
      doc.documentElement.cancelDialog();
    }
  }
});

/**
 * Opens the contextual menu on the element pointed by the given selector.
 *
 * @param selector
 *        Valid selector syntax
 * @return Promise
 *         Returns a Promise that resolves once the context menu has been
 *         opened.
 */
var openContextMenuForContentSelector = Task.async(function* (browser, selector) {
  info("wait for the context menu");
  let contextPromise = BrowserTestUtils.waitForEvent(document.getElementById("contentAreaContextMenu"),
                                                     "popupshown");
  yield ContentTask.spawn(browser, { selector }, function* (args) {
    let doc = content.document;
    let elt = doc.querySelector(args.selector)
    dump(`openContextMenuForContentSelector: found ${elt}\n`);

    /* Open context menu so chrome can access the element */
    const domWindowUtils =
      content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
             .getInterface(Components.interfaces.nsIDOMWindowUtils);
    let rect = elt.getBoundingClientRect();
    let left = rect.left + rect.width / 2;
    let top = rect.top + rect.height / 2;
    domWindowUtils.sendMouseEvent("contextmenu", left, top, 2,
                                  1, 0, false, 0, 0, true);
  });
  yield contextPromise;
});

/**
 * Waits for a specified condition to happen.
 *
 * @param conditionFn
 *        a Function or a generator function, returning a boolean for whether
 *        the condition is fulfilled.
 * @param errorMsg
 *        Error message to use if the condition has not been satisfied after a
 *        meaningful amount of tries.
 */
var waitForCondition = Task.async(function* (conditionFn, errorMsg) {
  for (let tries = 0; tries < 100; ++tries) {
    if ((yield conditionFn()))
      return;
    yield new Promise(resolve => {
      if (!waitForCondition._timers) {
        waitForCondition._timers = new Set();
        registerCleanupFunction(() => {
          is(waitForCondition._timers.size, 0, "All the wait timers have been removed");
          delete waitForCondition._timers;
        });
      }
      let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      waitForCondition._timers.add(timer);
      timer.init(() => {
        waitForCondition._timers.delete(timer);
        resolve();
      }, 100, Ci.nsITimer.TYPE_ONE_SHOT);
    });
  }
  ok(false, errorMsg);
});

/**
 * Fills a bookmarks dialog text field ensuring to cause expected edit events.
 *
 * @param id
 *        id of the text field
 * @param text
 *        text to fill in
 * @param win
 *        dialog window
 * @param [optional] blur
 *        whether to blur at the end.
 */
function fillBookmarkTextField(id, text, win, blur = true) {
  let elt = win.document.getElementById(id);
  elt.focus();
  elt.select();
  for (let c of text.split("")) {
    EventUtils.synthesizeKey(c, {}, win);
  }
  if (blur)
    elt.blur();
}

/**
 * Executes a task after opening the bookmarks or history sidebar. Takes care
 * of closing the sidebar once done.
 *
 * @param type
 *        either "bookmarks" or "history".
 * @param taskFn
 *        The task to execute once the sidebar is ready. Will get the Places
 *        tree view as input.
 */
var withSidebarTree = Task.async(function* (type, taskFn) {
  let sidebar = document.getElementById("sidebar");
  info("withSidebarTree: waiting sidebar load");
  let sidebarLoadedPromise = new Promise(resolve => {
    sidebar.addEventListener("load", function load() {
      sidebar.removeEventListener("load", load, true);
      resolve();
    }, true);
  });
  let sidebarId = type == "bookmarks" ? "viewBookmarksSidebar"
                                      : "viewHistorySidebar";
  SidebarUI.show(sidebarId);
  yield sidebarLoadedPromise;

  let treeId = type == "bookmarks" ? "bookmarks-view"
                                   : "historyTree";
  let tree = sidebar.contentDocument.getElementById(treeId);

  // Need to executeSoon since the tree is initialized on sidebar load.
  info("withSidebarTree: executing the task");
  try {
    yield taskFn(tree);
  } finally {
    SidebarUI.hide();
  }
});

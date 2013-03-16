Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

function whenDelayedStartupFinished(aWindow, aCallback) {
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      executeSoon(aCallback);
    }
  }, "browser-delayed-startup-finished", false);
}

function findChromeWindowByURI(aURI) {
  let windows = Services.wm.getEnumerator(null);
  while (windows.hasMoreElements()) {
    let win = windows.getNext();
    if (win.location.href == aURI)
      return win;
  }
  return null;
}

function updateTabContextMenu(tab) {
  let menu = document.getElementById("tabContextMenu");
  if (!tab)
    tab = gBrowser.selectedTab;
  var evt = new Event("");
  tab.dispatchEvent(evt);
  menu.openPopup(tab, "end_after", 0, 0, true, false, evt);
  is(TabContextMenu.contextTab, tab, "TabContextMenu context is the expected tab");
  menu.hidePopup();
}

function findToolbarCustomizationWindow(aBrowserWin) {
  if (!aBrowserWin)
    aBrowserWin = window;

  let iframe = aBrowserWin.document.getElementById("customizeToolbarSheetIFrame");
  let win = iframe && iframe.contentWindow;
  if (win)
    return win;

  win = findChromeWindowByURI("chrome://global/content/customizeToolbar.xul");
  if (win && win.opener == aBrowserWin)
    return win;

  throw Error("Failed to find the customization window");
}

function openToolbarCustomizationUI(aCallback, aBrowserWin) {
  if (!aBrowserWin)
    aBrowserWin = window;

  aBrowserWin.document.getElementById("cmd_CustomizeToolbars").doCommand();

  aBrowserWin.gNavToolbox.addEventListener("beforecustomization", function UI_loaded() {
    aBrowserWin.gNavToolbox.removeEventListener("beforecustomization", UI_loaded);

    let win = findToolbarCustomizationWindow(aBrowserWin);
    waitForFocus(function () {
      aCallback(win);
    }, win);
  });
}

function closeToolbarCustomizationUI(aCallback, aBrowserWin) {
  let win = findToolbarCustomizationWindow(aBrowserWin);

  win.addEventListener("unload", function unloaded() {
    win.removeEventListener("unload", unloaded);
    executeSoon(aCallback);
  });

  let button = win.document.getElementById("donebutton");
  button.focus();
  button.doCommand();
}

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    if (condition()) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

function getTestPlugin() {
  var ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  var tags = ph.getPluginTags();

  // Find the test plugin
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == "Test Plug-in")
      return tags[i];
  }
  ok(false, "Unable to find plugin");
  return null;
}

function updateBlocklist(aCallback) {
  var blocklistNotifier = Cc["@mozilla.org/extensions/blocklist;1"]
                          .getService(Ci.nsITimerCallback);
  var observer = function() {
    aCallback();
    Services.obs.removeObserver(observer, "blocklist-updated");
  };
  Services.obs.addObserver(observer, "blocklist-updated", false);
  blocklistNotifier.notify(null);
}

var _originalTestBlocklistURL = null;
function setAndUpdateBlocklist(aURL, aCallback) {
  if (!_originalTestBlocklistURL)
    _originalTestBlocklistURL = Services.prefs.getCharPref("extensions.blocklist.url");
  Services.prefs.setCharPref("extensions.blocklist.url", aURL);
  updateBlocklist(aCallback);
}

function resetBlocklist() {
  Services.prefs.setCharPref("extensions.blocklist.url", _originalTestBlocklistURL);
}

function whenNewWindowLoaded(aOptions, aCallback) {
  let win = OpenBrowserWindow(aOptions);
  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    aCallback(win);
  }, false);
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
function waitForAsyncUpdates(aCallback, aScope, aArguments) {
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
    handleCompletion: function(aReason) {
      aCallback.apply(scope, args);
    }
  });
  commit.finalize();
}

/**
 * Asynchronously check a url is visited.

 * @param aURI The URI.
 * @param aExpectedValue The expected value.
 * @return {Promise}
 * @resolves When the check has been added successfully.
 * @rejects JavaScript exception.
 */
function promiseIsURIVisited(aURI, aExpectedValue) {
  let deferred = Promise.defer();
  PlacesUtils.asyncHistory.isURIVisited(aURI, function(aURI, aIsVisited) {
    deferred.resolve(aIsVisited);
  });

  return deferred.promise;
}

function addVisits(aPlaceInfo, aCallback) {
  let places = [];
  if (aPlaceInfo instanceof Ci.nsIURI) {
    places.push({ uri: aPlaceInfo });
  } else if (Array.isArray(aPlaceInfo)) {
    places = places.concat(aPlaceInfo);
  } else {
    places.push(aPlaceInfo);
   }

  // Create mozIVisitInfo for each entry.
  let now = Date.now();
  for (let i = 0; i < places.length; i++) {
    if (!places[i].title) {
      places[i].title = "test visit for " + places[i].uri.spec;
    }
    places[i].visits = [{
      transitionType: places[i].transition === undefined ? Ci.nsINavHistoryService.TRANSITION_LINK
                                                         : places[i].transition,
      visitDate: places[i].visitDate || (now++) * 1000,
      referrerURI: places[i].referrer
    }];
  }

  PlacesUtils.asyncHistory.updatePlaces(
    places,
    {
      handleError: function AAV_handleError() {
        throw("Unexpected error in adding visit.");
      },
      handleResult: function () {},
      handleCompletion: function UP_handleCompletion() {
        if (aCallback)
          aCallback();
      }
    }
  );
}

/**
 * Ensures that the specified URIs are either cleared or not.
 *
 * @param aURIs
 *        Array of page URIs
 * @param aShouldBeCleared
 *        True if each visit to the URI should be cleared, false otherwise
 */
function promiseHistoryClearedState(aURIs, aShouldBeCleared) {
  let deferred = Promise.defer();
  let callbackCount = 0;
  let niceStr = aShouldBeCleared ? "no longer" : "still";
  function callbackDone() {
    if (++callbackCount == aURIs.length)
      deferred.resolve();
  }
  aURIs.forEach(function (aURI) {
    PlacesUtils.asyncHistory.isURIVisited(aURI, function(aURI, aIsVisited) {
      is(aIsVisited, !aShouldBeCleared,
         "history visit " + aURI.spec + " should " + niceStr + " exist");
      callbackDone();
    });
  });

  return deferred.promise;
}

let FullZoomHelper = {

  selectTabAndWaitForLocationChange: function selectTabAndWaitForLocationChange(tab) {
    let deferred = Promise.defer();
    if (tab && gBrowser.selectedTab == tab) {
      deferred.resolve();
      return deferred.promise;
    }
    if (tab)
      gBrowser.selectedTab = tab;
    Services.obs.addObserver(function obs() {
      Services.obs.removeObserver(obs, "browser-fullZoom:locationChange");
      deferred.resolve();
    }, "browser-fullZoom:locationChange", false);
    return deferred.promise;
  },

  load: function load(tab, url) {
    let deferred = Promise.defer();
    let didLoad = false;
    let didZoom = false;

    tab.linkedBrowser.addEventListener("load", function (event) {
      event.currentTarget.removeEventListener("load", arguments.callee, true);
      didLoad = true;
      if (didZoom)
        deferred.resolve();
    }, true);

    // Don't select background tabs.  That way tests can use this method on
    // background tabs without having them automatically be selected.  Just wait
    // for the zoom to change on the current tab if it's `tab`.
    if (tab == gBrowser.selectedTab) {
      this.selectTabAndWaitForLocationChange(null).then(function () {
        didZoom = true;
        if (didLoad)
          deferred.resolve();
      });
    }
    else
      didZoom = true;

    tab.linkedBrowser.loadURI(url);

    return deferred.promise;
  },

  zoomTest: function zoomTest(tab, val, msg) {
    is(ZoomManager.getZoomForBrowser(tab.linkedBrowser), val, msg);
  },

  enlarge: function enlarge() {
    let deferred = Promise.defer();
    FullZoom.enlarge(function () deferred.resolve());
    return deferred.promise;
  },

  reduce: function reduce() {
    let deferred = Promise.defer();
    FullZoom.reduce(function () deferred.resolve());
    return deferred.promise;
  },

  reset: function reset() {
    let deferred = Promise.defer();
    FullZoom.reset(function () deferred.resolve());
    return deferred.promise;
  },

  BACK: 0,
  FORWARD: 1,
  navigate: function navigate(direction) {
    let deferred = Promise.defer();
    let didPs = false;
    let didZoom = false;

    gBrowser.addEventListener("pageshow", function (event) {
      gBrowser.removeEventListener("pageshow", arguments.callee, true);
      didPs = true;
      if (didZoom)
        deferred.resolve();
    }, true);

    if (direction == this.BACK)
      gBrowser.goBack();
    else if (direction == this.FORWARD)
      gBrowser.goForward();

    this.selectTabAndWaitForLocationChange(null).then(function () {
      didZoom = true;
      if (didPs)
        deferred.resolve();
    });
    return deferred.promise;
  },

  failAndContinue: function failAndContinue(func) {
    return function (err) {
      ok(false, err);
      func();
    };
  },
};

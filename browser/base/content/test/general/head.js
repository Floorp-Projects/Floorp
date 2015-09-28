Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");

/**
 * Wait for a <notification> to be closed then call the specified callback.
 */
function waitForNotificationClose(notification, cb) {
  let parent = notification.parentNode;

  let observer = new MutationObserver(function onMutatations(mutations) {
    for (let mutation of mutations) {
      for (let i = 0; i < mutation.removedNodes.length; i++) {
        let node = mutation.removedNodes.item(i);
        if (node != notification) {
          continue;
        }
        observer.disconnect();
        cb();
      }
    }
  });
  observer.observe(parent, {childList: true});
}

function closeAllNotifications () {
  let notificationBox = document.getElementById("global-notificationbox");

  if (!notificationBox || !notificationBox.currentNotification) {
    return Promise.resolve();
  }

  let deferred = Promise.defer();
  for (let notification of notificationBox.allNotifications) {
    waitForNotificationClose(notification, function () {
      if (notificationBox.allNotifications.length === 0) {
        deferred.resolve();
      }
    });
    notification.close();
  }

  return deferred.promise;
}

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

function openToolbarCustomizationUI(aCallback, aBrowserWin) {
  if (!aBrowserWin)
    aBrowserWin = window;

  aBrowserWin.gCustomizeMode.enter();

  aBrowserWin.gNavToolbox.addEventListener("customizationready", function UI_loaded() {
    aBrowserWin.gNavToolbox.removeEventListener("customizationready", UI_loaded);
    executeSoon(function() {
      aCallback(aBrowserWin)
    });
  });
}

function closeToolbarCustomizationUI(aCallback, aBrowserWin) {
  aBrowserWin.gNavToolbox.addEventListener("aftercustomization", function unloaded() {
    aBrowserWin.gNavToolbox.removeEventListener("aftercustomization", unloaded);
    executeSoon(aCallback);
  });

  aBrowserWin.gCustomizeMode.exit();
}

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

function promiseWaitForCondition(aConditionFn) {
  let deferred = Promise.defer();
  waitForCondition(aConditionFn, deferred.resolve, "Condition didn't pass.");
  return deferred.promise;
}

function promiseWaitForEvent(object, eventName, capturing = false, chrome = false) {
  return new Promise((resolve) => {
    function listener(event) {
      info("Saw " + eventName);
      object.removeEventListener(eventName, listener, capturing, chrome);
      resolve(event);
    }

    info("Waiting for " + eventName);
    object.addEventListener(eventName, listener, capturing, chrome);
  });
}

/**
 * Allows setting focus on a window, and waiting for that window to achieve
 * focus.
 *
 * @param aWindow
 *        The window to focus and wait for.
 *
 * @return {Promise}
 * @resolves When the window is focused.
 * @rejects Never.
 */
function promiseWaitForFocus(aWindow) {
  return new Promise((resolve) => {
    waitForFocus(resolve, aWindow);
  });
}

function getTestPlugin(aName) {
  var pluginName = aName || "Test Plug-in";
  var ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  var tags = ph.getPluginTags();

  // Find the test plugin
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == pluginName)
      return tags[i];
  }
  ok(false, "Unable to find plugin");
  return null;
}

// call this to set the test plugin(s) initially expected enabled state.
// it will automatically be reset to it's previous value after the test
// ends
function setTestPluginEnabledState(newEnabledState, pluginName) {
  var plugin = getTestPlugin(pluginName);
  var oldEnabledState = plugin.enabledState;
  plugin.enabledState = newEnabledState;
  SimpleTest.registerCleanupFunction(function() {
    getTestPlugin(pluginName).enabledState = oldEnabledState;
  });
}

// after a test is done using the plugin doorhanger, we should just clear
// any permissions that may have crept in
function clearAllPluginPermissions() {
  clearAllPermissionsByPrefix("plugin");
}

function clearAllPermissionsByPrefix(aPrefix) {
  let perms = Services.perms.enumerator;
  while (perms.hasMoreElements()) {
    let perm = perms.getNext();
    if (perm.type.startsWith(aPrefix)) {
      Services.perms.removePermission(perm);
    }
  }
}

function pushPrefs(...aPrefs) {
  let deferred = Promise.defer();
  SpecialPowers.pushPrefEnv({"set": aPrefs}, deferred.resolve);
  return deferred.promise;
}

function updateBlocklist(aCallback) {
  var blocklistNotifier = Cc["@mozilla.org/extensions/blocklist;1"]
                          .getService(Ci.nsITimerCallback);
  var observer = function() {
    Services.obs.removeObserver(observer, "blocklist-updated");
    SimpleTest.executeSoon(aCallback);
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

function promiseWindowWillBeClosed(win) {
  return new Promise((resolve, reject) => {
    Services.obs.addObserver(function observe(subject, topic) {
      if (subject == win) {
        Services.obs.removeObserver(observe, topic);
        resolve();
      }
    }, "domwindowclosed", false);
  });
}

function promiseWindowClosed(win) {
  let promise = promiseWindowWillBeClosed(win);
  win.close();
  return promise;
}

function promiseOpenAndLoadWindow(aOptions, aWaitForDelayedStartup=false) {
  let deferred = Promise.defer();
  let win = OpenBrowserWindow(aOptions);
  if (aWaitForDelayedStartup) {
    Services.obs.addObserver(function onDS(aSubject, aTopic, aData) {
      if (aSubject != win) {
        return;
      }
      Services.obs.removeObserver(onDS, "browser-delayed-startup-finished");
      deferred.resolve(win);
    }, "browser-delayed-startup-finished", false);

  } else {
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad);
      deferred.resolve(win);
    });
  }
  return deferred.promise;
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

function whenNewTabLoaded(aWindow, aCallback) {
  aWindow.BrowserOpenTab();

  let browser = aWindow.gBrowser.selectedBrowser;
  if (browser.contentDocument.readyState === "complete") {
    aCallback();
    return;
  }

  whenTabLoaded(aWindow.gBrowser.selectedTab, aCallback);
}

function whenTabLoaded(aTab, aCallback) {
  promiseTabLoadEvent(aTab).then(aCallback);
}

function promiseTabLoaded(aTab) {
  let deferred = Promise.defer();
  whenTabLoaded(aTab, deferred.resolve);
  return deferred.promise;
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

/**
 * Waits for the next top-level document load in the current browser.  The URI
 * of the document is compared against aExpectedURL.  The load is then stopped
 * before it actually starts.
 *
 * @param aExpectedURL
 *        The URL of the document that is expected to load.
 * @return promise
 */
function waitForDocLoadAndStopIt(aExpectedURL, aBrowser=gBrowser.selectedBrowser) {
  function content_script() {
    let { interfaces: Ci, utils: Cu } = Components;
    Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
    let wp = docShell.QueryInterface(Ci.nsIWebProgress);

    let progressListener = {
      onStateChange: function (webProgress, req, flags, status) {
        dump("waitForDocLoadAndStopIt: onStateChange " + flags.toString(16) + ": " + req.name + "\n");

        if (webProgress.isTopLevel &&
            flags & Ci.nsIWebProgressListener.STATE_START) {
          wp.removeProgressListener(progressListener);

          let chan = req.QueryInterface(Ci.nsIChannel);
          dump(`waitForDocLoadAndStopIt: Document start: ${chan.URI.spec}\n`);

          /* Hammer time. */
          content.stop();

          /* Let the parent know we're done. */
          sendAsyncMessage("Test:WaitForDocLoadAndStopIt", { uri: chan.originalURI.spec });
        }
      },
      QueryInterface: XPCOMUtils.generateQI(["nsISupportsWeakReference"])
    };
    wp.addProgressListener(progressListener, wp.NOTIFY_STATE_WINDOW);

    /**
     * As |this| is undefined and we can't extend |docShell|, adding an unload
     * event handler is the easiest way to ensure the weakly referenced
     * progress listener is kept alive as long as necessary.
     */
    addEventListener("unload", function () {
      try {
        wp.removeProgressListener(progressListener);
      } catch (e) { /* Will most likely fail. */ }
    });
  }

  return new Promise((resolve, reject) => {
    function complete({ data }) {
      is(data.uri, aExpectedURL, "waitForDocLoadAndStopIt: The expected URL was loaded");
      mm.removeMessageListener("Test:WaitForDocLoadAndStopIt", complete);
      resolve();
    }

    let mm = aBrowser.messageManager;
    mm.loadFrameScript("data:,(" + content_script.toString() + ")();", true);
    mm.addMessageListener("Test:WaitForDocLoadAndStopIt", complete);
    info("waitForDocLoadAndStopIt: Waiting for URL: " + aExpectedURL);
  });
}

/**
 * Waits for the next load to complete in any browser or the given browser.
 * If a <tabbrowser> is given it waits for a load in any of its browsers.
 *
 * @return promise
 */
function waitForDocLoadComplete(aBrowser=gBrowser) {
  return new Promise(resolve => {
    let listener = {
      onStateChange: function (webProgress, req, flags, status) {
        let docStop = Ci.nsIWebProgressListener.STATE_IS_NETWORK |
                      Ci.nsIWebProgressListener.STATE_STOP;
        info("Saw state " + flags.toString(16) + " and status " + status.toString(16));

        // When a load needs to be retargetted to a new process it is cancelled
        // with NS_BINDING_ABORTED so ignore that case
        if ((flags & docStop) == docStop && status != Cr.NS_BINDING_ABORTED) {
          aBrowser.removeProgressListener(this);
          waitForDocLoadComplete.listeners.delete(this);

          let chan = req.QueryInterface(Ci.nsIChannel);
          info("Browser loaded " + chan.originalURI.spec);
          resolve();
        }
      },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                             Ci.nsISupportsWeakReference])
    };
    aBrowser.addProgressListener(listener);
    waitForDocLoadComplete.listeners.add(listener);
    info("Waiting for browser load");
  });
}

// Keep a set of progress listeners for waitForDocLoadComplete() to make sure
// they're not GC'ed before we saw the page load.
waitForDocLoadComplete.listeners = new Set();
registerCleanupFunction(() => waitForDocLoadComplete.listeners.clear());

var FullZoomHelper = {

  selectTabAndWaitForLocationChange: function selectTabAndWaitForLocationChange(tab) {
    if (!tab)
      throw new Error("tab must be given.");
    if (gBrowser.selectedTab == tab)
      return Promise.resolve();

    return Promise.all([BrowserTestUtils.switchTab(gBrowser, tab),
                        this.waitForLocationChange()]);
  },

  removeTabAndWaitForLocationChange: function removeTabAndWaitForLocationChange(tab) {
    tab = tab || gBrowser.selectedTab;
    let selected = gBrowser.selectedTab == tab;
    gBrowser.removeTab(tab);
    if (selected)
      return this.waitForLocationChange();
    return Promise.resolve();
  },

  waitForLocationChange: function waitForLocationChange() {
    return new Promise(resolve => {
      Services.obs.addObserver(function obs(subj, topic, data) {
        Services.obs.removeObserver(obs, topic);
        resolve();
      }, "browser-fullZoom:location-change", false);
    });
  },

  load: function load(tab, url) {
    return new Promise(resolve => {
      let didLoad = false;
      let didZoom = false;

      promiseTabLoadEvent(tab).then(event => {
        didLoad = true;
        if (didZoom)
          resolve();
      }, true);

      this.waitForLocationChange().then(function () {
        didZoom = true;
        if (didLoad)
          resolve();
      });

      tab.linkedBrowser.loadURI(url);
    });
  },

  zoomTest: function zoomTest(tab, val, msg) {
    is(ZoomManager.getZoomForBrowser(tab.linkedBrowser), val, msg);
  },

  enlarge: function enlarge() {
    return new Promise(resolve => FullZoom.enlarge(resolve));
  },

  reduce: function reduce() {
    return new Promise(resolve => FullZoom.reduce(resolve));
  },

  reset: function reset() {
    return new Promise(resolve => FullZoom.reset(resolve));
  },

  BACK: 0,
  FORWARD: 1,
  navigate: function navigate(direction) {
    return new Promise(resolve => {
      let didPs = false;
      let didZoom = false;

      gBrowser.addEventListener("pageshow", function listener(event) {
        gBrowser.removeEventListener("pageshow", listener, true);
        didPs = true;
        if (didZoom)
          resolve();
      }, true);

      if (direction == this.BACK)
        gBrowser.goBack();
      else if (direction == this.FORWARD)
        gBrowser.goForward();

      this.waitForLocationChange().then(function () {
        didZoom = true;
        if (didPs)
          resolve();
      });
    });
  },

  failAndContinue: function failAndContinue(func) {
    return function (err) {
      ok(false, err);
      func();
    };
  },
};

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param tab
 *        The tab to load into.
 * @param [optional] url
 *        The url to load, or the current url.
 * @param [optional] event
 *        The load event type to wait for.  Defaults to "load".
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url, eventType="load")
{
  let deferred = Promise.defer();
  info("Wait tab event: " + eventType);

  function handle(event) {
    if (event.originalTarget != tab.linkedBrowser.contentDocument ||
        event.target.location.href == "about:blank" ||
        (url && event.target.location.href != url)) {
      info("Skipping spurious '" + eventType + "'' event" +
           " for " + event.target.location.href);
      return;
    }
    clearTimeout(timeout);
    tab.linkedBrowser.removeEventListener(eventType, handle, true);
    info("Tab event received: " + eventType);
    deferred.resolve(event);
  }

  let timeout = setTimeout(() => {
    tab.linkedBrowser.removeEventListener(eventType, handle, true);
    deferred.reject(new Error("Timed out while waiting for a '" + eventType + "'' event"));
  }, 30000);

  tab.linkedBrowser.addEventListener(eventType, handle, true, true);
  if (url)
    tab.linkedBrowser.loadURI(url);
  return deferred.promise;
}

/**
 * Returns a Promise that resolves once a new tab has been opened in
 * a xul:tabbrowser.
 *
 * @param aTabBrowser
 *        The xul:tabbrowser to monitor for a new tab.
 * @return {Promise}
 *        Resolved when the new tab has been opened.
 * @resolves to the TabOpen event that was fired.
 * @rejects Never.
 */
function waitForNewTabEvent(aTabBrowser) {
  return promiseWaitForEvent(aTabBrowser.tabContainer, "TabOpen");
}

/**
 * Waits for a window with the given URL to exist.
 *
 * @param url
 *        The url of the window.
 * @return {Promise} resolved when the window exists.
 * @resolves to the window
 */
function promiseWindow(url) {
  info("expecting a " + url + " window");
  return new Promise(resolve => {
    Services.obs.addObserver(function obs(win) {
      win.QueryInterface(Ci.nsIDOMWindow);
      win.addEventListener("load", function loadHandler() {
        win.removeEventListener("load", loadHandler);

        if (win.location.href !== url) {
          info("ignoring a window with this url: " + win.location.href);
          return;
        }

        Services.obs.removeObserver(obs, "domwindowopened");
        resolve(win);
      });
    }, "domwindowopened", false);
  });
}

function promiseIndicatorWindow() {
  // We don't show the indicator window on Mac.
  if ("nsISystemStatusBar" in Ci)
    return Promise.resolve();

  return promiseWindow("chrome://browser/content/webrtcIndicator.xul");
}

function assertWebRTCIndicatorStatus(expected) {
  let ui = Cu.import("resource:///modules/webrtcUI.jsm", {}).webrtcUI;
  let expectedState = expected ? "visible" : "hidden";
  let msg = "WebRTC indicator " + expectedState;
  if (!expected && ui.showGlobalIndicator) {
    // It seems the global indicator is not always removed synchronously
    // in some cases.
    info("waiting for the global indicator to be hidden");
    yield promiseWaitForCondition(() => !ui.showGlobalIndicator);
  }
  is(ui.showGlobalIndicator, !!expected, msg);

  let expectVideo = false, expectAudio = false, expectScreen = false;
  if (expected) {
    if (expected.video)
      expectVideo = true;
    if (expected.audio)
      expectAudio = true;
    if (expected.screen)
      expectScreen = true;
  }
  is(ui.showCameraIndicator, expectVideo, "camera global indicator as expected");
  is(ui.showMicrophoneIndicator, expectAudio, "microphone global indicator as expected");
  is(ui.showScreenSharingIndicator, expectScreen, "screen global indicator as expected");

  let windows = Services.wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements()) {
    let win = windows.getNext();
    let menu = win.document.getElementById("tabSharingMenu");
    is(menu && !menu.hidden, !!expected, "WebRTC menu should be " + expectedState);
  }

  if (!("nsISystemStatusBar" in Ci)) {
    if (!expected) {
      let win = Services.wm.getMostRecentWindow("Browser:WebRTCGlobalIndicator");
      if (win) {
        yield new Promise((resolve, reject) => {
          win.addEventListener("unload", function listener(e) {
            if (e.target == win.document) {
              win.removeEventListener("unload", listener);
              resolve();
            }
          }, false);
        });
      }
    }
    let indicator = Services.wm.getEnumerator("Browser:WebRTCGlobalIndicator");
    let hasWindow = indicator.hasMoreElements();
    is(hasWindow, !!expected, "popup " + msg);
    if (hasWindow) {
      let document = indicator.getNext().document;
      let docElt = document.documentElement;

      if (document.readyState != "complete") {
        info("Waiting for the sharing indicator's document to load");
        let deferred = Promise.defer();
        document.addEventListener("readystatechange",
                                  function onReadyStateChange() {
          if (document.readyState != "complete")
            return;
          document.removeEventListener("readystatechange", onReadyStateChange);
          deferred.resolve();
        });
        yield deferred.promise;
      }

      for (let item of ["video", "audio", "screen"]) {
        let expectedValue = (expected && expected[item]) ? "true" : "";
        is(docElt.getAttribute("sharing" + item), expectedValue,
           item + " global indicator attribute as expected");
      }

      ok(!indicator.hasMoreElements(), "only one global indicator window");
    }
  }
}

/**
 * Test the state of the identity box and control center to make
 * sure they are correctly showing the expected mixed content states.
 *
 * @param tabbrowser
 * @param Object states
 *        MUST include the following properties:
 *        {
 *           activeLoaded: true|false,
 *           activeBlocked: true|false,
 *           passiveLoaded: true|false,
 *        }
 */
function assertMixedContentBlockingState(tabbrowser, states = {}) {
  if (!tabbrowser || !("activeLoaded" in states) ||
      !("activeBlocked" in states) || !("passiveLoaded" in states))  {
    throw new Error("assertMixedContentBlockingState requires a browser and a states object");
  }

  let {passiveLoaded,activeLoaded,activeBlocked} = states;
  let {gIdentityHandler} = tabbrowser.ownerGlobal;
  let doc = tabbrowser.ownerDocument;
  let identityBox = gIdentityHandler._identityBox;
  let classList = identityBox.classList;
  let identityBoxImage = tabbrowser.ownerGlobal.getComputedStyle(doc.getElementById("page-proxy-favicon"), "").
                         getPropertyValue("list-style-image");

  let stateSecure = gIdentityHandler._state & Ci.nsIWebProgressListener.STATE_IS_SECURE;
  let stateBroken = gIdentityHandler._state & Ci.nsIWebProgressListener.STATE_IS_BROKEN;
  let stateInsecure = gIdentityHandler._state & Ci.nsIWebProgressListener.STATE_IS_INSECURE;
  let stateActiveBlocked = gIdentityHandler._state & Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT;
  let stateActiveLoaded = gIdentityHandler._state & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT;
  let statePassiveLoaded = gIdentityHandler._state & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_DISPLAY_CONTENT;

  is(activeBlocked, !!stateActiveBlocked, "Expected state for activeBlocked matches UI state");
  is(activeLoaded, !!stateActiveLoaded, "Expected state for activeLoaded matches UI state");
  is(passiveLoaded, !!statePassiveLoaded, "Expected state for passiveLoaded matches UI state");

  if (stateInsecure) {
    // HTTP request, there should be no MCB classes for the identity box and the non secure icon
    // should always be visible regardless of MCB state.
    ok(classList.contains("unknownIdentity"), "unknownIdentity on HTTP page");
    is(identityBoxImage, "url(\"chrome://browser/skin/identity-not-secure.svg\")", "Using 'non-secure' icon");

    ok(!classList.contains("mixedActiveContent"), "No MCB icon on HTTP page");
    ok(!classList.contains("mixedActiveBlocked"), "No MCB icon on HTTP page");
    ok(!classList.contains("mixedDisplayContent"), "No MCB icon on HTTP page");
    ok(!classList.contains("mixedDisplayContentLoadedActiveBlocked"), "No MCB icon on HTTP page");
  } else {
    // Make sure the identity box UI has the correct mixedcontent states and icons
    is(classList.contains("mixedActiveContent"), activeLoaded,
        "identityBox has expected class for activeLoaded");
    is(classList.contains("mixedActiveBlocked"), activeBlocked && !passiveLoaded,
        "identityBox has expected class for activeBlocked && !passiveLoaded");
    is(classList.contains("mixedDisplayContent"), passiveLoaded && !(activeLoaded || activeBlocked),
       "identityBox has expected class for passiveLoaded && !(activeLoaded || activeBlocked)");
    is(classList.contains("mixedDisplayContentLoadedActiveBlocked"), passiveLoaded && activeBlocked,
       "identityBox has expected class for passiveLoaded && activeBlocked");

    if (activeLoaded) {
      is(identityBoxImage, "url(\"chrome://browser/skin/identity-mixed-active-loaded.svg\")",
        "Using active loaded icon");
    }
    if (activeBlocked && !passiveLoaded) {
      is(identityBoxImage, "url(\"chrome://browser/skin/identity-mixed-active-blocked.svg\")",
        "Using active blocked icon");
    }
    if (passiveLoaded && !(activeLoaded || activeBlocked)) {
      is(identityBoxImage, "url(\"chrome://browser/skin/identity-mixed-passive-loaded.svg\")",
        "Using passive loaded icon");
    }
    if (passiveLoaded && activeBlocked) {
      is(identityBoxImage, "url(\"chrome://browser/skin/identity-mixed-passive-loaded.svg\")",
        "Using active blocked and passive loaded icon");
    }
  }

  // Make sure the identity popup has the correct mixedcontent states
  gIdentityHandler._identityBox.click();
  let popupAttr = doc.getElementById("identity-popup").getAttribute("mixedcontent");
  let bodyAttr = doc.getElementById("identity-popup-securityView-body").getAttribute("mixedcontent");

  is(popupAttr.includes("active-loaded"), activeLoaded,
      "identity-popup has expected attr for activeLoaded");
  is(bodyAttr.includes("active-loaded"), activeLoaded,
      "securityView-body has expected attr for activeLoaded");

  is(popupAttr.includes("active-blocked"), activeBlocked,
      "identity-popup has expected attr for activeBlocked");
  is(bodyAttr.includes("active-blocked"), activeBlocked,
      "securityView-body has expected attr for activeBlocked");

  is(popupAttr.includes("passive-loaded"), passiveLoaded,
      "identity-popup has expected attr for passiveLoaded");
  is(bodyAttr.includes("passive-loaded"), passiveLoaded,
      "securityView-body has expected attr for passiveLoaded");

  // Make sure the correct icon is visible in the Control Center.
  // This logic is controlled with CSS, so this helps prevent regressions there.
  let securityView = doc.getElementById("identity-popup-securityView");
  let securityContent = doc.getElementById("identity-popup-security-content");
  let securityViewBG = tabbrowser.ownerGlobal.getComputedStyle(securityView, "").
                       getPropertyValue("background-image");
  let securityContentBG = tabbrowser.ownerGlobal.getComputedStyle(securityView, "").
                          getPropertyValue("background-image");

  if (stateInsecure) {
    is(securityViewBG, "url(\"chrome://browser/skin/controlcenter/conn-not-secure.svg\")",
      "CC using 'not secure' icon");
    is(securityContentBG, "url(\"chrome://browser/skin/controlcenter/conn-not-secure.svg\")",
      "CC using 'not secure' icon");
  }

  if (stateSecure) {
    is(securityViewBG, "url(\"chrome://browser/skin/controlcenter/conn-secure.svg\")",
      "CC using secure icon");
    is(securityContentBG, "url(\"chrome://browser/skin/controlcenter/conn-secure.svg\")",
      "CC using secure icon");
  }

  if (stateBroken) {
    if (activeLoaded) {
      is(securityViewBG, "url(\"chrome://browser/skin/controlcenter/mcb-disabled.svg\")",
        "CC using active loaded icon");
      is(securityContentBG, "url(\"chrome://browser/skin/controlcenter/mcb-disabled.svg\")",
        "CC using active loaded icon");
    } else if (activeBlocked || passiveLoaded) {
      is(securityViewBG, "url(\"chrome://browser/skin/controlcenter/conn-degraded.svg\")",
        "CC using degraded icon");
      is(securityContentBG, "url(\"chrome://browser/skin/controlcenter/conn-degraded.svg\")",
        "CC using degraded icon");
    } else {
      // There is a case here with weak ciphers, but no bc tests are handling this yet.
      is(securityViewBG, "url(\"chrome://browser/skin/controlcenter/conn-degraded.svg\")",
        "CC using degraded icon");
      is(securityContentBG, "url(\"chrome://browser/skin/controlcenter/conn-degraded.svg\")",
        "CC using degraded icon");
    }
  }

  gIdentityHandler._identityPopup.hidden = true;
}

function makeActionURI(action, params) {
  let url = "moz-action:" + action + "," + JSON.stringify(params);
  return NetUtil.newURI(url);
}

function is_hidden(element) {
  var style = element.ownerDocument.defaultView.getComputedStyle(element, "");
  if (style.display == "none")
    return true;
  if (style.visibility != "visible")
    return true;
  if (style.display == "-moz-popup")
    return ["hiding","closed"].indexOf(element.state) != -1;

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_hidden(element.parentNode);

  return false;
}

function is_visible(element) {
  var style = element.ownerDocument.defaultView.getComputedStyle(element, "");
  if (style.display == "none")
    return false;
  if (style.visibility != "visible")
    return false;
  if (style.display == "-moz-popup" && element.state != "open")
    return false;

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_visible(element.parentNode);

  return true;
}

function is_element_visible(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(is_visible(element), msg);
}

function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(is_hidden(element), msg);
}

function promisePopupEvent(popup, eventSuffix) {
  let endState = {shown: "open", hidden: "closed"}[eventSuffix];

  if (popup.state == endState)
    return Promise.resolve();

  let eventType = "popup" + eventSuffix;
  let deferred = Promise.defer();
  popup.addEventListener(eventType, function onPopupShown(event) {
    popup.removeEventListener(eventType, onPopupShown);
    deferred.resolve();
  });

  return deferred.promise;
}

function promisePopupShown(popup) {
  return promisePopupEvent(popup, "shown");
}

function promisePopupHidden(popup) {
  return promisePopupEvent(popup, "hidden");
}

function promiseNotificationShown(notification) {
  let win = notification.browser.ownerDocument.defaultView;
  if (win.PopupNotifications.panel.state == "open") {
    return Promise.resolved();
  }
  let panelPromise = promisePopupShown(win.PopupNotifications.panel);
  notification.reshow();
  return panelPromise;
}

function promiseSearchComplete(win = window) {
  return promisePopupShown(win.gURLBar.popup).then(() => {
    function searchIsComplete() {
      return win.gURLBar.controller.searchStatus >=
        Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH;
    }

    // Wait until there are at least two matches.
    return new Promise(resolve => waitForCondition(searchIsComplete, resolve));
  });
}

function promiseAutocompleteResultPopup(inputText, win = window) {
  waitForFocus(() => {
    win.gURLBar.focus();
    win.gURLBar.value = inputText;
    win.gURLBar.controller.startSearch(inputText);
  }, win);

  return promiseSearchComplete(win);
}

/**
 * Allows waiting for an observer notification once.
 *
 * @param aTopic
 *        Notification topic to observe.
 *
 * @return {Promise}
 * @resolves An object with subject and data properties from the observed
 *           notification.
 * @rejects Never.
 */
function promiseTopicObserved(aTopic)
{
  return new Promise((resolve) => {
    Services.obs.addObserver(
      function PTO_observe(aSubject, aTopic, aData) {
        Services.obs.removeObserver(PTO_observe, aTopic);
        resolve({subject: aSubject, data: aData});
      }, aTopic, false);
  });
}

function promiseNewSearchEngine(basename) {
  return new Promise((resolve, reject) => {
    info("Waiting for engine to be added: " + basename);
    let url = getRootDirectory(gTestPath) + basename;
    Services.search.addEngine(url, null, "", false, {
      onSuccess: function (engine) {
        info("Search engine added: " + basename);
        registerCleanupFunction(() => Services.search.removeEngine(engine));
        resolve(engine);
      },
      onError: function (errCode) {
        Assert.ok(false, "addEngine failed with error code " + errCode);
        reject();
      },
    });
  });
}

// Compares the security state of the page with what is expected
function isSecurityState(expectedState) {
  let ui = gTestBrowser.securityUI;
  if (!ui) {
    ok(false, "No security UI to get the security state");
    return;
  }

  const wpl = Components.interfaces.nsIWebProgressListener;

  // determine the security state
  let isSecure = ui.state & wpl.STATE_IS_SECURE;
  let isBroken = ui.state & wpl.STATE_IS_BROKEN;
  let isInsecure = ui.state & wpl.STATE_IS_INSECURE;

  let actualState;
  if (isSecure && !(isBroken || isInsecure)) {
    actualState = "secure";
  } else if (isBroken && !(isSecure || isInsecure)) {
    actualState = "broken";
  } else if (isInsecure && !(isSecure || isBroken)) {
    actualState = "insecure";
  } else {
    actualState = "unknown";
  }

  is(expectedState, actualState, "Expected state " + expectedState + " and the actual state is " + actualState + ".");
}

/**
 * Resolves when a bookmark with the given uri is added.
 */
function promiseOnBookmarkItemAdded(aExpectedURI) {
  return new Promise((resolve, reject) => {
    let bookmarksObserver = {
      onItemAdded: function (aItemId, aFolderId, aIndex, aItemType, aURI) {
        info("Added a bookmark to " + aURI.spec);
        PlacesUtils.bookmarks.removeObserver(bookmarksObserver);
        if (aURI.equals(aExpectedURI)) {
          resolve();
        }
        else {
          reject(new Error("Added an unexpected bookmark"));
        }
      },
      onBeginUpdateBatch: function () {},
      onEndUpdateBatch: function () {},
      onItemRemoved: function () {},
      onItemChanged: function () {},
      onItemVisited: function () {},
      onItemMoved: function () {},
      QueryInterface: XPCOMUtils.generateQI([
        Ci.nsINavBookmarkObserver,
      ])
    };
    info("Waiting for a bookmark to be added");
    PlacesUtils.bookmarks.addObserver(bookmarksObserver, false);
  });
}

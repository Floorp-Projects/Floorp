Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TabCrashHandler",
  "resource:///modules/ContentCrashHandlers.jsm");

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

function updateTabContextMenu(tab, onOpened) {
  let menu = document.getElementById("tabContextMenu");
  if (!tab)
    tab = gBrowser.selectedTab;
  var evt = new Event("");
  tab.dispatchEvent(evt);
  menu.openPopup(tab, "end_after", 0, 0, true, false, evt);
  is(TabContextMenu.contextTab, tab, "TabContextMenu context is the expected tab");
  const onFinished = () => menu.hidePopup();
  if (onOpened) {
    return Task.spawn(function*() {
      yield onOpened();
      onFinished();
    });
  }
  onFinished();
  return Promise.resolve();
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

function waitForCondition(condition, nextTest, errorMsg, retryTimes) {
  retryTimes = typeof retryTimes !== 'undefined' ?  retryTimes : 30;
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= retryTimes) {
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
 * @param aStopFromProgressListener
 *        Whether to cancel the load directly from the progress listener. Defaults to true.
 *        If you're using this method to avoid hitting the network, you want the default (true).
 *        However, the browser UI will behave differently for loads stopped directly from
 *        the progress listener (effectively in the middle of a call to loadURI) and so there
 *        are cases where you may want to avoid stopping the load directly from within the
 *        progress listener callback.
 * @return promise
 */
function waitForDocLoadAndStopIt(aExpectedURL, aBrowser=gBrowser.selectedBrowser, aStopFromProgressListener=true) {
  function content_script(aStopFromProgressListener) {
    let { interfaces: Ci, utils: Cu } = Components;
    Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
    let wp = docShell.QueryInterface(Ci.nsIWebProgress);

    function stopContent(now, uri) {
      if (now) {
        /* Hammer time. */
        content.stop();

        /* Let the parent know we're done. */
        sendAsyncMessage("Test:WaitForDocLoadAndStopIt", { uri });
      } else {
        setTimeout(stopContent.bind(null, true, uri), 0);
      }
    }

    let progressListener = {
      onStateChange: function (webProgress, req, flags, status) {
        dump("waitForDocLoadAndStopIt: onStateChange " + flags.toString(16) + ": " + req.name + "\n");

        if (webProgress.isTopLevel &&
            flags & Ci.nsIWebProgressListener.STATE_START) {
          wp.removeProgressListener(progressListener);

          let chan = req.QueryInterface(Ci.nsIChannel);
          dump(`waitForDocLoadAndStopIt: Document start: ${chan.URI.spec}\n`);

          stopContent(aStopFromProgressListener, chan.originalURI.spec);
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
    mm.loadFrameScript("data:,(" + content_script.toString() + ")(" + aStopFromProgressListener + ");", true);
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
    return FullZoom.reset();
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
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url)
{
  let deferred = Promise.defer();
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  // Create two promises: one resolved from the content process when the page
  // loads and one that is rejected if we take too long to load the url.
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  let timeout = setTimeout(() => {
    deferred.reject(new Error("Timed out while waiting for a 'load' event"));
  }, 30000);

  loaded.then(() => {
    clearTimeout(timeout);
    deferred.resolve()
  });

  if (url)
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);

  // Promise.all rejects if either promise rejects (i.e. if we time out) and
  // if our loaded promise resolves before the timeout, then we resolve the
  // timeout promise as well, causing the all promise to resolve.
  return Promise.all([deferred.promise, loaded]);
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
 * Test the state of the identity box and control center to make
 * sure they are correctly showing the expected mixed content states.
 *
 * @note The checks are done synchronously, but new code should wait on the
 *       returned Promise object to ensure the identity panel has closed.
 *       Bug 1221114 is filed to fix the existing code.
 *
 * @param tabbrowser
 * @param Object states
 *        MUST include the following properties:
 *        {
 *           activeLoaded: true|false,
 *           activeBlocked: true|false,
 *           passiveLoaded: true|false,
 *        }
 *
 * @return {Promise}
 * @resolves When the operation has finished and the identity panel has closed.
 */
function assertMixedContentBlockingState(tabbrowser, states = {}) {
  if (!tabbrowser || !("activeLoaded" in states) ||
      !("activeBlocked" in states) || !("passiveLoaded" in states))  {
    throw new Error("assertMixedContentBlockingState requires a browser and a states object");
  }

  let {passiveLoaded, activeLoaded, activeBlocked} = states;
  let {gIdentityHandler} = tabbrowser.ownerGlobal;
  let doc = tabbrowser.ownerDocument;
  let identityBox = gIdentityHandler._identityBox;
  let classList = identityBox.classList;
  let connectionIcon = doc.getElementById("connection-icon");
  let connectionIconImage = tabbrowser.ownerGlobal.getComputedStyle(connectionIcon).
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
    is_element_hidden(connectionIcon);

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

    is_element_visible(connectionIcon);
    if (activeLoaded) {
      is(connectionIconImage, "url(\"chrome://browser/skin/identity-mixed-active-loaded.svg\")",
        "Using active loaded icon");
    }
    if (activeBlocked && !passiveLoaded) {
      is(connectionIconImage, "url(\"chrome://browser/skin/identity-secure.svg\")",
        "Using active blocked icon");
    }
    if (passiveLoaded && !(activeLoaded || activeBlocked)) {
      is(connectionIconImage, "url(\"chrome://browser/skin/identity-mixed-passive-loaded.svg\")",
        "Using passive loaded icon");
    }
    if (passiveLoaded && activeBlocked) {
      is(connectionIconImage, "url(\"chrome://browser/skin/identity-mixed-passive-loaded.svg\")",
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
  let securityViewBG = tabbrowser.ownerGlobal.getComputedStyle(securityView).
                       getPropertyValue("background-image");
  let securityContentBG = tabbrowser.ownerGlobal.getComputedStyle(securityView).
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

  if (activeLoaded || activeBlocked || passiveLoaded) {
    doc.getElementById("identity-popup-security-expander").click();
    is(Array.filter(doc.querySelectorAll("[observes=identity-popup-mcb-learn-more]"),
                    element => !is_hidden(element)).length, 1,
       "The 'Learn more' link should be visible once.");
  }

  gIdentityHandler._identityPopup.hidden = true;

  // Wait for the panel to be closed before continuing. The promisePopupHidden
  // function cannot be used because it's unreliable unless promisePopupShown is
  // also called before closing the panel. This cannot be done until all callers
  // are made asynchronous (bug 1221114).
  return new Promise(resolve => executeSoon(resolve));
}

function is_hidden(element) {
  var style = element.ownerGlobal.getComputedStyle(element);
  if (style.display == "none")
    return true;
  if (style.visibility != "visible")
    return true;
  if (style.display == "-moz-popup")
    return ["hiding", "closed"].indexOf(element.state) != -1;

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_hidden(element.parentNode);

  return false;
}

function is_visible(element) {
  var style = element.ownerGlobal.getComputedStyle(element);
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
  ok(is_visible(element), msg || "Element should be visible");
}

function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(is_hidden(element), msg || "Element should be hidden");
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
  let win = notification.browser.ownerGlobal;
  if (win.PopupNotifications.panel.state == "open") {
    return Promise.resolve();
  }
  let panelPromise = promisePopupShown(win.PopupNotifications.panel);
  notification.reshow();
  return panelPromise;
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

/**
 * For an nsIPropertyBag, returns the value for a given
 * key.
 *
 * @param bag
 *        The nsIPropertyBag to retrieve the value from
 * @param key
 *        The key that we want to get the value for from the
 *        bag
 * @returns The value corresponding to the key from the bag,
 *          or null if the value could not be retrieved (for
 *          example, if no value is set at that key).
*/
function getPropertyBagValue(bag, key) {
  try {
    let val = bag.getProperty(key);
    return val;
  } catch (e) {
    if (e.result != Cr.NS_ERROR_FAILURE) {
      throw e;
    }
  }

  return null;
}

/**
 * Returns a Promise that resolves once a crash report has
 * been submitted. This function will also test the crash
 * reports extra data to see if it matches expectedExtra.
 *
 * @param expectedExtra (object)
 *        An Object whose key-value pairs will be compared
 *        against the key-value pairs in the extra data of the
 *        crash report. A test failure will occur if there is
 *        a mismatch.
 *
 *        If the value of the key-value pair is "null", this will
 *        be interpreted as "this key should not be included in the
 *        extra data", and will cause a test failure if it is detected
 *        in the crash report.
 *
 *        Note that this will ignore any keys that are not included
 *        in expectedExtra. It's possible that the crash report
 *        will contain other extra information that is not
 *        compared against.
 * @returns Promise
 */
function promiseCrashReport(expectedExtra={}) {
  return Task.spawn(function*() {
    info("Starting wait on crash-report-status");
    let [subject, data] =
      yield TestUtils.topicObserved("crash-report-status", (subject, data) => {
        return data == "success";
      });
    info("Topic observed!");

    if (!(subject instanceof Ci.nsIPropertyBag2)) {
      throw new Error("Subject was not a Ci.nsIPropertyBag2");
    }

    let remoteID = getPropertyBagValue(subject, "serverCrashID");
    if (!remoteID) {
      throw new Error("Report should have a server ID");
    }

    let file = Cc["@mozilla.org/file/local;1"]
                 .createInstance(Ci.nsILocalFile);
    file.initWithPath(Services.crashmanager._submittedDumpsDir);
    file.append(remoteID + ".txt");
    if (!file.exists()) {
      throw new Error("Report should have been received by the server");
    }

    file.remove(false);

    let extra = getPropertyBagValue(subject, "extra");
    if (!(extra instanceof Ci.nsIPropertyBag2)) {
      throw new Error("extra was not a Ci.nsIPropertyBag2");
    }

    info("Iterating crash report extra keys");
    let enumerator = extra.enumerator;
    while (enumerator.hasMoreElements()) {
      let key = enumerator.getNext().QueryInterface(Ci.nsIProperty).name;
      let value = extra.getPropertyAsAString(key);
      if (key in expectedExtra) {
        if (expectedExtra[key] == null) {
          ok(false, `Got unexpected key ${key} with value ${value}`);
        } else {
          is(value, expectedExtra[key],
             `Crash report had the right extra value for ${key}`);
        }
      }
    }
  });
}

function promiseErrorPageLoaded(browser) {
  return new Promise(resolve => {
    browser.addEventListener("DOMContentLoaded", function onLoad() {
      browser.removeEventListener("DOMContentLoaded", onLoad, false, true);
      resolve();
    }, false, true);
  });
}

function* loadBadCertPage(url) {
  const EXCEPTION_DIALOG_URI = "chrome://pippki/content/exceptionDialog.xul";
  let exceptionDialogResolved = new Promise(function(resolve) {
    // When the certificate exception dialog has opened, click the button to add
    // an exception.
    let certExceptionDialogObserver = {
      observe: function(aSubject, aTopic, aData) {
        if (aTopic == "cert-exception-ui-ready") {
          Services.obs.removeObserver(this, "cert-exception-ui-ready");
          let certExceptionDialog = getCertExceptionDialog(EXCEPTION_DIALOG_URI);
          ok(certExceptionDialog, "found exception dialog");
          executeSoon(function() {
            certExceptionDialog.documentElement.getButton("extra1").click();
            resolve();
          });
        }
      }
    };

    Services.obs.addObserver(certExceptionDialogObserver,
                             "cert-exception-ui-ready", false);
  });

  yield BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url);
  yield promiseErrorPageLoaded(gBrowser.selectedBrowser);
  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
    content.document.getElementById("exceptionDialogButton").click();
  });
  yield exceptionDialogResolved;
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
}

// Utility function to get a handle on the certificate exception dialog.
// Modified from toolkit/components/passwordmgr/test/prompt_common.js
function getCertExceptionDialog(aLocation) {
  let enumerator = Services.wm.getXULWindowEnumerator(null);

  while (enumerator.hasMoreElements()) {
    let win = enumerator.getNext();
    let windowDocShell = win.QueryInterface(Ci.nsIXULWindow).docShell;

    let containedDocShells = windowDocShell.getDocShellEnumerator(
                                      Ci.nsIDocShellTreeItem.typeChrome,
                                      Ci.nsIDocShell.ENUMERATE_FORWARDS);
    while (containedDocShells.hasMoreElements()) {
      // Get the corresponding document for this docshell
      let childDocShell = containedDocShells.getNext();
      let childDoc = childDocShell.QueryInterface(Ci.nsIDocShell)
                                  .contentViewer
                                  .DOMDocument;

      if (childDoc.location.href == aLocation) {
        return childDoc;
      }
    }
  }
  return undefined;
}

function setupRemoteClientsFixture(fixture) {
  let oldRemoteClientsGetter =
    Object.getOwnPropertyDescriptor(gFxAccounts, "remoteClients").get;

  Object.defineProperty(gFxAccounts, "remoteClients", {
    get: function() { return fixture; }
  });
  return oldRemoteClientsGetter;
}

function restoreRemoteClients(getter) {
  Object.defineProperty(gFxAccounts, "remoteClients", {
    get: getter
  });
}

function* openMenuItemSubmenu(id) {
  let menuPopup = document.getElementById(id).menupopup;
  let menuPopupPromise = BrowserTestUtils.waitForEvent(menuPopup, "popupshown");
  menuPopup.showPopup();
  yield menuPopupPromise;
}

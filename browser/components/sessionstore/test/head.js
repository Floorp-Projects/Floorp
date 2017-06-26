/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Utils} = Cu.import("resource://gre/modules/sessionstore/Utils.jsm", {});
const triggeringPrincipal_base64 = Utils.SERIALIZED_SYSTEMPRINCIPAL;

const TAB_STATE_NEEDS_RESTORE = 1;
const TAB_STATE_RESTORING = 2;

const ROOT = getRootDirectory(gTestPath);
const HTTPROOT = ROOT.replace("chrome://mochitests/content/", "http://example.com/");
const FRAME_SCRIPTS = [
  ROOT + "content.js",
  ROOT + "content-forms.js"
];

var globalMM = Cc["@mozilla.org/globalmessagemanager;1"]
           .getService(Ci.nsIMessageListenerManager);

for (let script of FRAME_SCRIPTS) {
  globalMM.loadFrameScript(script, true);
}

registerCleanupFunction(() => {
  for (let script of FRAME_SCRIPTS) {
    globalMM.removeDelayedFrameScript(script, true);
  }
});

const {SessionStore} = Cu.import("resource:///modules/sessionstore/SessionStore.jsm", {});
const {SessionSaver} = Cu.import("resource:///modules/sessionstore/SessionSaver.jsm", {});
const {SessionFile} = Cu.import("resource:///modules/sessionstore/SessionFile.jsm", {});
const {TabState} = Cu.import("resource:///modules/sessionstore/TabState.jsm", {});
const {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});

const ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

// Some tests here assume that all restored tabs are loaded without waiting for
// the user to bring them to the foreground. We ensure this by resetting the
// related preference (see the "firefox.js" defaults file for details).
Services.prefs.setBoolPref("browser.sessionstore.restore_on_demand", false);
registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");
});

// Obtain access to internals
Services.prefs.setBoolPref("browser.sessionstore.debug", true);
registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.sessionstore.debug");
});


// This kicks off the search service used on about:home and allows the
// session restore tests to be run standalone without triggering errors.
Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler).defaultArgs;

function provideWindow(aCallback, aURL, aFeatures) {
  function callbackSoon(aWindow) {
    executeSoon(function executeCallbackSoon() {
      aCallback(aWindow);
    });
  }

  let win = openDialog(getBrowserURL(), "", aFeatures || "chrome,all,dialog=no", aURL || "about:blank");
  whenWindowLoaded(win, function onWindowLoaded(aWin) {
    if (!aURL) {
      info("Loaded a blank window.");
      callbackSoon(aWin);
      return;
    }

    aWin.gBrowser.selectedBrowser.addEventListener("load", function() {
      callbackSoon(aWin);
    }, {capture: true, once: true});
  });
}

// This assumes that tests will at least have some state/entries
function waitForBrowserState(aState, aSetStateCallback) {
  if (typeof aState == "string") {
    aState = JSON.parse(aState);
  }
  if (typeof aState != "object") {
    throw new TypeError("Argument must be an object or a JSON representation of an object");
  }
  let windows = [window];
  let tabsRestored = 0;
  let expectedTabsRestored = 0;
  let expectedWindows = aState.windows.length;
  let windowsOpen = 1;
  let listening = false;
  let windowObserving = false;
  let restoreHiddenTabs = Services.prefs.getBoolPref(
                          "browser.sessionstore.restore_hidden_tabs");
  let restoreTabsLazily = Services.prefs.getBoolPref(
                          "browser.sessionstore.restore_tabs_lazily");

  aState.windows.forEach(function(winState) {
    winState.tabs.forEach(function(tabState) {
      if (!restoreTabsLazily && (restoreHiddenTabs || !tabState.hidden))
        expectedTabsRestored++;
    });
  });

  // If there are only hidden tabs and restoreHiddenTabs = false, we still
  // expect one of them to be restored because it gets shown automatically.
  // Otherwise if lazy tab restore there will only be one tab restored per window.
  if (!expectedTabsRestored) {
    expectedTabsRestored = 1;
  } else if (restoreTabsLazily) {
    expectedTabsRestored = aState.windows.length;
  }

  function onSSTabRestored(aEvent) {
    if (++tabsRestored == expectedTabsRestored) {
      // Remove the event listener from each window
      windows.forEach(function(win) {
        win.gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored, true);
      });
      listening = false;
      info("running " + aSetStateCallback.name);
      executeSoon(aSetStateCallback);
    }
  }

  // Used to add our listener to further windows so we can catch SSTabRestored
  // coming from them when creating a multi-window state.
  function windowObserver(aSubject, aTopic, aData) {
    if (aTopic == "domwindowopened") {
      let newWindow = aSubject.QueryInterface(Ci.nsIDOMWindow);
      newWindow.addEventListener("load", function() {
        if (++windowsOpen == expectedWindows) {
          Services.ww.unregisterNotification(windowObserver);
          windowObserving = false;
        }

        // Track this window so we can remove the progress listener later
        windows.push(newWindow);
        // Add the progress listener
        newWindow.gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored, true);
      }, {once: true});
    }
  }

  // We only want to register the notification if we expect more than 1 window
  if (expectedWindows > 1) {
    registerCleanupFunction(function() {
      if (windowObserving) {
        Services.ww.unregisterNotification(windowObserver);
      }
    });
    windowObserving = true;
    Services.ww.registerNotification(windowObserver);
  }

  registerCleanupFunction(function() {
    if (listening) {
      windows.forEach(function(win) {
        win.gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored, true);
      });
    }
  });
  // Add the event listener for this window as well.
  listening = true;
  gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored, true);

  // Ensure setBrowserState() doesn't remove the initial tab.
  gBrowser.selectedTab = gBrowser.tabs[0];

  // Finally, call setBrowserState
  ss.setBrowserState(JSON.stringify(aState));
}

function promiseBrowserState(aState) {
  return new Promise(resolve => waitForBrowserState(aState, resolve));
}

function promiseTabState(tab, state) {
  if (typeof(state) != "string") {
    state = JSON.stringify(state);
  }

  let promise = promiseTabRestored(tab);
  ss.setTabState(tab, state);
  return promise;
}

/**
 * Wait for a content -> chrome message.
 */
function promiseContentMessage(browser, name) {
  let mm = browser.messageManager;

  return new Promise(resolve => {
    function removeListener() {
      mm.removeMessageListener(name, listener);
    }

    function listener(msg) {
      removeListener();
      resolve(msg.data);
    }

    mm.addMessageListener(name, listener);
    registerCleanupFunction(removeListener);
  });
}

function waitForTopic(aTopic, aTimeout, aCallback) {
  let observing = false;
  function removeObserver() {
    if (!observing)
      return;
    Services.obs.removeObserver(observer, aTopic);
    observing = false;
  }

  let timeout = setTimeout(function() {
    removeObserver();
    aCallback(false);
  }, aTimeout);

  function observer(subject, topic, data) {
    removeObserver();
    timeout = clearTimeout(timeout);
    executeSoon(() => aCallback(true));
  }

  registerCleanupFunction(function() {
    removeObserver();
    if (timeout) {
      clearTimeout(timeout);
    }
  });

  observing = true;
  Services.obs.addObserver(observer, aTopic);
}

/**
 * Wait until session restore has finished collecting its data and is
 * has written that data ("sessionstore-state-write-complete").
 *
 * @param {function} aCallback If sessionstore-state-write-complete is sent
 * within buffering interval + 100 ms, the callback is passed |true|,
 * otherwise, it is passed |false|.
 */
function waitForSaveState(aCallback) {
  let timeout = 100 +
    Services.prefs.getIntPref("browser.sessionstore.interval");
  return waitForTopic("sessionstore-state-write-complete", timeout, aCallback);
}
function promiseSaveState() {
  return new Promise((resolve, reject) => {
    waitForSaveState(isSuccessful => {
      if (!isSuccessful) {
        reject(new Error("Save state timeout"));
      } else {
        resolve();
      }
    });
  });
}
function forceSaveState() {
  return SessionSaver.run();
}

function promiseRecoveryFileContents() {
  let promise = forceSaveState();
  return promise.then(function() {
    return OS.File.read(SessionFile.Paths.recovery, { encoding: "utf-8", compression: "lz4" });
  });
}

var promiseForEachSessionRestoreFile = async function(cb) {
  for (let key of SessionFile.Paths.loadOrder) {
    let data = "";
    try {
      data = await OS.File.read(SessionFile.Paths[key], { encoding: "utf-8", compression: "lz4" });
    } catch (ex) {
      // Ignore missing files
      if (!(ex instanceof OS.File.Error && ex.becauseNoSuchFile)) {
        throw ex;
      }
    }
    cb(data, key);
  }
};

function promiseBrowserLoaded(aBrowser, ignoreSubFrames = true, wantLoad = null) {
  return BrowserTestUtils.browserLoaded(aBrowser, !ignoreSubFrames, wantLoad);
}

function whenWindowLoaded(aWindow, aCallback) {
  aWindow.addEventListener("load", function() {
    executeSoon(function executeWhenWindowLoaded() {
      aCallback(aWindow);
    });
  }, {once: true});
}
function promiseWindowLoaded(aWindow) {
  return new Promise(resolve => whenWindowLoaded(aWindow, resolve));
}

var gUniqueCounter = 0;
function r() {
  return Date.now() + "-" + (++gUniqueCounter);
}

function* BrowserWindowIterator() {
  let windowsEnum = Services.wm.getEnumerator("navigator:browser");
  while (windowsEnum.hasMoreElements()) {
    let currentWindow = windowsEnum.getNext();
    if (!currentWindow.closed) {
      yield currentWindow;
    }
  }
}

var gWebProgressListener = {
  _callback: null,

  setCallback(aCallback) {
    if (!this._callback) {
      window.gBrowser.addTabsProgressListener(this);
    }
    this._callback = aCallback;
  },

  unsetCallback() {
    if (this._callback) {
      this._callback = null;
      window.gBrowser.removeTabsProgressListener(this);
    }
  },

  onStateChange(aBrowser, aWebProgress, aRequest,
                           aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
      this._callback(aBrowser);
    }
  }
};

registerCleanupFunction(function() {
  gWebProgressListener.unsetCallback();
});

var gProgressListener = {
  _callback: null,

  setCallback(callback) {
    Services.obs.addObserver(this, "sessionstore-debug-tab-restored");
    this._callback = callback;
  },

  unsetCallback() {
    if (this._callback) {
      this._callback = null;
    Services.obs.removeObserver(this, "sessionstore-debug-tab-restored");
    }
  },

  observe(browser, topic, data) {
    gProgressListener.onRestored(browser);
  },

  onRestored(browser) {
    if (browser.__SS_restoreState == TAB_STATE_RESTORING) {
      let args = [browser].concat(gProgressListener._countTabs());
      gProgressListener._callback.apply(gProgressListener, args);
    }
  },

  _countTabs() {
    let needsRestore = 0, isRestoring = 0, wasRestored = 0;

    for (let win of BrowserWindowIterator()) {
      for (let i = 0; i < win.gBrowser.tabs.length; i++) {
        let browser = win.gBrowser.tabs[i].linkedBrowser;
        if (browser.isConnected && !browser.__SS_restoreState)
          wasRestored++;
        else if (browser.__SS_restoreState == TAB_STATE_RESTORING)
          isRestoring++;
        else if (browser.__SS_restoreState == TAB_STATE_NEEDS_RESTORE || !browser.isConnected)
          needsRestore++;
      }
    }
    return [needsRestore, isRestoring, wasRestored];
  }
};

registerCleanupFunction(function() {
  gProgressListener.unsetCallback();
});

// Close all but our primary window.
function promiseAllButPrimaryWindowClosed() {
  let windows = [];
  for (let win of BrowserWindowIterator()) {
    if (win != window) {
      windows.push(win);
    }
  }

  return Promise.all(windows.map(BrowserTestUtils.closeWindow));
}

// Forget all closed windows.
function forgetClosedWindows() {
  while (ss.getClosedWindowCount() > 0) {
    ss.forgetClosedWindow(0);
  }
}

/**
 * When opening a new window it is not sufficient to wait for its load event.
 * We need to use whenDelayedStartupFinshed() here as the browser window's
 * delayedStartup() routine is executed one tick after the window's load event
 * has been dispatched. browser-delayed-startup-finished might be deferred even
 * further if parts of the window's initialization process take more time than
 * expected (e.g. reading a big session state from disk).
 */
function whenNewWindowLoaded(aOptions, aCallback) {
  let features = "";
  let url = "about:blank";

  if (aOptions && aOptions.private || false) {
    features = ",private";
    url = "about:privatebrowsing";
  }

  let win = openDialog(getBrowserURL(), "", "chrome,all,dialog=no" + features, url);
  let delayedStartup = promiseDelayedStartupFinished(win);

  let browserLoaded = new Promise(resolve => {
    if (url == "about:blank") {
      resolve();
      return;
    }

    win.addEventListener("load", function() {
      let browser = win.gBrowser.selectedBrowser;
      promiseBrowserLoaded(browser).then(resolve);
    }, {once: true});
  });

  Promise.all([delayedStartup, browserLoaded]).then(() => aCallback(win));
}
function promiseNewWindowLoaded(aOptions) {
  return new Promise(resolve => whenNewWindowLoaded(aOptions, resolve));
}

/**
 * This waits for the browser-delayed-startup-finished notification of a given
 * window. It indicates that the windows has loaded completely and is ready to
 * be used for testing.
 */
function whenDelayedStartupFinished(aWindow, aCallback) {
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      executeSoon(aCallback);
    }
  }, "browser-delayed-startup-finished");
}
function promiseDelayedStartupFinished(aWindow) {
  return new Promise(resolve => whenDelayedStartupFinished(aWindow, resolve));
}

function promiseEvent(element, eventType, isCapturing = false) {
  return new Promise(resolve => {
    element.addEventListener(eventType, function(event) {
      resolve(event);
    }, {capture: isCapturing, once: true});
  });
}

function promiseTabRestored(tab) {
  return promiseEvent(tab, "SSTabRestored");
}

function promiseTabRestoring(tab) {
  return promiseEvent(tab, "SSTabRestoring");
}

function sendMessage(browser, name, data = {}) {
  browser.messageManager.sendAsyncMessage(name, data);
  return promiseContentMessage(browser, name);
}

// This creates list of functions that we will map to their corresponding
// ss-test:* messages names. Those will be sent to the frame script and
// be used to read and modify form data.
/* global getTextContent, getInputValue, setInputValue, getInputChecked
          setInputChecked, getSelectedIndex, setSelectedIndex,
          getMultipleSelected, setMultipleSelected, getFileNameArray,
          setFileNameArray */
const FORM_HELPERS = [
  "getTextContent",
  "getInputValue", "setInputValue",
  "getInputChecked", "setInputChecked",
  "getSelectedIndex", "setSelectedIndex",
  "getMultipleSelected", "setMultipleSelected",
  "getFileNameArray", "setFileNameArray",
];

for (let name of FORM_HELPERS) {
  let msg = "ss-test:" + name;
  this[name] = (browser, data) => sendMessage(browser, msg, data);
}

// Removes the given tab immediately and returns a promise that resolves when
// all pending status updates (messages) of the closing tab have been received.
function promiseRemoveTab(tab) {
  return BrowserTestUtils.removeTab(tab);
}

// Write DOMSessionStorage data to the given browser.
function modifySessionStorage(browser, storageData, storageOptions = {}) {
  return ContentTask.spawn(browser, [storageData, storageOptions], async function([data, options]) {
    let frame = content;
    if (options && "frameIndex" in options) {
      frame = content.frames[options.frameIndex];
    }

    let keys = new Set(Object.keys(data));
    let storage = frame.sessionStorage;

    return new Promise(resolve => {
      addEventListener("MozSessionStorageChanged", function onStorageChanged(event) {
        if (event.storageArea == storage) {
          keys.delete(event.key);
        }

        if (keys.size == 0) {
          removeEventListener("MozSessionStorageChanged", onStorageChanged, true);
          resolve();
        }
      }, true);

      for (let key of keys) {
        frame.sessionStorage[key] = data[key];
      }
    });
  });
}

function pushPrefs(...aPrefs) {
  return SpecialPowers.pushPrefEnv({"set": aPrefs});
}

function popPrefs() {
  return SpecialPowers.popPrefEnv();
}

async function checkScroll(tab, expected, msg) {
  let browser = tab.linkedBrowser;
  await TabStateFlusher.flush(browser);

  let scroll = JSON.parse(ss.getTabState(tab)).scroll || null;
  is(JSON.stringify(scroll), JSON.stringify(expected), msg);
}

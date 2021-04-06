const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AboutNewTab",
  "resource:///modules/AboutNewTab.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "BrowserTestUtils",
  "resource://testing-common/BrowserTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TabCrashHandler",
  "resource:///modules/ContentCrashHandlers.jsm"
);

/**
 * Wait for a <notification> to be closed then call the specified callback.
 */
function waitForNotificationClose(notification, cb) {
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
  observer.observe(notification.control.stack, { childList: true });
}

function closeAllNotifications() {
  if (!gNotificationBox.currentNotification) {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    for (let notification of gNotificationBox.allNotifications) {
      waitForNotificationClose(notification, function() {
        if (gNotificationBox.allNotifications.length === 0) {
          resolve();
        }
      });
      notification.close();
    }
  });
}

function whenDelayedStartupFinished(aWindow, aCallback) {
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      executeSoon(aCallback);
    }
  }, "browser-delayed-startup-finished");
}

function openToolbarCustomizationUI(aCallback, aBrowserWin) {
  if (!aBrowserWin) {
    aBrowserWin = window;
  }

  aBrowserWin.gCustomizeMode.enter();

  aBrowserWin.gNavToolbox.addEventListener(
    "customizationready",
    function() {
      executeSoon(function() {
        aCallback(aBrowserWin);
      });
    },
    { once: true }
  );
}

function closeToolbarCustomizationUI(aCallback, aBrowserWin) {
  aBrowserWin.gNavToolbox.addEventListener(
    "aftercustomization",
    function() {
      executeSoon(aCallback);
    },
    { once: true }
  );

  aBrowserWin.gCustomizeMode.exit();
}

function waitForCondition(condition, nextTest, errorMsg, retryTimes) {
  retryTimes = typeof retryTimes !== "undefined" ? retryTimes : 30;
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
  var moveOn = function() {
    clearInterval(interval);
    nextTest();
  };
}

function promiseWaitForCondition(aConditionFn) {
  return new Promise(resolve => {
    waitForCondition(aConditionFn, resolve, "Condition didn't pass.");
  });
}

function promiseWaitForEvent(
  object,
  eventName,
  capturing = false,
  chrome = false
) {
  return new Promise(resolve => {
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
  return new Promise(resolve => {
    waitForFocus(resolve, aWindow);
  });
}

function pushPrefs(...aPrefs) {
  return SpecialPowers.pushPrefEnv({ set: aPrefs });
}

function popPrefs() {
  return SpecialPowers.popPrefEnv();
}

function promiseWindowClosed(win) {
  let promise = BrowserTestUtils.domWindowClosed(win);
  win.close();
  return promise;
}

function promiseOpenAndLoadWindow(aOptions, aWaitForDelayedStartup = false) {
  return new Promise(resolve => {
    let win = OpenBrowserWindow(aOptions);
    if (aWaitForDelayedStartup) {
      Services.obs.addObserver(function onDS(aSubject, aTopic, aData) {
        if (aSubject != win) {
          return;
        }
        Services.obs.removeObserver(onDS, "browser-delayed-startup-finished");
        resolve(win);
      }, "browser-delayed-startup-finished");
    } else {
      win.addEventListener(
        "load",
        function() {
          resolve(win);
        },
        { once: true }
      );
    }
  });
}

async function whenNewTabLoaded(aWindow, aCallback) {
  aWindow.BrowserOpenTab();

  let expectedURL = AboutNewTab.newTabURL;
  let browser = aWindow.gBrowser.selectedBrowser;
  let loadPromise = BrowserTestUtils.browserLoaded(browser, false, expectedURL);
  let alreadyLoaded = await SpecialPowers.spawn(browser, [expectedURL], url => {
    let doc = content.document;
    return doc && doc.readyState === "complete" && doc.location.href == url;
  });
  if (!alreadyLoaded) {
    await loadPromise;
  }
  aCallback();
}

function whenTabLoaded(aTab, aCallback) {
  promiseTabLoadEvent(aTab).then(aCallback);
}

function promiseTabLoaded(aTab) {
  return new Promise(resolve => {
    whenTabLoaded(aTab, resolve);
  });
}

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
function promiseTabLoadEvent(tab, url) {
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  if (url) {
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);
  }

  return loaded;
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
  return BrowserTestUtils.waitForEvent(aTabBrowser.tabContainer, "TabOpen");
}

function is_hidden(element) {
  var style = element.ownerGlobal.getComputedStyle(element);
  if (style.display == "none") {
    return true;
  }
  if (style.visibility != "visible") {
    return true;
  }
  if (style.display == "-moz-popup") {
    return ["hiding", "closed"].includes(element.state);
  }

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument) {
    return is_hidden(element.parentNode);
  }

  return false;
}

function is_element_visible(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_visible(element), msg || "Element should be visible");
}

function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(is_hidden(element), msg || "Element should be hidden");
}

function promisePopupShown(popup) {
  return BrowserTestUtils.waitForPopupEvent(popup, "shown");
}

function promisePopupHidden(popup) {
  return BrowserTestUtils.waitForPopupEvent(popup, "hidden");
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
 * Resolves when a bookmark with the given uri is added.
 */
function promiseOnBookmarkItemAdded(aExpectedURI) {
  return new Promise((resolve, reject) => {
    let listener = events => {
      is(events.length, 1, "Should only receive one event.");
      info("Added a bookmark to " + events[0].url);
      PlacesUtils.observers.removeListener(["bookmark-added"], listener);
      if (events[0].url == aExpectedURI.spec) {
        resolve();
      } else {
        reject(new Error("Added an unexpected bookmark"));
      }
    };
    info("Waiting for a bookmark to be added");
    PlacesUtils.observers.addListener(["bookmark-added"], listener);
  });
}

async function loadBadCertPage(url) {
  let loaded = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url);
  await loaded;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    content.document.getElementById("exceptionDialogButton").click();
  });
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
}

/**
 * Waits for the stylesheets to be loaded into the browser menu.
 *
 * @param tab
 *        The tab that contains the webpage we're testing.
 * @param styleSheetCount
 *        How many stylesheets we expect to be loaded.
 * @return Promise
 */
async function promiseStylesheetsLoaded(tab, styleSheetCount) {
  let styleMenu = tab.ownerGlobal.gPageStyleMenu;
  let permanentKey = tab.permanentKey;

  await TestUtils.waitForCondition(() => {
    let menu = styleMenu._pageStyleSheets.get(permanentKey);
    info(`waiting for sheets: ${menu && menu.filteredStyleSheets.length}`);
    return menu && menu.filteredStyleSheets.length >= styleSheetCount;
  }, "waiting for style sheets to load");
}

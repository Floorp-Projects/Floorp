Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

// The blocklist shim running in the content process does not initialize at
// start up, so it's not active until we load content that needs to do a
// check. This helper bypasses the delay to get the svc up and running
// immediately. Note, call this after remote content has loaded.
function promiseInitContentBlocklistSvc(aBrowser)
{
  return ContentTask.spawn(aBrowser, {}, function* () {
    try {
      let bls = Cc["@mozilla.org/extensions/blocklist;1"]
                          .getService(Ci.nsIBlocklistService);
    } catch (ex) {
      return ex.message;
    }
    return null;
  });
}

/**
  * Waits a specified number of miliseconds.
  *
  * Usage:
  *    let wait = yield waitForMs(2000);
  *    ok(wait, "2 seconds should now have elapsed");
  *
  * @param aMs the number of miliseconds to wait for
  * @returns a Promise that resolves to true after the time has elapsed
  */
function waitForMs(aMs) {
  let deferred = Promise.defer();
  let startTime = Date.now();
  setTimeout(done, aMs);
  function done() {
    deferred.resolve(true);
  }
  return deferred.promise;
}


// DOM Promise fails for unknown reasons here, so we're using
// resource://gre/modules/Promise.jsm.
function waitForEvent(subject, eventName, checkFn, useCapture, useUntrusted) {
  return new Promise((resolve, reject) => {
    subject.addEventListener(eventName, function listener(event) {
      try {
        if (checkFn && !checkFn(event)) {
          return;
        }
        subject.removeEventListener(eventName, listener, useCapture);
        resolve(event);
      } catch (ex) {
        try {
          subject.removeEventListener(eventName, listener, useCapture);
        } catch (ex2) {
          // Maybe the provided object does not support removeEventListener.
        }
        reject(ex);
      }
    }, useCapture, useUntrusted);
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
 * @param [optional] event
 *        The load event type to wait for.  Defaults to "load".
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url, eventType="load") {
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
  if (url) {
    tab.linkedBrowser.loadURI(url);
  }
  return deferred.promise;
}

function waitForCondition(condition, nextTest, errorMsg, aTries, aWait) {
  let tries = 0;
  let maxTries = aTries || 100; // 100 tries
  let maxWait = aWait || 100; // 100 msec x 100 tries = ten seconds
  let interval = setInterval(function() {
    if (tries >= maxTries) {
      ok(false, errorMsg);
      moveOn();
    }
    let conditionPassed;
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
  }, maxWait);
  let moveOn = function() { clearInterval(interval); nextTest(); };
}

// Waits for a conditional function defined by the caller to return true.
function promiseForCondition(aConditionFn, aMessage, aTries, aWait) {
  let deferred = Promise.defer();
  waitForCondition(aConditionFn, deferred.resolve,
                   (aMessage || "Condition didn't pass."),
                   aTries, aWait);
  return deferred.promise;
}

// Returns the chrome side nsIPluginTag for this plugin
function getTestPlugin(aName) {
  let pluginName = aName || "Test Plug-in";
  let ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  let tags = ph.getPluginTags();

  // Find the test plugin
  for (let i = 0; i < tags.length; i++) {
    if (tags[i].name == pluginName)
      return tags[i];
  }
  ok(false, "Unable to find plugin");
  return null;
}

// Set the 'enabledState' on the nsIPluginTag stored in the main or chrome
// process.
function setTestPluginEnabledState(newEnabledState, pluginName) {
  let name = pluginName || "Test Plug-in";
  let plugin = getTestPlugin(name);
  plugin.enabledState = newEnabledState;
}

// Get the 'enabledState' on the nsIPluginTag stored in the main or chrome
// process.
function getTestPluginEnabledState(pluginName) {
  let name = pluginName || "Test Plug-in";
  let plugin = getTestPlugin(name);
  return plugin.enabledState;
}

// Returns a promise for nsIObjectLoadingContent props data.
function promiseForPluginInfo(aId, aBrowser) {
  let browser = aBrowser || gTestBrowser;
  return ContentTask.spawn(browser, aId, function* (aId) {
    let plugin = content.document.getElementById(aId);
    if (!(plugin instanceof Ci.nsIObjectLoadingContent))
      throw new Error("no plugin found");
    return {
      pluginFallbackType: plugin.pluginFallbackType,
      activated: plugin.activated,
      hasRunningPlugin: plugin.hasRunningPlugin,
      displayedType: plugin.displayedType,
    };
  });
}

// Return a promise and call the plugin's nsIObjectLoadingContent
// playPlugin() method.
function promisePlayObject(aId, aBrowser) {
  let browser = aBrowser || gTestBrowser;
  return ContentTask.spawn(browser, aId, function* (aId) {
    let plugin = content.document.getElementById(aId);
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    objLoadingContent.playPlugin();
  });
}

function promiseCrashObject(aId, aBrowser) {
  let browser = aBrowser || gTestBrowser;
  return ContentTask.spawn(browser, aId, function* (aId) {
    let plugin = content.document.getElementById(aId);
    let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    Components.utils.waiveXrays(plugin).crash();
  });
}

// Return a promise and call the plugin's getObjectValue() method.
function promiseObjectValueResult(aId, aBrowser) {
  let browser = aBrowser || gTestBrowser;
  return ContentTask.spawn(browser, aId, function* (aId) {
    let plugin = content.document.getElementById(aId);
    return Components.utils.waiveXrays(plugin).getObjectValue();
  });
}

// Return a promise and reload the target plugin in the page
function promiseReloadPlugin(aId, aBrowser) {
  let browser = aBrowser || gTestBrowser;
  return ContentTask.spawn(browser, aId, function* (aId) {
    let plugin = content.document.getElementById(aId);
    plugin.src = plugin.src;
  });
}

// after a test is done using the plugin doorhanger, we should just clear
// any permissions that may have crept in
function clearAllPluginPermissions() {
  let perms = Services.perms.enumerator;
  while (perms.hasMoreElements()) {
    let perm = perms.getNext();
    if (perm.type.startsWith('plugin')) {
      info("removing permission:" + perm.principal.origin + " " + perm.type + "\n");
      Services.perms.removePermission(perm);
    }
  }
}

function updateBlocklist(aCallback) {
  let blocklistNotifier = Cc["@mozilla.org/extensions/blocklist;1"]
                          .getService(Ci.nsITimerCallback);
  let observer = function() {
    Services.obs.removeObserver(observer, "blocklist-updated");
    SimpleTest.executeSoon(aCallback);
  };
  Services.obs.addObserver(observer, "blocklist-updated", false);
  blocklistNotifier.notify(null);
}

let _originalTestBlocklistURL = null;
function setAndUpdateBlocklist(aURL, aCallback) {
  if (!_originalTestBlocklistURL) {
    _originalTestBlocklistURL = Services.prefs.getCharPref("extensions.blocklist.url");
  }
  Services.prefs.setCharPref("extensions.blocklist.url", aURL);
  updateBlocklist(aCallback);
}

// A generator that insures a new blocklist is loaded (in both
// processes if applicable).
function* asyncSetAndUpdateBlocklist(aURL, aBrowser) {
  info("*** loading new blocklist: " + aURL);
  let doTestRemote = aBrowser ? aBrowser.isRemoteBrowser : false;
  if (!_originalTestBlocklistURL) {
    _originalTestBlocklistURL = Services.prefs.getCharPref("extensions.blocklist.url");
  }
  Services.prefs.setCharPref("extensions.blocklist.url", aURL);
  let localPromise = TestUtils.topicObserved("blocklist-updated");
  let remotePromise;
  if (doTestRemote) {
    remotePromise = TestUtils.topicObserved("content-blocklist-updated");
  }
  let blocklistNotifier = Cc["@mozilla.org/extensions/blocklist;1"]
                            .getService(Ci.nsITimerCallback);
  blocklistNotifier.notify(null);
  info("*** waiting on local load");
  yield localPromise;
  if (doTestRemote) {
    info("*** waiting on remote load");
    yield remotePromise;
  }
  info("*** blocklist loaded.");
}

// Reset back to the blocklist we had at the start of the test run.
function resetBlocklist() {
  Services.prefs.setCharPref("extensions.blocklist.url", _originalTestBlocklistURL);
}

// Insure there's a popup notification present. This test does not indicate
// open state. aBrowser can be undefined.
function promisePopupNotification(aName, aBrowser) {
  let deferred = Promise.defer();

  waitForCondition(() => PopupNotifications.getNotification(aName, aBrowser),
                   () => {
    ok(!!PopupNotifications.getNotification(aName, aBrowser),
       aName + " notification appeared");

    deferred.resolve();
  }, "timeout waiting for popup notification " + aName);

  return deferred.promise;
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

/**
 * Returns a Promise that resolves when a notification bar
 * for a browser is shown. Alternatively, for old-style callers,
 * can automatically call a callback before it resolves.
 *
 * @param notificationID
 *        The ID of the notification to look for.
 * @param browser
 *        The browser to check for the notification bar.
 * @param callback (optional)
 *        A function to be called just before the Promise resolves.
 *
 * @return Promise
 */
function waitForNotificationBar(notificationID, browser, callback) {
  return new Promise((resolve, reject) => {
    let notification;
    let notificationBox = gBrowser.getNotificationBox(browser);
    waitForCondition(
      () => (notification = notificationBox.getNotificationWithValue(notificationID)),
      () => {
        ok(notification, `Successfully got the ${notificationID} notification bar`);
        if (callback) {
          callback(notification);
        }
        resolve(notification);
      },
      `Waited too long for the ${notificationID} notification bar`
    );
  });
}

function promiseForNotificationBar(notificationID, browser) {
  let deferred = Promise.defer();
  waitForNotificationBar(notificationID, browser, deferred.resolve);
  return deferred.promise;
}

/**
 * Reshow a notification and call a callback when it is reshown.
 * @param notification
 *        The notification to reshow
 * @param callback
 *        A function to be called when the notification has been reshown
 */
function waitForNotificationShown(notification, callback) {
  if (PopupNotifications.panel.state == "open") {
    executeSoon(callback);
    return;
  }
  PopupNotifications.panel.addEventListener("popupshown", function onShown(e) {
    PopupNotifications.panel.removeEventListener("popupshown", onShown);
    callback();
  }, false);
  notification.reshow();
}

function promiseForNotificationShown(notification) {
  let deferred = Promise.defer();
  waitForNotificationShown(notification, deferred.resolve);
  return deferred.promise;
}

/**
 * Due to layout being async, "PluginBindAttached" may trigger later. This
 * returns a Promise that resolves once we've forced a layout flush, which
 * triggers the PluginBindAttached event to fire. This trick only works if
 * there is some sort of plugin in the page.
 * @param browser
 *        The browser to force plugin bindings in.
 * @return Promise
 */
function promiseUpdatePluginBindings(browser) {
  return ContentTask.spawn(browser, {}, function* () {
    let doc = content.document;
    let elems = doc.getElementsByTagName('embed');
    if (!elems || elems.length < 1) {
      elems = doc.getElementsByTagName('object');
    }
    if (elems && elems.length > 0) {
      elems[0].clientTop;
    }
  });
}

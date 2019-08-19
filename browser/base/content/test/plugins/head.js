var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetters(this, {
  uuidGen: ["@mozilla.org/uuid-generator;1", "nsIUUIDGenerator"],
});

// Various tests in this directory may define gTestBrowser, to use as the
// default browser under test in some of the functions below.
/* global gTestBrowser:true */

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
  return new Promise(resolve => {
    setTimeout(done, aMs);
    function done() {
      resolve(true);
    }
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
  let moveOn = function() {
    clearInterval(interval);
    nextTest();
  };
}

// Waits for a conditional function defined by the caller to return true.
function promiseForCondition(aConditionFn, aMessage, aTries, aWait) {
  return new Promise(resolve => {
    waitForCondition(
      aConditionFn,
      resolve,
      aMessage || "Condition didn't pass.",
      aTries,
      aWait
    );
  });
}

// Returns the chrome side nsIPluginTag for this plugin
function getTestPlugin(aName) {
  let pluginName = aName || "Test Plug-in";
  let ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  let tags = ph.getPluginTags();

  // Find the test plugin
  for (let i = 0; i < tags.length; i++) {
    if (tags[i].name == pluginName) {
      return tags[i];
    }
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
  return ContentTask.spawn(browser, aId, async function(contentId) {
    let plugin = content.document.getElementById(contentId);
    if (!(plugin instanceof Ci.nsIObjectLoadingContent)) {
      throw new Error("no plugin found");
    }
    return {
      pluginFallbackType: plugin.pluginFallbackType,
      activated: plugin.activated,
      hasRunningPlugin: plugin.hasRunningPlugin,
      displayedType: plugin.displayedType,
    };
  });
}

// Return a promise and call the plugin's playPlugin() method.
function promisePlayObject(aId, aBrowser) {
  let browser = aBrowser || gTestBrowser;
  return ContentTask.spawn(browser, aId, async function(contentId) {
    content.document.getElementById(contentId).playPlugin();
  });
}

function promiseCrashObject(aId, aBrowser) {
  let browser = aBrowser || gTestBrowser;
  return ContentTask.spawn(browser, aId, async function(contentId) {
    let plugin = content.document.getElementById(contentId);
    Cu.waiveXrays(plugin).crash();
  });
}

// Return a promise and call the plugin's getObjectValue() method.
function promiseObjectValueResult(aId, aBrowser) {
  let browser = aBrowser || gTestBrowser;
  return ContentTask.spawn(browser, aId, async function(contentId) {
    let plugin = content.document.getElementById(contentId);
    return Cu.waiveXrays(plugin).getObjectValue();
  });
}

// Return a promise and reload the target plugin in the page
function promiseReloadPlugin(aId, aBrowser) {
  let browser = aBrowser || gTestBrowser;
  return ContentTask.spawn(browser, aId, async function(contentId) {
    let plugin = content.document.getElementById(contentId);
    // eslint-disable-next-line no-self-assign
    plugin.src = plugin.src;
  });
}

// after a test is done using the plugin doorhanger, we should just clear
// any permissions that may have crept in
function clearAllPluginPermissions() {
  let perms = Services.perms.enumerator;
  while (perms.hasMoreElements()) {
    let perm = perms.getNext();
    if (perm.type.startsWith("plugin")) {
      info(
        "removing permission:" + perm.principal.origin + " " + perm.type + "\n"
      );
      Services.perms.removePermission(perm);
    }
  }
}

// Ported from AddonTestUtils.jsm
let JSONBlocklistWrapper = {
  /**
   * Load the data from the specified files into the *real* blocklist providers.
   * Loads using loadBlocklistRawData, which will treat this as an update.
   *
   * @param {nsIFile} dir
   *        The directory in which the files live.
   * @param {string} prefix
   *        a prefix for the files which ought to be loaded.
   *        This method will suffix -extensions.json and -plugins.json
   *        to the prefix it is given, and attempt to load both.
   *        Insofar as either exists, their data will be dumped into
   *        the respective store, and the respective update handlers
   *        will be called.
   */
  async loadBlocklistData(url) {
    const fullURL = `${url}-plugins.json`;
    let jsonObj;
    try {
      jsonObj = await (await fetch(fullURL)).json();
    } catch (ex) {
      ok(false, ex);
    }
    info(`Loaded ${fullURL}`);

    return this.loadBlocklistRawData({ plugins: jsonObj });
  },

  /**
   * Load the following data into the *real* blocklist providers.
   * While `overrideBlocklist` replaces the blocklist entirely with a mock
   * that returns dummy data, this method instead loads data into the actual
   * blocklist, fires update methods as would happen if this data came from
   * an actual blocklist update, etc.
   *
   * @param {object} data
   *        An object that can optionally have `extensions` and/or `plugins`
   *        properties, each being an array of blocklist items.
   *        This code only uses plugin blocks, that can look something like:
   *
   * {
   *   "matchFilename": "libnptest\\.so|nptest\\.dll|Test\\.plugin",
   *   "versionRange": [
   *     {
   *       "severity": "0",
   *       "vulnerabilityStatus": "1"
   *     }
   *   ],
   *   "blockID": "p9999"
   * }
   *
   */
  async loadBlocklistRawData(data) {
    const bsPass = ChromeUtils.import(
      "resource://gre/modules/Blocklist.jsm",
      null
    );
    const blocklistMapping = {
      extensions: bsPass.ExtensionBlocklistRS,
      plugins: bsPass.PluginBlocklistRS,
    };

    for (const [dataProp, blocklistObj] of Object.entries(blocklistMapping)) {
      let newData = data[dataProp];
      if (!newData) {
        continue;
      }
      if (!Array.isArray(newData)) {
        throw new Error(
          "Expected an array of new items to put in the " +
            dataProp +
            " blocklist!"
        );
      }
      for (let item of newData) {
        if (!item.id) {
          item.id = uuidGen.generateUUID().number.slice(1, -1);
        }
        if (!item.last_modified) {
          item.last_modified = Date.now();
        }
      }
      await blocklistObj.ensureInitialized();
      let collection = await blocklistObj._client.openCollection();
      await collection.clear();
      await collection.loadDump(newData);
      // We manually call _onUpdate... which is evil, but at the moment kinto doesn't have
      // a better abstraction unless you want to mock your own http server to do the update.
      await blocklistObj._onUpdate();
    }
  },
};

// An async helper that insures a new blocklist is loaded (in both
// processes if applicable).
var _originalTestBlocklistURL = null;
async function asyncSetAndUpdateBlocklist(aURL, aBrowser) {
  let doTestRemote = aBrowser ? aBrowser.isRemoteBrowser : false;
  if (!_originalTestBlocklistURL) {
    _originalTestBlocklistURL = Services.prefs.getCharPref(
      "extensions.blocklist.url"
    );
  }
  let localPromise = TestUtils.topicObserved("plugin-blocklist-updated");
  if (Services.prefs.getBoolPref("extensions.blocklist.useXML", true)) {
    info("*** loading new blocklist: " + aURL + ".xml");
    Services.prefs.setCharPref("extensions.blocklist.url", aURL + ".xml");
    let blocklistNotifier = Cc[
      "@mozilla.org/extensions/blocklist;1"
    ].getService(Ci.nsITimerCallback);
    blocklistNotifier.notify(null);
  } else {
    info("*** loading blocklist using json: " + aURL);
    await JSONBlocklistWrapper.loadBlocklistData(aURL);
  }
  info("*** waiting on local load");
  await localPromise;
  if (doTestRemote) {
    info("*** waiting on remote load");
    // Ensure content has been updated with the blocklist
    await ContentTask.spawn(aBrowser, null, () => {});
  }
  info("*** blocklist loaded.");
}

// Reset back to the blocklist we had at the start of the test run.
function resetBlocklist() {
  Services.prefs.setCharPref(
    "extensions.blocklist.url",
    _originalTestBlocklistURL
  );
}

// Insure there's a popup notification present. This test does not indicate
// open state. aBrowser can be undefined.
function promisePopupNotification(aName, aBrowser) {
  return new Promise(resolve => {
    waitForCondition(
      () => PopupNotifications.getNotification(aName, aBrowser),
      () => {
        ok(
          !!PopupNotifications.getNotification(aName, aBrowser),
          aName + " notification appeared"
        );

        resolve();
      },
      "timeout waiting for popup notification " + aName
    );
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
      () =>
        (notification = notificationBox.getNotificationWithValue(
          notificationID
        )),
      () => {
        ok(
          notification,
          `Successfully got the ${notificationID} notification bar`
        );
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
  return new Promise(resolve => {
    waitForNotificationBar(notificationID, browser, resolve);
  });
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
  PopupNotifications.panel.addEventListener(
    "popupshown",
    function(e) {
      callback();
    },
    { once: true }
  );
  notification.reshow();
}

function promiseForNotificationShown(notification) {
  return new Promise(resolve => {
    waitForNotificationShown(notification, resolve);
  });
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
  return ContentTask.spawn(browser, {}, async function() {
    let doc = content.document;
    let elems = doc.getElementsByTagName("embed");
    if (!elems || elems.length < 1) {
      elems = doc.getElementsByTagName("object");
    }
    if (elems && elems.length > 0) {
      elems[0].clientTop;
    }
  });
}

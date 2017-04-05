Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");

// This url must sync with the table, url in SafeBrowsing.jsm addMozEntries
const PHISH_TABLE = "test-phish-simple";
const PHISH_URL = "https://www.itisatrap.org/firefox/its-a-trap.html";

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
function promiseTabLoadEvent(tab, url, eventType = "load") {
  info(`Wait tab event: ${eventType}`);

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  let loaded;
  if (eventType === "load") {
    loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);
  } else {
    // No need to use handle.
    loaded =
      BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, eventType,
                                           true, undefined, true);
  }

  if (url)
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);

  return loaded;
}

// This function is mostly ported from classifierCommon.js
// under toolkit/components/url-classifier/tests/mochitest.
function waitForDBInit(callback) {
  // Since there are two cases that may trigger the callback,
  // we have to carefully avoid multiple callbacks and observer
  // leaking.
  let didCallback = false;
  function callbackOnce() {
    if (!didCallback) {
      Services.obs.removeObserver(obsFunc, "mozentries-update-finished");
      callback();
    }
    didCallback = true;
  }

  // The first part: listen to internal event.
  function obsFunc() {
    ok(true, "Received internal event!");
    callbackOnce();
  }
  Services.obs.addObserver(obsFunc, "mozentries-update-finished", false);

  // The second part: we might have missed the event. Just do
  // an internal database lookup to confirm if the url has been
  // added.
  let principal = Services.scriptSecurityManager
    .createCodebasePrincipal(Services.io.newURI(PHISH_URL), {});

  let dbService = Cc["@mozilla.org/url-classifier/dbservice;1"]
    .getService(Ci.nsIUrlClassifierDBService);
  dbService.lookup(principal, PHISH_TABLE, value => {
    if (value === PHISH_TABLE) {
      ok(true, "DB lookup success!");
      callbackOnce();
    }
  });
}

Services.prefs.setCharPref("urlclassifier.malwareTable", "test-malware-simple,test-unwanted-simple");
Services.prefs.setCharPref("urlclassifier.phishTable", "test-phish-simple");
Services.prefs.setCharPref("urlclassifier.blockedTable", "test-block-simple");
SafeBrowsing.init();

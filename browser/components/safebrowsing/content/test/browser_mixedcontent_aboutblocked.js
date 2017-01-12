/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci, results: Cr } = Components;

// This url must sync with the table, url in SafeBrowsing.jsm addMozEntries
const PHISH_TABLE = "test-phish-simple";
const PHISH_URL = "https://www.itisatrap.org/firefox/its-a-trap.html";

const SECURE_CONTAINER_URL = "https://example.com/browser/browser/components/safebrowsing/content/test/empty_file.html";

// This function is mostly ported from classifierCommon.js
// under toolkit/components/url-classifier/tests/mochitest.
function waitForDBInit(callback) {
  // Since there are two cases that may trigger the callback,
  // we have to carefully avoid multiple callbacks and observer
  // leaking.
  let didCallback = false;
  function callbackOnce() {
    Services.obs.removeObserver(obsFunc, "mozentries-update-finished");
    if (!didCallback) {
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

add_task(function* testNormalBrowsing() {
  yield BrowserTestUtils.withNewTab(SECURE_CONTAINER_URL, function* (browser) {
    // Before we load the phish url, we have to make sure the hard-coded
    // black list has been added to the database.
    yield new Promise(resolve => waitForDBInit(resolve));

    yield ContentTask.spawn(browser, PHISH_URL, function* (aPhishUrl) {
      return new Promise(resolve => {
        // Register listener before loading phish URL.
        let listener = e => {
          removeEventListener('AboutBlockedLoaded', listener, false, true);
          resolve();
        };
        addEventListener('AboutBlockedLoaded', listener, false, true);

        // Create an iframe which is going to load a phish url.
        let iframe = content.document.createElement("iframe");
        iframe.src = aPhishUrl;
        content.document.body.appendChild(iframe);
      });
    });

    ok(true, "about:blocked is successfully loaded!");
  });
});

/*
 * Bug 1278037 - A Test case for checking whether forgetting APIs are working for the quota manager.
 */

const CC = Components.Constructor;

const TEST_HOST = "example.com";
const TEST_URL =
  "http://" +
  TEST_HOST +
  "/browser/browser/components/contextualidentity/test/browser/";

const USER_CONTEXTS = ["default", "personal"];

//
// Support functions.
//

async function openTabInUserContext(uri, userContextId) {
  // Open the tab in the correct userContextId.
  let tab = BrowserTestUtils.addTab(gBrowser, uri, { userContextId });

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return { tab, browser };
}

// Setup an entry for the indexedDB.
async function setupIndexedDB(browser) {
  await SpecialPowers.spawn(
    browser,
    [{ input: "TestForgetAPIs" }],
    async function (arg) {
      let request = content.indexedDB.open("idb", 1);

      request.onerror = function () {
        throw new Error("error opening db connection");
      };

      request.onupgradeneeded = event => {
        let db = event.target.result;
        let store = db.createObjectStore("obj", { keyPath: "id" });
        store.createIndex("userContext", "userContext", { unique: false });
      };

      let db = await new Promise(resolve => {
        request.onsuccess = event => {
          resolve(event.target.result);
        };
      });

      // Add an entry into the indexedDB.
      let transaction = db.transaction(["obj"], "readwrite");
      let store = transaction.objectStore("obj");
      store.add({ id: 1, userContext: arg.input });

      await new Promise(resolve => {
        transaction.oncomplete = () => {
          resolve();
        };
      });

      // Check the indexedDB has been set properly.
      transaction = db.transaction(["obj"], "readonly");
      store = transaction.objectStore("obj");
      let getRequest = store.get(1);
      await new Promise(resolve => {
        getRequest.onsuccess = () => {
          let res = getRequest.result;
          is(res.userContext, arg.input, "Check the indexedDB value");
          resolve();
        };
      });
    }
  );
}

// Check whether the indexedDB has been cleared.
async function checkIndexedDB(browser) {
  await SpecialPowers.spawn(browser, [], async function () {
    let request = content.indexedDB.open("idb", 1);

    let db = await new Promise(done => {
      request.onsuccess = event => {
        done(event.target.result);
      };
    });

    try {
      db.transaction(["obj"], "readonly");
      ok(false, "The indexedDB should not exist");
    } catch (e) {
      is(e.name, "NotFoundError", "The indexedDB does not exist as expected");
    }

    db.close();

    content.indexedDB.deleteDatabase("idb");
  });
}

//
// Test functions.
//

add_setup(async function () {
  // Make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });
});

add_task(async function test_quota_clearStoragesForPrincipal() {
  let tabs = [];

  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Open our tab in the given user context.
    tabs[userContextId] = await openTabInUserContext(
      TEST_URL + "empty_file.html",
      userContextId
    );

    // Setup an entry for the indexedDB.
    await setupIndexedDB(tabs[userContextId].browser);

    // Close this tab.
    BrowserTestUtils.removeTab(tabs[userContextId].tab);
  }

  // Using quota manager to clear all indexed DB for a given domain.
  let caUtils = {};
  Services.scriptloader.loadSubScript(
    "chrome://global/content/contentAreaUtils.js",
    caUtils
  );
  let httpURI = caUtils.makeURI("http://" + TEST_HOST);
  let httpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    httpURI,
    {}
  );
  let clearRequest = Services.qms.clearStoragesForOriginPrefix(httpPrincipal);
  await new Promise(resolve => {
    clearRequest.callback = () => {
      resolve();
    };
  });

  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Open our tab in the given user context.
    tabs[userContextId] = await openTabInUserContext(
      TEST_URL + "empty_file.html",
      userContextId
    );

    // Check whether indexed DB has been cleared.
    await checkIndexedDB(tabs[userContextId].browser);

    // Close this tab.
    BrowserTestUtils.removeTab(tabs[userContextId].tab);
  }
});

/*
 * Bug 1238183 - Test cases for forgetAboutSite with userContextId.
 */

const CC = Components.Constructor;

let { ForgetAboutSite } = ChromeUtils.import(
  "resource://gre/modules/ForgetAboutSite.jsm"
);
let { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

const USER_CONTEXTS = ["default", "personal"];
const TEST_HOST = "example.com";
const TEST_URL =
  "http://" +
  TEST_HOST +
  "/browser/browser/components/contextualidentity/test/browser/";
const COOKIE_NAME = "userContextId";

// Counter for image load hits.
let gHits = 0;

let gHttpServer = null;

function imageHandler(metadata, response) {
  // A 1x1 PNG image.
  // Source: https://commons.wikimedia.org/wiki/File:1x1.png (Public Domain)
  const IMAGE = atob(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
      "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII="
  );
  gHits++;
  response.setHeader("Cache-Control", "max-age=10000", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "image/png", false);
  response.write(IMAGE);
}

function loadImagePageHandler(metadata, response) {
  response.setHeader("Cache-Control", "max-age=10000", false);
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/html", false);
  let body =
    "<!DOCTYPE HTML>\
              <html>\
                <head>\
                  <meta charset='utf-8'>\
                  <title>Load Image</title>\
                </head>\
                <body>\
                <img src='image.png'>\
                </body>\
              </html>";
  response.bodyOutputStream.write(body, body.length);
}

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

function getCookiesForOA(host, userContextId) {
  return Services.cookies.getCookiesFromHost(host, { userContextId });
}

function createURI(uri) {
  return Services.io.newURI(uri);
}

function getCacheStorage(where, lci) {
  if (!lci) {
    lci = Services.loadContextInfo.default;
  }
  switch (where) {
    case "disk":
      return Services.cache2.diskCacheStorage(lci);
    case "memory":
      return Services.cache2.memoryCacheStorage(lci);
    case "pin":
      return Services.cache2.pinningCacheStorage(lci);
  }
  return null;
}

function OpenCacheEntry(key, where, flags, lci) {
  return new Promise(resolve => {
    key = createURI(key);
    function CacheListener() {}
    CacheListener.prototype = {
      QueryInterface: ChromeUtils.generateQI(["nsICacheEntryOpenCallback"]),

      onCacheEntryCheck(entry) {
        return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
      },

      onCacheEntryAvailable(entry, isnew, status) {
        resolve();
      },

      run() {
        let storage = getCacheStorage(where, lci);
        storage.asyncOpenURI(key, "", flags, this);
      },
    };

    new CacheListener().run();
  });
}

//
// Test functions.
//

// Cookies
async function test_cookie_cleared() {
  let tabs = [];

  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Load the page in 2 different contexts and set a cookie
    // which should only be visible in that context.
    let value = USER_CONTEXTS[userContextId];

    // Open our tab in the given user context.
    tabs[userContextId] = await openTabInUserContext(
      TEST_URL + "file_reflect_cookie_into_title.html?" + value,
      userContextId
    );

    // Close this tab.
    BrowserTestUtils.removeTab(tabs[userContextId].tab);
  }
  // Check that cookies have been set properly.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    let cookies = getCookiesForOA(TEST_HOST, userContextId);
    ok(cookies.length, "Cookies available");

    let foundCookie = cookies[0];
    Assert.equal(foundCookie.name, COOKIE_NAME, "Check cookie name");
    Assert.equal(
      foundCookie.value,
      USER_CONTEXTS[userContextId],
      "Check cookie value"
    );
  }

  // Forget the site.
  await ForgetAboutSite.removeDataFromDomain(TEST_HOST);

  // Check that whether cookies has been cleared or not.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    let cookies = getCookiesForOA(TEST_HOST, userContextId);
    ok(!cookies.length, "No Cookie should be here");
  }
}

// Cache
async function test_cache_cleared() {
  // First, add some caches.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    await OpenCacheEntry(
      "http://" + TEST_HOST + "/",
      "disk",
      Ci.nsICacheStorage.OPEN_NORMALLY,
      Services.loadContextInfo.custom(false, { userContextId })
    );

    await OpenCacheEntry(
      "http://" + TEST_HOST + "/",
      "memory",
      Ci.nsICacheStorage.OPEN_NORMALLY,
      Services.loadContextInfo.custom(false, { userContextId })
    );
  }

  // Check that caches have been set correctly.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    let mem = getCacheStorage(
      "memory",
      Services.loadContextInfo.custom(false, { userContextId })
    );
    let disk = getCacheStorage(
      "disk",
      Services.loadContextInfo.custom(false, { userContextId })
    );

    Assert.ok(
      mem.exists(createURI("http://" + TEST_HOST + "/"), ""),
      "The memory cache has been set correctly"
    );
    Assert.ok(
      disk.exists(createURI("http://" + TEST_HOST + "/"), ""),
      "The disk cache has been set correctly"
    );
  }

  // Forget the site.
  await ForgetAboutSite.removeDataFromDomain(TEST_HOST);

  // Check that do caches be removed or not?
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    let mem = getCacheStorage(
      "memory",
      Services.loadContextInfo.custom(false, { userContextId })
    );
    let disk = getCacheStorage(
      "disk",
      Services.loadContextInfo.custom(false, { userContextId })
    );

    Assert.ok(
      !mem.exists(createURI("http://" + TEST_HOST + "/"), ""),
      "The memory cache is cleared"
    );
    Assert.ok(
      !disk.exists(createURI("http://" + TEST_HOST + "/"), ""),
      "The disk cache is cleared"
    );
  }
}

// Image Cache
async function test_image_cache_cleared() {
  let tabs = [];

  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Open our tab in the given user context to cache image.
    tabs[userContextId] = await openTabInUserContext(
      "http://localhost:" +
        gHttpServer.identity.primaryPort +
        "/loadImage.html",
      userContextId
    );
    BrowserTestUtils.removeTab(tabs[userContextId].tab);
  }

  let expectedHits = USER_CONTEXTS.length;

  // Check that image cache works with the userContextId.
  is(
    gHits,
    expectedHits,
    "The image should be loaded" + expectedHits + "times."
  );

  // Reset the cache count.
  gHits = 0;

  // Forget the site.
  await ForgetAboutSite.removeDataFromDomain("localhost");

  // Load again.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Open our tab in the given user context to cache image.
    tabs[userContextId] = await openTabInUserContext(
      "http://localhost:" +
        gHttpServer.identity.primaryPort +
        "/loadImage.html",
      userContextId
    );
    BrowserTestUtils.removeTab(tabs[userContextId].tab);
  }

  // Check that image cache was cleared and the server gets another two hits.
  is(
    gHits,
    expectedHits,
    "The image should be loaded" + expectedHits + "times."
  );
}

// Offline Storage
async function test_storage_cleared() {
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Load the page in 2 different contexts and set the local storage
    // which should only be visible in that context.
    let value = USER_CONTEXTS[userContextId];

    // Open our tab in the given user context.
    let tabInfo = await openTabInUserContext(
      TEST_URL + "file_set_storages.html?" + value,
      userContextId
    );

    // Check that the storages has been set correctly.
    await SpecialPowers.spawn(
      tabInfo.browser,
      [{ userContext: USER_CONTEXTS[userContextId] }],
      async function(arg) {
        // Check that the local storage has been set correctly.
        Assert.equal(
          content.localStorage.getItem("userContext"),
          arg.userContext,
          "Check the local storage value"
        );

        // Check that the session storage has been set correctly.
        Assert.equal(
          content.sessionStorage.getItem("userContext"),
          arg.userContext,
          "Check the session storage value"
        );

        // Check that the indexedDB has been set correctly.
        let request = content.indexedDB.open("idb", 1);

        let db = await new Promise(done => {
          request.onsuccess = event => {
            done(event.target.result);
          };
        });

        let transaction = db.transaction(["obj"], "readonly");
        let store = transaction.objectStore("obj");
        let storeRequest = store.get(1);

        await new Promise(done => {
          storeRequest.onsuccess = event => {
            let res = storeRequest.result;
            Assert.equal(
              res.userContext,
              arg.userContext,
              "Check the indexedDB value"
            );
            done();
          };
        });
      }
    );

    // Close this tab.
    BrowserTestUtils.removeTab(tabInfo.tab);
  }

  // Forget the site.
  await ForgetAboutSite.removeDataFromDomain(TEST_HOST);

  // Open the tab again without setting the localStorage and check that the
  // local storage has been cleared or not.
  for (let userContextId of Object.keys(USER_CONTEXTS)) {
    // Open our tab in the given user context without setting local storage.
    let tabInfo = await openTabInUserContext(
      TEST_URL + "file_set_storages.html",
      userContextId
    );

    // Check that do storages be cleared or not.
    await SpecialPowers.spawn(tabInfo.browser, [], async function() {
      // Check that does the local storage be cleared or not.
      Assert.ok(
        !content.localStorage.getItem("userContext"),
        "The local storage has been cleared"
      );

      // Check that does the session storage be cleared or not.
      Assert.ok(
        !content.sessionStorage.getItem("userContext"),
        "The session storage has been cleared"
      );

      // Check that does the indexedDB be cleared or not.
      let request = content.indexedDB.open("idb", 1);

      let db = await new Promise(done => {
        request.onsuccess = event => {
          done(event.target.result);
        };
      });
      try {
        db.transaction(["obj"], "readonly");
        Assert.ok(false, "The indexedDB should not exist");
      } catch (e) {
        Assert.equal(
          e.name,
          "NotFoundError",
          "The indexedDB does not exist as expected"
        );
      }
    });

    // Close the tab.
    BrowserTestUtils.removeTab(tabInfo.tab);
  }
}

add_task(async function setup() {
  // Make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });

  // Create a http server for the image cache test.
  if (!gHttpServer) {
    gHttpServer = new HttpServer();
    gHttpServer.registerPathHandler("/image.png", imageHandler);
    gHttpServer.registerPathHandler("/loadImage.html", loadImagePageHandler);
    gHttpServer.start(-1);
  }
});

let tests = [
  test_cookie_cleared,
  test_cache_cleared,
  test_image_cache_cleared,
  test_storage_cleared,
];

add_task(async function test() {
  for (let i = 0; i < tests.length; i++) {
    add_task(tests[i]);
  }
});

registerCleanupFunction(() => {
  gHttpServer.stop(() => {
    gHttpServer = null;
  });
});

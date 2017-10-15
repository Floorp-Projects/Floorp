/**
 * Bug 1270338 - Add a mochitest to ensure Sanitizer clears data for all containers
 */

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

let {LoadContextInfo} = Cu.import("resource://gre/modules/LoadContextInfo.jsm", {});

const TEST_DOMAIN = "http://example.net/";

let tempScope = {};
Services.scriptloader.loadSubScript("chrome://browser/content/sanitize.js",
                                    tempScope);
let Sanitizer = tempScope.Sanitizer;

function setCookies(aBrowser) {
  ContentTask.spawn(aBrowser, null, function() {
    content.document.cookie = "key=value";
  });
}

function cacheDataForContext(loadContextInfo) {
  return new Promise(resolve => {
    let cachedURIs = [];
    let cacheVisitor = {
      onCacheStorageInfo(num, consumption) {},
      onCacheEntryInfo(uri, idEnhance) {
        cachedURIs.push(uri.asciiSpec);
      },
      onCacheEntryVisitCompleted() {
        resolve(cachedURIs);
      },
      QueryInterface(iid) {
        if (iid.equals(Ci.nsICacheStorageVisitor))
          return this;

        throw Components.results.NS_ERROR_NO_INTERFACE;
      }
    };
    // Visiting the disk cache also visits memory storage so we do not
    // need to use Services.cache2.memoryCacheStorage() here.
    let storage = Services.cache2.diskCacheStorage(loadContextInfo, false);
    storage.asyncVisitStorage(cacheVisitor, true);
  });
}

function checkCookiesSanitized(aBrowser) {
  ContentTask.spawn(aBrowser, null, function() {
    is(content.document.cookie, "",
       "Cookies of all origin attributes should be cleared.");
  });
}

function checkCacheExists(aShouldExist) {
  return async function() {
    let loadContextInfos = [
      LoadContextInfo.default,
      LoadContextInfo.custom(false, { userContextId: 1 }),
      LoadContextInfo.custom(false, { userContextId: 2 }),
      LoadContextInfo.custom(false, { firstPartyDomain: "example.com" }),
      LoadContextInfo.custom(false, { firstPartyDomain: "example.org" }),
    ];
    let i = 0;
    for (let loadContextInfo of loadContextInfos) {
      let cacheURIs = await cacheDataForContext(loadContextInfo);
      is(cacheURIs.includes(TEST_DOMAIN), aShouldExist, TEST_DOMAIN + " should "
        + (aShouldExist ? "not " : "") + "be cached for all origin attributes." + i++);
    }
  };
}

add_task(async function setup() {
  let networkCache = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
    .getService(Ci.nsICacheStorageService);
  networkCache.clear();
});

// This will set the cookies and the cache.
IsolationTestTools.runTests(TEST_DOMAIN, setCookies, () => true);

add_task(checkCacheExists(true));

add_task(async function sanitize() {
  let sanitizer = new Sanitizer();
  await sanitizer.sanitize(["cookies", "cache"]);
});

add_task(checkCacheExists(false));
IsolationTestTools.runTests(TEST_DOMAIN, checkCookiesSanitized, () => true);

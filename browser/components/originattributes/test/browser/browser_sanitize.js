/**
 * Bug 1270338 - Add a mochitest to ensure Sanitizer clears data for all containers
 */

const CC = Components.Constructor;

const TEST_DOMAIN = "http://example.net/";

const {Sanitizer} = ChromeUtils.import("resource:///modules/Sanitizer.jsm", {});

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
      QueryInterface: ChromeUtils.generateQI(["nsICacheStorageVisitor"])
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
      Services.loadContextInfo.default,
      Services.loadContextInfo.custom(false, { userContextId: 1 }),
      Services.loadContextInfo.custom(false, { userContextId: 2 }),
      Services.loadContextInfo.custom(false, { firstPartyDomain: "example.com" }),
      Services.loadContextInfo.custom(false, { firstPartyDomain: "example.org" }),
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
  Services.cache2.clear();
});

// This will set the cookies and the cache.
IsolationTestTools.runTests(TEST_DOMAIN, setCookies, () => true);

add_task(checkCacheExists(true));

add_task(async function sanitize() {
  await Sanitizer.sanitize(["cookies", "cache"]);
});

add_task(checkCacheExists(false));
IsolationTestTools.runTests(TEST_DOMAIN, checkCookiesSanitized, () => true);

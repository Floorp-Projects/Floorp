/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Simply tests that the notification is dispatched when new page data is
 * discovered.
 */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  PageDataCollector: "resource:///modules/pagedata/PageDataCollector.jsm",
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
  Services: "resource://gre/modules/Services.jsm",
  Snapshots: "resource:///modules/Snapshots.jsm",
});

add_task(async function test_pageDataDiscoverd_notifies() {
  let url = "https://www.mozilla.org/";

  Assert.equal(
    PageDataService.getCached(url),
    null,
    "Should be no cached data."
  );

  let promise = PageDataService.once("page-data");
  let fakeBrowser = {};

  PageDataService.pageDataDiscovered(
    url,
    [
      {
        type: PageDataCollector.DATA_TYPE.PRODUCT,
        data: {
          price: 276,
        },
      },
    ],
    fakeBrowser
  );

  let pageData = await promise;
  Assert.equal(
    pageData.url,
    "https://www.mozilla.org/",
    "Should have notified data for the expected url"
  );
  Assert.deepEqual(
    pageData.data,
    [
      {
        type: PageDataCollector.DATA_TYPE.PRODUCT,
        data: {
          price: 276,
        },
      },
    ],
    "Should have returned the correct product data"
  );
  Assert.equal(
    pageData.weakBrowser.get(),
    fakeBrowser,
    "Should have returned the fakeBrowser object"
  );

  Assert.deepEqual(
    PageDataService.getCached(url),
    pageData,
    "Should return the same pageData from the cache as was notified."
  );
});

add_task(async function test_queueFetch_notifies() {
  let promise = PageDataService.once("no-page-data");

  PageDataService.queueFetch("https://example.org");

  let pageData = await promise;
  Assert.equal(
    pageData.url,
    "https://example.org",
    "Should have notified data for the expected url"
  );
  Assert.equal(pageData.data.length, 0, "Should have returned no data");
  Assert.equal(
    pageData.weakBrowser,
    null,
    "Should have returned a null weakBrowser"
  );
});

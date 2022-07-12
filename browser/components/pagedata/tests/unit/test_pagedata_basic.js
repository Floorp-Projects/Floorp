/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Simply tests that the notification is dispatched when new page data is
 * discovered.
 */

ChromeUtils.defineESModuleGetters(this, {
  PageDataService: "resource:///modules/pagedata/PageDataService.sys.mjs",
});

add_task(async function test_pageDataDiscovered_notifies() {
  let url = "https://www.mozilla.org/";

  Assert.equal(
    PageDataService.getCached(url),
    null,
    "Should be no cached data."
  );

  let promise = PageDataService.once("page-data");

  PageDataService.pageDataDiscovered({
    url,
    date: 32453456,
    data: {
      [PageDataSchema.DATA_TYPE.PRODUCT]: {
        name: "Bolts",
        price: { value: 276 },
      },
    },
  });

  let pageData = await promise;
  Assert.equal(
    pageData.url,
    url,
    "Should have notified data for the expected url"
  );

  Assert.deepEqual(
    pageData,
    {
      url,
      date: 32453456,
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bolts",
          price: { value: 276 },
        },
      },
    },
    "Should have returned the correct product data"
  );

  Assert.equal(
    PageDataService.getCached(url),
    null,
    "Should not have cached the data as there was no actor locking."
  );

  let actor = {};
  PageDataService.lockEntry(actor, url);

  PageDataService.pageDataDiscovered({
    url,
    date: 32453456,
    data: {
      [PageDataSchema.DATA_TYPE.PRODUCT]: {
        name: "Bolts",
        price: { value: 276 },
      },
    },
  });

  // Should now be in the cache.
  Assert.deepEqual(
    PageDataService.getCached(url),
    {
      url,
      date: 32453456,
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bolts",
          price: { value: 276 },
        },
      },
    },
    "Should have cached the data"
  );

  PageDataService.unlockEntry(actor, url);

  Assert.equal(
    PageDataService.getCached(url),
    null,
    "Should have dropped the data from the cache."
  );
});

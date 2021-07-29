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
  Services: "resource://gre/modules/Services.jsm",
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
  Snapshots: "resource:///modules/Snapshots.jsm",
});

add_task(async function notifies() {
  let url = "https://www.mozilla.org/";

  Assert.equal(
    PageDataService.getCached(url),
    null,
    "Should be no cached data."
  );

  let listener = () => {
    Assert.ok(false, "Should not notify for no data.");
  };

  PageDataService.on("page-data", listener);

  let pageData = await PageDataService.queueFetch(url);
  Assert.equal(pageData.url, "https://www.mozilla.org/");
  Assert.equal(pageData.data.length, 0);

  pageData = PageDataService.getCached(url);
  Assert.equal(pageData.url, "https://www.mozilla.org/");
  Assert.equal(pageData.data.length, 0);

  PageDataService.off("page-data", listener);

  let promise = PageDataService.once("page-data");

  PageDataService.pageDataDiscovered(url, [
    {
      type: Snapshots.DATA_TYPE.PRODUCT,
      data: {
        price: 276,
      },
    },
  ]);

  pageData = await promise;
  Assert.equal(pageData.url, "https://www.mozilla.org/");
  Assert.equal(pageData.data.length, 1);
  Assert.equal(pageData.data[0].type, Snapshots.DATA_TYPE.PRODUCT);

  Assert.equal(PageDataService.getCached(url), pageData);
  Assert.equal(await PageDataService.queueFetch(url), pageData);
});

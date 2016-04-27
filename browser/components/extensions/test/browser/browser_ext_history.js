/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
                                  "resource://testing-common/PlacesTestUtils.jsm");

add_task(function* test_delete() {
  function background() {
    browser.test.onMessage.addListener((msg, arg) => {
      if (msg === "delete-url") {
        browser.history.deleteUrl({url: arg}).then(result => {
          browser.test.assertEq(undefined, result, "browser.history.deleteUrl returns nothing");
          browser.test.sendMessage("url-deleted");
        });
      } else if (msg === "delete-range") {
        browser.history.deleteRange(arg).then(result => {
          browser.test.assertEq(undefined, result, "browser.history.deleteUrl returns nothing");
          browser.test.sendMessage("range-deleted");
        });
      } else if (msg === "delete-all") {
        browser.history.deleteAll().then(result => {
          browser.test.assertEq(undefined, result, "browser.history.deleteUrl returns nothing");
          browser.test.sendMessage("urls-deleted");
        });
      }
    });

    browser.test.sendMessage("ready");
  }

  const REFERENCE_DATE = new Date(1999, 9, 9, 9, 9);
  let {PlacesUtils} = Cu.import("resource://gre/modules/PlacesUtils.jsm", {});

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background: `(${background})()`,
  });

  yield extension.startup();
  yield PlacesTestUtils.clearHistory();
  yield extension.awaitMessage("ready");

  let visits = [];

  // Add 5 visits for one uri and 3 visits for 3 others
  for (let i = 0; i < 8; ++i) {
    let baseUri = "http://mozilla.com/test_history/";
    let uri = (i > 4) ? `${baseUri}${i}/` : baseUri;
    let dbDate = (Number(REFERENCE_DATE) + 3600 * 1000 * i) * 1000;

    let visit = {
      uri,
      title: "visit " + i,
      visitDate: dbDate,
    };
    visits.push(visit);
  }

  yield PlacesTestUtils.addVisits(visits);

  is(yield PlacesTestUtils.visitsInDB(visits[0].uri), 5, "5 visits for uri found in history database");

  let testUrl = visits[6].uri.spec;
  ok(yield PlacesTestUtils.isPageInDB(testUrl), "expected url found in history database");

  extension.sendMessage("delete-url", testUrl);
  yield extension.awaitMessage("url-deleted");
  is(yield PlacesTestUtils.isPageInDB(testUrl), false, "expected url not found in history database");

  let filter = {
    startTime: visits[1].visitDate / 1000,
    endTime: visits[3].visitDate / 1000,
  };

  extension.sendMessage("delete-range", filter);
  yield extension.awaitMessage("range-deleted");

  ok(yield PlacesTestUtils.isPageInDB(visits[0].uri), "expected uri found in history database");
  is(yield PlacesTestUtils.visitsInDB(visits[0].uri), 2, "2 visits for uri found in history database");
  ok(yield PlacesTestUtils.isPageInDB(visits[5].uri), "expected uri found in history database");
  is(yield PlacesTestUtils.visitsInDB(visits[5].uri), 1, "1 visit for uri found in history database");

  filter.startTime = visits[0].visitDate / 1000;
  filter.endTime = visits[5].visitDate / 1000;

  extension.sendMessage("delete-range", filter);
  yield extension.awaitMessage("range-deleted");

  is(yield PlacesTestUtils.isPageInDB(visits[0].uri), false, "expected uri not found in history database");
  is(yield PlacesTestUtils.visitsInDB(visits[0].uri), 0, "0 visits for uri found in history database");
  is(yield PlacesTestUtils.isPageInDB(visits[5].uri), false, "expected uri not found in history database");
  is(yield PlacesTestUtils.visitsInDB(visits[5].uri), 0, "0 visits for uri found in history database");

  ok(yield PlacesTestUtils.isPageInDB(visits[7].uri), "expected uri found in history database");

  extension.sendMessage("delete-all");
  yield extension.awaitMessage("urls-deleted");
  is(PlacesUtils.history.hasHistoryEntries, false, "history is empty");

  yield extension.unload();
});

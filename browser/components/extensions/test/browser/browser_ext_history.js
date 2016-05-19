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
    let dbDate = PlacesUtils.toPRTime(Number(REFERENCE_DATE) + 3600 * 1000 * i);

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
    startTime: PlacesUtils.toTime(visits[1].visitDate),
    endTime: PlacesUtils.toTime(visits[3].visitDate),
  };

  extension.sendMessage("delete-range", filter);
  yield extension.awaitMessage("range-deleted");

  ok(yield PlacesTestUtils.isPageInDB(visits[0].uri), "expected uri found in history database");
  is(yield PlacesTestUtils.visitsInDB(visits[0].uri), 2, "2 visits for uri found in history database");
  ok(yield PlacesTestUtils.isPageInDB(visits[5].uri), "expected uri found in history database");
  is(yield PlacesTestUtils.visitsInDB(visits[5].uri), 1, "1 visit for uri found in history database");

  filter.startTime = PlacesUtils.toTime(visits[0].visitDate);
  filter.endTime = PlacesUtils.toTime(visits[5].visitDate);

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

add_task(function* test_search() {
  const SINGLE_VISIT_URL = "http://example.com/";
  const DOUBLE_VISIT_URL = "http://example.com/2/";
  const MOZILLA_VISIT_URL = "http://mozilla.com/";

  function background() {
    const futureTime = Date.now() + 24 * 60 * 60 * 1000;

    browser.test.onMessage.addListener(msg => {
      browser.history.search({text: ""}).then(results => {
        browser.test.sendMessage("empty-search", results);
        return browser.history.search({text: "mozilla.com"});
      }).then(results => {
        browser.test.sendMessage("text-search", results);
        return browser.history.search({text: "example.com", maxResults: 1});
      }).then(results => {
        browser.test.sendMessage("max-results-search", results);
        return browser.history.search({text: "", startTime: futureTime});
      }).then(results => {
        browser.test.assertEq(0, results.length, "no results returned for late start time");
        return browser.history.search({text: "", endTime: 0});
      }).then(results => {
        browser.test.assertEq(0, results.length, "no results returned for early end time");
        return browser.history.search({text: "", startTime: Date.now(), endTime: 0});
      }).then(results => {
        browser.test.fail("history.search rejects with startTime that is after the endTime");
      }, error => {
        browser.test.assertEq(
          "The startTime cannot be after the endTime",
          error.message,
          "history.search rejects with startTime that is after the endTime");
      }).then(() => {
        browser.test.notifyPass("search");
      });
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background: `(${background})()`,
  });

  function findResult(url, results) {
    return results.find(r => r.url === url);
  }

  function checkResult(results, url, expectedCount) {
    let result = findResult(url, results);
    isnot(result, null, `history.search result was found for ${url}`);
    is(result.visitCount, expectedCount, `history.search reports ${expectedCount} visit(s)`);
    is(result.title, `test visit for ${url}`, "title for search result is correct");
  }

  yield extension.startup();
  yield extension.awaitMessage("ready");
  yield PlacesTestUtils.clearHistory();

  yield PlacesTestUtils.addVisits([
    {uri: makeURI(MOZILLA_VISIT_URL)},
    {uri: makeURI(DOUBLE_VISIT_URL)},
    {uri: makeURI(SINGLE_VISIT_URL)},
    {uri: makeURI(DOUBLE_VISIT_URL)},
  ]);

  extension.sendMessage("check-history");

  let results = yield extension.awaitMessage("empty-search");
  is(results.length, 3, "history.search returned 3 results");
  checkResult(results, SINGLE_VISIT_URL, 1);
  checkResult(results, DOUBLE_VISIT_URL, 2);
  checkResult(results, MOZILLA_VISIT_URL, 1);

  results = yield extension.awaitMessage("text-search");
  is(results.length, 1, "history.search returned 1 result");
  checkResult(results, MOZILLA_VISIT_URL, 1);

  results = yield extension.awaitMessage("max-results-search");
  is(results.length, 1, "history.search returned 1 result");
  checkResult(results, DOUBLE_VISIT_URL, 2);

  yield extension.awaitFinish("search");
  yield extension.unload();
  yield PlacesTestUtils.clearHistory();
});

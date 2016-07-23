/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
                                  "resource://testing-common/PlacesTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionUtils",
                                  "resource://gre/modules/ExtensionUtils.jsm");

add_task(function* test_delete() {
  function background() {
    let historyClearedCount = 0;
    let removedUrls = [];

    browser.history.onVisitRemoved.addListener(data => {
      if (data.allHistory) {
        historyClearedCount++;
      } else {
        browser.test.assertEq(1, data.urls.length, "onVisitRemoved received one URL");
        removedUrls.push(data.urls[0]);
      }
    });

    browser.test.onMessage.addListener((msg, arg) => {
      if (msg === "delete-url") {
        browser.history.deleteUrl({url: arg}).then(result => {
          browser.test.assertEq(undefined, result, "browser.history.deleteUrl returns nothing");
          browser.test.sendMessage("url-deleted");
        });
      } else if (msg === "delete-range") {
        browser.history.deleteRange(arg).then(result => {
          browser.test.assertEq(undefined, result, "browser.history.deleteRange returns nothing");
          browser.test.sendMessage("range-deleted", removedUrls);
        });
      } else if (msg === "delete-all") {
        browser.history.deleteAll().then(result => {
          browser.test.assertEq(undefined, result, "browser.history.deleteAll returns nothing");
          browser.test.sendMessage("history-cleared", [historyClearedCount, removedUrls]);
        });
      }
    });

    browser.test.sendMessage("ready");
  }

  const BASE_URL = "http://mozilla.com/test_history/";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background: `(${background})()`,
  });

  yield extension.startup();
  yield extension.awaitMessage("ready");
  yield PlacesTestUtils.clearHistory();

  let historyClearedCount;
  let visits = [];
  let visitDate = new Date(1999, 9, 9, 9, 9).getTime();

  function pushVisit(visits) {
    visitDate += 1000;
    visits.push({date: new Date(visitDate)});
  }

  // Add 5 visits for one uri and 3 visits for 3 others
  for (let i = 0; i < 4; ++i) {
    let visit = {
      url: `${BASE_URL}${i}`,
      title: "visit " + i,
      visits: [],
    };
    if (i === 0) {
      for (let j = 0; j < 5; ++j) {
        pushVisit(visit.visits);
      }
    } else {
      pushVisit(visit.visits);
    }
    visits.push(visit);
  }

  yield PlacesUtils.history.insertMany(visits);
  equal((yield PlacesTestUtils.visitsInDB(visits[0].url)), 5, "5 visits for uri found in history database");

  let testUrl = visits[2].url;
  ok((yield PlacesTestUtils.isPageInDB(testUrl)), "expected url found in history database");

  extension.sendMessage("delete-url", testUrl);
  yield extension.awaitMessage("url-deleted");
  equal((yield PlacesTestUtils.isPageInDB(testUrl)), false, "expected url not found in history database");

  // delete 3 of the 5 visits for url 1
  let filter = {
    startTime: visits[0].visits[0].date,
    endTime: visits[0].visits[2].date,
  };

  extension.sendMessage("delete-range", filter);
  let removedUrls = yield extension.awaitMessage("range-deleted");
  ok(!removedUrls.includes(visits[0].url), `${visits[0].url} not received by onVisitRemoved`);
  ok((yield PlacesTestUtils.isPageInDB(visits[0].url)), "expected uri found in history database");
  equal((yield PlacesTestUtils.visitsInDB(visits[0].url)), 2, "2 visits for uri found in history database");
  ok((yield PlacesTestUtils.isPageInDB(visits[1].url)), "expected uri found in history database");
  equal((yield PlacesTestUtils.visitsInDB(visits[1].url)), 1, "1 visit for uri found in history database");

  // delete the rest of the visits for url 1, and the visit for url 2
  filter.startTime = visits[0].visits[0].date;
  filter.endTime = visits[1].visits[0].date;

  extension.sendMessage("delete-range", filter);
  yield extension.awaitMessage("range-deleted");

  equal((yield PlacesTestUtils.isPageInDB(visits[0].url)), false, "expected uri not found in history database");
  equal((yield PlacesTestUtils.visitsInDB(visits[0].url)), 0, "0 visits for uri found in history database");
  equal((yield PlacesTestUtils.isPageInDB(visits[1].url)), false, "expected uri not found in history database");
  equal((yield PlacesTestUtils.visitsInDB(visits[1].url)), 0, "0 visits for uri found in history database");

  ok((yield PlacesTestUtils.isPageInDB(visits[3].url)), "expected uri found in history database");

  extension.sendMessage("delete-all");
  [historyClearedCount, removedUrls] = yield extension.awaitMessage("history-cleared");
  equal(PlacesUtils.history.hasHistoryEntries, false, "history is empty");
  equal(historyClearedCount, 2, "onVisitRemoved called for each clearing of history");
  equal(removedUrls.length, 3, "onVisitRemoved called the expected number of times");
  for (let i = 1; i < 3; ++i) {
    let url = visits[i].url;
    ok(removedUrls.includes(url), `${url} received by onVisitRemoved`);
  }
  yield extension.unload();
});

add_task(function* test_search() {
  const SINGLE_VISIT_URL = "http://example.com/";
  const DOUBLE_VISIT_URL = "http://example.com/2/";
  const MOZILLA_VISIT_URL = "http://mozilla.com/";
  const REFERENCE_DATE = new Date();
  // pages/visits to add via History.insert
  const PAGE_INFOS = [
    {
      url: SINGLE_VISIT_URL,
      title: `test visit for ${SINGLE_VISIT_URL}`,
      visits: [
        {date: new Date(Number(REFERENCE_DATE) - 1000)},
      ],
    },
    {
      url: DOUBLE_VISIT_URL,
      title: `test visit for ${DOUBLE_VISIT_URL}`,
      visits: [
        {date: REFERENCE_DATE},
        {date: new Date(Number(REFERENCE_DATE) - 2000)},
      ],
    },
    {
      url: MOZILLA_VISIT_URL,
      title: `test visit for ${MOZILLA_VISIT_URL}`,
      visits: [
        {date: new Date(Number(REFERENCE_DATE) - 3000)},
      ],
    },
  ];

  function background(REFERENCE_DATE) {
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
        return browser.history.search({text: "", startTime: REFERENCE_DATE - 2000, endTime: REFERENCE_DATE - 1000});
      }).then(results => {
        browser.test.sendMessage("date-range-search", results);
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
    background: `(${background})(${Number(REFERENCE_DATE)})`,
  });

  function findResult(url, results) {
    return results.find(r => r.url === url);
  }

  function checkResult(results, url, expectedCount) {
    let result = findResult(url, results);
    notEqual(result, null, `history.search result was found for ${url}`);
    equal(result.visitCount, expectedCount, `history.search reports ${expectedCount} visit(s)`);
    equal(result.title, `test visit for ${url}`, "title for search result is correct");
  }

  yield extension.startup();
  yield extension.awaitMessage("ready");
  yield PlacesTestUtils.clearHistory();

  yield PlacesUtils.history.insertMany(PAGE_INFOS);

  extension.sendMessage("check-history");

  let results = yield extension.awaitMessage("empty-search");
  equal(results.length, 3, "history.search with empty text returned 3 results");
  checkResult(results, SINGLE_VISIT_URL, 1);
  checkResult(results, DOUBLE_VISIT_URL, 2);
  checkResult(results, MOZILLA_VISIT_URL, 1);

  results = yield extension.awaitMessage("text-search");
  equal(results.length, 1, "history.search with specific text returned 1 result");
  checkResult(results, MOZILLA_VISIT_URL, 1);

  results = yield extension.awaitMessage("max-results-search");
  equal(results.length, 1, "history.search with maxResults returned 1 result");
  checkResult(results, DOUBLE_VISIT_URL, 2);

  results = yield extension.awaitMessage("date-range-search");
  equal(results.length, 2, "history.search with a date range returned 2 result");
  checkResult(results, DOUBLE_VISIT_URL, 2);
  checkResult(results, SINGLE_VISIT_URL, 1);

  yield extension.awaitFinish("search");
  yield extension.unload();
  yield PlacesTestUtils.clearHistory();
});

add_task(function* test_add_url() {
  function background() {
    const TEST_DOMAIN = "http://example.com/";

    browser.test.onMessage.addListener((msg, testData) => {
      let [details, type] = testData;
      details.url = details.url || `${TEST_DOMAIN}${type}`;
      if (msg === "add-url") {
        details.title = `Title for ${type}`;
        browser.history.addUrl(details).then(() => {
          return browser.history.search({text: details.url});
        }).then(results => {
          browser.test.assertEq(1, results.length, "1 result found when searching for added URL");
          browser.test.sendMessage("url-added", {details, result: results[0]});
        });
      } else if (msg === "expect-failure") {
        let expectedMsg = testData[2];
        browser.history.addUrl(details).then(() => {
          browser.test.fail(`Expected error thrown for ${type}`);
        }, error => {
          browser.test.assertTrue(
            error.message.includes(expectedMsg),
            `"Expected error thrown when trying to add a URL with  ${type}`
          );
          browser.test.sendMessage("add-failed");
        });
      }
    });

    browser.test.sendMessage("ready");
  }

  let addTestData = [
    [{}, "default"],
    [{visitTime: new Date()}, "with_date"],
    [{visitTime: Date.now()}, "with_ms_number"],
    [{visitTime: Date.now().toString()}, "with_ms_string"],
    [{visitTime: new Date().toISOString()}, "with_iso_string"],
    [{transition: "typed"}, "valid_transition"],
  ];

  let failTestData = [
    [{transition: "generated"}, "an invalid transition", "|generated| is not a supported transition for history"],
    [{visitTime: Date.now() + 1000000}, "a future date", "cannot be a future date"],
    [{url: "about.config"}, "an invalid url", "about.config is not a valid URL"],
  ];

  function* checkUrl(results) {
    ok((yield PlacesTestUtils.isPageInDB(results.details.url)), `${results.details.url} found in history database`);
    ok(PlacesUtils.isValidGuid(results.result.id), "URL was added with a valid id");
    equal(results.result.title, results.details.title, "URL was added with the correct title");
    if (results.details.visitTime) {
      equal(results.result.lastVisitTime,
         Number(ExtensionUtils.normalizeTime(results.details.visitTime)),
         "URL was added with the correct date");
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background: `(${background})()`,
  });

  yield PlacesTestUtils.clearHistory();
  yield extension.startup();
  yield extension.awaitMessage("ready");

  for (let data of addTestData) {
    extension.sendMessage("add-url", data);
    let results = yield extension.awaitMessage("url-added");
    yield checkUrl(results);
  }

  for (let data of failTestData) {
    extension.sendMessage("expect-failure", data);
    yield extension.awaitMessage("add-failed");
  }

  yield extension.unload();
});

add_task(function* test_get_visits() {
  function background() {
    const TEST_DOMAIN = "http://example.com/";
    const FIRST_DATE = Date.now();
    const INITIAL_DETAILS = {
      url: TEST_DOMAIN,
      visitTime: FIRST_DATE,
      transition: "link",
    };

    let visitIds = new Set();

    function checkVisit(visit, expected) {
      visitIds.add(visit.visitId);
      browser.test.assertEq(expected.visitTime, visit.visitTime, "visit has the correct visitTime");
      browser.test.assertEq(expected.transition, visit.transition, "visit has the correct transition");
      browser.history.search({text: expected.url}).then(results => {
        // all results will have the same id, so we only need to use the first one
        browser.test.assertEq(results[0].id, visit.id, "visit has the correct id");
      });
    }

    let details = Object.assign({}, INITIAL_DETAILS);

    browser.history.addUrl(details).then(() => {
      return browser.history.getVisits({url: details.url});
    }).then(results => {
      browser.test.assertEq(1, results.length, "the expected number of visits were returned");
      checkVisit(results[0], details);
      details.url = `${TEST_DOMAIN}/1/`;
      return browser.history.addUrl(details);
    }).then(() => {
      return browser.history.getVisits({url: details.url});
    }).then(results => {
      browser.test.assertEq(1, results.length, "the expected number of visits were returned");
      checkVisit(results[0], details);
      details.visitTime = FIRST_DATE - 1000;
      details.transition = "typed";
      return browser.history.addUrl(details);
    }).then(() => {
      return browser.history.getVisits({url: details.url});
    }).then(results => {
      browser.test.assertEq(2, results.length, "the expected number of visits were returned");
      checkVisit(results[0], INITIAL_DETAILS);
      checkVisit(results[1], details);
    }).then(() => {
      browser.test.assertEq(3, visitIds.size, "each visit has a unique visitId");
      browser.test.notifyPass("get-visits");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background: `(${background})()`,
  });

  yield PlacesTestUtils.clearHistory();
  yield extension.startup();

  yield extension.awaitFinish("get-visits");
  yield extension.unload();
});

add_task(function* test_on_visited() {
  const SINGLE_VISIT_URL = "http://example.com/1/";
  const DOUBLE_VISIT_URL = "http://example.com/2/";
  let visitDate = new Date(1999, 9, 9, 9, 9).getTime();

  // pages/visits to add via History.insertMany
  const PAGE_INFOS = [
    {
      url: SINGLE_VISIT_URL,
      title: `visit to ${SINGLE_VISIT_URL}`,
      visits: [
        {date: new Date(visitDate)},
      ],
    },
    {
      url: DOUBLE_VISIT_URL,
      title: `visit to ${DOUBLE_VISIT_URL}`,
      visits: [
        {date: new Date(visitDate += 1000)},
        {date: new Date(visitDate += 1000)},
      ],
    },
  ];

  function background() {
    let onVisitedData = [];

    browser.history.onVisited.addListener(data => {
      if (data.url.includes("moz-extension")) {
        return;
      }
      onVisitedData.push(data);
      if (onVisitedData.length == 3) {
        browser.test.sendMessage("on-visited-data", onVisitedData);
      }
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background: `(${background})()`,
  });

  yield PlacesTestUtils.clearHistory();
  yield extension.startup();
  yield extension.awaitMessage("ready");

  yield PlacesUtils.history.insertMany(PAGE_INFOS);

  let onVisitedData = yield extension.awaitMessage("on-visited-data");

  function checkOnVisitedData(index, expected) {
    let onVisited = onVisitedData[index];
    ok(PlacesUtils.isValidGuid(onVisited.id), "onVisited received a valid id");
    equal(onVisited.url, expected.url, "onVisited received the expected url");
    // Title will be blank until bug 1287928 lands
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1287928
    equal(onVisited.title, "", "onVisited received a blank title");
    equal(onVisited.lastVisitTime, expected.time, "onVisited received the expected time");
    equal(onVisited.visitCount, expected.visitCount, "onVisited received the expected visitCount");
  }

  let expected = {
    url: PAGE_INFOS[0].url,
    title: PAGE_INFOS[0].title,
    time: PAGE_INFOS[0].visits[0].date.getTime(),
    visitCount: 1,
  };
  checkOnVisitedData(0, expected);

  expected.url = PAGE_INFOS[1].url;
  expected.title = PAGE_INFOS[1].title;
  expected.time = PAGE_INFOS[1].visits[0].date.getTime();
  checkOnVisitedData(1, expected);

  expected.time = PAGE_INFOS[1].visits[1].date.getTime();
  expected.visitCount = 2;
  checkOnVisitedData(2, expected);

  yield extension.unload();
});

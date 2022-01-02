/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionCommon",
  "resource://gre/modules/ExtensionCommon.jsm"
);

add_task(async function test_delete() {
  function background() {
    let historyClearedCount = 0;
    let removedUrls = [];

    browser.history.onVisitRemoved.addListener(data => {
      if (data.allHistory) {
        historyClearedCount++;
        browser.test.assertEq(
          0,
          data.urls.length,
          "onVisitRemoved received an empty urls array"
        );
      } else {
        removedUrls.push(...data.urls);
      }
    });

    browser.test.onMessage.addListener((msg, arg) => {
      if (msg === "delete-url") {
        browser.history.deleteUrl({ url: arg }).then(result => {
          browser.test.assertEq(
            undefined,
            result,
            "browser.history.deleteUrl returns nothing"
          );
          browser.test.sendMessage("url-deleted");
        });
      } else if (msg === "delete-range") {
        browser.history.deleteRange(arg).then(result => {
          browser.test.assertEq(
            undefined,
            result,
            "browser.history.deleteRange returns nothing"
          );
          browser.test.sendMessage("range-deleted", removedUrls);
        });
      } else if (msg === "delete-all") {
        browser.history.deleteAll().then(result => {
          browser.test.assertEq(
            undefined,
            result,
            "browser.history.deleteAll returns nothing"
          );
          browser.test.sendMessage("history-cleared", [
            historyClearedCount,
            removedUrls,
          ]);
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

  await extension.startup();
  await extension.awaitMessage("ready");
  await PlacesUtils.history.clear();

  let historyClearedCount;
  let visits = [];
  let visitDate = new Date(1999, 9, 9, 9, 9).getTime();

  function pushVisit(subvisits) {
    visitDate += 1000;
    subvisits.push({ date: new Date(visitDate) });
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

  await PlacesUtils.history.insertMany(visits);
  equal(
    await PlacesTestUtils.visitsInDB(visits[0].url),
    5,
    "5 visits for uri found in history database"
  );

  let testUrl = visits[2].url;
  ok(
    await PlacesTestUtils.isPageInDB(testUrl),
    "expected url found in history database"
  );

  extension.sendMessage("delete-url", testUrl);
  await extension.awaitMessage("url-deleted");
  equal(
    await PlacesTestUtils.isPageInDB(testUrl),
    false,
    "expected url not found in history database"
  );

  // delete 3 of the 5 visits for url 1
  let filter = {
    startTime: visits[0].visits[0].date,
    endTime: visits[0].visits[2].date,
  };

  extension.sendMessage("delete-range", filter);
  let removedUrls = await extension.awaitMessage("range-deleted");
  ok(
    !removedUrls.includes(visits[0].url),
    `${visits[0].url} not received by onVisitRemoved`
  );
  ok(
    await PlacesTestUtils.isPageInDB(visits[0].url),
    "expected uri found in history database"
  );
  equal(
    await PlacesTestUtils.visitsInDB(visits[0].url),
    2,
    "2 visits for uri found in history database"
  );
  ok(
    await PlacesTestUtils.isPageInDB(visits[1].url),
    "expected uri found in history database"
  );
  equal(
    await PlacesTestUtils.visitsInDB(visits[1].url),
    1,
    "1 visit for uri found in history database"
  );

  // delete the rest of the visits for url 1, and the visit for url 2
  filter.startTime = visits[0].visits[0].date;
  filter.endTime = visits[1].visits[0].date;

  extension.sendMessage("delete-range", filter);
  await extension.awaitMessage("range-deleted");

  equal(
    await PlacesTestUtils.isPageInDB(visits[0].url),
    false,
    "expected uri not found in history database"
  );
  equal(
    await PlacesTestUtils.visitsInDB(visits[0].url),
    0,
    "0 visits for uri found in history database"
  );
  equal(
    await PlacesTestUtils.isPageInDB(visits[1].url),
    false,
    "expected uri not found in history database"
  );
  equal(
    await PlacesTestUtils.visitsInDB(visits[1].url),
    0,
    "0 visits for uri found in history database"
  );

  ok(
    await PlacesTestUtils.isPageInDB(visits[3].url),
    "expected uri found in history database"
  );

  extension.sendMessage("delete-all");
  [historyClearedCount, removedUrls] = await extension.awaitMessage(
    "history-cleared"
  );
  equal(
    historyClearedCount,
    2,
    "onVisitRemoved called for each clearing of history"
  );
  equal(
    removedUrls.length,
    3,
    "onVisitRemoved called the expected number of times"
  );
  for (let i = 1; i < 3; ++i) {
    let url = visits[i].url;
    ok(removedUrls.includes(url), `${url} received by onVisitRemoved`);
  }
  await extension.unload();
});

add_task(async function test_search() {
  const SINGLE_VISIT_URL = "http://example.com/";
  const DOUBLE_VISIT_URL = "http://example.com/2/";
  const MOZILLA_VISIT_URL = "http://mozilla.com/";
  const REFERENCE_DATE = new Date();
  // pages/visits to add via History.insert
  const PAGE_INFOS = [
    {
      url: SINGLE_VISIT_URL,
      title: `test visit for ${SINGLE_VISIT_URL}`,
      visits: [{ date: new Date(Number(REFERENCE_DATE) - 1000) }],
    },
    {
      url: DOUBLE_VISIT_URL,
      title: `test visit for ${DOUBLE_VISIT_URL}`,
      visits: [
        { date: REFERENCE_DATE },
        { date: new Date(Number(REFERENCE_DATE) - 2000) },
      ],
    },
    {
      url: MOZILLA_VISIT_URL,
      title: `test visit for ${MOZILLA_VISIT_URL}`,
      visits: [{ date: new Date(Number(REFERENCE_DATE) - 3000) }],
    },
  ];

  function background(BGSCRIPT_REFERENCE_DATE) {
    const futureTime = Date.now() + 24 * 60 * 60 * 1000;

    browser.test.onMessage.addListener(msg => {
      browser.history
        .search({ text: "" })
        .then(results => {
          browser.test.sendMessage("empty-search", results);
          return browser.history.search({ text: "mozilla.com" });
        })
        .then(results => {
          browser.test.sendMessage("text-search", results);
          return browser.history.search({ text: "example.com", maxResults: 1 });
        })
        .then(results => {
          browser.test.sendMessage("max-results-search", results);
          return browser.history.search({
            text: "",
            startTime: BGSCRIPT_REFERENCE_DATE - 2000,
            endTime: BGSCRIPT_REFERENCE_DATE - 1000,
          });
        })
        .then(results => {
          browser.test.sendMessage("date-range-search", results);
          return browser.history.search({ text: "", startTime: futureTime });
        })
        .then(results => {
          browser.test.assertEq(
            0,
            results.length,
            "no results returned for late start time"
          );
          return browser.history.search({ text: "", endTime: 0 });
        })
        .then(results => {
          browser.test.assertEq(
            0,
            results.length,
            "no results returned for early end time"
          );
          return browser.history.search({
            text: "",
            startTime: Date.now(),
            endTime: 0,
          });
        })
        .then(
          results => {
            browser.test.fail(
              "history.search rejects with startTime that is after the endTime"
            );
          },
          error => {
            browser.test.assertEq(
              "The startTime cannot be after the endTime",
              error.message,
              "history.search rejects with startTime that is after the endTime"
            );
          }
        )
        .then(() => {
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
    equal(
      result.visitCount,
      expectedCount,
      `history.search reports ${expectedCount} visit(s)`
    );
    equal(
      result.title,
      `test visit for ${url}`,
      "title for search result is correct"
    );
  }

  await extension.startup();
  await extension.awaitMessage("ready");
  await PlacesUtils.history.clear();

  await PlacesUtils.history.insertMany(PAGE_INFOS);

  extension.sendMessage("check-history");

  let results = await extension.awaitMessage("empty-search");
  equal(results.length, 3, "history.search with empty text returned 3 results");
  checkResult(results, SINGLE_VISIT_URL, 1);
  checkResult(results, DOUBLE_VISIT_URL, 2);
  checkResult(results, MOZILLA_VISIT_URL, 1);

  results = await extension.awaitMessage("text-search");
  equal(
    results.length,
    1,
    "history.search with specific text returned 1 result"
  );
  checkResult(results, MOZILLA_VISIT_URL, 1);

  results = await extension.awaitMessage("max-results-search");
  equal(results.length, 1, "history.search with maxResults returned 1 result");
  checkResult(results, DOUBLE_VISIT_URL, 2);

  results = await extension.awaitMessage("date-range-search");
  equal(
    results.length,
    2,
    "history.search with a date range returned 2 result"
  );
  checkResult(results, DOUBLE_VISIT_URL, 2);
  checkResult(results, SINGLE_VISIT_URL, 1);

  await extension.awaitFinish("search");
  await extension.unload();
  await PlacesUtils.history.clear();
});

add_task(async function test_add_url() {
  function background() {
    const TEST_DOMAIN = "http://example.com/";

    browser.test.onMessage.addListener((msg, testData) => {
      let [details, type] = testData;
      details.url = details.url || `${TEST_DOMAIN}${type}`;
      if (msg === "add-url") {
        details.title = `Title for ${type}`;
        browser.history
          .addUrl(details)
          .then(() => {
            return browser.history.search({ text: details.url });
          })
          .then(results => {
            browser.test.assertEq(
              1,
              results.length,
              "1 result found when searching for added URL"
            );
            browser.test.sendMessage("url-added", {
              details,
              result: results[0],
            });
          });
      } else if (msg === "expect-failure") {
        let expectedMsg = testData[2];
        browser.history.addUrl(details).then(
          () => {
            browser.test.fail(`Expected error thrown for ${type}`);
          },
          error => {
            browser.test.assertTrue(
              error.message.includes(expectedMsg),
              `"Expected error thrown when trying to add a URL with  ${type}`
            );
            browser.test.sendMessage("add-failed");
          }
        );
      }
    });

    browser.test.sendMessage("ready");
  }

  let addTestData = [
    [{}, "default"],
    [{ visitTime: new Date() }, "with_date"],
    [{ visitTime: Date.now() }, "with_ms_number"],
    [{ visitTime: new Date().toISOString() }, "with_iso_string"],
    [{ transition: "typed" }, "valid_transition"],
  ];

  let failTestData = [
    [
      { transition: "generated" },
      "an invalid transition",
      "|generated| is not a supported transition for history",
    ],
    [{ visitTime: Date.now() + 1000000 }, "a future date", "Invalid value"],
    [{ url: "about.config" }, "an invalid url", "Invalid value"],
  ];

  async function checkUrl(results) {
    ok(
      await PlacesTestUtils.isPageInDB(results.details.url),
      `${results.details.url} found in history database`
    );
    ok(
      PlacesUtils.isValidGuid(results.result.id),
      "URL was added with a valid id"
    );
    equal(
      results.result.title,
      results.details.title,
      "URL was added with the correct title"
    );
    if (results.details.visitTime) {
      equal(
        results.result.lastVisitTime,
        Number(ExtensionCommon.normalizeTime(results.details.visitTime)),
        "URL was added with the correct date"
      );
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background: `(${background})()`,
  });

  await PlacesUtils.history.clear();
  await extension.startup();
  await extension.awaitMessage("ready");

  for (let data of addTestData) {
    extension.sendMessage("add-url", data);
    let results = await extension.awaitMessage("url-added");
    await checkUrl(results);
  }

  for (let data of failTestData) {
    extension.sendMessage("expect-failure", data);
    await extension.awaitMessage("add-failed");
  }

  await extension.unload();
});

add_task(async function test_get_visits() {
  async function background() {
    const TEST_DOMAIN = "http://example.com/";
    const FIRST_DATE = Date.now();
    const INITIAL_DETAILS = {
      url: TEST_DOMAIN,
      visitTime: FIRST_DATE,
      transition: "link",
    };

    let visitIds = new Set();

    async function checkVisit(visit, expected) {
      visitIds.add(visit.visitId);
      browser.test.assertEq(
        expected.visitTime,
        visit.visitTime,
        "visit has the correct visitTime"
      );
      browser.test.assertEq(
        expected.transition,
        visit.transition,
        "visit has the correct transition"
      );
      let results = await browser.history.search({ text: expected.url });
      // all results will have the same id, so we only need to use the first one
      browser.test.assertEq(
        results[0].id,
        visit.id,
        "visit has the correct id"
      );
    }

    let details = Object.assign({}, INITIAL_DETAILS);

    await browser.history.addUrl(details);
    let results = await browser.history.getVisits({ url: details.url });

    browser.test.assertEq(
      1,
      results.length,
      "the expected number of visits were returned"
    );
    await checkVisit(results[0], details);

    details.url = `${TEST_DOMAIN}/1/`;
    await browser.history.addUrl(details);

    results = await browser.history.getVisits({ url: details.url });
    browser.test.assertEq(
      1,
      results.length,
      "the expected number of visits were returned"
    );
    await checkVisit(results[0], details);

    details.visitTime = FIRST_DATE - 1000;
    details.transition = "typed";
    await browser.history.addUrl(details);
    results = await browser.history.getVisits({ url: details.url });

    browser.test.assertEq(
      2,
      results.length,
      "the expected number of visits were returned"
    );
    await checkVisit(results[0], INITIAL_DETAILS);
    await checkVisit(results[1], details);
    browser.test.assertEq(3, visitIds.size, "each visit has a unique visitId");
    await browser.test.notifyPass("get-visits");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background: `(${background})()`,
  });

  await PlacesUtils.history.clear();
  await extension.startup();

  await extension.awaitFinish("get-visits");
  await extension.unload();
});

add_task(async function test_transition_types() {
  const VISIT_URL_PREFIX = "http://example.com/";
  const TRANSITIONS = [
    ["link", Ci.nsINavHistoryService.TRANSITION_LINK],
    ["typed", Ci.nsINavHistoryService.TRANSITION_TYPED],
    ["auto_bookmark", Ci.nsINavHistoryService.TRANSITION_BOOKMARK],
    // Only session history contains TRANSITION_EMBED visits,
    // So global history query cannot find them.
    // ["auto_subframe", Ci.nsINavHistoryService.TRANSITION_EMBED],
    // Redirects are not correctly tested here because History
    // will not make redirect entries hidden.
    ["link", Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT],
    ["link", Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY],
    ["link", Ci.nsINavHistoryService.TRANSITION_DOWNLOAD],
    ["manual_subframe", Ci.nsINavHistoryService.TRANSITION_FRAMED_LINK],
    ["reload", Ci.nsINavHistoryService.TRANSITION_RELOAD],
  ];

  // pages/visits to add via History.insertMany
  let pageInfos = [];
  let visitDate = new Date(1999, 9, 9, 9, 9).getTime();
  for (let [, transitionType] of TRANSITIONS) {
    pageInfos.push({
      url: VISIT_URL_PREFIX + transitionType + "/",
      visits: [
        { transition: transitionType, date: new Date((visitDate -= 1000)) },
      ],
    });
  }

  function background() {
    browser.test.onMessage.addListener(async (msg, url) => {
      switch (msg) {
        case "search": {
          let results = await browser.history.search({
            text: "",
            startTime: new Date(0),
          });
          browser.test.sendMessage("search-result", results);
          break;
        }
        case "get-visits": {
          let results = await browser.history.getVisits({ url });
          browser.test.sendMessage("get-visits-result", results);
          break;
        }
      }
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background,
  });

  await PlacesUtils.history.clear();
  await extension.startup();
  await extension.awaitMessage("ready");

  await PlacesUtils.history.insertMany(pageInfos);

  extension.sendMessage("search");
  let results = await extension.awaitMessage("search-result");
  equal(
    results.length,
    pageInfos.length,
    "search returned expected length of results"
  );
  for (let i = 0; i < pageInfos.length; ++i) {
    equal(results[i].url, pageInfos[i].url, "search returned the expected url");

    extension.sendMessage("get-visits", pageInfos[i].url);
    let visits = await extension.awaitMessage("get-visits-result");
    equal(visits.length, 1, "getVisits returned expected length of visits");
    equal(
      visits[0].transition,
      TRANSITIONS[i][0],
      "getVisits returned the expected transition"
    );
  }

  await extension.unload();
});

add_task(async function test_on_visited() {
  const SINGLE_VISIT_URL = "http://example.com/1/";
  const DOUBLE_VISIT_URL = "http://example.com/2/";
  let visitDate = new Date(1999, 9, 9, 9, 9).getTime();

  // pages/visits to add via History.insertMany
  const PAGE_INFOS = [
    {
      url: SINGLE_VISIT_URL,
      title: `visit to ${SINGLE_VISIT_URL}`,
      visits: [{ date: new Date(visitDate) }],
    },
    {
      url: DOUBLE_VISIT_URL,
      title: `visit to ${DOUBLE_VISIT_URL}`,
      visits: [
        { date: new Date((visitDate += 1000)) },
        { date: new Date((visitDate += 1000)) },
      ],
    },
    {
      url: SINGLE_VISIT_URL,
      title: "Title Changed",
      visits: [{ date: new Date(visitDate) }],
    },
  ];

  function background() {
    let onVisitedData = [];

    browser.history.onVisited.addListener(data => {
      if (data.url.includes("moz-extension")) {
        return;
      }
      onVisitedData.push(data);
      if (onVisitedData.length == 4) {
        browser.test.sendMessage("on-visited-data", onVisitedData);
      }
    });

    // Verifying onTitleChange Event along with onVisited event
    browser.history.onTitleChanged.addListener(data => {
      browser.test.sendMessage("on-title-changed-data", data);
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["history"],
    },
    background: `(${background})()`,
  });

  await PlacesUtils.history.clear();
  await extension.startup();
  await extension.awaitMessage("ready");

  await PlacesUtils.history.insertMany(PAGE_INFOS);

  let onVisitedData = await extension.awaitMessage("on-visited-data");

  function checkOnVisitedData(index, expected) {
    let onVisited = onVisitedData[index];
    ok(PlacesUtils.isValidGuid(onVisited.id), "onVisited received a valid id");
    equal(onVisited.url, expected.url, "onVisited received the expected url");
    // Title will be blank until bug 1287928 lands
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1287928
    equal(
      onVisited.title,
      expected.title,
      "onVisited received the expected title"
    );
    equal(
      onVisited.lastVisitTime,
      expected.time,
      "onVisited received the expected time"
    );
    equal(
      onVisited.visitCount,
      expected.visitCount,
      "onVisited received the expected visitCount"
    );
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

  expected.url = PAGE_INFOS[2].url;
  expected.title = PAGE_INFOS[2].title;
  expected.time = PAGE_INFOS[2].visits[0].date.getTime();
  expected.visitCount = 2;
  checkOnVisitedData(3, expected);

  let onTitleChangedData = await extension.awaitMessage(
    "on-title-changed-data"
  );
  equal(
    onTitleChangedData.title,
    "Title Changed",
    "ontitleChanged received the expected title."
  );

  await extension.unload();
});

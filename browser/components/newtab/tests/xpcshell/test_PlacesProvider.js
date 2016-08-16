"use strict";

/* global XPCOMUtils, PlacesUtils, PlacesTestUtils, PlacesProvider, NetUtil */
/* global do_get_profile, run_next_test, add_task */
/* global equal, ok */
/* exported run_test */

const {
  utils: Cu,
  interfaces: Ci,
} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesProvider",
    "resource:///modules/PlacesProvider.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
    "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
    "resource://testing-common/PlacesTestUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
    "resource://gre/modules/NetUtil.jsm");

// ensure a profile exists
do_get_profile();

function run_test() {
  run_next_test();
}

// url prefix for test history population
const TEST_URL = "https://mozilla.com/";
// time when the test starts execution
const TIME_NOW = (new Date()).getTime();

// utility function to compute past timestap
function timeDaysAgo(numDays) {
  return TIME_NOW - (numDays * 24 * 60 * 60 * 1000);
}

// utility function to make a visit for insetion into places db
function makeVisit(index, daysAgo, isTyped, domain=TEST_URL) {
  let {
    TRANSITION_TYPED,
    TRANSITION_LINK
  } = PlacesUtils.history;

  return {
    uri: NetUtil.newURI(`${domain}${index}`),
    visitDate: timeDaysAgo(daysAgo),
    transition: (isTyped) ? TRANSITION_TYPED : TRANSITION_LINK,
  };
}

/** Test LinkChecker **/

add_task(function test_LinkChecker_securityCheck() {

  let urls = [
    {url: "file://home/file/image.png", expected: false},
    {url: "resource:///modules/PlacesProvider.jsm", expected: false},
    {url: "javascript:alert('hello')", expected: false}, // jshint ignore:line
    {url: "data:image/png;base64,XXX", expected: false},
    {url: "about:newtab", expected: true},
    {url: "https://example.com", expected: true},
    {url: "ftp://example.com", expected: true},
  ];
  for (let {url, expected} of urls) {
    let observed = PlacesProvider.LinkChecker.checkLoadURI(url);
    equal(observed, expected, `can load "${url}"?`);
  }
});

/** Test Provider **/

add_task(function* test_Links_getLinks() {
  yield PlacesTestUtils.clearHistory();
  let provider = PlacesProvider.links;

  let links = yield provider.getLinks();
  equal(links.length, 0, "empty history yields empty links");

  // add a visit
  let testURI = NetUtil.newURI("http://mozilla.com");
  yield PlacesTestUtils.addVisits(testURI);

  links = yield provider.getLinks();
  equal(links.length, 1, "adding a visit yields a link");
  equal(links[0].url, testURI.spec, "added visit corresponds to added url");
});

add_task(function* test_Links_getLinks_Order() {
  yield PlacesTestUtils.clearHistory();
  let provider = PlacesProvider.links;

  // all four visits must come from different domains to avoid deduplication
  let visits = [
    makeVisit(0, 0, true, "http://bar.com/"), // frecency 200, today
    makeVisit(1, 0, true, "http://foo.com/"), // frecency 200, today
    makeVisit(2, 2, true, "http://buz.com/"), // frecency 200, 2 days ago
    makeVisit(3, 2, false, "http://aaa.com/"), // frecency 10, 2 days ago, transition
  ];

  let links = yield provider.getLinks();
  equal(links.length, 0, "empty history yields empty links");
  yield PlacesTestUtils.addVisits(visits);

  links = yield provider.getLinks();
  equal(links.length, visits.length, "number of links added is the same as obtain by getLinks");
  for (let i = 0; i < links.length; i++) {
    equal(links[i].url, visits[i].uri.spec, "links are obtained in the expected order");
  }
});

add_task(function* test_Links_getLinks_Deduplication() {
  yield PlacesTestUtils.clearHistory();
  let provider = PlacesProvider.links;

  // all for visits must come from different domains to avoid deduplication
  let visits = [
    makeVisit(0, 2, true, "http://bar.com/"), // frecency 200, 2 days ago
    makeVisit(1, 0, true, "http://bar.com/"), // frecency 200, today
    makeVisit(2, 0, false, "http://foo.com/"), // frecency 10, today
    makeVisit(3, 0, true, "http://foo.com/"), // frecency 200, today
  ];

  let links = yield provider.getLinks();
  equal(links.length, 0, "empty history yields empty links");
  yield PlacesTestUtils.addVisits(visits);

  links = yield provider.getLinks();
  equal(links.length, 2, "only two links must be left after deduplication");
  equal(links[0].url, visits[1].uri.spec, "earliest link is present");
  equal(links[1].url, visits[3].uri.spec, "most fresent link is present");
});

add_task(function* test_Links_onLinkChanged() {
  let provider = PlacesProvider.links;
  provider.init();

  let url = "https://example.com/onFrecencyChanged1";
  let linkChangedMsgCount = 0;

  let linkChangedPromise = new Promise(resolve => {
    let handler = (_, link) => { // jshint ignore:line
      /* There are 3 linkChanged events:
       * 1. visit insertion (-1 frecency by default)
       * 2. frecency score update (after transition type calculation etc)
       * 3. title change
       */
      if (link.url === url) {
        equal(link.url, url, `expected url on linkChanged event`);
        linkChangedMsgCount += 1;
        if (linkChangedMsgCount === 3) {
          ok(true, `all linkChanged events captured`);
          provider.off("linkChanged", this);
          resolve();
        }
      }
    };
    provider.on("linkChanged", handler);
  });

  // add a visit
  let testURI = NetUtil.newURI(url);
  yield PlacesTestUtils.addVisits(testURI);
  yield linkChangedPromise;

  yield PlacesTestUtils.clearHistory();
  provider.uninit();
});

add_task(function* test_Links_onClearHistory() {
  let provider = PlacesProvider.links;
  provider.init();

  let clearHistoryPromise = new Promise(resolve => {
    let handler = () => {
      ok(true, `clearHistory event captured`);
      provider.off("clearHistory", handler);
      resolve();
    };
    provider.on("clearHistory", handler);
  });

  // add visits
  for (let i = 0; i <= 10; i++) {
    let url = `https://example.com/onClearHistory${i}`;
    let testURI = NetUtil.newURI(url);
    yield PlacesTestUtils.addVisits(testURI);
  }
  yield PlacesTestUtils.clearHistory();
  yield clearHistoryPromise;
  provider.uninit();
});

add_task(function* test_Links_onDeleteURI() {
  let provider = PlacesProvider.links;
  provider.init();

  let testURL = "https://example.com/toDelete";

  let deleteURIPromise = new Promise(resolve => {
    let handler = (_, {url}) => { // jshint ignore:line
      equal(testURL, url, "deleted url and expected url are the same");
      provider.off("deleteURI", handler);
      resolve();
    };

    provider.on("deleteURI", handler);
  });

  let testURI = NetUtil.newURI(testURL);
  yield PlacesTestUtils.addVisits(testURI);
  yield PlacesUtils.history.remove(testURL);
  yield deleteURIPromise;
  provider.uninit();
});

add_task(function* test_Links_onManyLinksChanged() {
  let provider = PlacesProvider.links;
  provider.init();

  let promise = new Promise(resolve => {
    let handler = () => {
      ok(true);
      provider.off("manyLinksChanged", handler);
      resolve();
    };

    provider.on("manyLinksChanged", handler);
  });

  let testURL = "https://example.com/toDelete";
  let testURI = NetUtil.newURI(testURL);
  yield PlacesTestUtils.addVisits(testURI);

  // trigger DecayFrecency
  PlacesUtils.history.QueryInterface(Ci.nsIObserver).
    observe(null, "idle-daily", "");

  yield promise;
  provider.uninit();
});

add_task(function* test_Links_execute_query() {
  yield PlacesTestUtils.clearHistory();
  let provider = PlacesProvider.links;

  let visits = [
    makeVisit(0, 0, true), // frecency 200, today
    makeVisit(1, 0, true), // frecency 200, today
    makeVisit(2, 2, true), // frecency 200, 2 days ago
    makeVisit(3, 2, false), // frecency 10, 2 days ago, transition
  ];

  yield PlacesTestUtils.addVisits(visits);

  function testItemValue(results, index, value) {
    equal(results[index][0], `${TEST_URL}${value}`, "raw url");
    equal(results[index][1], `test visit for ${TEST_URL}${value}`, "raw title");
  }

  function testItemObject(results, index, columnValues) {
    Object.keys(columnValues).forEach(name => {
      equal(results[index][name], columnValues[name], "object name " + name);
    });
  }

  // select all 4 records
  let results = yield provider.executePlacesQuery("select url, title from moz_places");
  equal(results.length, 4, "expect 4 items");
  // check for insert order sequence
  for (let i = 0; i < results.length; i++) {
    testItemValue(results, i, i);
  }

  // test parameter passing
  results = yield provider.executePlacesQuery(
              "select url, title from moz_places limit :limit",
              {params: {limit: 2}}
            );
  equal(results.length, 2, "expect 2 items");
  for (let i = 0; i < results.length; i++) {
    testItemValue(results, i, i);
  }

  // test extracting items by name
  results = yield provider.executePlacesQuery(
              "select url, title from moz_places limit :limit",
              {columns: ["url", "title"], params: {limit: 4}}
            );
  equal(results.length, 4, "expect 4 items");
  for (let i = 0; i < results.length; i++) {
    testItemObject(results, i, {
      "url": `${TEST_URL}${i}`,
      "title": `test visit for ${TEST_URL}${i}`,
    });
  }

  // test ordering
  results = yield provider.executePlacesQuery(
              "select url, title, last_visit_date, frecency from moz_places " +
              "order by frecency DESC, last_visit_date DESC, url DESC limit :limit",
              {columns: ["url", "title", "last_visit_date", "frecency"], params: {limit: 4}}
            );
  equal(results.length, 4, "expect 4 items");
  testItemObject(results, 0, {url: `${TEST_URL}1`});
  testItemObject(results, 1, {url: `${TEST_URL}0`});
  testItemObject(results, 2, {url: `${TEST_URL}2`});
  testItemObject(results, 3, {url: `${TEST_URL}3`});

  // test callback passing
  results = [];
  function handleRow(aRow) {
    results.push({
      url:              aRow.getResultByName("url"),
      title:            aRow.getResultByName("title"),
      last_visit_date:  aRow.getResultByName("last_visit_date"),
      frecency:         aRow.getResultByName("frecency")
    });
  }
  yield provider.executePlacesQuery(
        "select url, title, last_visit_date, frecency from moz_places " +
        "order by frecency DESC, last_visit_date DESC, url DESC",
        {callback: handleRow}
      );
  equal(results.length, 4, "expect 4 items");
  testItemObject(results, 0, {url: `${TEST_URL}1`});
  testItemObject(results, 1, {url: `${TEST_URL}0`});
  testItemObject(results, 2, {url: `${TEST_URL}2`});
  testItemObject(results, 3, {url: `${TEST_URL}3`});

  // negative test cases
  // bad sql
  try {
    yield provider.executePlacesQuery("select from moz");
    do_throw("bad sql should've thrown");
  }
  catch (e) {
    do_check_true("expected failure - bad sql");
  }
  // missing bindings
  try {
    yield provider.executePlacesQuery("select * from moz_places limit :limit");
    do_throw("bad sql should've thrown");
  }
  catch (e) {
    do_check_true("expected failure - missing bidning");
  }
  // non-existent column name
  try {
    yield provider.executePlacesQuery("select * from moz_places limit :limit",
                                     {columns: ["no-such-column"], params: {limit: 4}});
    do_throw("bad sql should've thrown");
  }
  catch (e) {
    do_check_true("expected failure - wrong column name");
  }

  // cleanup
  yield PlacesTestUtils.clearHistory();
});

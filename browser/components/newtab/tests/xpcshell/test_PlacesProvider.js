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
    equal(observed , expected, `can load "${url}"?`);
  }
});

/** Test LinkUtils **/

add_task(function test_LinkUtils_compareLinks() {

  let fixtures = {
    firstOlder: {
      url: "http://www.mozilla.org/firstolder",
      title: "Mozilla",
      frecency: 1337,
      lastVisitDate: 1394678824766431,
    },
    older: {
      url: "http://www.mozilla.org/older",
      title: "Mozilla",
      frecency: 1337,
      lastVisitDate: 1394678824766431,
    },
    newer: {
      url: "http://www.mozilla.org/newer",
      title: "Mozilla",
      frecency: 1337,
      lastVisitDate: 1494678824766431,
    },
    moreFrecent: {
      url: "http://www.mozilla.org/moreFrecent",
      title: "Mozilla",
      frecency: 1337357,
      lastVisitDate: 1394678824766431,
    }
  };

  let links = [
    // tests string ordering, f is before o
    {link1: fixtures.firstOlder, link2: fixtures.older, expected: false},

    // test identity
    {link1: fixtures.older, link2: fixtures.older, expected: false},

    // test ordering by date
    {link1: fixtures.older, link2: fixtures.newer, expected: true},
    {link1: fixtures.newer, link2: fixtures.older, expected: false},

    // test frecency
    {link1: fixtures.moreFrecent, link2: fixtures.older, expected: false},
  ];

  for (let {link1, link2, expected} of links) {
    let observed = PlacesProvider.LinkUtils.compareLinks(link1, link2) > 0;
    equal(observed , expected, `comparing ${link1.url} and ${link2.url}`);
  }

  // test error scenarios

  let errorFixtures = {
    missingFrecency: {
      url: "http://www.mozilla.org/firstolder",
      title: "Mozilla",
      lastVisitDate: 1394678824766431,
    },
    missingVisitDate: {
      url: "http://www.mozilla.org/firstolder",
      title: "Mozilla",
      frecency: 1337,
    },
    missingURL: {
      title: "Mozilla",
      frecency: 1337,
      lastVisitDate: 1394678824766431,
    }
  };

  let errorLinks = [
    {link1: fixtures.older, link2: errorFixtures.missingFrecency},
    {link2: fixtures.older, link1: errorFixtures.missingFrecency},
    {link1: fixtures.older, link2: errorFixtures.missingVisitDate},
    {link1: fixtures.older, link2: errorFixtures.missingURL},
    {link1: errorFixtures.missingFrecency, link2: errorFixtures.missingVisitDate}
  ];

  let errorCount = 0;
  for (let {link1, link2} of errorLinks) {
    try {
      let observed = PlacesProvider.LinkUtils.compareLinks(link1, link2) > 0; // jshint ignore:line
    } catch (e) {
      ok(true, `exception for comparison of ${link1.url} and ${link2.url}`);
      errorCount += 1;
    }
  }
  equal(errorCount, errorLinks.length);
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
  let {
    TRANSITION_TYPED,
    TRANSITION_LINK
  } = PlacesUtils.history;

  function timeDaysAgo(numDays) {
    let now = new Date();
    return now.getTime() - (numDays * 24 * 60 * 60 * 1000);
  }

  let timeEarlier = timeDaysAgo(0);
  let timeLater = timeDaysAgo(2);

  let visits = [
    // frecency 200
    {uri: NetUtil.newURI("https://mozilla.com/0"), visitDate: timeEarlier, transition: TRANSITION_TYPED},
    // sort by url, frecency 200
    {uri: NetUtil.newURI("https://mozilla.com/1"), visitDate: timeEarlier, transition: TRANSITION_TYPED},
    // sort by last visit date, frecency 200
    {uri: NetUtil.newURI("https://mozilla.com/2"), visitDate: timeLater, transition: TRANSITION_TYPED},
    // sort by frecency, frecency 10
    {uri: NetUtil.newURI("https://mozilla.com/3"), visitDate: timeLater, transition: TRANSITION_LINK},
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
  provider.destroy();
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
  provider.destroy();
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
  provider.destroy();
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
  provider.destroy();
});

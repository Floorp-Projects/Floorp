/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

// Bug 1588193 - BrowserTestUtils.waitForContentEvent now resolves slightly
// earlier than before, so it no longer suffices to only wait for a single event
// tick before checking if browser.engines has been updated. Instead we use a 1s
// timeout, which may cause the test to take more time.
requestLongerTimeout(2);

add_task(async function() {
  let url =
    "http://mochi.test:8888/browser/browser/components/search/test/browser/discovery.html";
  info("Test search discovery");
  await BrowserTestUtils.withNewTab(url, searchDiscovery);
});

let searchDiscoveryTests = [
  { text: "rel search discovered" },
  { rel: "SEARCH", text: "rel is case insensitive" },
  { rel: "-search-", pass: false, text: "rel -search- not discovered" },
  {
    rel: "foo bar baz search quux",
    text: "rel may contain additional rels separated by spaces",
  },
  { href: "https://not.mozilla.com", text: "HTTPS ok" },
  { href: "data:text/foo,foo", pass: false, text: "data URI not permitted" },
  { href: "javascript:alert(0)", pass: false, text: "JS URI not permitted" },
  {
    type: "APPLICATION/OPENSEARCHDESCRIPTION+XML",
    text: "type is case insensitve",
  },
  {
    type: " application/opensearchdescription+xml ",
    text: "type may contain extra whitespace",
  },
  {
    type: "application/opensearchdescription+xml; charset=utf-8",
    text: "type may have optional parameters (RFC2046)",
  },
  {
    type: "aapplication/opensearchdescription+xml",
    pass: false,
    text: "type should not be loosely matched",
  },
  {
    rel: "search search search",
    count: 1,
    text: "only one engine should be added",
  },
];

async function searchDiscovery() {
  let browser = gBrowser.selectedBrowser;

  for (let testCase of searchDiscoveryTests) {
    if (testCase.pass == undefined) {
      testCase.pass = true;
    }
    testCase.title = testCase.title || searchDiscoveryTests.indexOf(testCase);

    let promiseLinkAdded = BrowserTestUtils.waitForContentEvent(
      gBrowser.selectedBrowser,
      "DOMLinkAdded",
      false,
      null,
      true
    );

    await SpecialPowers.spawn(gBrowser.selectedBrowser, [testCase], test => {
      let doc = content.document;
      let head = doc.getElementById("linkparent");
      let link = doc.createElement("link");
      link.rel = test.rel || "search";
      link.href = test.href || "http://so.not.here.mozilla.com/search.xml";
      link.type = test.type || "application/opensearchdescription+xml";
      link.title = test.title;
      head.appendChild(link);
    });

    await promiseLinkAdded;
    await new Promise(resolve => setTimeout(resolve, 1000));

    if (browser.engines) {
      info(`Found ${browser.engines.length} engines`);
      info(`First engine title: ${browser.engines[0].title}`);
      let hasEngine = testCase.count
        ? browser.engines[0].title == testCase.title &&
          browser.engines.length == testCase.count
        : browser.engines[0].title == testCase.title;
      ok(hasEngine, testCase.text);
      browser.engines = null;
    } else {
      ok(!testCase.pass, testCase.text);
    }
  }

  info("Test multiple engines with the same title");
  let promiseLinkAdded = BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser,
    "DOMLinkAdded",
    false,
    e => e.target.href == "http://second.mozilla.com/search.xml",
    true
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let doc = content.document;
    let head = doc.getElementById("linkparent");
    let link = doc.createElement("link");
    link.rel = "search";
    link.href = "http://first.mozilla.com/search.xml";
    link.type = "application/opensearchdescription+xml";
    link.title = "Test Engine";
    let link2 = link.cloneNode(false);
    link2.href = "http://second.mozilla.com/search.xml";
    head.appendChild(link);
    head.appendChild(link2);
  });

  await promiseLinkAdded;
  await new Promise(resolve => setTimeout(resolve, 1000));

  ok(browser.engines, "has engines");
  is(browser.engines.length, 1, "only one engine");
  is(
    browser.engines[0].uri,
    "http://first.mozilla.com/search.xml",
    "first engine wins"
  );
  browser.engines = null;
}

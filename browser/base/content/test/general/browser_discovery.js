/* eslint-disable mozilla/no-arbitrary-setTimeout */
var browser;

function doc() {
  return browser.contentDocument;
}

function setHandlerFunc(aResultFunc) {
  gBrowser.addEventListener("DOMLinkAdded", function(event) {
    executeSoon(aResultFunc);
  }, {once: true});
}

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function(event) {
    iconDiscovery();
  }, {capture: true, once: true});
  var rootDir = getRootDirectory(gTestPath);
  content.location = rootDir + "discovery.html";
}

var iconDiscoveryTests = [
  { text: "rel icon discovered" },
  { rel: "abcdefg icon qwerty", text: "rel may contain additional rels separated by spaces" },
  { rel: "ICON", text: "rel is case insensitive" },
  { rel: "shortcut-icon", pass: false, text: "rel shortcut-icon not discovered" },
  { href: "moz.png", text: "relative href works" },
  { href: "notthere.png", text: "404'd icon is removed properly" },
  { href: "data:image/x-icon,%00", type: "image/x-icon", text: "data: URIs work" },
  { type: "image/png; charset=utf-8", text: "type may have optional parameters (RFC2046)" }
];

function runIconDiscoveryTest() {
  let testCase = iconDiscoveryTests[0];
  let head = doc().getElementById("linkparent");

  // Because there is debounce logic in ContentLinkHandler.jsm to reduce the
  // favicon loads, we have to wait some time before checking that icon was
  // stored properly.
  BrowserTestUtils.waitForCondition(() => {
    return gBrowser.getIcon() != null;
  }, "wait for icon load to finish", 100, 5)
  .then(() => {
    ok(testCase.pass, testCase.text);
  })
  .catch(() => {
    ok(!testCase.pass, testCase.text);
  })
  .then(() => {
    head.removeChild(head.getElementsByTagName("link")[0]);
    iconDiscoveryTests.shift();
    iconDiscovery(); // Run the next test.
  });
}

function iconDiscovery() {
  if (iconDiscoveryTests.length) {
    setHandlerFunc(runIconDiscoveryTest);
    gBrowser.setIcon(gBrowser.selectedTab, null,
                     Services.scriptSecurityManager.getSystemPrincipal());

    var testCase = iconDiscoveryTests[0];
    var head = doc().getElementById("linkparent");
    var link = doc().createElement("link");

    var rootDir = getRootDirectory(gTestPath);
    var rel = testCase.rel || "icon";
    var href = testCase.href || rootDir + "moz.png";
    var type = testCase.type || "image/png";
    if (testCase.pass == undefined)
      testCase.pass = true;

    link.rel = rel;
    link.href = href;
    link.type = type;
    head.appendChild(link);
  } else {
    richIconDiscovery();
  }
}

let richIconDiscoveryTests = [
  { rel: "apple-touch-icon", text: "apple-touch-icon discovered" },
  { rel: "apple-touch-icon-precomposed", text: "apple-touch-icon-precomposed discovered" },
  { rel: "fluid-icon", text: "fluid-icon discovered" },
  { rel: "unknown-icon", pass: false, text: "unknown icon not discovered" }
];

function runRichIconDiscoveryTest() {
  let testCase = richIconDiscoveryTests[0];
  let head = doc().getElementById("linkparent");

  // Rich icons are not set as tab icons, so just check for the message.
  (async function() {
    let mm = window.messageManager;
    let deferred = PromiseUtils.defer();
    testCase.listener = function(msg) {
      deferred.resolve(msg.data);
    }
    mm.addMessageListener("Link:SetIcon", testCase.listener);
    try {
      let data = await Promise.race([deferred.promise,
                                     new Promise((resolve, reject) => setTimeout(reject, 1000))]);
      is(data.canUseForTab, false, "Rich icons cannot be used for tabs");
      ok(testCase.pass, testCase.text);
    } catch (ex) {
      ok(!testCase.pass, testCase.text);
    } finally {
      mm.removeMessageListener("Link:SetIcon", testCase.listener);
    }
    head.removeChild(head.getElementsByTagName("link")[0]);
    richIconDiscoveryTests.shift();
    richIconDiscovery(); // Run the next test.
  })();
}

function richIconDiscovery() {
  if (richIconDiscoveryTests.length) {
    setHandlerFunc(runRichIconDiscoveryTest);
    gBrowser.setIcon(gBrowser.selectedTab, null,
                     Services.scriptSecurityManager.getSystemPrincipal()
    );

    let testCase = richIconDiscoveryTests[0];
    let head = doc().getElementById("linkparent");
    let link = doc().createElement("link");

    let rel = testCase.rel;
    let rootDir = getRootDirectory(gTestPath);
    let href = testCase.href || rootDir + "moz.png";
    let type = testCase.type || "image/png";
    if (testCase.pass === undefined)
      testCase.pass = true;

    link.rel = rel;
    link.href = href;
    link.type = type;
    head.appendChild(link);
  } else {
    searchDiscovery();
  }
}

var searchDiscoveryTests = [
  { text: "rel search discovered" },
  { rel: "SEARCH", text: "rel is case insensitive" },
  { rel: "-search-", pass: false, text: "rel -search- not discovered" },
  { rel: "foo bar baz search quux", text: "rel may contain additional rels separated by spaces" },
  { href: "https://not.mozilla.com", text: "HTTPS ok" },
  { href: "ftp://not.mozilla.com", text: "FTP ok" },
  { href: "data:text/foo,foo", pass: false, text: "data URI not permitted" },
  { href: "javascript:alert(0)", pass: false, text: "JS URI not permitted" },
  { type: "APPLICATION/OPENSEARCHDESCRIPTION+XML", text: "type is case insensitve" },
  { type: " application/opensearchdescription+xml ", text: "type may contain extra whitespace" },
  { type: "application/opensearchdescription+xml; charset=utf-8", text: "type may have optional parameters (RFC2046)" },
  { type: "aapplication/opensearchdescription+xml", pass: false, text: "type should not be loosely matched" },
  { rel: "search search search", count: 1, text: "only one engine should be added" }
];

function runSearchDiscoveryTest() {
  var testCase = searchDiscoveryTests[0];
  var title = testCase.title || searchDiscoveryTests.length;
  if (browser.engines) {
    var hasEngine = (testCase.count) ? (browser.engines[0].title == title &&
                                        browser.engines.length == testCase.count) :
                                       (browser.engines[0].title == title);
    ok(hasEngine, testCase.text);
    browser.engines = null;
  } else
    ok(!testCase.pass, testCase.text);

  searchDiscoveryTests.shift();
  searchDiscovery(); // Run the next test.
}

// This handler is called twice, once for each added link element.
// Only want to check once the second link element has been added.
var ranOnce = false;
function runMultipleEnginesTestAndFinalize() {
  if (!ranOnce) {
    ranOnce = true;
    return;
  }
  ok(browser.engines, "has engines");
  is(browser.engines.length, 1, "only one engine");
  is(browser.engines[0].uri, "http://first.mozilla.com/search.xml", "first engine wins");

  gBrowser.removeCurrentTab();
  finish();
}

function searchDiscovery() {
  let head = doc().getElementById("linkparent");

  if (searchDiscoveryTests.length) {
    setHandlerFunc(runSearchDiscoveryTest);
    let testCase = searchDiscoveryTests[0];
    let link = doc().createElement("link");

    let rel = testCase.rel || "search";
    let href = testCase.href || "http://so.not.here.mozilla.com/search.xml";
    let type = testCase.type || "application/opensearchdescription+xml";
    let title = testCase.title || searchDiscoveryTests.length;
    if (testCase.pass == undefined)
      testCase.pass = true;

    link.rel = rel;
    link.href = href;
    link.type = type;
    link.title = title;
    head.appendChild(link);
  } else {
    setHandlerFunc(runMultipleEnginesTestAndFinalize);
    setHandlerFunc(runMultipleEnginesTestAndFinalize);
    // Test multiple engines with the same title
    let link = doc().createElement("link");
    link.rel = "search";
    link.href = "http://first.mozilla.com/search.xml";
    link.type = "application/opensearchdescription+xml";
    link.title = "Test Engine";
    let link2 = link.cloneNode(false);
    link2.href = "http://second.mozilla.com/search.xml";

    head.appendChild(link);
    head.appendChild(link2);
  }
}

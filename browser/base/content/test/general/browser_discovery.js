var browser;

function doc() {
  return browser.contentDocument;
}

function setHandlerFunc(aResultFunc) {
  gBrowser.addEventListener("DOMLinkAdded", function (event) {
    gBrowser.removeEventListener("DOMLinkAdded", arguments.callee, false);
    executeSoon(aResultFunc);
  }, false);
}

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function (event) {
    event.currentTarget.removeEventListener("load", arguments.callee, true);
    iconDiscovery();
  }, true);
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
  var test = iconDiscoveryTests[0];
  var head = doc().getElementById("linkparent");
  var hasSrc = gBrowser.getIcon() != null;
  if (test.pass)
    ok(hasSrc, test.text);
  else
    ok(!hasSrc, test.text);

  head.removeChild(head.getElementsByTagName('link')[0]);
  iconDiscoveryTests.shift();
  iconDiscovery(); // Run the next test.
}

function iconDiscovery() {
  if (iconDiscoveryTests.length) {
    setHandlerFunc(runIconDiscoveryTest);
    gBrowser.setIcon(gBrowser.selectedTab, null);

    var test = iconDiscoveryTests[0];
    var head = doc().getElementById("linkparent");
    var link = doc().createElement("link");

    var rootDir = getRootDirectory(gTestPath);
    var rel = test.rel || "icon";
    var href = test.href || rootDir + "moz.png";
    var type = test.type || "image/png";
    if (test.pass == undefined)
      test.pass = true;

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
  var test = searchDiscoveryTests[0];
  var title = test.title || searchDiscoveryTests.length;
  if (browser.engines) {
    var hasEngine = (test.count) ? (browser.engines[0].title == title &&
                                    browser.engines.length == test.count) :
                                   (browser.engines[0].title == title);
    ok(hasEngine, test.text);
    browser.engines = null;
  }
  else
    ok(!test.pass, test.text);

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
  var head = doc().getElementById("linkparent");

  if (searchDiscoveryTests.length) {
    setHandlerFunc(runSearchDiscoveryTest);
    var test = searchDiscoveryTests[0];
    var link = doc().createElement("link");

    var rel = test.rel || "search";
    var href = test.href || "http://so.not.here.mozilla.com/search.xml";
    var type = test.type || "application/opensearchdescription+xml";
    var title = test.title || searchDiscoveryTests.length;
    if (test.pass == undefined)
      test.pass = true;

    link.rel = rel;
    link.href = href;
    link.type = type;
    link.title = title;
    head.appendChild(link);
  } else {
    setHandlerFunc(runMultipleEnginesTestAndFinalize);
    setHandlerFunc(runMultipleEnginesTestAndFinalize);
    // Test multiple engines with the same title
    var link = doc().createElement("link");
    link.rel = "search";
    link.href = "http://first.mozilla.com/search.xml";
    link.type = "application/opensearchdescription+xml";
    link.title = "Test Engine";
    var link2 = link.cloneNode(false);
    link2.href = "http://second.mozilla.com/search.xml";

    head.appendChild(link);
    head.appendChild(link2);
  }
}

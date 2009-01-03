function url(spec) {
  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  return ios.newURI(spec, null, null);
}

var gTestPage = null;
var gBrowserHandler;

function setHandlerFunc(aResultFunc) {
  DOMLinkHandler.handleEvent = function (event) {
    gBrowserHandler.call(DOMLinkHandler, event);
    aResultFunc();
  }
}

function test() {
  gBrowserHandler = DOMLinkHandler.handleEvent;
  ok(gBrowserHandler, "found browser handler");

  waitForExplicitFinish();
  var activeWin = Application.activeWindow;
  gTestPage = activeWin.open(url("chrome://mochikit/content/browser/browser/base/content/test/discovery.html"));
  gTestPage.focus();
  setTimeout(iconDiscovery, 1000);
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
  var head = gTestPage.document.getElementById("linkparent");
  var hasSrc = gProxyFavIcon.hasAttribute("src");
  if (test.pass)
    ok(hasSrc, test.text);
  else
    ok(!hasSrc, test.text);

  head.removeChild(head.getElementsByTagName('link')[0]);
  iconDiscoveryTests.shift();
  iconDiscovery(); // Run the next test.
}

function iconDiscovery() {
  setHandlerFunc(runIconDiscoveryTest);
  if (iconDiscoveryTests.length) {
    gProxyFavIcon.removeAttribute("src");

    var test = iconDiscoveryTests[0];
    var head = gTestPage.document.getElementById("linkparent");
    var link = gTestPage.document.createElement("link");

    var rel = test.rel || "icon";
    var href = test.href || "chrome://mochikit/content/browser/browser/base/content/test/moz.png";
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
  var browser = gBrowser.getBrowserForDocument(gTestPage.document);
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
  var browser = gBrowser.getBrowserForDocument(gTestPage.document);
  ok(browser.engines, "has engines");
  is(browser.engines.length, 1, "only one engine");
  is(browser.engines[0].uri, "http://first.mozilla.com/search.xml", "first engine wins");

  gTestPage.close();

  // Reset the default link handler
  DOMLinkHandler.handleEvent = gBrowserHandler;

  finish();
}

function searchDiscovery() {
  var head = gTestPage.document.getElementById("linkparent");
  var browser = gBrowser.getBrowserForDocument(gTestPage.document);

  if (searchDiscoveryTests.length) {
    setHandlerFunc(runSearchDiscoveryTest);
    var test = searchDiscoveryTests[0];
    var link = gTestPage.document.createElement("link");

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
    // Test multiple engines with the same title
    var link = gTestPage.document.createElement("link");
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

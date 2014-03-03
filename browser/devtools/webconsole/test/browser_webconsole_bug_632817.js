/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that network log messages bring up the network panel.

const TEST_NETWORK_REQUEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-network-request.html";

const TEST_IMG = "http://example.com/browser/browser/devtools/webconsole/test/test-image.png";

const TEST_DATA_JSON_CONTENT =
  '{ id: "test JSON data", myArray: [ "foo", "bar", "baz", "biff" ] }';

let lastRequest = null;
let requestCallback = null;

function test()
{
  const PREF = "devtools.webconsole.persistlog";
  let original = Services.prefs.getBoolPref("devtools.webconsole.filter.networkinfo");
  Services.prefs.setBoolPref("devtools.webconsole.filter.networkinfo", true);
  Services.prefs.setBoolPref(PREF, true);
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref("devtools.webconsole.filter.networkinfo", original);
    Services.prefs.clearUserPref(PREF);
  });

  addTab("data:text/html;charset=utf-8,Web Console network logging tests");

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    openConsole(null, function(aHud) {
      hud = aHud;

      HUDService.lastFinishedRequest.callback = function(aRequest) {
        lastRequest = aRequest;
        if (requestCallback) {
          requestCallback();
        }
      };

      executeSoon(testPageLoad);
    });
  }, true);
}

function testPageLoad()
{
  requestCallback = function() {
    // Check if page load was logged correctly.
    ok(lastRequest, "Page load was logged");
    is(lastRequest.request.url, TEST_NETWORK_REQUEST_URI,
      "Logged network entry is page load");
    is(lastRequest.request.method, "GET", "Method is correct");
    lastRequest = null;
    requestCallback = null;
    executeSoon(testPageLoadBody);
  };

  content.location = TEST_NETWORK_REQUEST_URI;
}

function testPageLoadBody()
{
  let loaded = false;
  let requestCallbackInvoked = false;

  // Turn off logging of request bodies and check again.
  requestCallback = function() {
    ok(lastRequest, "Page load was logged again");
    lastRequest = null;
    requestCallback = null;
    requestCallbackInvoked = true;

    if (loaded) {
      executeSoon(testXhrGet);
    }
  };

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    loaded = true;

    if (requestCallbackInvoked) {
      executeSoon(testXhrGet);
    }
  }, true);

  content.location.reload();
}

function testXhrGet()
{
  requestCallback = function() {
    ok(lastRequest, "testXhrGet() was logged");
    is(lastRequest.request.method, "GET", "Method is correct");
    lastRequest = null;
    requestCallback = null;
    executeSoon(testXhrPost);
  };

  // Start the XMLHttpRequest() GET test.
  content.wrappedJSObject.testXhrGet();
}

function testXhrPost()
{
  requestCallback = function() {
    ok(lastRequest, "testXhrPost() was logged");
    is(lastRequest.request.method, "POST", "Method is correct");
    lastRequest = null;
    requestCallback = null;
    executeSoon(testFormSubmission);
  };

  // Start the XMLHttpRequest() POST test.
  content.wrappedJSObject.testXhrPost();
}

function testFormSubmission()
{
  // Start the form submission test. As the form is submitted, the page is
  // loaded again. Bind to the load event to catch when this is done.
  requestCallback = function() {
    ok(lastRequest, "testFormSubmission() was logged");
    is(lastRequest.request.method, "POST", "Method is correct");

    // There should be 3 network requests pointing to the HTML file.
    waitForMessages({
      webconsole: hud,
      messages: [
        {
          text: "test-network-request.html",
          category: CATEGORY_NETWORK,
          severity: SEVERITY_LOG,
          count: 3,
        },
        {
          text: "test-data.json",
          category: CATEGORY_NETWORK,
          severity: SEVERITY_LOG,
          count: 2,
        },
      ],
    }).then(testLiveFilteringOnSearchStrings);
  };

  let form = content.document.querySelector("form");
  ok(form, "we have the HTML form");
  form.submit();
}

function testLiveFilteringOnSearchStrings() {
  setStringFilter("http");
  isnot(countMessageNodes(), 0, "the log nodes are not hidden when the " +
    "search string is set to \"http\"");

  setStringFilter("HTTP");
  isnot(countMessageNodes(), 0, "the log nodes are not hidden when the " +
    "search string is set to \"HTTP\"");

  setStringFilter("hxxp");
  is(countMessageNodes(), 0, "the log nodes are hidden when the search " +
    "string is set to \"hxxp\"");

  setStringFilter("ht tp");
  isnot(countMessageNodes(), 0, "the log nodes are not hidden when the " +
    "search string is set to \"ht tp\"");

  setStringFilter("");
  isnot(countMessageNodes(), 0, "the log nodes are not hidden when the " +
    "search string is removed");

  setStringFilter("json");
  is(countMessageNodes(), 2, "the log nodes show only the nodes with \"json\"");

  setStringFilter("'foo'");
  is(countMessageNodes(), 0, "the log nodes are hidden when searching for " +
    "the string 'foo'");

  setStringFilter("foo\"bar'baz\"boo'");
  is(countMessageNodes(), 0, "the log nodes are hidden when searching for " +
    "the string \"foo\"bar'baz\"boo'\"");

  HUDService.lastFinishedRequest.callback = null;
  lastRequest = null;
  requestCallback = null;
  finishTest();
}

function countMessageNodes() {
  let messageNodes = hud.outputNode.querySelectorAll(".message");
  let displayedMessageNodes = 0;
  let view = hud.iframeWindow;
  for (let i = 0; i < messageNodes.length; i++) {
    let computedStyle = view.getComputedStyle(messageNodes[i], null);
    if (computedStyle.display !== "none")
      displayedMessageNodes++;
  }

  return displayedMessageNodes;
}

function setStringFilter(aValue)
{
  hud.ui.filterBox.value = aValue;
  hud.ui.adjustVisibilityOnSearchStringChange();
}

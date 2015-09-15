/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that network log messages bring up the network panel.

"use strict";

const TEST_NETWORK_REQUEST_URI = "https://example.com/browser/browser/" +
                                 "devtools/webconsole/test/test-network-request.html";

const TEST_IMG = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-image.png";

const TEST_DATA_JSON_CONTENT =
  '{ id: "test JSON data", myArray: [ "foo", "bar", "baz", "biff" ] }';

const TEST_URI = "data:text/html;charset=utf-8,Web Console network logging " +
                 "tests";

var lastRequest = null;
var requestCallback = null;
var hud, browser;

function test() {
  const PREF = "devtools.webconsole.persistlog";
  const NET_PREF = "devtools.webconsole.filter.networkinfo";
  const NETXHR_PREF = "devtools.webconsole.filter.netxhr";
  const MIXED_AC_PREF = "security.mixed_content.block_active_content";
  let original = Services.prefs.getBoolPref(NET_PREF);
  let originalXhr = Services.prefs.getBoolPref(NETXHR_PREF);
  let originalMixedActive = Services.prefs.getBoolPref(MIXED_AC_PREF);
  Services.prefs.setBoolPref(NET_PREF, true);
  Services.prefs.setBoolPref(NETXHR_PREF, true);
  Services.prefs.setBoolPref(MIXED_AC_PREF, false);
  Services.prefs.setBoolPref(PREF, true);
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref(NET_PREF, original);
    Services.prefs.setBoolPref(NETXHR_PREF, originalXhr);
    Services.prefs.setBoolPref(MIXED_AC_PREF, originalMixedActive);
    Services.prefs.clearUserPref(PREF);
  });

  loadTab(TEST_URI).then((tab) => {
    browser = tab.browser;
    openConsole().then((hudConsole) => {
      hud = hudConsole;

      HUDService.lastFinishedRequest.callback = function(request) {
        lastRequest = request;
        if (requestCallback) {
          requestCallback();
        }
      };

      executeSoon(testPageLoad);
    });
  });
}

function testPageLoad() {
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

function testPageLoadBody() {
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

function testXhrGet() {
  requestCallback = function() {
    ok(lastRequest, "testXhrGet() was logged");
    is(lastRequest.request.method, "GET", "Method is correct");
    lastRequest = null;
    requestCallback = null;
    executeSoon(testXhrWarn);
  };

  // Start the XMLHttpRequest() GET test.
  content.wrappedJSObject.testXhrGet();
}

function testXhrWarn() {
  requestCallback = function() {
    ok(lastRequest, "testXhrWarn() was logged");
    is(lastRequest.request.method, "GET", "Method is correct");
    lastRequest = null;
    requestCallback = null;
    executeSoon(testXhrPost);
  };

  // Start the XMLHttpRequest() warn test.
  content.wrappedJSObject.testXhrWarn();
}

function testXhrPost() {
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

function testFormSubmission() {
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
          severity: SEVERITY_INFO,
          isXhr: true,
          count: 2,
        },
        {
          text: "http://example.com/",
          category: CATEGORY_NETWORK,
          severity: SEVERITY_WARNING,
          isXhr: true,
          count: 1,
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
  hud = browser = null;
  finishTest();
}

function countMessageNodes() {
  let messageNodes = hud.outputNode.querySelectorAll(".message");
  let displayedMessageNodes = 0;
  let view = hud.iframeWindow;
  for (let i = 0; i < messageNodes.length; i++) {
    let computedStyle = view.getComputedStyle(messageNodes[i], null);
    if (computedStyle.display !== "none") {
      displayedMessageNodes++;
    }
  }

  return displayedMessageNodes;
}

function setStringFilter(value) {
  hud.ui.filterBox.value = value;
  hud.ui.adjustVisibilityOnSearchStringChange();
}

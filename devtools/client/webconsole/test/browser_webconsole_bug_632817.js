/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that Web console messages can be filtered for NET events.

"use strict";

const TEST_NETWORK_REQUEST_URI =
  "https://example.com/browser/devtools/client/webconsole/test/" +
  "test-network-request.html";

const TEST_IMG = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-image.png";

const TEST_DATA_JSON_CONTENT =
  '{ id: "test JSON data", myArray: [ "foo", "bar", "baz", "biff" ] }';

const TEST_URI = "data:text/html;charset=utf-8,Web Console network logging " +
                 "tests";

const PAGE_REQUEST_PREDICATE =
  ({ request }) => request.url.endsWith("test-network-request.html");

const TEST_DATA_REQUEST_PREDICATE =
  ({ request }) => request.url.endsWith("test-data.json");

const XHR_WARN_REQUEST_PREDICATE =
  ({ request }) => request.url.endsWith("sjs_cors-test-server.sjs");

let hud;

add_task(function*() {
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
    hud = null;
  });

  yield loadTab(TEST_URI);
  hud = yield openConsole();

  yield testPageLoad();
  yield testXhrGet();
  yield testXhrWarn();
  yield testXhrPost();
  yield testFormSubmission();
  yield testLiveFilteringOnSearchStrings();
});

function testPageLoad() {

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_NETWORK_REQUEST_URI);
  let lastRequest = yield waitForFinishedRequest(PAGE_REQUEST_PREDICATE);

  // Check if page load was logged correctly.
  ok(lastRequest, "Page load was logged");
  is(lastRequest.request.url, TEST_NETWORK_REQUEST_URI,
    "Logged network entry is page load");
  is(lastRequest.request.method, "GET", "Method is correct");
}

function testXhrGet() {
  // Start the XMLHttpRequest() GET test.
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function*() {
    content.wrappedJSObject.testXhrGet();
  });

  let lastRequest = yield waitForFinishedRequest(TEST_DATA_REQUEST_PREDICATE);

  ok(lastRequest, "testXhrGet() was logged");
  is(lastRequest.request.method, "GET", "Method is correct");
  ok(lastRequest.isXHR, "It's an XHR request");
}

function testXhrWarn() {
  // Start the XMLHttpRequest() warn test.
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function*() {
    content.wrappedJSObject.testXhrWarn();
  });

  let lastRequest = yield waitForFinishedRequest(XHR_WARN_REQUEST_PREDICATE);
  if (lastRequest.request.method == "HEAD") {
    lastRequest = yield waitForFinishedRequest(XHR_WARN_REQUEST_PREDICATE);
  }

  ok(lastRequest, "testXhrWarn() was logged");
  is(lastRequest.request.method, "GET", "Method is correct");
  ok(lastRequest.isXHR, "It's an XHR request");
  is(lastRequest.securityInfo, "insecure", "It's an insecure request");
}

function testXhrPost() {
  // Start the XMLHttpRequest() POST test.
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function*() {
    content.wrappedJSObject.testXhrPost();
  });

  let lastRequest = yield waitForFinishedRequest(TEST_DATA_REQUEST_PREDICATE);

  ok(lastRequest, "testXhrPost() was logged");
  is(lastRequest.request.method, "POST", "Method is correct");
  ok(lastRequest.isXHR, "It's an XHR request");
}

function testFormSubmission() {
  // Start the form submission test. As the form is submitted, the page is
  // loaded again. Bind to the load event to catch when this is done.
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function*() {
    let form = content.document.querySelector("form");
    ok(form, "we have the HTML form");
    form.submit();
  });

  // The form POSTs to the page URL but over https (page over http).
  let lastRequest = yield waitForFinishedRequest(PAGE_REQUEST_PREDICATE);

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
  });
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
}

function countMessageNodes() {
  let messageNodes = hud.outputNode.querySelectorAll(".message");
  let displayedMessageNodes = 0;
  let view = hud.iframeWindow;
  for (let i = 0; i < messageNodes.length; i++) {
    let computedStyle = view.getComputedStyle(messageNodes[i]);
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

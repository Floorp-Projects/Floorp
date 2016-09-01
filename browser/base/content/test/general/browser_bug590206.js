/*
 * Test the identity mode UI for a variety of page types
 */

"use strict";

const DUMMY = "browser/browser/base/content/test/general/dummy_page.html";

function loadNewTab(url) {
  return BrowserTestUtils.openNewForegroundTab(gBrowser, url);
}

function getIdentityMode() {
  return document.getElementById("identity-box").className;
}

function getConnectionState() {
  gIdentityHandler.refreshIdentityPopup();
  return document.getElementById("identity-popup").getAttribute("connection");
}

// This test is slow on Linux debug e10s
requestLongerTimeout(2);

add_task(function* test_webpage() {
  let oldTab = gBrowser.selectedTab;

  let newTab = yield loadNewTab("http://example.com/" + DUMMY);
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.removeTab(newTab);
});

add_task(function* test_blank() {
  let oldTab = gBrowser.selectedTab;

  let newTab = yield loadNewTab("about:blank");
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.removeTab(newTab);
});

add_task(function* test_chrome() {
  let oldTab = gBrowser.selectedTab;

  let newTab = yield loadNewTab("chrome://mozapps/content/extensions/extensions.xul");
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.removeTab(newTab);
});

add_task(function* test_https() {
  let oldTab = gBrowser.selectedTab;

  let newTab = yield loadNewTab("https://example.com/" + DUMMY);
  is(getIdentityMode(), "verifiedDomain", "Identity should be verified");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "verifiedDomain", "Identity should be verified");

  gBrowser.removeTab(newTab);
});

add_task(function* test_addons() {
  let oldTab = gBrowser.selectedTab;

  let newTab = yield loadNewTab("about:addons");
  is(getIdentityMode(), "chromeUI", "Identity should be chrome");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "chromeUI", "Identity should be chrome");

  gBrowser.removeTab(newTab);
});

add_task(function* test_file() {
  let oldTab = gBrowser.selectedTab;
  let fileURI = getTestFilePath("");

  let newTab = yield loadNewTab(fileURI);
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.removeTab(newTab);
});

add_task(function* test_resource_uri() {
  let oldTab = gBrowser.selectedTab;
  let dataURI = "resource://gre/modules/Services.jsm";

  let newTab = yield loadNewTab(dataURI);

  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.removeTab(newTab);
});

add_task(function* test_data_uri() {
  let oldTab = gBrowser.selectedTab;
  let dataURI = "data:text/html,hi"

  let newTab = yield loadNewTab(dataURI);
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.removeTab(newTab);
});

add_task(function* test_about_uri() {
  let oldTab = gBrowser.selectedTab;
  let aboutURI = "about:robots"

  let newTab = yield loadNewTab(aboutURI);
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.removeTab(newTab);
});

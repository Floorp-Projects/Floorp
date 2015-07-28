/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const DUMMY = "browser/browser/base/content/test/general/dummy_page.html";

function loadNewTab(aURL, aCallback) {
  BrowserTestUtils.openNewForegroundTab(gBrowser, aURL).then(aCallback);
}

function getIdentityMode() {
  return document.getElementById("identity-box").className;
}

var TESTS = [
function test_webpage() {
  let oldTab = gBrowser.selectedTab;

  loadNewTab("http://example.com/" + DUMMY, function(aNewTab) {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.selectedTab = oldTab;
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.selectedTab = aNewTab;
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.removeTab(aNewTab);

    runNextTest();
  });
},

function test_blank() {
  let oldTab = gBrowser.selectedTab;

  loadNewTab("about:blank", function(aNewTab) {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.selectedTab = oldTab;
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.selectedTab = aNewTab;
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.removeTab(aNewTab);

    runNextTest();
  });
},

function test_chrome() {
  let oldTab = gBrowser.selectedTab;

  loadNewTab("chrome://mozapps/content/extensions/extensions.xul", function(aNewTab) {
    is(getIdentityMode(), "fileURI", "Identity should be file");

    gBrowser.selectedTab = oldTab;
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.selectedTab = aNewTab;
    is(getIdentityMode(), "fileURI", "Identity should be file");

    gBrowser.removeTab(aNewTab);

    runNextTest();
  });
},

function test_https() {
  let oldTab = gBrowser.selectedTab;

  loadNewTab("https://example.com/" + DUMMY, function(aNewTab) {
    is(getIdentityMode(), "verifiedDomain", "Identity should be verified");

    gBrowser.selectedTab = oldTab;
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.selectedTab = aNewTab;
    is(getIdentityMode(), "verifiedDomain", "Identity should be verified");

    gBrowser.removeTab(aNewTab);

    runNextTest();
  });
},

function test_addons() {
  let oldTab = gBrowser.selectedTab;

  loadNewTab("about:addons", function(aNewTab) {
    is(getIdentityMode(), "chromeUI", "Identity should be chrome");

    gBrowser.selectedTab = oldTab;
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.selectedTab = aNewTab;
    is(getIdentityMode(), "chromeUI", "Identity should be chrome");

    gBrowser.removeTab(aNewTab);

    runNextTest();
  });
},

function test_file() {
  let oldTab = gBrowser.selectedTab;
  let fileURI = getTestFilePath("");

  loadNewTab(fileURI, function(aNewTab) {
    is(getIdentityMode(), "fileURI", "Identity should be file");

    gBrowser.selectedTab = oldTab;
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.selectedTab = aNewTab;
    is(getIdentityMode(), "fileURI", "Identity should be file");

    gBrowser.removeTab(aNewTab);

    runNextTest();
  });
},

function test_resource_uri() {
  let oldTab = gBrowser.selectedTab;
  let dataURI = "resource://gre/modules/Services.jsm"

  loadNewTab(dataURI, function(aNewTab) {
    is(getIdentityMode(), "fileURI", "Identity should be unknown");

    gBrowser.selectedTab = oldTab;
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.selectedTab = aNewTab;
    is(getIdentityMode(), "fileURI", "Identity should be unknown");

    gBrowser.removeTab(aNewTab);

    runNextTest();
  });
},

function test_data_uri() {
  let oldTab = gBrowser.selectedTab;
  let dataURI = "data:text/html,hi"

  loadNewTab(dataURI, function(aNewTab) {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.selectedTab = oldTab;
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.selectedTab = aNewTab;
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

    gBrowser.removeTab(aNewTab);

    runNextTest();
  });
},
];

var gTestStart = null;

function runNextTest() {
  if (gTestStart)
    info("Test part took " + (Date.now() - gTestStart) + "ms");

  if (TESTS.length == 0) {
    finish();
    return;
  }

  info("Running " + TESTS[0].name);
  gTestStart = Date.now();
  TESTS.shift()();
};

function test() {
  waitForExplicitFinish();

  runNextTest();
}

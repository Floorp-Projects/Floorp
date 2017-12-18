/*
 * Test the identity mode UI for a variety of page types
 */

"use strict";

const DUMMY = "browser/browser/base/content/test/siteIdentity/dummy_page.html";
const INSECURE_ICON_PREF = "security.insecure_connection_icon.enabled";
const INSECURE_PBMODE_ICON_PREF = "security.insecure_connection_icon.pbmode.enabled";

function loadNewTab(url) {
  return BrowserTestUtils.openNewForegroundTab(gBrowser, url, true);
}

function getIdentityMode(aWindow = window) {
  return aWindow.document.getElementById("identity-box").className;
}

function getConnectionState() {
  // Prevents items that are being lazy loaded causing issues
  document.getElementById("identity-box").click();
  gIdentityHandler.refreshIdentityPopup();
  return document.getElementById("identity-popup").getAttribute("connection");
}

// This test is slow on Linux debug e10s
requestLongerTimeout(2);

async function webpageTest(secureCheck) {
  await SpecialPowers.pushPrefEnv({set: [[INSECURE_ICON_PREF, secureCheck]]});
  let oldTab = gBrowser.selectedTab;

  let newTab = await loadNewTab("http://example.com/" + DUMMY);
  if (secureCheck) {
    is(getIdentityMode(), "notSecure", "Identity should be not secure");
  } else {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  }

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  if (secureCheck) {
    is(getIdentityMode(), "notSecure", "Identity should be not secure");
  } else {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  }

  gBrowser.removeTab(newTab);
  await SpecialPowers.popPrefEnv();
}

add_task(async function test_webpage() {
  await webpageTest(false);
  await webpageTest(true);
});

async function blankPageTest(secureCheck) {
  let oldTab = gBrowser.selectedTab;
  await SpecialPowers.pushPrefEnv({set: [[INSECURE_ICON_PREF, secureCheck]]});

  let newTab = await loadNewTab("about:blank");
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.removeTab(newTab);
  await SpecialPowers.popPrefEnv();
}

add_task(async function test_blank() {
  await blankPageTest(true);
  await blankPageTest(false);
});

async function secureTest(secureCheck) {
  let oldTab = gBrowser.selectedTab;
  await SpecialPowers.pushPrefEnv({set: [[INSECURE_ICON_PREF, secureCheck]]});

  let newTab = await loadNewTab("https://example.com/" + DUMMY);
  is(getIdentityMode(), "verifiedDomain", "Identity should be verified");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "verifiedDomain", "Identity should be verified");

  gBrowser.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_secure_enabled() {
  await secureTest(true);
  await secureTest(false);
});

async function insecureTest(secureCheck) {
  let oldTab = gBrowser.selectedTab;
  await SpecialPowers.pushPrefEnv({set: [[INSECURE_ICON_PREF, secureCheck]]});

  let newTab = await loadNewTab("http://example.com/" + DUMMY);
  if (secureCheck) {
    is(getIdentityMode(), "notSecure", "Identity should be not secure");
  } else {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  }

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  if (secureCheck) {
    is(getIdentityMode(), "notSecure", "Identity should be not secure");
  } else {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  }

  gBrowser.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_insecure() {
  await insecureTest(true);
  await insecureTest(false);
});

async function addonsTest(secureCheck) {
  let oldTab = gBrowser.selectedTab;
  await SpecialPowers.pushPrefEnv({set: [[INSECURE_ICON_PREF, secureCheck]]});

  let newTab = await loadNewTab("about:addons");
  is(getIdentityMode(), "chromeUI", "Identity should be chrome");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "chromeUI", "Identity should be chrome");

  gBrowser.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_addons() {
  await addonsTest(true);
  await addonsTest(false);
});

async function fileTest(secureCheck) {
  let oldTab = gBrowser.selectedTab;
  await SpecialPowers.pushPrefEnv({set: [[INSECURE_ICON_PREF, secureCheck]]});
  let fileURI = getTestFilePath("");

  let newTab = await loadNewTab(fileURI);
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_file() {
  await fileTest(true);
  await fileTest(false);
});

async function resourceUriTest(secureCheck) {
  let oldTab = gBrowser.selectedTab;
  await SpecialPowers.pushPrefEnv({set: [[INSECURE_ICON_PREF, secureCheck]]});
  let dataURI = "resource://gre/modules/Services.jsm";

  let newTab = await loadNewTab(dataURI);

  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_resource_uri() {
  await resourceUriTest(true);
  await resourceUriTest(false);
});

async function noCertErrorTest(secureCheck) {
  let oldTab = gBrowser.selectedTab;
  await SpecialPowers.pushPrefEnv({set: [[INSECURE_ICON_PREF, secureCheck]]});
  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;

  let promise = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  gBrowser.loadURI("https://nocert.example.com/");
  await promise;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  is(getConnectionState(), "not-secure", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  is(getConnectionState(), "not-secure", "Connection should be file");

  gBrowser.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_about_net_error_uri() {
  await noCertErrorTest(true);
  await noCertErrorTest(false);
});

async function noCertErrorFromNavigationTest(secureCheck) {
  await SpecialPowers.pushPrefEnv({set: [[INSECURE_ICON_PREF, secureCheck]]});
  let newTab = await loadNewTab("http://example.com/" + DUMMY);

  let promise = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.document.getElementById("no-cert").click();
  });
  await promise;
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    is(content.window.location.href, "https://nocert.example.com/", "Should be the cert error URL");
  });


  is(newTab.linkedBrowser.documentURI.spec.startsWith("about:certerror?"), true, "Should be an about:certerror");
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  is(getConnectionState(), "not-secure", "Connection should be file");

  gBrowser.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_about_net_error_uri_from_navigation_tab() {
  await noCertErrorFromNavigationTest(true);
  await noCertErrorFromNavigationTest(false);
});

async function aboutUriTest(secureCheck) {
  let oldTab = gBrowser.selectedTab;
  await SpecialPowers.pushPrefEnv({set: [[INSECURE_ICON_PREF, secureCheck]]});
  let aboutURI = "about:robots";

  let newTab = await loadNewTab(aboutURI);
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_about_uri() {
  await aboutUriTest(true);
  await aboutUriTest(false);
});

async function dataUriTest(secureCheck) {
  let oldTab = gBrowser.selectedTab;
  await SpecialPowers.pushPrefEnv({set: [[INSECURE_ICON_PREF, secureCheck]]});
  let dataURI = "data:text/html,hi";

  let newTab = await loadNewTab(dataURI);
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_data_uri() {
   dataUriTest(true);
   dataUriTest(false);
});

async function pbModeTest(prefs, secureCheck) {
  await SpecialPowers.pushPrefEnv({set: prefs});

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let oldTab = privateWin.gBrowser.selectedTab;
  let newTab = await BrowserTestUtils.openNewForegroundTab(privateWin.gBrowser, "http://example.com/" + DUMMY);

  if (secureCheck) {
    is(getIdentityMode(privateWin), "notSecure", "Identity should be not secure");
  } else {
    is(getIdentityMode(privateWin), "unknownIdentity", "Identity should be unknown");
  }

  privateWin.gBrowser.selectedTab = oldTab;
  is(getIdentityMode(privateWin), "unknownIdentity", "Identity should be unknown");

  privateWin.gBrowser.selectedTab = newTab;
  if (secureCheck) {
    is(getIdentityMode(privateWin), "notSecure", "Identity should be not secure");
  } else {
    is(getIdentityMode(privateWin), "unknownIdentity", "Identity should be unknown");
  }

  await BrowserTestUtils.closeWindow(privateWin);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_pb_mode() {
  let prefs = [
    [INSECURE_ICON_PREF, true],
    [INSECURE_PBMODE_ICON_PREF, true]
  ];
  await pbModeTest(prefs, true);
  prefs = [
    [INSECURE_ICON_PREF, false],
    [INSECURE_PBMODE_ICON_PREF, true]
  ];
  await pbModeTest(prefs, true);
  prefs = [
    [INSECURE_ICON_PREF, false],
    [INSECURE_PBMODE_ICON_PREF, false]
  ];
  await pbModeTest(prefs, false);
});


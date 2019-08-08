/*
 * Test the identity mode UI for a variety of page types
 */

"use strict";

const DUMMY = "browser/browser/base/content/test/siteIdentity/dummy_page.html";
const INSECURE_ICON_PREF = "security.insecure_connection_icon.enabled";
const INSECURE_TEXT_PREF = "security.insecure_connection_text.enabled";
const INSECURE_PBMODE_ICON_PREF =
  "security.insecure_connection_icon.pbmode.enabled";

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

function getReaderModeURL() {
  // Gets the reader mode URL from "identity-popup mainView panel header span"
  document.getElementById("identity-box").click();
  gIdentityHandler.refreshIdentityPopup();
  return document.getElementById("identity-popup-mainView-panel-header-span")
    .innerHTML;
}

// This test is slow on Linux debug e10s
requestLongerTimeout(2);

async function webpageTest(secureCheck) {
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });
  let oldTab = await loadNewTab("about:robots");

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
  gBrowser.removeTab(oldTab);
  await SpecialPowers.popPrefEnv();
}

add_task(async function test_webpage() {
  await webpageTest(false);
  await webpageTest(true);
});

async function webpageTestTextWarning(secureCheck) {
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_TEXT_PREF, secureCheck]] });
  let oldTab = await loadNewTab("about:robots");

  let newTab = await loadNewTab("http://example.com/" + DUMMY);
  if (secureCheck) {
    is(
      getIdentityMode(),
      "notSecure notSecureText",
      "Identity should have not secure text"
    );
  } else {
    is(getIdentityMode(), "notSecure", "Identity should be not secure");
  }

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  if (secureCheck) {
    is(
      getIdentityMode(),
      "notSecure notSecureText",
      "Identity should have not secure text"
    );
  } else {
    is(getIdentityMode(), "notSecure", "Identity should be not secure");
  }

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);
  await SpecialPowers.popPrefEnv();
}

add_task(async function test_webpage_text_warning() {
  await webpageTestTextWarning(false);
  await webpageTestTextWarning(true);
});

async function webpageTestTextWarningCombined(secureCheck) {
  await SpecialPowers.pushPrefEnv({
    set: [[INSECURE_TEXT_PREF, secureCheck], [INSECURE_ICON_PREF, secureCheck]],
  });
  let oldTab = await loadNewTab("about:robots");

  let newTab = await loadNewTab("http://example.com/" + DUMMY);
  if (secureCheck) {
    is(
      getIdentityMode(),
      "notSecure notSecureText",
      "Identity should be not secure"
    );
  } else {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  }

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  if (secureCheck) {
    is(
      getIdentityMode(),
      "notSecure notSecureText",
      "Identity should be not secure"
    );
  } else {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  }

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);
  await SpecialPowers.popPrefEnv();
}

add_task(async function test_webpage_text_warning_combined() {
  await webpageTestTextWarning(false);
  await webpageTestTextWarning(true);
});

async function blankPageTest(secureCheck) {
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });

  let newTab = await loadNewTab("about:blank");
  is(
    gURLBar.getAttribute("pageproxystate"),
    "invalid",
    "pageproxystate should be invalid"
  );

  gBrowser.selectedTab = oldTab;
  is(
    gURLBar.getAttribute("pageproxystate"),
    "valid",
    "pageproxystate should be valid"
  );
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(
    gURLBar.getAttribute("pageproxystate"),
    "invalid",
    "pageproxystate should be invalid"
  );

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);
  await SpecialPowers.popPrefEnv();
}

add_task(async function test_blank() {
  await blankPageTest(true);
  await blankPageTest(false);
});

async function secureTest(secureCheck) {
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });

  let newTab = await loadNewTab("https://example.com/" + DUMMY);
  is(getIdentityMode(), "verifiedDomain", "Identity should be verified");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "verifiedDomain", "Identity should be verified");

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_secure_enabled() {
  await secureTest(true);
  await secureTest(false);
});

async function insecureTest(secureCheck) {
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });

  let newTab = await loadNewTab("http://example.com/" + DUMMY);
  if (secureCheck) {
    is(getIdentityMode(), "notSecure", "Identity should be not secure");
    is(
      document.getElementById("identity-box").getAttribute("tooltiptext"),
      gNavigatorBundle.getString("identity.notSecure.tooltip"),
      "The insecure lock icon has a correct tooltip text."
    );
  } else {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  }

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  if (secureCheck) {
    is(getIdentityMode(), "notSecure", "Identity should be not secure");
    is(
      document.getElementById("identity-box").getAttribute("tooltiptext"),
      gNavigatorBundle.getString("identity.notSecure.tooltip"),
      "The insecure lock icon has a correct tooltip text."
    );
  } else {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  }

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_insecure() {
  await insecureTest(true);
  await insecureTest(false);
});

async function addonsTest(secureCheck) {
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });

  let newTab = await loadNewTab("about:addons");
  is(getIdentityMode(), "chromeUI", "Identity should be chrome");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "chromeUI", "Identity should be chrome");

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_addons() {
  await addonsTest(true);
  await addonsTest(false);
});

async function fileTest(secureCheck) {
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });
  let fileURI = getTestFilePath("");

  let newTab = await loadNewTab(fileURI);
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_file() {
  await fileTest(true);
  await fileTest(false);
});

async function resourceUriTest(secureCheck) {
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });
  let dataURI = "resource://gre/modules/Services.jsm";

  let newTab = await loadNewTab(dataURI);

  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_resource_uri() {
  await resourceUriTest(true);
  await resourceUriTest(false);
});

async function noCertErrorTest(secureCheck) {
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });
  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;

  let promise = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  BrowserTestUtils.loadURI(gBrowser, "https://nocert.example.com/");
  await promise;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  is(getConnectionState(), "not-secure", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  is(getConnectionState(), "not-secure", "Connection should be file");

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_about_net_error_uri() {
  await noCertErrorTest(true);
  await noCertErrorTest(false);
});

async function noCertErrorFromNavigationTest(secureCheck) {
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });
  let newTab = await loadNewTab("http://example.com/" + DUMMY);

  let promise = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.document.getElementById("no-cert").click();
  });
  await promise;
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    is(
      content.window.location.href,
      "https://nocert.example.com/",
      "Should be the cert error URL"
    );
  });

  is(
    newTab.linkedBrowser.documentURI.spec.startsWith("about:certerror?"),
    true,
    "Should be an about:certerror"
  );
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
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });
  let aboutURI = "about:robots";

  let newTab = await loadNewTab(aboutURI);
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");

  gBrowser.selectedTab = newTab;
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_about_uri() {
  await aboutUriTest(true);
  await aboutUriTest(false);
});

async function readerUriTest(secureCheck) {
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });

  let newTab = await loadNewTab("about:reader?url=http://example.com");
  gBrowser.selectedTab = newTab;
  let readerURL = getReaderModeURL();
  is(
    readerURL,
    "Site Information for example.com",
    "should be the correct URI in reader mode"
  );

  gBrowser.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_reader_uri() {
  await readerUriTest(true);
  await readerUriTest(false);
});

async function dataUriTest(secureCheck) {
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });
  let dataURI = "data:text/html,hi";

  let newTab = await loadNewTab(dataURI);
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
  gBrowser.removeTab(oldTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_data_uri() {
  await dataUriTest(true);
  await dataUriTest(false);
});

async function pbModeTest(prefs, secureCheck) {
  await SpecialPowers.pushPrefEnv({ set: prefs });

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let oldTab = await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    "about:robots"
  );
  let newTab = await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser,
    "http://example.com/" + DUMMY
  );

  if (secureCheck) {
    is(
      getIdentityMode(privateWin),
      "notSecure",
      "Identity should be not secure"
    );
  } else {
    is(
      getIdentityMode(privateWin),
      "unknownIdentity",
      "Identity should be unknown"
    );
  }

  privateWin.gBrowser.selectedTab = oldTab;
  is(
    getIdentityMode(privateWin),
    "unknownIdentity",
    "Identity should be unknown"
  );

  privateWin.gBrowser.selectedTab = newTab;
  if (secureCheck) {
    is(
      getIdentityMode(privateWin),
      "notSecure",
      "Identity should be not secure"
    );
  } else {
    is(
      getIdentityMode(privateWin),
      "unknownIdentity",
      "Identity should be unknown"
    );
  }

  await BrowserTestUtils.closeWindow(privateWin);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_pb_mode() {
  let prefs = [[INSECURE_ICON_PREF, true], [INSECURE_PBMODE_ICON_PREF, true]];
  await pbModeTest(prefs, true);
  prefs = [[INSECURE_ICON_PREF, false], [INSECURE_PBMODE_ICON_PREF, true]];
  await pbModeTest(prefs, true);
  prefs = [[INSECURE_ICON_PREF, false], [INSECURE_PBMODE_ICON_PREF, false]];
  await pbModeTest(prefs, false);
});

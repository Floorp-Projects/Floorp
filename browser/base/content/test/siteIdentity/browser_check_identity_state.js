/*
 * Test the identity mode UI for a variety of page types
 */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const DUMMY = "browser/browser/base/content/test/siteIdentity/dummy_page.html";
const INSECURE_ICON_PREF = "security.insecure_connection_icon.enabled";
const INSECURE_TEXT_PREF = "security.insecure_connection_text.enabled";
const INSECURE_PBMODE_ICON_PREF =
  "security.insecure_connection_icon.pbmode.enabled";
const HTTPS_FIRST_PBM_PREF = "dom.security.https_first_pbm";

function loadNewTab(url) {
  return BrowserTestUtils.openNewForegroundTab(gBrowser, url, true);
}

function getConnectionState() {
  // Prevents items that are being lazy loaded causing issues
  document.getElementById("identity-icon-box").click();
  gIdentityHandler.refreshIdentityPopup();
  return document.getElementById("identity-popup").getAttribute("connection");
}

function getSecurityConnectionBG() {
  // Get the background image of the security connection.
  document.getElementById("identity-icon-box").click();
  gIdentityHandler.refreshIdentityPopup();
  return gBrowser.ownerGlobal
    .getComputedStyle(
      document
        .getElementById("identity-popup-mainView")
        .getElementsByClassName("identity-popup-security-connection")[0]
    )
    .getPropertyValue("background-image");
}

async function getReaderModeURL() {
  // Gets the reader mode URL from "identity-popup mainView panel header span"
  document.getElementById("identity-icon-box").click();
  gIdentityHandler.refreshIdentityPopup();

  let headerSpan = document.getElementById(
    "identity-popup-mainView-panel-header-span"
  );
  await BrowserTestUtils.waitForCondition(() =>
    headerSpan.innerHTML.includes("example.com")
  );
  return headerSpan.innerHTML;
}

// This test is slow on Linux debug e10s
requestLongerTimeout(2);

add_task(async function chromeUITest() {
  // needs to be set due to bug in ion.js that occurs when testing
  SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.pioneer.testCachedContent", "[]"],
      ["toolkit.pioneer.testCachedAddons", "[]"],
    ],
  });
  // Might needs to be extended with new secure chrome pages
  // about:debugging is a secure chrome UI but is not tested for causing problems.
  let secureChromePages = [
    "addons",
    "cache",
    "certificate",
    "compat",
    "config",
    "downloads",
    "ion",
    "license",
    "logins",
    "loginsimportreport",
    "performance",
    "plugins",
    "policies",
    "preferences",
    "processes",
    "profiles",
    "profiling",
    "protections",
    "rights",
    "sessionrestore",
    "studies",
    "support",
    "telemetry",
    "welcomeback",
  ];

  // else skip about:crashes, it is only available with plugin
  if (AppConstants.MOZ_CRASHREPORTER) {
    secureChromePages.push("crashes");
  }

  let nonSecureExamplePages = [
    "about:about",
    "about:credits",
    "about:home",
    "about:logo",
    "about:memory",
    "about:mozilla",
    "about:networking",
    "about:privatebrowsing",
    "about:robots",
    "about:serviceWorkers",
    "about:sync-log",
    "about:unloads",
    "about:url-classifier",
    "about:webrtc",
    "about:welcome",
    "http://example.com/" + DUMMY,
  ];

  for (let i = 0; i < secureChromePages.length; i++) {
    await BrowserTestUtils.withNewTab("about:" + secureChromePages[i], () => {
      is(getIdentityMode(), "chromeUI", "Identity should be chromeUI");
    });
  }

  for (let i = 0; i < nonSecureExamplePages.length; i++) {
    console.log(nonSecureExamplePages[i]);
    await BrowserTestUtils.withNewTab(nonSecureExamplePages[i], () => {
      ok(getIdentityMode() != "chromeUI", "Identity should not be chromeUI");
    });
  }
});

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
  is(getIdentityMode(), "localResource", "Identity should be localResource");

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
  is(getIdentityMode(), "localResource", "Identity should be localResource");

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
    set: [
      [INSECURE_TEXT_PREF, secureCheck],
      [INSECURE_ICON_PREF, secureCheck],
    ],
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
  is(getIdentityMode(), "localResource", "Identity should be localResource");

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
  is(getIdentityMode(), "localResource", "Identity should be localResource");

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

async function viewSourceTest() {
  let sourceTab = await loadNewTab("view-source:https://example.com/" + DUMMY);

  gBrowser.selectedTab = sourceTab;
  is(
    getIdentityMode(),
    "verifiedDomain",
    "Identity should be verified while viewing source"
  );

  gBrowser.removeTab(sourceTab);
}

add_task(async function test_viewSource() {
  await viewSourceTest();
});

async function insecureTest(secureCheck) {
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });

  let newTab = await loadNewTab("http://example.com/" + DUMMY);
  if (secureCheck) {
    is(getIdentityMode(), "notSecure", "Identity should be not secure");
    is(
      document.getElementById("identity-icon").getAttribute("tooltiptext"),
      gNavigatorBundle.getString("identity.notSecure.tooltip"),
      "The insecure lock icon has a correct tooltip text."
    );
  } else {
    is(getIdentityMode(), "unknownIdentity", "Identity should be unknown");
  }

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "localResource", "Identity should be localResource");

  gBrowser.selectedTab = newTab;
  if (secureCheck) {
    is(getIdentityMode(), "notSecure", "Identity should be not secure");
    is(
      document.getElementById("identity-icon").getAttribute("tooltiptext"),
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
  is(getIdentityMode(), "localResource", "Identity should be localResource");

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
  is(getIdentityMode(), "localResource", "Identity should be localResource");

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
  let dataURI = "resource://gre/modules/XPCOMUtils.sys.mjs";

  let newTab = await loadNewTab(dataURI);

  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(
    getIdentityMode(),
    "localResource",
    "Identity should be a local a resource"
  );

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
  is(
    getIdentityMode(),
    "certErrorPage notSecureText",
    "Identity should be the cert error page."
  );
  is(
    getConnectionState(),
    "cert-error-page",
    "Connection should be the cert error page."
  );

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "localResource", "Identity should be localResource");

  gBrowser.selectedTab = newTab;
  is(
    getIdentityMode(),
    "certErrorPage notSecureText",
    "Identity should be the cert error page."
  );
  is(
    getConnectionState(),
    "cert-error-page",
    "Connection should be the cert error page."
  );

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_about_net_error_uri() {
  await noCertErrorTest(true);
  await noCertErrorTest(false);
});

add_task(async function httpsOnlyErrorTest() {
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });
  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;

  let promise = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  BrowserTestUtils.loadURI(gBrowser, "http://nocert.example.com/");
  await promise;
  is(
    getIdentityMode(),
    "httpsOnlyErrorPage",
    "Identity should be the https-only mode error page."
  );
  is(
    getConnectionState(),
    "https-only-error-page",
    "Connection should be the https-only mode error page."
  );

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "localResource", "Identity should be localResource");

  gBrowser.selectedTab = newTab;
  is(
    getIdentityMode(),
    "httpsOnlyErrorPage",
    "Identity should be the https-only mode error page."
  );
  is(
    getConnectionState(),
    "https-only-error-page",
    "Connection should be the https-only mode page."
  );

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);

  await SpecialPowers.popPrefEnv();
});

async function noCertErrorFromNavigationTest(secureCheck) {
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });
  let newTab = await loadNewTab("http://example.com/" + DUMMY);

  let promise = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.document.getElementById("no-cert").click();
  });
  await promise;
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
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
  is(
    getIdentityMode(),
    "certErrorPage notSecureText",
    "Identity should be the cert error page."
  );
  is(
    getConnectionState(),
    "cert-error-page",
    "Connection should be the cert error page."
  );

  gBrowser.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_about_net_error_uri_from_navigation_tab() {
  await noCertErrorFromNavigationTest(true);
  await noCertErrorFromNavigationTest(false);
});

add_task(async function tlsErrorPageTest() {
  const TLS10_PAGE = "https://tls1.example.com/";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.tls.version.min", 3],
      ["security.tls.version.max", 4],
    ],
  });

  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TLS10_PAGE);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the net error");
  await pageLoaded;

  await SpecialPowers.spawn(browser, [], function() {
    const doc = content.document;
    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );
  });

  is(
    getConnectionState(),
    "cert-error-page",
    "Connection state should be the cert error page."
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await SpecialPowers.popPrefEnv();
});

add_task(async function netErrorPageTest() {
  // Connect to a server that rejects all requests, to test network error pages:
  let { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
  let server = new HttpServer();
  server.registerPrefixHandler("/", (req, res) =>
    res.abort(new Error("Noooope."))
  );
  server.start(-1);
  let port = server.identity.primaryPort;
  const ERROR_PAGE = `http://localhost:${port}/`;

  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, ERROR_PAGE);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the net error");
  await pageLoaded;

  await SpecialPowers.spawn(browser, [], function() {
    const doc = content.document;
    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );
  });

  is(
    getConnectionState(),
    "net-error-page",
    "Connection should be the net error page."
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

async function aboutBlockedTest(secureCheck) {
  let url = "http://www.itisatrap.org/firefox/its-an-attack.html";
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({
    set: [
      [INSECURE_ICON_PREF, secureCheck],
      ["urlclassifier.blockedTable", "moztest-block-simple"],
    ],
  });
  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url);

  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    url,
    true
  );

  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown.");
  is(getConnectionState(), "not-secure", "Connection should be not secure.");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "localResource", "Identity should be localResource");

  gBrowser.selectedTab = newTab;
  is(getIdentityMode(), "unknownIdentity", "Identity should be unknown.");
  is(getConnectionState(), "not-secure", "Connection should be not secure.");

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(oldTab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_about_blocked() {
  await aboutBlockedTest(true);
  await aboutBlockedTest(false);
});

add_task(async function noCertErrorSecurityConnectionBGTest() {
  let tab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = tab;
  let promise = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  BrowserTestUtils.loadURI(gBrowser, "https://nocert.example.com/");
  await promise;

  is(
    getSecurityConnectionBG(),
    `url("chrome://global/skin/icons/security-warning.svg")`,
    "Security connection should show a warning lock icon."
  );

  BrowserTestUtils.removeTab(tab);
});

async function aboutUriTest(secureCheck) {
  let oldTab = await loadNewTab("about:robots");
  await SpecialPowers.pushPrefEnv({ set: [[INSECURE_ICON_PREF, secureCheck]] });
  let aboutURI = "about:robots";

  let newTab = await loadNewTab(aboutURI);
  is(getConnectionState(), "file", "Connection should be file");

  gBrowser.selectedTab = oldTab;
  is(getIdentityMode(), "localResource", "Identity should be localResource");

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
  let readerURL = await getReaderModeURL();
  is(
    readerURL,
    "Site information for example.com",
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
  is(getIdentityMode(), "localResource", "Identity should be localResource");

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
    "localResource",
    "Identity should be localResource"
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
  let prefs = [
    [INSECURE_ICON_PREF, true],
    [INSECURE_PBMODE_ICON_PREF, true],
    [HTTPS_FIRST_PBM_PREF, false],
  ];
  await pbModeTest(prefs, true);
  prefs = [
    [INSECURE_ICON_PREF, false],
    [INSECURE_PBMODE_ICON_PREF, true],
    [HTTPS_FIRST_PBM_PREF, false],
  ];
  await pbModeTest(prefs, true);
  prefs = [
    [INSECURE_ICON_PREF, false],
    [INSECURE_PBMODE_ICON_PREF, false],
    [HTTPS_FIRST_PBM_PREF, false],
  ];
  await pbModeTest(prefs, false);
});

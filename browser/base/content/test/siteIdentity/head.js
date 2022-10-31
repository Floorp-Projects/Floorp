var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

function openIdentityPopup() {
  gIdentityHandler._initializePopup();
  let mainView = document.getElementById("identity-popup-mainView");
  let viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  gIdentityHandler._identityIconBox.click();
  return viewShown;
}

function openPermissionPopup() {
  gPermissionPanel._initializePopup();
  let mainView = document.getElementById("permission-popup-mainView");
  let viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  gPermissionPanel.openPopup();
  return viewShown;
}

function getIdentityMode(aWindow = window) {
  return aWindow.document.getElementById("identity-box").className;
}

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param tab
 *        The tab to load into.
 * @param [optional] url
 *        The url to load, or the current url.
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url) {
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  if (url) {
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);
  }

  return loaded;
}

// Compares the security state of the page with what is expected
function isSecurityState(browser, expectedState) {
  let ui = browser.securityUI;
  if (!ui) {
    ok(false, "No security UI to get the security state");
    return;
  }

  const wpl = Ci.nsIWebProgressListener;

  // determine the security state
  let isSecure = ui.state & wpl.STATE_IS_SECURE;
  let isBroken = ui.state & wpl.STATE_IS_BROKEN;
  let isInsecure = ui.state & wpl.STATE_IS_INSECURE;

  let actualState;
  if (isSecure && !(isBroken || isInsecure)) {
    actualState = "secure";
  } else if (isBroken && !(isSecure || isInsecure)) {
    actualState = "broken";
  } else if (isInsecure && !(isSecure || isBroken)) {
    actualState = "insecure";
  } else {
    actualState = "unknown";
  }

  is(
    expectedState,
    actualState,
    "Expected state " +
      expectedState +
      " and the actual state is " +
      actualState +
      "."
  );
}

/**
 * Test the state of the identity box and control center to make
 * sure they are correctly showing the expected mixed content states.
 *
 * @note The checks are done synchronously, but new code should wait on the
 *       returned Promise object to ensure the identity panel has closed.
 *       Bug 1221114 is filed to fix the existing code.
 *
 * @param tabbrowser
 * @param Object states
 *        MUST include the following properties:
 *        {
 *           activeLoaded: true|false,
 *           activeBlocked: true|false,
 *           passiveLoaded: true|false,
 *        }
 *
 * @return {Promise}
 * @resolves When the operation has finished and the identity panel has closed.
 */
async function assertMixedContentBlockingState(tabbrowser, states = {}) {
  if (
    !tabbrowser ||
    !("activeLoaded" in states) ||
    !("activeBlocked" in states) ||
    !("passiveLoaded" in states)
  ) {
    throw new Error(
      "assertMixedContentBlockingState requires a browser and a states object"
    );
  }

  let { passiveLoaded, activeLoaded, activeBlocked } = states;
  let { gIdentityHandler } = tabbrowser.ownerGlobal;
  let doc = tabbrowser.ownerDocument;
  let identityBox = gIdentityHandler._identityBox;
  let classList = identityBox.classList;
  let identityIcon = doc.getElementById("identity-icon");
  let identityIconImage = tabbrowser.ownerGlobal
    .getComputedStyle(identityIcon)
    .getPropertyValue("list-style-image");

  let stateSecure =
    gIdentityHandler._state & Ci.nsIWebProgressListener.STATE_IS_SECURE;
  let stateBroken =
    gIdentityHandler._state & Ci.nsIWebProgressListener.STATE_IS_BROKEN;
  let stateInsecure =
    gIdentityHandler._state & Ci.nsIWebProgressListener.STATE_IS_INSECURE;
  let stateActiveBlocked =
    gIdentityHandler._state &
    Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT;
  let stateActiveLoaded =
    gIdentityHandler._state &
    Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT;
  let statePassiveLoaded =
    gIdentityHandler._state &
    Ci.nsIWebProgressListener.STATE_LOADED_MIXED_DISPLAY_CONTENT;

  is(
    activeBlocked,
    !!stateActiveBlocked,
    "Expected state for activeBlocked matches UI state"
  );
  is(
    activeLoaded,
    !!stateActiveLoaded,
    "Expected state for activeLoaded matches UI state"
  );
  is(
    passiveLoaded,
    !!statePassiveLoaded,
    "Expected state for passiveLoaded matches UI state"
  );

  if (stateInsecure) {
    const insecureConnectionIcon = Services.prefs.getBoolPref(
      "security.insecure_connection_icon.enabled"
    );
    if (!insecureConnectionIcon) {
      // HTTP request, there should be no MCB classes for the identity box and the non secure icon
      // should always be visible regardless of MCB state.
      ok(classList.contains("unknownIdentity"), "unknownIdentity on HTTP page");
      ok(
        BrowserTestUtils.is_visible(identityIcon),
        "information icon should be still visible"
      );
    } else {
      // HTTP request, there should be a broken padlock shown always.
      ok(classList.contains("notSecure"), "notSecure on HTTP page");
      ok(
        !BrowserTestUtils.is_hidden(identityIcon),
        "information icon should be visible"
      );
    }

    ok(!classList.contains("mixedActiveContent"), "No MCB icon on HTTP page");
    ok(!classList.contains("mixedActiveBlocked"), "No MCB icon on HTTP page");
    ok(!classList.contains("mixedDisplayContent"), "No MCB icon on HTTP page");
    ok(
      !classList.contains("mixedDisplayContentLoadedActiveBlocked"),
      "No MCB icon on HTTP page"
    );
  } else {
    // Make sure the identity box UI has the correct mixedcontent states and icons
    is(
      classList.contains("mixedActiveContent"),
      activeLoaded,
      "identityBox has expected class for activeLoaded"
    );
    is(
      classList.contains("mixedActiveBlocked"),
      activeBlocked && !passiveLoaded,
      "identityBox has expected class for activeBlocked && !passiveLoaded"
    );
    is(
      classList.contains("mixedDisplayContent"),
      passiveLoaded && !(activeLoaded || activeBlocked),
      "identityBox has expected class for passiveLoaded && !(activeLoaded || activeBlocked)"
    );
    is(
      classList.contains("mixedDisplayContentLoadedActiveBlocked"),
      passiveLoaded && activeBlocked,
      "identityBox has expected class for passiveLoaded && activeBlocked"
    );

    ok(
      !BrowserTestUtils.is_hidden(identityIcon),
      "information icon should be visible"
    );
    if (activeLoaded) {
      is(
        identityIconImage,
        'url("chrome://global/skin/icons/security-broken.svg")',
        "Using active loaded icon"
      );
    }
    if (activeBlocked && !passiveLoaded) {
      is(
        identityIconImage,
        'url("chrome://global/skin/icons/security.svg")',
        "Using active blocked icon"
      );
    }
    if (passiveLoaded && !(activeLoaded || activeBlocked)) {
      is(
        identityIconImage,
        'url("chrome://global/skin/icons/security-warning.svg")',
        "Using passive loaded icon"
      );
    }
    if (passiveLoaded && activeBlocked) {
      is(
        identityIconImage,
        'url("chrome://global/skin/icons/security-warning.svg")',
        "Using active blocked and passive loaded icon"
      );
    }
  }

  // Make sure the identity popup has the correct mixedcontent states
  let promisePanelOpen = BrowserTestUtils.waitForEvent(
    tabbrowser.ownerGlobal,
    "popupshown",
    true,
    event => event.target == gIdentityHandler._identityPopup
  );
  gIdentityHandler._identityIconBox.click();
  await promisePanelOpen;
  let popupAttr = doc
    .getElementById("identity-popup")
    .getAttribute("mixedcontent");
  let bodyAttr = doc
    .getElementById("identity-popup-securityView-body")
    .getAttribute("mixedcontent");

  is(
    popupAttr.includes("active-loaded"),
    activeLoaded,
    "identity-popup has expected attr for activeLoaded"
  );
  is(
    bodyAttr.includes("active-loaded"),
    activeLoaded,
    "securityView-body has expected attr for activeLoaded"
  );

  is(
    popupAttr.includes("active-blocked"),
    activeBlocked,
    "identity-popup has expected attr for activeBlocked"
  );
  is(
    bodyAttr.includes("active-blocked"),
    activeBlocked,
    "securityView-body has expected attr for activeBlocked"
  );

  is(
    popupAttr.includes("passive-loaded"),
    passiveLoaded,
    "identity-popup has expected attr for passiveLoaded"
  );
  is(
    bodyAttr.includes("passive-loaded"),
    passiveLoaded,
    "securityView-body has expected attr for passiveLoaded"
  );

  // Make sure the correct icon is visible in the Control Center.
  // This logic is controlled with CSS, so this helps prevent regressions there.
  let securityViewBG = tabbrowser.ownerGlobal
    .getComputedStyle(
      document
        .getElementById("identity-popup-securityView")
        .getElementsByClassName("identity-popup-security-connection")[0]
    )
    .getPropertyValue("background-image");
  let securityContentBG = tabbrowser.ownerGlobal
    .getComputedStyle(
      document
        .getElementById("identity-popup-mainView")
        .getElementsByClassName("identity-popup-security-connection")[0]
    )
    .getPropertyValue("background-image");

  if (stateInsecure) {
    is(
      securityViewBG,
      'url("chrome://global/skin/icons/security-broken.svg")',
      "CC using 'not secure' icon"
    );
    is(
      securityContentBG,
      'url("chrome://global/skin/icons/security-broken.svg")',
      "CC using 'not secure' icon"
    );
  }

  if (stateSecure) {
    is(
      securityViewBG,
      'url("chrome://global/skin/icons/security.svg")',
      "CC using secure icon"
    );
    is(
      securityContentBG,
      'url("chrome://global/skin/icons/security.svg")',
      "CC using secure icon"
    );
  }

  if (stateBroken) {
    if (activeLoaded) {
      is(
        securityViewBG,
        'url("chrome://browser/skin/controlcenter/mcb-disabled.svg")',
        "CC using active loaded icon"
      );
      is(
        securityContentBG,
        'url("chrome://browser/skin/controlcenter/mcb-disabled.svg")',
        "CC using active loaded icon"
      );
    } else if (activeBlocked || passiveLoaded) {
      is(
        securityViewBG,
        'url("chrome://global/skin/icons/security-warning.svg")',
        "CC using degraded icon"
      );
      is(
        securityContentBG,
        'url("chrome://global/skin/icons/security-warning.svg")',
        "CC using degraded icon"
      );
    } else {
      // There is a case here with weak ciphers, but no bc tests are handling this yet.
      is(
        securityViewBG,
        'url("chrome://global/skin/icons/security.svg")',
        "CC using degraded icon"
      );
      is(
        securityContentBG,
        'url("chrome://global/skin/icons/security.svg")',
        "CC using degraded icon"
      );
    }
  }

  if (activeLoaded || activeBlocked || passiveLoaded) {
    let promiseViewShown = BrowserTestUtils.waitForEvent(
      gIdentityHandler._identityPopup,
      "ViewShown"
    );
    doc.getElementById("identity-popup-security-button").click();
    await promiseViewShown;
    is(
      Array.prototype.filter.call(
        doc
          .getElementById("identity-popup-securityView")
          .querySelectorAll(".identity-popup-mcb-learn-more"),
        element => !BrowserTestUtils.is_hidden(element)
      ).length,
      1,
      "The 'Learn more' link should be visible once."
    );
  }

  if (gIdentityHandler._identityPopup.state != "closed") {
    let hideEvent = BrowserTestUtils.waitForEvent(
      gIdentityHandler._identityPopup,
      "popuphidden"
    );
    info("Hiding identity popup");
    gIdentityHandler._identityPopup.hidePopup();
    await hideEvent;
  }
}

async function loadBadCertPage(url) {
  let loaded = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url);
  await loaded;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    content.document.getElementById("exceptionDialogButton").click();
  });
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
}

// nsITLSServerSocket needs a certificate with a corresponding private key
// available. In mochitests, the certificate with the common name "Mochitest
// client" has such a key.
function getTestServerCertificate() {
  const certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  for (const cert of certDB.getCerts()) {
    if (cert.commonName == "Mochitest client") {
      return cert;
    }
  }
  return null;
}

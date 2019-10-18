/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from common.js */

// Configuration vars
let homeURL = "https://www.mozilla.org/en-US/";
// Bug 1586294 - Localize the privacy policy URL (Services.urlFormatter?)
let privacyPolicyURL = "https://www.mozilla.org/en-US/privacy/firefox/";
let reportIssueURL = "https://mzl.la/fxr";
let licenseURL = "https://wiki.mozilla.org/Main_Page";

// https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XUL/browser
let browser = null;
// The following variable map to UI elements whose behavior changes depending
// on some state from the browser control
let urlInput = null;
let secureIcon = null;
let backButton = null;
let forwardButton = null;
let refreshButton = null;
let stopButton = null;

let { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

// Note: FxR UI uses a fork of browser-fullScreenAndPointerLock.js which removes
// the dependencies on browser.js.
// Bug 1587946 - Rationalize the fork of browser-fullScreenAndPointerLock.js
XPCOMUtils.defineLazyScriptGetter(
  this,
  "FullScreen",
  "chrome://fxr/content/fxr-fullScreen.js"
);
XPCOMUtils.defineLazyGetter(this, "gSystemPrincipal", () =>
  Services.scriptSecurityManager.getSystemPrincipal()
);

window.addEventListener(
  "DOMContentLoaded",
  () => {
    urlInput = document.getElementById("eUrlInput");
    secureIcon = document.getElementById("eUrlSecure");
    backButton = document.getElementById("eBack");
    forwardButton = document.getElementById("eForward");
    refreshButton = document.getElementById("eRefresh");
    stopButton = document.getElementById("eStop");

    setupBrowser();
    setupNavButtons();
    setupUrlBar();
  },
  { once: true }
);

// Create XUL browser object
function setupBrowser() {
  // Note: createXULElement is undefined when this page is not loaded
  // via chrome protocol
  if (document.createXULElement) {
    browser = document.createXULElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("remote", "true");
    browser.classList.add("browser_instance");
    document.getElementById("eBrowserContainer").appendChild(browser);

    browser.loadUrlWithSystemPrincipal = function(url) {
      this.loadURI(url, { triggeringPrincipal: gSystemPrincipal });
    };

    urlInput.value = homeURL;
    browser.loadUrlWithSystemPrincipal(homeURL);

    browser.addProgressListener(
      {
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIWebProgressListener,
          Ci.nsISupportsWeakReference,
        ]),
        onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
          // When URL changes, update the URL in the URL bar and update
          // whether the back/forward buttons are enabled.
          urlInput.value = browser.currentURI.spec;

          backButton.disabled = !browser.canGoBack;
          forwardButton.disabled = !browser.canGoForward;
        },
        onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
          if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
            // Network requests are complete. Disable (hide) the stop button
            // and enable (show) the refresh button
            refreshButton.disabled = false;
            stopButton.disabled = true;
          } else {
            // Network requests are outstanding. Disable (hide) the refresh
            // button and enable (show) the stop button
            refreshButton.disabled = true;
            stopButton.disabled = false;
          }
        },
        onSecurityChange(aWebProgress, aRequest, aState) {
          // Update the Secure Icon when the security status of the
          // content changes
          if (aState & Ci.nsIWebProgressListener.STATE_IS_SECURE) {
            secureIcon.style.visibility = "visible";
          } else {
            secureIcon.style.visibility = "hidden";
          }
        },
      },
      Ci.nsIWebProgress.NOTIFY_LOCATION |
        Ci.nsIWebProgress.NOTIFY_SECURITY |
        Ci.nsIWebProgress.NOTIFY_STATE_REQUEST
    );

    FullScreen.init();
  }
}

function setupNavButtons() {
  let aryNavButtons = [
    "eBack",
    "eForward",
    "eRefresh",
    "eStop",
    "eHome",
    "ePrefs",
  ];

  function navButtonHandler(e) {
    if (!this.disabled) {
      switch (this.id) {
        case "eBack":
          browser.goBack();
          break;

        case "eForward":
          browser.goForward();
          break;

        case "eRefresh":
          browser.reload();
          break;

        case "eStop":
          browser.stop();
          break;

        case "eHome":
          browser.loadUrlWithSystemPrincipal(homeURL);
          break;

        case "ePrefs":
          openSettings();
          break;
      }
    }
  }

  for (let btnName of aryNavButtons) {
    let elem = document.getElementById(btnName);
    elem.addEventListener("click", navButtonHandler);
  }
}

function setupUrlBar() {
  // Navigate to new value when the user presses "Enter"
  urlInput.addEventListener("keypress", async function(e) {
    if (e.key == "Enter") {
      // Use the URL Fixup Service in case the user wants to search instead
      // of directly navigating to a location.
      await Services.search.init();

      let valueToFixUp = urlInput.value;
      let flags =
        Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
        Services.uriFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
      if (PrivateBrowsingUtils.isWindowPrivate(window)) {
        flags |= Services.uriFixup.FIXUP_FLAG_PRIVATE_CONTEXT;
      }

      let uriToLoad = Services.uriFixup.createFixupURI(valueToFixUp, flags);

      browser.loadUrlWithSystemPrincipal(uriToLoad.spec);
      browser.focus();
    }
  });

  // Upon focus, highlight the whole URL
  urlInput.addEventListener("focus", function() {
    urlInput.select();
  });
}

function openSettings() {
  let browserSettingsUI = document.createXULElement("browser");
  browserSettingsUI.setAttribute("type", "chrome");
  browserSettingsUI.classList.add("browser_settings");

  showModalContainer(browserSettingsUI);

  browserSettingsUI.loadURI("chrome://fxr/content/prefs.html", {
    triggeringPrincipal: gSystemPrincipal,
  });
}

function closeSettings() {
  clearModalContainer();
}

function showPrivacyPolicy() {
  closeSettings();
  browser.loadUrlWithSystemPrincipal(privacyPolicyURL);
}

function showLicenseInfo() {
  closeSettings();
  browser.loadUrlWithSystemPrincipal(licenseURL);
}

function showReportIssue() {
  closeSettings();
  browser.loadUrlWithSystemPrincipal(reportIssueURL);
}

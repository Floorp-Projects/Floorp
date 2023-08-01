/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from common.js */
/* import-globals-from permissions.js */

// Configuration vars
let homeURL = "https://webxr.today/";
// Bug 1586294 - Localize the privacy policy URL (Services.urlFormatter?)
let privacyPolicyURL = "https://www.mozilla.org/en-US/privacy/firefox/";
let reportIssueURL = "https://mzl.la/fxr";
let licenseURL =
  "https://mixedreality.mozilla.org/FirefoxRealityPC/license.html";

// https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XUL/browser
let browser = null;
// Keep track of the current Permissions request to only allow one outstanding
// request/prompt at a time.
let currentPermissionRequest = null;
// And, keep a queue of pending Permissions requests to resolve when the
// current request finishes
let pendingPermissionRequests = [];
// The following variable map to UI elements whose behavior changes depending
// on some state from the browser control
let urlInput = null;
let secureIcon = null;
let backButton = null;
let forwardButton = null;
let refreshButton = null;
let stopButton = null;

const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

// Note: FxR UI uses a fork of browser-fullScreenAndPointerLock.js which removes
// the dependencies on browser.js.
// Bug 1587946 - Rationalize the fork of browser-fullScreenAndPointerLock.js
XPCOMUtils.defineLazyScriptGetter(
  this,
  "FullScreen",
  "chrome://fxr/content/fxr-fullScreen.js"
);
ChromeUtils.defineLazyGetter(this, "gSystemPrincipal", () =>
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

    browser.loadUrlWithSystemPrincipal = function (url) {
      this.loadURI(url, { triggeringPrincipal: gSystemPrincipal });
    };

    // Expose this function for Permissions to be used on this browser element
    // in other parts of the frontend
    browser.fxrPermissionPrompt = permissionPrompt;

    urlInput.value = homeURL;
    browser.loadUrlWithSystemPrincipal(homeURL);

    browser.addProgressListener(
      {
        QueryInterface: ChromeUtils.generateQI([
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
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

    // Send this notification to start and allow background scripts for
    // WebExtensions, since this FxR UI doesn't participate in typical
    // startup activities
    Services.obs.notifyObservers(window, "extensions-late-startup");
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
  urlInput.addEventListener("keypress", async function (e) {
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
      let { preferredURI } = Services.uriFixup.getFixupURIInfo(
        valueToFixUp,
        flags
      );

      browser.loadUrlWithSystemPrincipal(preferredURI.spec);
      browser.focus();
    }
  });

  // Upon focus, highlight the whole URL
  urlInput.addEventListener("focus", function () {
    urlInput.select();
  });
}

//
// Code to manage Settings UI
//

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

//
// Code to manage Permissions UI
//

function permissionPrompt(aRequest) {
  let newPrompt;
  if (aRequest instanceof Ci.nsIContentPermissionRequest) {
    newPrompt = new FxrContentPrompt(aRequest, this, finishPrompt);
  } else {
    newPrompt = new FxrWebRTCPrompt(aRequest, this, finishPrompt);
  }

  if (currentPermissionRequest) {
    // There is already an outstanding request running. Cache this new request
    // to be prompted later
    pendingPermissionRequests.push(newPrompt);
  } else {
    currentPermissionRequest = newPrompt;
    currentPermissionRequest.showPrompt();
  }
}

function finishPrompt() {
  if (pendingPermissionRequests.length) {
    // Prompt the next request
    currentPermissionRequest = pendingPermissionRequests.shift();
    currentPermissionRequest.showPrompt();
  } else {
    currentPermissionRequest = null;
  }
}

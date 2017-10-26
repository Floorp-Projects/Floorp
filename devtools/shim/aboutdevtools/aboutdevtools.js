/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu } = Components;
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

const DEVTOOLS_ENABLED_PREF = "devtools.enabled";

const MESSAGES = {
  AboutDebugging: "about-debugging-message",
  ContextMenu: "inspect-element-message",
  HamburgerMenu: "menu-message",
  KeyShortcut: "key-shortcut-message",
  SystemMenu: "menu-message",
};

// URL constructor doesn't support about: scheme,
// we have to use http in order to have working searchParams.
let url = new URL(window.location.href.replace("about:", "http://"));
let reason = url.searchParams.get("reason");
let tabid = parseInt(url.searchParams.get("tabid"), 10);

function getToolboxShortcut() {
  const bundleUrl = "chrome://devtools-shim/locale/key-shortcuts.properties";
  const bundle = Services.strings.createBundle(bundleUrl);
  const modifier = Services.appinfo.OS == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+";
  return modifier + bundle.GetStringFromName("toggleToolbox.commandkey");
}

function onInstallButtonClick() {
  Services.prefs.setBoolPref("devtools.enabled", true);
}

function updatePage() {
  const installPage = document.getElementById("install-page");
  const welcomePage = document.getElementById("welcome-page");
  const isEnabled = Services.prefs.getBoolPref("devtools.enabled");
  if (isEnabled) {
    installPage.setAttribute("hidden", "true");
    welcomePage.removeAttribute("hidden");
  } else {
    welcomePage.setAttribute("hidden", "true");
    installPage.removeAttribute("hidden");
  }
}

window.addEventListener("load", function () {
  const inspectorShortcut = getToolboxShortcut();
  const welcomeMessage = document.getElementById("welcome-message");
  welcomeMessage.textContent = welcomeMessage.textContent.replace(
    "##INSPECTOR_SHORTCUT##", inspectorShortcut);

  Services.prefs.addObserver(DEVTOOLS_ENABLED_PREF, updatePage);

  // Set the appropriate title message.
  if (reason == "ContextMenu") {
    document.getElementById("inspect-title").removeAttribute("hidden");
  } else {
    document.getElementById("common-title").removeAttribute("hidden");
  }

  // Display the message specific to the reason
  let id = MESSAGES[reason];
  if (id) {
    let message = document.getElementById(id);
    message.removeAttribute("hidden");
  }

  let installButton = document.getElementById("install");
  installButton.addEventListener("click", onInstallButtonClick);

  // Update the current page based on the current value of DEVTOOLS_ENABLED_PREF.
  updatePage();
}, { once: true });

window.addEventListener("beforeunload", function () {
  // Focus the tab that triggered the DevTools onboarding.
  if (document.visibilityState != "visible") {
    // Only try to focus the correct tab if the current tab is the about:devtools page.
    return;
  }

  // Retrieve the original tab if it is still available.
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let { gBrowser } = browserWindow;
  let originalBrowser = gBrowser.getBrowserForOuterWindowID(tabid);
  let originalTab = gBrowser.getTabForBrowser(originalBrowser);

  if (originalTab) {
    // If the original tab was found, select it.
    gBrowser.selectedTab = originalTab;
  }
}, {once: true});

window.addEventListener("unload", function () {
  let installButton = document.getElementById("install");
  installButton.removeEventListener("click", onInstallButtonClick);
  Services.prefs.removeObserver(DEVTOOLS_ENABLED_PREF, updatePage);
}, {once: true});

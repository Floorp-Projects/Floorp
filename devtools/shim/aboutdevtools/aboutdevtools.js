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

const ABOUTDEVTOOLS_STRINGS = "chrome://devtools-shim/locale/aboutdevtools.properties";
const aboutDevtoolsBundle = Services.strings.createBundle(ABOUTDEVTOOLS_STRINGS);

const KEY_SHORTCUTS_STRINGS = "chrome://devtools-shim/locale/key-shortcuts.properties";
const keyShortcutsBundle = Services.strings.createBundle(KEY_SHORTCUTS_STRINGS);

// URL constructor doesn't support about: scheme,
// we have to use http in order to have working searchParams.
let url = new URL(window.location.href.replace("about:", "http://"));
let reason = url.searchParams.get("reason");
let tabid = parseInt(url.searchParams.get("tabid"), 10);

function getToolboxShortcut() {
  const modifier = Services.appinfo.OS == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+";
  return modifier + keyShortcutsBundle.GetStringFromName("toggleToolbox.commandkey");
}

function onInstallButtonClick() {
  Services.prefs.setBoolPref("devtools.enabled", true);
}

function onCloseButtonClick() {
  window.close();
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

/**
 * Array of descriptors for features displayed on about:devtools.
 * Each feature should contain:
 * - icon: the name of the image to use
 * - title: the key of the localized title (from aboutdevtools.properties)
 * - desc: the key of the localized description (from aboutdevtools.properties)
 * - link: the MDN documentation link
 */
const features = [
  {
    icon: "chrome://devtools-shim/content/aboutdevtools/images/feature-inspector.svg",
    title: "features.inspector.title",
    desc: "features.inspector.desc",
    link: "https://developer.mozilla.org/docs/Tools/Page_Inspector",
  }, {
    icon: "chrome://devtools-shim/content/aboutdevtools/images/feature-console.svg",
    title: "features.console.title",
    desc: "features.console.desc",
    link: "https://developer.mozilla.org/docs/Tools/Web_Console",
  }, {
    icon: "chrome://devtools-shim/content/aboutdevtools/images/feature-debugger.svg",
    title: "features.debugger.title",
    desc: "features.debugger.desc",
    link: "https://developer.mozilla.org/docs/Tools/Debugger",
  }, {
    icon: "chrome://devtools-shim/content/aboutdevtools/images/feature-network.svg",
    title: "features.network.title",
    desc: "features.network.desc",
    link: "https://developer.mozilla.org/docs/Tools/Network_Monitor",
  }, {
    icon: "chrome://devtools-shim/content/aboutdevtools/images/feature-storage.svg",
    title: "features.storage.title",
    desc: "features.storage.desc",
    link: "https://developer.mozilla.org/docs/Tools/Storage_Inspector",
  }, {
    icon: "chrome://devtools-shim/content/aboutdevtools/images/feature-responsive.svg",
    title: "features.responsive.title",
    desc: "features.responsive.desc",
    link: "https://developer.mozilla.org/docs/Tools/Responsive_Design_Mode",
  }, {
    icon: "chrome://devtools-shim/content/aboutdevtools/images/feature-visualediting.svg",
    title: "features.visualediting.title",
    desc: "features.visualediting.desc",
    link: "https://developer.mozilla.org/docs/Tools/Style_Editor",
  }, {
    icon: "chrome://devtools-shim/content/aboutdevtools/images/feature-performance.svg",
    title: "features.performance.title",
    desc: "features.performance.desc",
    link: "https://developer.mozilla.org/docs/Tools/Performance",
  }, {
    icon: "chrome://devtools-shim/content/aboutdevtools/images/feature-memory.svg",
    title: "features.memory.title",
    desc: "features.memory.desc",
    link: "https://developer.mozilla.org/docs/Tools/Memory",
  },
];

/**
 * Helper to create a DOM element to represent a DevTools feature.
 */
function createFeatureEl(feature) {
  let li = document.createElement("li");
  li.classList.add("feature");
  let learnMore = aboutDevtoolsBundle.GetStringFromName("features.learnMore");

  let {icon, link, title, desc} = feature;
  title = aboutDevtoolsBundle.GetStringFromName(title);
  desc = aboutDevtoolsBundle.GetStringFromName(desc);
  // eslint-disable-next-line no-unsanitized/property
  li.innerHTML =
    `<a class="feature-link" href="${link}" target="_blank">
       <img class="feature-icon" src="${icon}"/>
     </a>
     <h3 class="feature-name">${title}</h3>
     <p class="feature-desc">
       ${desc}
       <a class="external feature-link" href="${link}" target="_blank">${learnMore}</a>
     </p>`;

  return li;
}

window.addEventListener("load", function () {
  const inspectorShortcut = getToolboxShortcut();
  const welcomeMessage = document.getElementById("welcome-message");
  welcomeMessage.textContent = welcomeMessage.textContent.replace(
    "##INSPECTOR_SHORTCUT##", inspectorShortcut);

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

  // Attach event listeners
  document.getElementById("install").addEventListener("click", onInstallButtonClick);
  document.getElementById("close").addEventListener("click", onCloseButtonClick);
  Services.prefs.addObserver(DEVTOOLS_ENABLED_PREF, updatePage);

  let featuresContainer = document.querySelector(".features-list");
  for (let feature of features) {
    featuresContainer.appendChild(createFeatureEl(feature));
  }

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
  document.getElementById("install").removeEventListener("click", onInstallButtonClick);
  document.getElementById("close").removeEventListener("click", onCloseButtonClick);
  Services.prefs.removeObserver(DEVTOOLS_ENABLED_PREF, updatePage);
}, {once: true});

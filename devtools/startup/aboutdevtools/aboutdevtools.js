/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});
const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const Telemetry = require("devtools/client/shared/telemetry");
const telemetry = new Telemetry();

const DEVTOOLS_ENABLED_PREF = "devtools.enabled";

const TELEMETRY_OPENED_KEY = "DEVTOOLS_ABOUT_DEVTOOLS_OPENED_KEY";
const TELEMETRY_OPENED_REASON = "DEVTOOLS_ABOUT_DEVTOOLS_OPENED_REASON";
const TELEMETRY_OPENED = "devtools.aboutdevtools.opened";
const TELEMETRY_INSTALLED = "devtools.aboutdevtools.installed";
const TELEMETRY_NOINSTALL_EXITS = "devtools.aboutdevtools.noinstall_exits";

const MESSAGES = {
  AboutDebugging: "about-debugging-message",
  ContextMenu: "inspect-element-message",
  HamburgerMenu: "menu-message",
  KeyShortcut: "key-shortcut-message",
  SystemMenu: "menu-message",
};

// Google analytics parameters that should be added to all outgoing links.
const GA_PARAMETERS = [
  ["utm_source", "devtools"],
  ["utm_medium", "onboarding"],
];

const ABOUTDEVTOOLS_STRINGS = "chrome://devtools-startup/locale/aboutdevtools.properties";
const aboutDevtoolsBundle = Services.strings.createBundle(ABOUTDEVTOOLS_STRINGS);

const KEY_SHORTCUTS_STRINGS = "chrome://devtools-startup/locale/key-shortcuts.properties";
const keyShortcutsBundle = Services.strings.createBundle(KEY_SHORTCUTS_STRINGS);

// URL constructor doesn't support about: scheme,
// we have to use http in order to have working searchParams.
const url = new URL(window.location.href.replace("about:", "http://"));
const reason = url.searchParams.get("reason");
const keyid = url.searchParams.get("keyid");
const tabid = parseInt(url.searchParams.get("tabid"), 10);

// Keep track of the initial devtools.enabled value to track exits in telemetry.
let isEnabledOnLoad;

function getToolboxShortcut() {
  const modifier = Services.appinfo.OS == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+";
  return modifier + keyShortcutsBundle.GetStringFromName("toggleToolbox.commandkey");
}

function onInstallButtonClick() {
  Services.prefs.setBoolPref("devtools.enabled", true);

  try {
    telemetry.scalarAdd(TELEMETRY_INSTALLED, 1);
  } catch (e) {
    dump("about:devtools oninstall telemetry failed: " + e + "\n");
  }
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
    icon: "chrome://devtools-startup/content/aboutdevtools/images/feature-inspector.svg",
    title: "features.inspector.title",
    desc: "features.inspector.desc",
    link: "https://developer.mozilla.org/docs/Tools/Page_Inspector",
  }, {
    icon: "chrome://devtools-startup/content/aboutdevtools/images/feature-console.svg",
    title: "features.console.title",
    desc: "features.console.desc",
    link: "https://developer.mozilla.org/docs/Tools/Web_Console",
  }, {
    icon: "chrome://devtools-startup/content/aboutdevtools/images/feature-debugger.svg",
    title: "features.debugger.title",
    desc: "features.debugger.desc",
    link: "https://developer.mozilla.org/docs/Tools/Debugger",
  }, {
    icon: "chrome://devtools-startup/content/aboutdevtools/images/feature-network.svg",
    title: "features.network.title",
    desc: "features.network.desc",
    link: "https://developer.mozilla.org/docs/Tools/Network_Monitor",
  }, {
    icon: "chrome://devtools-startup/content/aboutdevtools/images/feature-storage.svg",
    title: "features.storage.title",
    desc: "features.storage.desc",
    link: "https://developer.mozilla.org/docs/Tools/Storage_Inspector",
  }, {
    icon: "chrome://devtools-startup/content/aboutdevtools/images/feature-responsive.svg",
    title: "features.responsive.title",
    desc: "features.responsive.desc",
    link: "https://developer.mozilla.org/docs/Tools/Responsive_Design_Mode",
  }, {
    icon: "chrome://devtools-startup/content/aboutdevtools/images/feature-visualediting.svg",
    title: "features.visualediting.title",
    desc: "features.visualediting.desc",
    link: "https://developer.mozilla.org/docs/Tools/Style_Editor",
  }, {
    icon: "chrome://devtools-startup/content/aboutdevtools/images/feature-performance.svg",
    title: "features.performance.title",
    desc: "features.performance.desc",
    link: "https://developer.mozilla.org/docs/Tools/Performance",
  }, {
    icon: "chrome://devtools-startup/content/aboutdevtools/images/feature-memory.svg",
    title: "features.memory.title",
    desc: "features.memory.desc",
    link: "https://developer.mozilla.org/docs/Tools/Memory",
  },
];

/**
 * Helper to create a DOM element to represent a DevTools feature.
 */
function createFeatureEl(feature) {
  const li = document.createElement("li");
  li.classList.add("feature");
  const learnMore = aboutDevtoolsBundle.GetStringFromName("features.learnMore");

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

window.addEventListener("load", function() {
  const inspectorShortcut = getToolboxShortcut();
  const welcomeMessage = document.getElementById("welcome-message");

  // Set the welcome message content with the correct keyboard sortcut for the current
  // platform.
  welcomeMessage.textContent = aboutDevtoolsBundle.formatStringFromName("welcome.message",
    [inspectorShortcut], 1);

  // Set the appropriate title message.
  if (reason == "ContextMenu") {
    document.getElementById("inspect-title").removeAttribute("hidden");
  } else {
    document.getElementById("common-title").removeAttribute("hidden");
  }

  // Display the message specific to the reason
  const id = MESSAGES[reason];
  if (id) {
    const message = document.getElementById(id);
    message.removeAttribute("hidden");
  }

  // Attach event listeners
  document.getElementById("install").addEventListener("click", onInstallButtonClick);
  document.getElementById("close").addEventListener("click", onCloseButtonClick);
  Services.prefs.addObserver(DEVTOOLS_ENABLED_PREF, updatePage);

  const featuresContainer = document.querySelector(".features-list");
  for (const feature of features) {
    featuresContainer.appendChild(createFeatureEl(feature));
  }

  // Add Google Analytics parameters to all the external links.
  const externalLinks = [...document.querySelectorAll("a.external")];
  for (const link of externalLinks) {
    const linkUrl = new URL(link.getAttribute("href"));
    GA_PARAMETERS.forEach(([key, value]) => linkUrl.searchParams.set(key, value));
    link.setAttribute("href", linkUrl.href);
  }

  // Update the current page based on the current value of DEVTOOLS_ENABLED_PREF.
  updatePage();

  try {
    if (reason) {
      telemetry.getHistogramById(TELEMETRY_OPENED_REASON).add(reason);
    }

    if (keyid) {
      telemetry.getHistogramById(TELEMETRY_OPENED_KEY).add(keyid);
    }

    telemetry.scalarAdd(TELEMETRY_OPENED, 1);
  } catch (e) {
    dump("about:devtools onload telemetry failed: " + e + "\n");
  }
}, { once: true });

window.addEventListener("beforeunload", function() {
  // Focus the tab that triggered the DevTools onboarding.
  if (document.visibilityState != "visible") {
    // Only try to focus the correct tab if the current tab is the about:devtools page.
    return;
  }

  // Retrieve the original tab if it is still available.
  const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  const { gBrowser } = browserWindow;
  const originalBrowser = gBrowser.getBrowserForOuterWindowID(tabid);
  const originalTab = gBrowser.getTabForBrowser(originalBrowser);

  if (originalTab) {
    // If the original tab was found, select it.
    gBrowser.selectedTab = originalTab;
  }
}, {once: true});

window.addEventListener("unload", function() {
  document.getElementById("install").removeEventListener("click", onInstallButtonClick);
  document.getElementById("close").removeEventListener("click", onCloseButtonClick);
  Services.prefs.removeObserver(DEVTOOLS_ENABLED_PREF, updatePage);

  const isEnabled = Services.prefs.getBoolPref("devtools.enabled");
  if (!isEnabledOnLoad && !isEnabled) {
    try {
      telemetry.scalarAdd(TELEMETRY_NOINSTALL_EXITS, 1);
    } catch (e) {
      dump("about:devtools onunload telemetry failed: " + e + "\n");
    }
  }
}, {once: true});

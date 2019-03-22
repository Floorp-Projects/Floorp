/**
 * For detailed information on our policies, and a documention on this format
 * and its possibilites, please check the Mozilla-Wiki at
 *
 * https://wiki.mozilla.org/Compatibility/Go_Faster_Addon/Override_Policies_and_Workflows#User_Agent_overrides
 */

"use strict";

/* globals browser, filterOverrides, Injections, portsToAboutCompatTabs */

let InjectionsEnabled = true;

for (const injection of [
  {
    id: "testinjection",
    platform: "all",
    domain: "webcompat-addon-testcases.schub.io",
    bug: "1287966",
    contentScripts: {
      matches: ["*://webcompat-addon-testcases.schub.io/*"],
      css: [{file: "injections/css/bug0000000-dummy-css-injection.css"}],
      js: [{file: "injections/js/bug0000000-dummy-js-injection.js"}],
      runAt: "document_start",
    },
  }, {
    id: "bug1452707",
    platform: "desktop",
    domain: "ib.absa.co.za",
    bug: "1452707",
    contentScripts: {
      matches: ["https://ib.absa.co.za/*"],
      js: [{file: "injections/js/bug1452707-window.controllers-shim-ib.absa.co.za.js"}],
      runAt: "document_start",
    },
  }, {
    id: "bug1457335",
    platform: "desktop",
    domain: "histography.io",
    bug: "1457335",
    contentScripts: {
      matches: ["*://histography.io/*"],
      js: [{file: "injections/js/bug1457335-histography.io-ua-change.js"}],
      runAt: "document_start",
    },
  }, {
    id: "bug1472075",
    platform: "desktop",
    domain: "bankofamerica.com",
    bug: "1472075",
    contentScripts: {
      matches: ["*://*.bankofamerica.com/*"],
      js: [{file: "injections/js/bug1472075-bankofamerica.com-ua-change.js"}],
      runAt: "document_start",
    },
  }, {
    id: "bug1472081",
    platform: "desktop",
    domain: "election.gov.np",
    bug: "1472081",
    contentScripts: {
      matches: ["http://202.166.205.141/bbvrs/*"],
      js: [{file: "injections/js/bug1472081-election.gov.np-window.sidebar-shim.js"}],
      runAt: "document_start",
      allFrames: true,
    },
  }, {
    id: "bug1482066",
    platform: "desktop",
    domain: "portalminasnet.com",
    bug: "1482066",
    contentScripts: {
      matches: ["*://portalminasnet.com/*"],
      js: [{file: "injections/js/bug1482066-portalminasnet.com-window.sidebar-shim.js"}],
      runAt: "document_start",
      allFrames: true,
    },
  }, {
    id: "bug1526977",
    platform: "desktop",
    domain: "sreedharscce.in",
    bug: "1526977",
    contentScripts: {
      matches: ["*://*.sreedharscce.in/authenticate"],
      css: [{file: "injections/css/bug1526977-sreedharscce.in-login-fix.css"}],
    },
  }, {
    id: "bug1518781",
    platform: "desktop",
    domain: "twitch.tv",
    bug: "1518781",
    contentScripts: {
      matches: ["*://*.twitch.tv/*"],
      css: [{file: "injections/css/bug1518781-twitch.tv-webkit-scrollbar.css"}],
    },
  }, {
    id: "bug1305028",
    platform: "desktop",
    domain: "gaming.youtube.com",
    bug: "1305028",
    contentScripts: {
      matches: ["*://gaming.youtube.com/*"],
      css: [{file: "injections/css/bug1305028-gaming.youtube.com-webkit-scrollbar.css"}],
    },
  },
]) {
  Injections.push(injection);
}

let port = browser.runtime.connect();
const ActiveInjections = new Map();

async function registerContentScripts() {
  const platformMatches = ["all"];
  let platformInfo = await browser.runtime.getPlatformInfo();
  platformMatches.push(platformInfo.os == "android" ? "android" : "desktop");

  for (const injection of Injections) {
    if (platformMatches.includes(injection.platform)) {
      injection.availableOnPlatform = true;
      await enableInjection(injection);
    }
  }

  InjectionsEnabled = true;
  portsToAboutCompatTabs.broadcast({interventionsChanged: filterOverrides(Injections)});
}

async function enableInjection(injection) {
  if (injection.active) {
    return;
  }

  try {
    const handle = await browser.contentScripts.register(injection.contentScripts);
    ActiveInjections.set(injection, handle);
    injection.active = true;
  } catch (ex) {
    console.error("Registering WebCompat GoFaster content scripts failed: ", ex);
  }
}

function unregisterContentScripts() {
  for (const injection of Injections) {
    disableInjection(injection);
  }
  InjectionsEnabled = false;
  portsToAboutCompatTabs.broadcast({interventionsChanged: false});
}

async function disableInjection(injection) {
  if (!injection.active) {
    return;
  }

  const contentScript = ActiveInjections.get(injection);
  await contentScript.unregister();
  ActiveInjections.delete(injection);
  injection.active = false;
}

port.onMessage.addListener((message) => {
  switch (message.type) {
    case "injection-pref-changed":
      if (message.prefState) {
        registerContentScripts();
      } else {
        unregisterContentScripts();
      }
      break;
  }
});

const INJECTION_PREF = "perform_injections";
function checkInjectionPref() {
  browser.aboutConfigPrefs.getPref(INJECTION_PREF).then(value => {
    if (value === undefined) {
      browser.aboutConfigPrefs.setPref(INJECTION_PREF, true);
    } else if (value === false) {
      unregisterContentScripts();
    } else {
      registerContentScripts();
    }
  });
}
browser.aboutConfigPrefs.onPrefChange.addListener(checkInjectionPref, INJECTION_PREF);
checkInjectionPref();

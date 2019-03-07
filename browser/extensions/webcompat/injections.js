/**
 * For detailed information on our policies, and a documention on this format
 * and its possibilites, please check the Mozilla-Wiki at
 *
 * https://wiki.mozilla.org/Compatibility/Go_Faster_Addon/Override_Policies_and_Workflows#User_Agent_overrides
 */
const contentScripts = {
  universal: [
    {
      matches: ["*://webcompat-addon-testcases.schub.io/*"],
      css: [{file: "injections/css/bug0000000-dummy-css-injection.css"}],
      js: [{file: "injections/js/bug0000000-dummy-js-injection.js"}],
      runAt: "document_start",
    },
  ],
  desktop: [
    {
      matches: ["https://ib.absa.co.za/*"],
      js: [{file: "injections/js/bug1452707-window.controllers-shim-ib.absa.co.za.js"}],
      runAt: "document_start",
    },
    {
      matches: ["*://histography.io/*"],
      js: [{file: "injections/js/bug1457335-histography.io-ua-change.js"}],
      runAt: "document_start",
    },
    {
      matches: ["*://*.bankofamerica.com/*"],
      js: [{file: "injections/js/bug1472075-bankofamerica.com-ua-change.js"}],
      runAt: "document_start",
    },
    {
      matches: ["http://202.166.205.141/bbvrs/*"],
      js: [{file: "injections/js/bug1472081-election.gov.np-window.sidebar-shim.js"}],
      runAt: "document_start",
      allFrames: true,
    },
    {
      matches: ["*://portalminasnet.com/*"],
      js: [{file: "injections/js/bug1482066-portalminasnet.com-window.sidebar-shim.js"}],
      runAt: "document_start",
      allFrames: true,
    },
    {
      matches: ["*://*.sreedharscce.in/authenticate"],
      css: [{file: "injections/css/bug1526977-sreedharscce.in-login-fix.css"}],
    },
  ],
  android: [],
};

/* globals browser */

let port = browser.runtime.connect();
let registeredContentScripts = [];

async function registerContentScripts() {
  let platform = "desktop";
  let platformInfo = await browser.runtime.getPlatformInfo();
  if (platformInfo.os == "android") {
    platform = "android";
  }

  let targetContentScripts = contentScripts.universal.concat(contentScripts[platform]);
  targetContentScripts.forEach(async (contentScript) => {
    try {
      let handle = await browser.contentScripts.register(contentScript);
      registeredContentScripts.push(handle);
    } catch (ex) {
      console.error("Registering WebCompat GoFaster content scripts failed: ", ex);
    }
  });
}

function unregisterContentScripts() {
  registeredContentScripts.forEach((contentScript) => {
    contentScript.unregister();
  });
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

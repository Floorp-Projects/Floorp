/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Preferences.addAll([
  { id: "floorp.optimized.msbutton.ope", type: "bool" },
  { id: "floorp.hide.tabbrowser-tab.enable", type: "bool" },
  { id: "floorp.optimized.verticaltab", type: "bool" },
  { id: "floorp.horizontal.tab.position.shift", type: "bool" },
  { id: "floorp.Tree-type.verticaltab.optimization", type: "bool" },
  { id: "floorp.bookmarks.bar.focus.mode", type: "bool" },
  { id: "floorp.material.effect.enable", type: "bool" },
  { id: "enable.floorp.update", type: "bool" },
  { id: "enable.floorp.updater.latest", type: "bool" },
  { id: "ui.systemUsesDarkTheme", type: "int" },
  { id: "floorp.browser.user.interface", type: "int" },
  { id: "floorp.browser.tabbar.settings", type: "int" },
  { id: "floorp.bookmarks.fakestatus.mode", type: "bool" },
  { id: "floorp.search.top.mode", type: "bool" },
  { id: "floorp.legacy.menu.mode", type: "bool" },
  { id: "floorp.enable.auto.restart", type: "bool" },
  { id: "floorp.enable.multitab", type: "bool" },
  { id: "toolkit.legacyUserProfileCustomizations.script", type: "bool" },
  { id: "toolkit.tabbox.switchByScrolling", type: "bool" },
  { id: "browser.tabs.closeTabByDblclick", type: "bool" },
  { id: "floorp.download.notification", type: "int" },
  { id: "floorp.chrome.theme.mode", type: "int" },
  { id: "floorp.browser.UserAgent", type: "int" },
  { id: "floorp.legacy.dlui.enable", type: "bool" },
  { id: "floorp.downloading.red.color", type: "bool" },
  { id: "floorp.enable.dualtheme", type: "bool" },
  { id: "floorp.browser.rest.mode", type: "bool" },
  { id: "floorp.browser.sidebar.right", type: "bool" },
  { id: "floorp.browser.sidebar.enable", type: "bool" },
  { id: "floorp.browser.sidebar2.mode", type: "int" },
  { id: "floorp.browser.restore.sidebar.panel", type: "bool" },
  { id: "floorp.browser.sidebar.useIconProvider", type: "string" },
  { id: "floorp.navbar.bottom", type: "bool" },
  { id: "floorp.disable.fullscreen.notification", type: "bool" },
  { id: "floorp.tabsleep.enabled", type: "bool" },
  { id: "floorp.tabs.showPinnedTabsTitle", type: "bool" },
  { id: "floorp.browser.tabbar.multirow.max.enable", type: "bool"},
  { id: "floorp.browser.tabbar.multirow.newtab.enable", type: "bool"},
]);

window.addEventListener("pageshow", async function() {
  await gMainPane.initialized;
  const needreboot = document.getElementsByClassName("needeboot");
  for (let i = 0; i < needreboot.length; i++) {
    needreboot[i].addEventListener("click", function () {
      if (!Services.prefs.getBoolPref("floorp.enable.auto.restart", false)) {
        (async () => {
          let userConfirm = await confirmRestartPrompt(null)
          if (userConfirm == CONFIRM_RESTART_PROMPT_RESTART_NOW) {
            Services.startup.quit(
              Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
            );
          }
        })()
      } else {
        window.setTimeout(function () {
          Services.startup.quit(Services.startup.eAttemptQuit | Services.startup.eRestart);
        }, 500);
      }
    });
  }

  document.getElementById("translateoption").addEventListener("click", async function opentranslateroption() {
    const addon = await AddonManager.getAddonByID("{036a55b4-5e72-4d05-a06c-cba2dfcc134a}");
    await window.open(addon.optionsURL, '_blank');
  });

  document.getElementById("treestyletaboption").addEventListener("click", async function opentreestyletaboption() {
    const addon = await AddonManager.getAddonByID("treestyletab@piro.sakura.ne.jp");
    await window.open(addon.optionsURL, '_blank');
  });

  {
    let prefName = "floorp.browser.sidebar2.global.webpanel.width";
    let elem = document.getElementById("GlobalWidth");
    elem.value = Services.prefs.getIntPref(prefName, undefined);
    elem.addEventListener('change', function () {
      Services.prefs.setIntPref(prefName, Number(elem.value));
    });
    Services.prefs.addObserver(prefName, function () {
      elem.value = Services.prefs.getIntPref(prefName, undefined);
    });
  }

  {
    let prefName = "floorp.browser.tabbar.multirow.max.row";
    let elem = document.getElementById("MultirowValue");
    elem.value = Services.prefs.getIntPref(prefName, undefined);
    elem.addEventListener('change', function () {
      Services.prefs.setIntPref(prefName, Number(elem.value));
    });
    Services.prefs.addObserver(prefName, function () {
      elem.value = Services.prefs.getIntPref(prefName, undefined);
    });
  }

  {
    let prefName = "browser.tabs.tabMinWidth";
    let elem = document.getElementById("tabWidthValue");
    elem.value = Services.prefs.getIntPref(prefName, undefined);
    elem.addEventListener('change', function () {
      Services.prefs.setIntPref(prefName, Number(elem.value));
    });
    Services.prefs.addObserver(prefName, function () {
      elem.value = Services.prefs.getIntPref(prefName, undefined);
    });
  }

  {
    let prefName = "floorp.tabsleep.tabTimeoutMinutes";
    let elem = document.getElementById("tabSleepTimeoutMinutesValue");
    elem.value = Services.prefs.getIntPref(prefName, undefined);
    elem.addEventListener('change', function () {
      Services.prefs.setIntPref(prefName, Number(elem.value));
    });
    Services.prefs.addObserver(prefName, function () {
      elem.value = Services.prefs.getIntPref(prefName, undefined);
    });
  }

  document.getElementById("leptonButton").addEventListener("click", function () {
    window.location.href = "about:preferences#lepton";
  });
  document.getElementById("SetCustomURL").addEventListener("click", function () {
    window.location.href = "about:preferences#bSB";
  });
}, { once: true });

// Optimize for portable version
if (Services.prefs.getBoolPref("floorp.isPortable", false)) {
  // https://searchfox.org/mozilla-esr102/source/browser/components/preferences/main.js#1306-1311
  getShellService = function() {};
}

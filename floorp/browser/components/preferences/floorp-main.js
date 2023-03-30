/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Preferences.addAll([
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
  { id: "floorp.browser.tabbar.multirow.max.enabled", type: "bool"},
  { id: "floorp.browser.tabbar.multirow.newtab-inside.enabled", type: "bool"},
  { id: "floorp.openLinkInExternal.enabled", type: "bool" },
  { id: "floorp.openLinkInExternal.browserId", type: "string" },
  { id: "floorp.delete.browser.border", type: "bool" },
  { id: "floorp.browser.tabs.openNewTabPosition", type: "int" },
  { id: "floorp.browser.native.verticaltabs.enabled", type: "bool" },
  { id: "floorp.verticaltab.hover.enabled", type: "bool" },
]);

window.addEventListener("pageshow", async function() {
  await gMainPane.initialized;
  const needreboot = document.getElementsByClassName("needreboot");
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

  {
    function setOverrideUA(){
      if (document.getElementById("floorpUAs").value == 5) {
        document.getElementById("customUsergent").disabled = false;} else { document.getElementById("customUsergent").disabled = true;
      }
    }

    setOverrideUA();
    let prefName = "floorp.general.useragent.override";
    let elem = document.getElementById("customUsergent");
    elem.value = Services.prefs.getStringPref(prefName, undefined);

    elem.addEventListener('change', function () {
      Services.prefs.setStringPref(prefName, elem.value);
      Services.prefs.setStringPref("general.useragent.override", Services.prefs.getCharPref("floorp.general.useragent.override"));
    });
    Services.prefs.addObserver(prefName, function () {
      elem.value = Services.prefs.getStringPref(prefName, undefined);
    });
  }
    document.getElementById("floorpUAs").addEventListener("click", function () {
      setOverrideUA();
  });

  document.getElementById("leptonButton").addEventListener("click", function () {
    window.location.href = "about:preferences#lepton";
  });
  document.getElementById("SetCustomURL").addEventListener("click", function () {
    window.location.href = "about:preferences#bSB";
  });

  {
    const basePrefName = "extensions.checkCompatibility";
    const isNightlyPref = ![
      "aurora",
      "beta",
      "release",
      "esr",
    ].includes(AppConstants.MOZ_UPDATE_CHANNEL);
    const appVersion = Services.appinfo.version.replace(/^([^\.]+\.[0-9]+[a-z]*).*/gi, "$1");
    const appVersionMajor = appVersion.replace(/^([^\.]+)\.[0-9]+[a-z]*/gi, "$1");
    const prefNameNightly = `${basePrefName}.nightly`;
    const prefNameVersion = `${basePrefName}.${appVersion}`;
    const prefName = isNightlyPref
      ? prefNameNightly
      : prefNameVersion;
    let elem = document.getElementById("disableExtensionCheckCompatibility");
    elem.checked = !Services.prefs.getBoolPref(prefName, true);
    elem.addEventListener("command", function () {
      Services.prefs.setBoolPref(prefNameNightly, !elem.checked);
      for (let minor = 0; minor <= 15; minor++) {
        Services.prefs.setBoolPref(`${basePrefName}.${appVersionMajor}.${minor}`, !elem.checked);
      }
    });
    Services.prefs.addObserver(prefName, function () {
      elem.checked = !Services.prefs.getBoolPref(prefName, true);
    });
  }

  let elems = document.getElementsByClassName("multiRowTabs")
  for (let i = 0; i < elems.length; i++) {
    elems[i].disabled = Services.prefs.getBoolPref("floorp.browser.native.verticaltabs.enabled", false);
  }
}, { once: true });

// Optimize for portable version
if (Services.prefs.getBoolPref("floorp.isPortable", false)) {
  // https://searchfox.org/mozilla-esr102/source/browser/components/preferences/main.js#1306-1311
  getShellService = function() {};
}

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test records at which phase of startup the JS components and modules
 * are first loaded.
 * If you made changes that cause this test to fail, it's likely because you
 * are loading more JS code during startup.
 * Most code has no reason to run off of the app-startup notification
 * (this is very early, before we have selected the user profile, so
 *  preferences aren't accessible yet).
 * If your code isn't strictly required to show the first browser window,
 * it shouldn't be loaded before we are done with first paint.
 * Finally, if your code isn't really needed during startup, it should not be
 * loaded before we have started handling user events.
 */

"use strict";

/* Set this to true only for debugging purpose; it makes the output noisy. */
const kDumpAllStacks = false;

const startupPhases = {
  // For app-startup, we have an allowlist of acceptable JS files.
  // Anything loaded during app-startup must have a compelling reason
  // to run before we have even selected the user profile.
  // Consider loading your code after first paint instead,
  // eg. from BrowserGlue.jsm' _onFirstWindowLoaded method).
  "before profile selection": {
    allowlist: {
      modules: new Set([
        "resource:///modules/BrowserGlue.jsm",
        "resource://gre/modules/AppConstants.jsm",
        "resource://gre/modules/ActorManagerParent.jsm",
        "resource://gre/modules/ComponentUtils.jsm",
        "resource://gre/modules/CustomElementsListener.jsm",
        "resource://gre/modules/MainProcessSingleton.jsm",
        "resource://gre/modules/XPCOMUtils.jsm",
        "resource://gre/modules/Services.jsm",
      ]),
    },
  },

  // For the following phases of startup we have only a list of files that
  // are **not** allowed to load in this phase, as too many other scripts
  // load during this time.

  // We are at this phase after creating the first browser window (ie. after final-ui-startup).
  "before opening first browser window": {
    denylist: {
      modules: new Set([]),
    },
  },

  // We reach this phase right after showing the first browser window.
  // This means that anything already loaded at this point has been loaded
  // before first paint and delayed it.
  "before first paint": {
    denylist: {
      components: new Set(["nsSearchService.js"]),
      modules: new Set([
        "chrome://webcompat/content/data/ua_overrides.jsm",
        "chrome://webcompat/content/lib/ua_overrider.jsm",
        "resource:///modules/AboutNewTab.jsm",
        "resource:///modules/BrowserUsageTelemetry.jsm",
        "resource:///modules/ContentCrashHandlers.jsm",
        "resource:///modules/ShellService.jsm",
        "resource://gre/modules/NewTabUtils.jsm",
        "resource://gre/modules/PageThumbs.jsm",
        "resource://gre/modules/PlacesUtils.jsm",
        "resource://gre/modules/Preferences.jsm",
        "resource://gre/modules/Sqlite.jsm",
      ]),
      services: new Set(["@mozilla.org/browser/search-service;1"]),
    },
  },

  // We are at this phase once we are ready to handle user events.
  // Anything loaded at this phase or before gets in the way of the user
  // interacting with the first browser window.
  "before handling user events": {
    denylist: {
      components: new Set([
        "PageIconProtocolHandler.js",
        "nsPlacesExpiration.js",
      ]),
      modules: new Set([
        "resource://gre/modules/Blocklist.jsm",
        // Bug 1391495 - BrowserWindowTracker.jsm is intermittently used.
        // "resource:///modules/BrowserWindowTracker.jsm",
        "resource://gre/modules/BookmarkHTMLUtils.jsm",
        "resource://gre/modules/Bookmarks.jsm",
        "resource://gre/modules/ContextualIdentityService.jsm",
        "resource://gre/modules/CrashSubmit.jsm",
        "resource://gre/modules/FxAccounts.jsm",
        "resource://gre/modules/FxAccountsStorage.jsm",
        "resource://gre/modules/PlacesBackups.jsm",
        "resource://gre/modules/PlacesSyncUtils.jsm",
        "resource://gre/modules/PushComponents.jsm",
      ]),
      services: new Set([
        "@mozilla.org/browser/annotation-service;1",
        "@mozilla.org/browser/nav-bookmarks-service;1",
      ]),
    },
  },

  // Things that are expected to be completely out of the startup path
  // and loaded lazily when used for the first time by the user should
  // be listed here.
  "before becoming idle": {
    denylist: {
      components: new Set(["UnifiedComplete.js"]),
      modules: new Set([
        "resource://gre/modules/AsyncPrefs.jsm",
        "resource://gre/modules/LoginManagerContextMenu.jsm",
        "resource://pdf.js/PdfStreamConverter.jsm",
      ]),
    },
  },
};

if (
  Services.prefs.getBoolPref("browser.startup.blankWindow") &&
  Services.prefs.getCharPref(
    "extensions.activeThemeID",
    "default-theme@mozilla.org"
  ) == "default-theme@mozilla.org"
) {
  startupPhases["before profile selection"].allowlist.modules.add(
    "resource://gre/modules/XULStore.jsm"
  );
}

add_task(async function() {
  if (
    !AppConstants.NIGHTLY_BUILD &&
    !AppConstants.MOZ_DEV_EDITION &&
    !AppConstants.DEBUG
  ) {
    ok(
      !("@mozilla.org/test/startuprecorder;1" in Cc),
      "the startup recorder component shouldn't exist in this non-nightly/non-devedition/" +
        "non-debug build."
    );
    return;
  }

  let startupRecorder = Cc["@mozilla.org/test/startuprecorder;1"].getService()
    .wrappedJSObject;
  await startupRecorder.done;

  let componentStacks = new Map();
  let data = Cu.cloneInto(startupRecorder.data.code, {});
  // Keep only the file name for components, as the path is an absolute file
  // URL rather than a resource:// URL like for modules.
  for (let phase in data) {
    data[phase].components = data[phase].components
      .map(uri => {
        let fileName = uri.replace(/.*\//, "");
        componentStacks.set(fileName, Cu.getComponentLoadStack(uri));
        return fileName;
      })
      .filter(c => c != "startupRecorder.js");
  }

  function getStack(scriptType, name) {
    if (scriptType == "modules") {
      return Cu.getModuleImportStack(name);
    }
    if (scriptType == "components") {
      return componentStacks.get(name);
    }
    return "";
  }

  // This block only adds debug output to help find the next bugs to file,
  // it doesn't contribute to the actual test.
  SimpleTest.requestCompleteLog();
  let previous;
  for (let phase in data) {
    for (let scriptType in data[phase]) {
      for (let f of data[phase][scriptType]) {
        // phases are ordered, so if a script wasn't loaded yet at the immediate
        // previous phase, it wasn't loaded during any of the previous phases
        // either, and is new in the current phase.
        if (!previous || !data[previous][scriptType].includes(f)) {
          info(`${scriptType} loaded ${phase}: ${f}`);
          if (kDumpAllStacks) {
            info(getStack(scriptType, f));
          }
        }
      }
    }
    previous = phase;
  }

  for (let phase in startupPhases) {
    let loadedList = data[phase];
    let allowlist = startupPhases[phase].allowlist || null;
    if (allowlist) {
      for (let scriptType in allowlist) {
        loadedList[scriptType] = loadedList[scriptType].filter(c => {
          if (!allowlist[scriptType].has(c)) {
            return true;
          }
          allowlist[scriptType].delete(c);
          return false;
        });
        is(
          loadedList[scriptType].length,
          0,
          `should have no unexpected ${scriptType} loaded ${phase}`
        );
        for (let script of loadedList[scriptType]) {
          let message = `unexpected ${scriptType}: ${script}`;
          record(false, message, undefined, getStack(scriptType, script));
        }
        is(
          allowlist[scriptType].size,
          0,
          `all ${scriptType} allowlist entries should have been used`
        );
        for (let script of allowlist[scriptType]) {
          ok(false, `unused ${scriptType} allowlist entry: ${script}`);
        }
      }
    }
    let denylist = startupPhases[phase].denylist || null;
    if (denylist) {
      for (let scriptType in denylist) {
        for (let file of denylist[scriptType]) {
          let loaded = loadedList[scriptType].includes(file);
          let message = `${file} is not allowed ${phase}`;
          if (!loaded) {
            ok(true, message);
          } else {
            record(false, message, undefined, getStack(scriptType, file));
          }
        }
      }
    }
  }
});

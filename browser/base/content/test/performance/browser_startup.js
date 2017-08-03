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

const startupPhases = {
  // For app-startup, we have a whitelist of acceptable JS files.
  // Anything loaded during app-startup must have a compelling reason
  // to run before we have even selected the user profile.
  // Consider loading your code after first paint instead,
  // eg. from nsBrowserGlue.js' _onFirstWindowLoaded method).
  "before profile selection": {whitelist: {
    components: new Set([
      "nsBrowserGlue.js",
      "MainProcessSingleton.js",

      // Bugs to fix: The following components shouldn't be initialized that early.
      "WebContentConverter.js", // bug 1369443
      "nsSessionStartup.js", // bug 1369456
      "PushComponents.js", // bug 1369436
    ]),
    modules: new Set([
      "resource://gre/modules/AppConstants.jsm",
      "resource://gre/modules/XPCOMUtils.jsm",
      "resource://gre/modules/Services.jsm",

      // Bugs to fix: Probably loaded too early, needs investigation.
      "resource://gre/modules/RemotePageManager.jsm", // bug 1369466
    ])
  }},

  // For the following phases of startup we have only a black list for now

  // We are at this phase after creating the first browser window (ie. after final-ui-startup).
  "before opening first browser window": {blacklist: {
    components: new Set([
      "nsAsyncShutdown.js",
    ]),
    modules: new Set([
      "resource://gre/modules/PlacesBackups.jsm",
      "resource://gre/modules/PlacesUtils.jsm",
    ])
  }},

  // We reach this phase right after showing the first browser window.
  // This means that anything already loaded at this point has been loaded
  // before first paint and delayed it.
  "before first paint": {blacklist: {
    components: new Set([
      "UnifiedComplete.js",
      "nsSearchService.js",
    ]),
    modules: new Set([
      "chrome://webcompat-reporter/content/TabListener.jsm",
      "chrome://webcompat-reporter/content/WebCompatReporter.jsm",
      "resource:///modules/AboutNewTab.jsm",
      "resource:///modules/BrowserUITelemetry.jsm",
      "resource:///modules/BrowserUsageTelemetry.jsm",
      "resource:///modules/ContentCrashHandlers.jsm",
      "resource:///modules/DirectoryLinksProvider.jsm",
      "resource://gre/modules/NewTabUtils.jsm",
      "resource://gre/modules/PageThumbs.jsm",
      "resource://gre/modules/Promise.jsm", // imported by devtools during _delayedStartup
      "resource://gre/modules/Preferences.jsm",
    ]),
    services: new Set([
      "@mozilla.org/browser/search-service;1",
    ])
  }},

  // We are at this phase once we are ready to handle user events.
  // Anything loaded at this phase or before gets in the way of the user
  // interacting with the first browser window.
  "before handling user events": {blacklist: {
    components: new Set([
      "PageIconProtocolHandler.js",
      "PlacesCategoriesStarter.js",
      "nsPlacesExpiration.js",
    ]),
    modules: new Set([
      "resource:///modules/RecentWindow.jsm",
      "resource://gre/modules/BookmarkHTMLUtils.jsm",
      "resource://gre/modules/Bookmarks.jsm",
      "resource://gre/modules/ContextualIdentityService.jsm",
      "resource://gre/modules/CrashSubmit.jsm",
      "resource://gre/modules/FxAccounts.jsm",
      "resource://gre/modules/FxAccountsStorage.jsm",
      "resource://gre/modules/PlacesSyncUtils.jsm",
      "resource://gre/modules/Sqlite.jsm",
    ]),
    services: new Set([
      "@mozilla.org/browser/annotation-service;1",
      "@mozilla.org/browser/favicon-service;1",
      "@mozilla.org/browser/nav-bookmarks-service;1",
    ])
  }},

  // Things that are expected to be completely out of the startup path
  // and loaded lazily when used for the first time by the user should
  // be blacklisted here.
  "before becoming idle": {blacklist: {
    modules: new Set([
      "resource://gre/modules/AsyncPrefs.jsm",
      "resource://gre/modules/LoginManagerContextMenu.jsm",
      "resource://gre/modules/Task.jsm",
    ]),
  }},
};

if (!gBrowser.selectedBrowser.isRemoteBrowser) {
  // With e10s disabled, Places and RecentWindow.jsm (from a
  // SessionSaver.jsm timer) intermittently get loaded earlier. Likely
  // due to messages from the 'content' process arriving synchronously
  // instead of crossing a process boundary.
  info("merging the 'before handling user events' blacklist into the " +
       "'before first paint' one when e10s is disabled.");
  let from = startupPhases["before handling user events"].blacklist;
  let to = startupPhases["before first paint"].blacklist;
  for (let scriptType in from) {
    if (!(scriptType in to)) {
      to[scriptType] = from[scriptType];
    } else {
      for (let item of from[scriptType])
        to[scriptType].add(item);
    }
  }
  startupPhases["before handling user events"].blacklist = null;
}

add_task(async function() {
  if (!AppConstants.NIGHTLY_BUILD && !AppConstants.DEBUG) {
    ok(!("@mozilla.org/test/startuprecorder;1" in Cc),
       "the startup recorder component shouldn't exist in this non-nightly non-debug build.");
    return;
  }

  let startupRecorder = Cc["@mozilla.org/test/startuprecorder;1"].getService().wrappedJSObject;
  await startupRecorder.done;

  let data = startupRecorder.data.code;
  // Keep only the file name for components, as the path is an absolute file
  // URL rather than a resource:// URL like for modules.
  for (let phase in data) {
    data[phase].components =
      data[phase].components.map(f => f.replace(/.*\//, ""))
                            .filter(c => c != "startupRecorder.js");
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
        if (!previous || !data[previous][scriptType].includes(f))
          info(`${scriptType} loaded ${phase}: ${f}`);
      }
    }
    previous = phase;
  }

  for (let phase in startupPhases) {
    let loadedList = data[phase];
    let whitelist = startupPhases[phase].whitelist || null;
    if (whitelist) {
      for (let scriptType in whitelist) {
        loadedList[scriptType] = loadedList[scriptType].filter(c => {
          if (!whitelist[scriptType].has(c))
            return true;
          whitelist[scriptType].delete(c);
          return false;
        });
        is(loadedList[scriptType].length, 0,
           `should have no unexpected ${scriptType} loaded ${phase}`);
        for (let script of loadedList[scriptType]) {
          ok(false, `unexpected ${scriptType}: ${script}`);
        }
        is(whitelist[scriptType].size, 0,
           `all ${scriptType} whitelist entries should have been used`);
        for (let script of whitelist[scriptType]) {
          ok(false, `unused ${scriptType} whitelist entry: ${script}`);
        }
      }
    }
    let blacklist = startupPhases[phase].blacklist || null;
    if (blacklist) {
      for (let scriptType in blacklist) {
        for (let file of blacklist[scriptType]) {
          ok(!loadedList[scriptType].includes(file), `${file} is not allowed ${phase}`);
        }
      }
    }
  }
});

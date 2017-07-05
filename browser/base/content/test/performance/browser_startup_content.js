/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test records which services, JS components and JS modules are loaded
 * when creating a new content process.
 *
 * If you made changes that cause this test to fail, it's likely because you
 * are loading more JS code during content process startup.
 *
 * If your code isn't strictly required to show a page, consider loading it
 * lazily. If you can't, consider delaying its load until after we have started
 * handling user events.
 */

"use strict";

const blacklist = {
  components: new Set([
    "PushComponents.js",
    "TelemetryStartup.js",
  ]),
  modules: new Set([
    "resource:///modules/ContentWebRTC.jsm",
    "resource://gre/modules/InlineSpellChecker.jsm",
    "resource://gre/modules/InlineSpellCheckerContent.jsm",
    "resource://gre/modules/Promise.jsm",
    "resource://gre/modules/Task.jsm",
    "resource://gre/modules/debug.js",
    "resource://gre/modules/osfile.jsm",
  ]),
  services: new Set([
    "@mozilla.org/base/telemetry-startup;1",
    "@mozilla.org/push/Service;1",
  ])
};

add_task(async function() {
  SimpleTest.requestCompleteLog();

  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
                                                         forceNewProcess: true});

  let mm = gBrowser.selectedBrowser.messageManager;
  let promise = BrowserTestUtils.waitForMessage(mm, "Test:LoadedScripts");

  // Load a custom frame script to avoid using ContentTask which loads Task.jsm
  mm.loadFrameScript("data:text/javascript,(" + function() {
    /* eslint-env mozilla/frame-script */
    const {classes: Cc, interfaces: Ci, manager: Cm} = Components;
    Cm.QueryInterface(Ci.nsIServiceManager);

    let loader = Cc["@mozilla.org/moz/jsloader;1"].getService(Ci.xpcIJSModuleLoader);
    sendAsyncMessage("Test:LoadedScripts", {
      /* Keep only the file name for components, as the path is an absolute file
         URL rather than a resource:// URL like for modules. */
      components: loader.loadedComponents().map(f => f.replace(/.*\//, "")),
      modules: loader.loadedModules(),
      services: Object.keys(Cc).filter(c => {
        try {
          Cm.isServiceInstantiatedByContractID(c, Ci.nsISupports);
          return true;
        } catch (e) {
          return false;
        }
      })
    });
  } + ")()", false);

  let loadedList = await promise;
  for (let scriptType in blacklist) {
    info(scriptType);
    for (let file of blacklist[scriptType]) {
      ok(!loadedList[scriptType].includes(file), `${file} is not allowed`);
    }
    for (let file of loadedList[scriptType]) {
      info(file);
    }
  }

  await BrowserTestUtils.removeTab(tab);
});

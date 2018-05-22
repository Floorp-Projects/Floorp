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

/* Set this to true only for debugging purpose; it makes the output noisy. */
const kDumpAllStacks = false;

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
    "resource://gre/modules/osfile.jsm",
    "resource://pdf.js/PdfJs.jsm",
    "resource://pdf.js/PdfStreamConverter.jsm",
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
    const Cm = Components.manager;
    Cm.QueryInterface(Ci.nsIServiceManager);
    ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
    let collectStacks = AppConstants.NIGHTLY_BUILD || AppConstants.DEBUG;
    let loader = Cc["@mozilla.org/moz/jsloader;1"].getService(Ci.xpcIJSModuleLoader);
    let components = {};
    for (let component of loader.loadedComponents()) {
      /* Keep only the file name for components, as the path is an absolute file
         URL rather than a resource:// URL like for modules. */
      components[component.replace(/.*\//, "")] =
        collectStacks ? loader.getComponentLoadStack(component) : "";
    }
    let modules = {};
    for (let module of loader.loadedModules()) {
      modules[module] = collectStacks ? loader.getModuleImportStack(module) : "";
    }
    let services = {};
    for (let contractID in Object.keys(Cc)) {
      try {
        if (Cm.isServiceInstantiatedByContractID(contractID, Ci.nsISupports))
          services[contractID] = "";
      } catch (e) {}
    }
    sendAsyncMessage("Test:LoadedScripts", {components, modules, services});
  } + ")()", false);

  let loadedList = await promise;
  for (let scriptType in blacklist) {
    info(scriptType);
    for (let file of blacklist[scriptType]) {
      let loaded = file in loadedList[scriptType];
      ok(!loaded, `${file} is not allowed`);
      if (loaded && loadedList[scriptType][file])
        info(loadedList[scriptType][file]);
    }
    for (let file in loadedList[scriptType]) {
      info(file);
      if (kDumpAllStacks && loadedList[scriptType][file])
        info(loadedList[scriptType][file]);
    }
  }

  BrowserTestUtils.removeTab(tab);
});

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test records which services, frame scripts, process scripts, and
 * JS modules are loaded when creating a new content process.
 *
 * If you made changes that cause this test to fail, it's likely because you
 * are loading more JS code during content process startup. Please try to
 * avoid this.
 *
 * If your code isn't strictly required to show a page, consider loading it
 * lazily. If you can't, consider delaying its load until after we have started
 * handling user events.
 */

"use strict";

/* Set this to true only for debugging purpose; it makes the output noisy. */
const kDumpAllStacks = false;

const known_scripts = {
  modules: new Set([
    "chrome://mochikit/content/ShutdownLeaksCollector.jsm",

    // General utilities
    "resource://gre/modules/AppConstants.jsm",
    "resource://gre/modules/DeferredTask.jsm",
    "resource://gre/modules/Timer.jsm",
    "resource://gre/modules/XPCOMUtils.sys.mjs",

    // Logging related
    "resource://gre/modules/Log.jsm",

    // Browser front-end
    "resource:///actors/AboutReaderChild.jsm",
    "resource:///actors/BrowserTabChild.jsm",
    "resource:///actors/LinkHandlerChild.jsm",
    "resource:///actors/PageStyleChild.jsm",
    "resource:///actors/SearchSERPTelemetryChild.jsm",
    "resource://gre/actors/ContentMetaChild.jsm",
    "resource://gre/modules/Readerable.jsm",

    // Telemetry
    "resource://gre/modules/TelemetryControllerBase.jsm", // bug 1470339
    "resource://gre/modules/TelemetryControllerContent.jsm", // bug 1470339

    // Extensions
    "resource://gre/modules/ExtensionProcessScript.jsm",
    "resource://gre/modules/ExtensionUtils.jsm",
  ]),
  frameScripts: new Set([
    // Test related
    "chrome://mochikit/content/shutdown-leaks-collector.js",
  ]),
  processScripts: new Set([
    "chrome://global/content/process-content.js",
    "resource://gre/modules/extensionProcessScriptLoader.js",
  ]),
};

if (!gFissionBrowser) {
  known_scripts.modules.add(
    "resource:///modules/sessionstore/ContentSessionStore.jsm"
  );
}

// Items on this list *might* load when creating the process, as opposed to
// items in the main list, which we expect will always load.
const intermittently_loaded_scripts = {
  modules: new Set([
    "resource://gre/modules/nsAsyncShutdown.jsm",
    "resource://gre/modules/sessionstore/Utils.jsm",

    // Session store.
    "resource://gre/modules/sessionstore/SessionHistory.jsm",

    // Webcompat about:config front-end. This is part of a system add-on which
    // may not load early enough for the test.
    "resource://webcompat/AboutCompat.jsm",

    // Test related
    "chrome://remote/content/marionette/actors/MarionetteEventsChild.jsm",
    "chrome://remote/content/shared/Log.jsm",
    "resource://testing-common/BrowserTestUtilsChild.jsm",
    "resource://testing-common/ContentEventListenerChild.jsm",
    "resource://specialpowers/AppTestDelegateChild.jsm",
    "resource://specialpowers/SpecialPowersChild.jsm",
    "resource://specialpowers/WrapPrivileged.jsm",
  ]),
  frameScripts: new Set([]),
  processScripts: new Set([
    // Webcompat about:config front-end. This is presently nightly-only and
    // part of a system add-on which may not load early enough for the test.
    "resource://webcompat/aboutPageProcessScript.js",
  ]),
};

const forbiddenScripts = {
  services: new Set([
    "@mozilla.org/base/telemetry-startup;1",
    "@mozilla.org/embedcomp/default-tooltiptextprovider;1",
    "@mozilla.org/push/Service;1",
  ]),
};

add_task(async function() {
  SimpleTest.requestCompleteLog();

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url:
      getRootDirectory(gTestPath).replace(
        "chrome://mochitests/content",
        "http://example.com"
      ) + "file_empty.html",
    forceNewProcess: true,
  });

  let mm = gBrowser.selectedBrowser.messageManager;
  let promise = BrowserTestUtils.waitForMessage(mm, "Test:LoadedScripts");

  // Load a custom frame script to avoid using ContentTask which loads Task.jsm
  mm.loadFrameScript(
    "data:text/javascript,(" +
      function() {
        /* eslint-env mozilla/frame-script */
        const Cm = Components.manager;
        Cm.QueryInterface(Ci.nsIServiceManager);
        const { AppConstants } = ChromeUtils.import(
          "resource://gre/modules/AppConstants.jsm"
        );
        let collectStacks = AppConstants.NIGHTLY_BUILD || AppConstants.DEBUG;
        let modules = {};
        for (let module of Cu.loadedJSModules) {
          modules[module] = collectStacks
            ? Cu.getModuleImportStack(module)
            : "";
        }
        for (let module of Cu.loadedESModules) {
          modules[module] = collectStacks
            ? Cu.getModuleImportStack(module)
            : "";
        }
        let services = {};
        for (let contractID of Object.keys(Cc)) {
          try {
            if (
              Cm.isServiceInstantiatedByContractID(contractID, Ci.nsISupports)
            ) {
              services[contractID] = "";
            }
          } catch (e) {}
        }
        sendAsyncMessage("Test:LoadedScripts", {
          modules,
          services,
        });
      } +
      ")()",
    false
  );

  let loadedInfo = await promise;

  // Gather loaded frame scripts.
  loadedInfo.frameScripts = {};
  for (let [uri] of Services.mm.getDelayedFrameScripts()) {
    loadedInfo.frameScripts[uri] = "";
  }

  // Gather loaded process scripts.
  loadedInfo.processScripts = {};
  for (let [uri] of Services.ppmm.getDelayedProcessScripts()) {
    loadedInfo.processScripts[uri] = "";
  }

  await checkLoadedScripts({
    loadedInfo,
    known: known_scripts,
    intermittent: intermittently_loaded_scripts,
    forbidden: forbiddenScripts,
    dumpAllStacks: kDumpAllStacks,
  });

  BrowserTestUtils.removeTab(tab);
});

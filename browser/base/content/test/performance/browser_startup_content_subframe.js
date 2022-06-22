/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test records which services, JS components, frame scripts, process
 * scripts, and JS modules are loaded when creating a new content process for a
 * subframe.
 *
 * If you made changes that cause this test to fail, it's likely because you
 * are loading more JS code during content process startup. Please try to
 * avoid this.
 *
 * If your code isn't strictly required to show an iframe, consider loading it
 * lazily. If you can't, consider delaying its load until after we have started
 * handling user events.
 *
 * This test differs from browser_startup_content.js in that it tests a process
 * with no toplevel browsers opened, but with a single subframe document
 * loaded. This leads to a different set of scripts being loaded.
 */

"use strict";

const actorModuleURI =
  getRootDirectory(gTestPath) + "StartupContentSubframe.jsm";
const subframeURI =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://example.com"
  ) + "file_empty.html";

// Set this to true only for debugging purpose; it makes the output noisy.
const kDumpAllStacks = false;

const known_scripts = {
  modules: new Set([
    // Loaded by this test
    actorModuleURI,

    "resource:///modules/StartupRecorder.jsm",

    // General utilities
    "resource://gre/modules/AppConstants.jsm",
    "resource://gre/modules/DeferredTask.jsm",
    "resource://gre/modules/Services.jsm", // bug 1464542
    "resource://gre/modules/XPCOMUtils.jsm",

    // Logging related
    "resource://gre/modules/Log.jsm",

    // Browser front-end
    "resource:///actors/PageStyleChild.jsm",

    // Telemetry
    "resource://gre/modules/TelemetryControllerBase.jsm", // bug 1470339
    "resource://gre/modules/TelemetryControllerContent.jsm", // bug 1470339

    // Extensions
    "resource://gre/modules/ExtensionProcessScript.jsm",
    "resource://gre/modules/ExtensionUtils.jsm",
  ]),
  processScripts: new Set([
    "chrome://global/content/process-content.js",
    "resource://gre/modules/extensionProcessScriptLoader.js",
  ]),
};

// Items on this list *might* load when creating the process, as opposed to
// items in the main list, which we expect will always load.
const intermittently_loaded_scripts = {
  modules: new Set([
    "resource://gre/modules/nsAsyncShutdown.jsm",

    // Test related
    "chrome://remote/content/marionette/actors/MarionetteEventsChild.jsm",
    "chrome://remote/content/shared/Log.jsm",
    "resource://testing-common/BrowserTestUtilsChild.jsm",
    "resource://testing-common/ContentEventListenerChild.jsm",
    "resource://specialpowers/SpecialPowersChild.jsm",
    "resource://specialpowers/AppTestDelegateChild.jsm",
    "resource://specialpowers/WrapPrivileged.jsm",
  ]),
  processScripts: new Set([]),
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

  // Increase the maximum number of webIsolated content processes to make sure
  // our newly-created iframe is spawned into a new content process.
  //
  // Unfortunately, we don't have something like `forceNewProcess` for subframe
  // loads.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount.webIsolated", 10]],
  });
  Services.ppmm.releaseCachedProcesses();

  // Register a custom window actor which will send us a notification when the
  // script loading information is available.
  ChromeUtils.registerWindowActor("StartupContentSubframe", {
    parent: {
      moduleURI: actorModuleURI,
    },
    child: {
      moduleURI: actorModuleURI,
      events: {
        load: { mozSystemGroup: true, capture: true },
      },
    },
    matches: [subframeURI],
    allFrames: true,
  });

  // Create a tab, and load a remote subframe with the specific URI in it.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  SpecialPowers.spawn(tab.linkedBrowser, [subframeURI], uri => {
    let iframe = content.document.createElement("iframe");
    iframe.src = uri;
    content.document.body.appendChild(iframe);
  });

  // Wait for the reply to come in, remove the XPCOM wrapper, and unregister our actor.
  let [subject] = await TestUtils.topicObserved(
    "startup-content-subframe-loaded-scripts"
  );
  let loadedInfo = subject.wrappedJSObject;

  ChromeUtils.unregisterWindowActor("StartupContentSubframe");
  BrowserTestUtils.removeTab(tab);

  // Gather loaded process scripts.
  loadedInfo.processScripts = {};
  for (let [uri] of Services.ppmm.getDelayedProcessScripts()) {
    loadedInfo.processScripts[uri] = "";
  }

  checkLoadedScripts({
    loadedInfo,
    known: known_scripts,
    intermittent: intermittently_loaded_scripts,
    forbidden: forbiddenScripts,
    dumpAllStacks: kDumpAllStacks,
  });
});

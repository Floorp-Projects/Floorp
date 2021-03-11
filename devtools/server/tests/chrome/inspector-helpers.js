/* exported attachURL, promiseDone,
   promiseOnce,
   addTest, addAsyncTest,
   runNextTest, _documentWalker,
   createResourceWatcher */
"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const {
  TabDescriptorFactory,
} = require("devtools/client/framework/tab-descriptor-factory");
const { DevToolsServer } = require("devtools/server/devtools-server");
const {
  BrowserTestUtils,
} = require("resource://testing-common/BrowserTestUtils.jsm");
const {
  DocumentWalker: _documentWalker,
} = require("devtools/server/actors/inspector/document-walker");

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const Services = require("Services");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

if (!DevToolsServer.initialized) {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  SimpleTest.registerCleanupFunction(function() {
    DevToolsServer.destroy();
  });
}

var gAttachCleanups = [];

SimpleTest.registerCleanupFunction(function() {
  for (const cleanup of gAttachCleanups) {
    cleanup();
  }
});

/**
 * Add a new test tab in the browser and load the given url.
 * @return Promise a promise that resolves to the new target representing
 *         the page currently opened.
 */

async function getTargetForSelectedTab(gBrowser) {
  const selectedTab = gBrowser.selectedTab;
  await BrowserTestUtils.browserLoaded(selectedTab.linkedBrowser);
  const descriptor = await TabDescriptorFactory.createDescriptorForTab(
    selectedTab
  );
  return descriptor.getTarget();
}

/**
 * Open a tab, load the url, wait for it to signal its readiness,
 * find the tab with the devtools server, and call the callback.
 *
 * Returns a function which can be called to close the opened ta
 * and disconnect its devtools client.
 */
async function attachURL(url) {
  // Get the current browser window
  const gBrowser = Services.wm.getMostRecentWindow("navigator:browser")
    .gBrowser;

  // open the url in a new tab, save a reference to the new inner window global object
  // and wait for it to load. The tests rely on this window object to send a "ready"
  // event to its opener (the test page). This window reference is used within
  // the test tab, to reference the webpage being tested against, which is in another
  // tab.
  const windowOpened = BrowserTestUtils.waitForNewTab(gBrowser, url);
  const win = window.open(url, "_blank");
  await windowOpened;

  const target = await getTargetForSelectedTab(gBrowser);
  await target.attach();

  const cleanup = async function() {
    await target.destroy();
    if (win) {
      win.close();
    }
  };

  gAttachCleanups.push(cleanup);
  return { target, doc: win.document };
}

function promiseOnce(target, event) {
  return new Promise(resolve => {
    target.on(event, (...args) => {
      if (args.length === 1) {
        resolve(args[0]);
      } else {
        resolve(args);
      }
    });
  });
}

function promiseDone(currentPromise) {
  currentPromise.catch(err => {
    ok(false, "Promise failed: " + err);
    if (err.stack) {
      dump(err.stack);
    }
    SimpleTest.finish();
  });
}

var _tests = [];
function addTest(test) {
  _tests.push(test);
}

function addAsyncTest(generator) {
  _tests.push(() => generator().catch(ok.bind(null, false)));
}

function runNextTest() {
  if (_tests.length == 0) {
    SimpleTest.finish();
    return;
  }
  const fn = _tests.shift();
  try {
    fn();
  } catch (ex) {
    info(
      "Test function " +
        (fn.name ? "'" + fn.name + "' " : "") +
        "threw an exception: " +
        ex
    );
  }
}

async function createResourceWatcher(target) {
  const commands = await target.descriptorFront.getCommands();
  const targetList = commands.targetCommand;
  await targetList.startListening();
  return new ResourceWatcher(targetList);
}

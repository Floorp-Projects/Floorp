/* exported attachURL, promiseDone,
   promiseOnce,
   addTest, addAsyncTest,
   runNextTest, _documentWalker */
"use strict";

const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const {
  CommandsFactory,
} = require("devtools/shared/commands/commands-factory");
const { DevToolsServer } = require("devtools/server/devtools-server");
const {
  BrowserTestUtils,
} = require("resource://testing-common/BrowserTestUtils.jsm");
const {
  DocumentWalker: _documentWalker,
} = require("devtools/server/actors/inspector/document-walker");

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
 * Open a tab, load the url, wait for it to signal its readiness,
 * connect to this tab via DevTools protocol and return.
 *
 * Returns an object with a few helpful attributes:
 * - commands {Object}: The commands object defined by modules from devtools/shared/commands
 * - target {TargetFront}: The current top-level target front.
 * - doc {HtmlDocument}: the tab's document that got opened
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

  const commands = await CommandsFactory.forTab(gBrowser.selectedTab);
  await commands.targetCommand.startListening();

  const cleanup = async function() {
    await commands.destroy();
    if (win) {
      win.close();
    }
  };

  gAttachCleanups.push(cleanup);
  return {
    commands,
    target: commands.targetCommand.targetFront,
    doc: win.document,
  };
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
  if (!_tests.length) {
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

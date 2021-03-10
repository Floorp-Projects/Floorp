/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

/* import-globals-from ../../inspector/test/shared-head.js */

"use strict";

// This shared-head.js file is used for multiple mochitest test directories in
// devtools.
// It contains various common helper functions.

const { Constructor: CC } = Components;

// Print allocation count if DEBUG_DEVTOOLS_ALLOCATIONS is set to "normal",
// and allocation sites if DEBUG_DEVTOOLS_ALLOCATIONS is set to "verbose".
const env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);
const DEBUG_ALLOCATIONS = env.get("DEBUG_DEVTOOLS_ALLOCATIONS");
if (DEBUG_ALLOCATIONS) {
  // Use a custom loader with `invisibleToDebugger` flag for the allocation tracker
  // as it instantiates custom Debugger API instances and has to be running in a distinct
  // compartments from DevTools and system scopes (JSMs, XPCOM,...)
  const { DevToolsLoader } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );
  const loader = new DevToolsLoader({
    invisibleToDebugger: true,
  });

  const { allocationTracker } = loader.require(
    "devtools/shared/test-helpers/allocation-tracker"
  );
  const tracker = allocationTracker({ watchAllGlobals: true });
  registerCleanupFunction(() => {
    if (DEBUG_ALLOCATIONS == "normal") {
      tracker.logCount();
    } else if (DEBUG_ALLOCATIONS == "verbose") {
      tracker.logAllocationSites();
    }
    tracker.stop();
  });
}

const { loader, require } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);

const { gDevTools } = require("devtools/client/framework/devtools");
const {
  TabTargetFactory,
} = require("devtools/client/framework/tab-target-factory");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

// This is overridden in files that load shared-head via loadSubScript.
// eslint-disable-next-line prefer-const
let promise = require("promise");
const defer = require("devtools/shared/defer");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");

const TEST_DIR = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
const CHROME_URL_ROOT = TEST_DIR + "/";
const URL_ROOT = CHROME_URL_ROOT.replace(
  "chrome://mochitests/content/",
  "http://example.com/"
);
const URL_ROOT_SSL = CHROME_URL_ROOT.replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);

// Add aliases which make it more explicit that URL_ROOT uses a com TLD.
const URL_ROOT_COM = URL_ROOT;
const URL_ROOT_COM_SSL = URL_ROOT_SSL;

// Also expose http://example.org, http://example.net, https://example.org to
// test Fission scenarios easily.
// Note: example.net is not available for https.
const URL_ROOT_ORG = CHROME_URL_ROOT.replace(
  "chrome://mochitests/content/",
  "http://example.org/"
);
const URL_ROOT_ORG_SSL = CHROME_URL_ROOT.replace(
  "chrome://mochitests/content/",
  "https://example.org/"
);
const URL_ROOT_NET = CHROME_URL_ROOT.replace(
  "chrome://mochitests/content/",
  "http://example.net/"
);
// mochi.test:8888 is the actual primary location where files are served.
const URL_ROOT_MOCHI_8888 = CHROME_URL_ROOT.replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

try {
  Services.scriptloader.loadSubScript(
    "chrome://mochitests/content/browser/devtools/client/shared/test/telemetry-test-helpers.js",
    this
  );
} catch (e) {
  ok(
    false,
    "MISSING DEPENDENCY ON telemetry-test-helpers.js\n" +
      "Please add the following line in browser.ini:\n" +
      "  !/devtools/client/shared/test/telemetry-test-helpers.js\n"
  );
  throw e;
}

// Force devtools to be initialized so menu items and keyboard shortcuts get installed
require("devtools/client/framework/devtools-browser");

/**
 * Observer code to register the test actor in every DevTools server which
 * starts registering its own actors.
 *
 * We require immediately the test actor file, because it will force to load and
 * register the front and the spec for TestActor. Normally specs and fronts are
 * in separate files registered in specs/index.js. But here to simplify the
 * setup everything is in the same file and we force to load it here.
 *
 * DevToolsServer will emit "devtools-server-initialized" after finishing its
 * initialization. We watch this observable to add our custom actor.
 *
 * As a single test may create several DevTools servers, we keep the observer
 * alive until the test ends.
 *
 * To avoid leaks, the observer needs to be removed at the end of each test.
 * The test cleanup will send the async message "remove-devtools-testactor-observer",
 * we listen to this message to cleanup the observer.
 */
function testActorBootstrap() {
  const TEST_ACTOR_URL =
    "chrome://mochitests/content/browser/devtools/client/shared/test/test-actor.js";

  const { require: _require } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );
  _require(TEST_ACTOR_URL);

  const Services = _require("Services");

  const actorRegistryObserver = subject => {
    const actorRegistry = subject.wrappedJSObject;
    actorRegistry.registerModule(TEST_ACTOR_URL, {
      prefix: "test",
      constructor: "TestActor",
      type: { target: true },
    });
  };
  Services.obs.addObserver(
    actorRegistryObserver,
    "devtools-server-initialized"
  );

  const unloadListener = () => {
    Services.cpmm.removeMessageListener(
      "remove-devtools-testactor-observer",
      unloadListener
    );
    Services.obs.removeObserver(
      actorRegistryObserver,
      "devtools-server-initialized"
    );
  };
  Services.cpmm.addMessageListener(
    "remove-devtools-testactor-observer",
    unloadListener
  );
}

const testActorBootstrapScript = "data:,(" + testActorBootstrap + ")()";
Services.ppmm.loadProcessScript(
  testActorBootstrapScript,
  // Load this script in all processes (created or to be created)
  true
);

registerCleanupFunction(() => {
  Services.ppmm.broadcastAsyncMessage("remove-devtools-testactor-observer");
  Services.ppmm.removeDelayedProcessScript(testActorBootstrapScript);
});

// Spawn an instance of the test actor for the given toolbox
async function getTestActor(toolbox) {
  // Loading the Inspector panel in order to overwrite the TestActor getter for the
  // highlighter instance with a method that points to the currently visible
  // Box Model Highlighter managed by the Inspector panel.
  const inspector = await toolbox.loadTool("inspector");
  const testActor = await toolbox.target.getFront("test");
  // Override the highligher getter with a method to return the active box model
  // highlighter. Adaptation for multi-process scenarios where there can be multiple
  // highlighters, one per process.
  testActor.highlighter = () => {
    return inspector.highlighters.getActiveHighlighter("BoxModelHighlighter");
  };
  return testActor;
}

// Sometimes, we need the test actor before opening or without a toolbox then just
// create a front for the given `tab`
async function getTestActorWithoutToolbox(tab) {
  const { DevToolsServer } = require("devtools/server/devtools-server");
  const { DevToolsClient } = require("devtools/client/devtools-client");

  // We need to spawn a client instance,
  // but for that we have to first ensure a server is running
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  const client = new DevToolsClient(DevToolsServer.connectPipe());
  await client.connect();

  const descriptor = await client.mainRoot.getTab({ tab });
  const targetFront = await descriptor.getTarget();
  return targetFront.getFront("test");
}

// All test are asynchronous
waitForExplicitFinish();

var EXPECTED_DTU_ASSERT_FAILURE_COUNT = 0;

registerCleanupFunction(function() {
  if (
    DevToolsUtils.assertionFailureCount !== EXPECTED_DTU_ASSERT_FAILURE_COUNT
  ) {
    ok(
      false,
      "Should have had the expected number of DevToolsUtils.assert() failures." +
        " Expected " +
        EXPECTED_DTU_ASSERT_FAILURE_COUNT +
        ", got " +
        DevToolsUtils.assertionFailureCount
    );
  }
});

// Uncomment this pref to dump all devtools emitted events to the console.
// Services.prefs.setBoolPref("devtools.dump.emit", true);

/**
 * Watch console messages for failed propType definitions in React components.
 */
const ConsoleObserver = {
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  observe: function(subject) {
    const message = subject.wrappedJSObject.arguments[0];

    if (message && /Failed propType/.test(message.toString())) {
      ok(false, message);
    }
  },
};

Services.obs.addObserver(ConsoleObserver, "console-api-log-event");
registerCleanupFunction(() => {
  Services.obs.removeObserver(ConsoleObserver, "console-api-log-event");
});

Services.prefs.setBoolPref("devtools.inspector.three-pane-enabled", true);

// Disable this preference to reduce exceptions related to pending `listWorkers`
// requests occuring after a process is created/destroyed. See Bug 1620983.
Services.prefs.setBoolPref("dom.ipc.processPrelaunch.enabled", false);

// Disable this preference to capture async stacks across all locations during
// DevTools mochitests. Async stacks provide very valuable information to debug
// intermittents, but come with a performance overhead, which is why they are
// only captured in Debuggees by default.
Services.prefs.setBoolPref(
  "javascript.options.asyncstack_capture_debuggee_only",
  false
);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.dump.emit");
  Services.prefs.clearUserPref("devtools.inspector.three-pane-enabled");
  Services.prefs.clearUserPref("dom.ipc.processPrelaunch.enabled");
  Services.prefs.clearUserPref("devtools.toolbox.host");
  Services.prefs.clearUserPref("devtools.toolbox.previousHost");
  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleHeight");
  Services.prefs.clearUserPref(
    "javascript.options.asyncstack_capture_debuggee_only"
  );
});

var {
  BrowserConsoleManager,
} = require("devtools/client/webconsole/browser-console-manager");

registerCleanupFunction(async function cleanup() {
  // Closing the browser console if there's one
  const browserConsole = BrowserConsoleManager.getBrowserConsole();
  if (browserConsole) {
    await safeCloseBrowserConsole({ clearOutput: true });
  }

  // Close any tab opened by the test.
  // There should be only one tab opened by default when firefox starts the test.
  while (gBrowser.tabs.length > 1) {
    await closeTabAndToolbox(gBrowser.selectedTab);
  }

  // Note that this will run before cleanup functions registered by tests or other head.js files.
  // So all connections must be cleaned up by the test when the test ends,
  // before the harness starts invoking the cleanup functions
  await waitForTick();

  // All connections must be cleaned up by the test when the test ends.
  const { DevToolsServer } = require("devtools/server/devtools-server");
  ok(
    !DevToolsServer.hasConnection(),
    "The main process DevToolsServer has no pending connection when the test ends"
  );
  // If there is still open connection, close all of them so that following tests
  // could pass.
  if (DevToolsServer.hasConnection()) {
    for (const conn of Object.values(DevToolsServer._connections)) {
      conn.close();
    }
  }
});

async function safeCloseBrowserConsole({ clearOutput = false } = {}) {
  const hud = BrowserConsoleManager.getBrowserConsole();
  if (!hud) {
    return;
  }

  if (clearOutput) {
    info("Clear the browser console output");
    const { ui } = hud;
    const promises = [ui.once("messages-cleared")];
    // If there's an object inspector, we need to wait for the actors to be released.
    if (ui.outputNode.querySelector(".object-inspector")) {
      promises.push(ui.once("fronts-released"));
    }
    ui.clearOutput(true);
    await Promise.all(promises);
    info("Browser console cleared");
  }

  info("Wait for all Browser Console targets to be attached");
  // It might happen that waitForAllTargetsToBeAttached does not resolve, so we set a
  // timeout of 1s before closing
  await Promise.race([
    waitForAllTargetsToBeAttached(hud.targetList),
    wait(1000),
  ]);

  hud.targetList.destroy();

  info("Close the Browser Console");
  await BrowserConsoleManager.closeBrowserConsole();
  info("Browser Console closed");
}

/**
 * Returns a Promise that resolves when all the targets are fully attached.
 *
 * @param {TargetList} targetList
 */
function waitForAllTargetsToBeAttached(targetList) {
  return Promise.allSettled(
    targetList
      .getAllTargets(targetList.ALL_TYPES)
      .map(target => target._onThreadInitialized)
  );
}

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @param {Object} options Object with various optional fields:
 *   - {Boolean} background If true, open the tab in background
 *   - {ChromeWindow} window Firefox top level window we should use to open the tab
 *   - {Number} userContextId The userContextId of the tab.
 *   - {String} preferredRemoteType
 *   - {Boolean} waitForLoad Wait for the page in the new tab to load. (Defaults to true.)
 * @return a promise that resolves to the tab object when the url is loaded
 */
var addTab = async function(url, options = {}) {
  info("Adding a new tab with URL: " + url);

  const {
    background = false,
    userContextId,
    preferredRemoteType,
    waitForLoad = true,
  } = options;
  const { gBrowser } = options.window ? options.window : window;

  const tab = BrowserTestUtils.addTab(gBrowser, url, {
    userContextId,
    preferredRemoteType,
  });

  if (!background) {
    gBrowser.selectedTab = tab;
  }

  if (waitForLoad) {
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    info("Tab added and finished loading");
  } else {
    info("Tab added");
  }

  return tab;
};

/**
 * Remove the given tab.
 * @param {Object} tab The tab to be removed.
 * @return Promise<undefined> resolved when the tab is successfully removed.
 */
var removeTab = async function(tab) {
  info("Removing tab.");

  const { gBrowser } = tab.ownerDocument.defaultView;
  const onClose = once(gBrowser.tabContainer, "TabClose");
  gBrowser.removeTab(tab);
  await onClose;

  info("Tab removed and finished closing");
};

/**
 * Refresh the provided tab.
 * @param {Object} tab The tab to be refreshed. Defaults to the currently selected tab.
 * @return Promise<undefined> resolved when the tab is successfully refreshed.
 */
var refreshTab = async function(tab = gBrowser.selectedTab) {
  info("Refreshing tab.");
  const finished = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.reloadTab(tab);
  await finished;
  info("Tab finished refreshing.");
};

/**
 * Navigate the currently selected tab to a new URL and wait for it to load.
 * Also wait for the toolbox to attach to the new target, if we navigated
 * to a new process.
 *
 * @param {String} url The url to be loaded in the current tab.
 * @param {JSON} options Optional dictionary object with the following keys:
 *        - {Boolean} isErrorPage You may pass `true` is the URL is an error
 *                    page. Otherwise BrowserTestUtils.browserLoaded will wait
 *                    for 'load' event, which never fires for error pages.
 *
 * @return a promise that resolves when the page has fully loaded.
 */
async function navigateTo(uri, { isErrorPage = false } = {}) {
  const toolbox = await gDevTools.getToolboxForTab(gBrowser.selectedTab);
  const target = toolbox.target;

  // If we're switching origins, we need to wait for the 'switched-target'
  // event to make sure everything is ready.
  // Navigating from/to pages loaded in the parent process, like about:robots,
  // also spawn new targets.
  // (If target switching is disabled, the toolbox will reboot)
  const onTargetSwitched = toolbox.targetList.once("switched-target");
  // Otherwise, if we don't switch target, it is safe to wait for navigate event.
  const onNavigate = target.once("navigate");

  // Register panel-specific listeners, which would be useful to wait
  // for panel-specific events.
  const onPanelReloaded = waitForPanelReload(
    toolbox.currentToolId,
    toolbox.target,
    toolbox.getCurrentPanel()
  );

  info(`Load document "${uri}"`);
  const browser = gBrowser.selectedBrowser;
  const currentPID = browser.browsingContext.currentWindowGlobal.osPid;
  const onBrowserLoaded = BrowserTestUtils.browserLoaded(
    browser,
    false,
    null,
    isErrorPage
  );
  BrowserTestUtils.loadURI(browser, uri);

  info(`Waiting for page to be loaded…`);
  await onBrowserLoaded;
  info(`→ page loaded`);

  // Compare the PIDs (and not the toolbox's targets) as PIDs are updated also immediately,
  // while target may be updated slightly later.
  const switchedToAnotherProcess =
    currentPID !== browser.browsingContext.currentWindowGlobal.osPid;

  if (onPanelReloaded) {
    info(`Waiting for ${toolbox.currentToolId} to be reloaded…`);
    await onPanelReloaded();
    info(`→ panel reloaded`);
  }

  // If the tab navigated to another process, expect a target switching
  if (switchedToAnotherProcess) {
    info(`Waiting for target switch…`);
    await onTargetSwitched;
    info(`→ switched-target emitted`);
  } else {
    info(`Waiting for target 'navigate' event…`);
    await onNavigate;
    info(`→ 'navigate' emitted`);
  }
}

/**
 * Create a Target for the provided tab and attach to it before resolving.
 * This should only be used for tests which don't involve the frontend or a
 * toolbox. Typically, retrieving the target and attaching to it should be
 * handled at framework level when a Toolbox is used.
 *
 * @param {XULTab} tab
 *        The tab for which a target should be created.
 * @return {BrowsingContextTargetFront} The attached target front.
 */
async function createAndAttachTargetForTab(tab) {
  info("Creating and attaching to a local tab target");
  const target = await TabTargetFactory.forTab(tab);
  await target.attach();
  return target;
}

/**
 * Return a function, specific for each panel, in order
 * to wait for any update which may happen when reloading a page.
 */
function waitForPanelReload(currentToolId, target, panel) {
  if (currentToolId == "inspector") {
    const inspector = panel;
    const markuploaded = inspector.once("markuploaded");
    const onNewRoot = inspector.once("new-root");
    const onUpdated = inspector.once("inspector-updated");
    const onReloaded = inspector.once("reloaded");

    return async function() {
      info("Waiting for markup view to load after navigation.");
      await markuploaded;

      info("Waiting for new root.");
      await onNewRoot;

      info("Waiting for inspector to update after new-root event.");
      await onUpdated;

      info("Waiting for inspector updates after page reload");
      await onReloaded;
    };
  } else if (currentToolId == "netmonitor") {
    const monitor = panel;
    const onReloaded = monitor.once("reloaded");
    return async function() {
      info("Waiting for netmonitor updates after page reload");
      await onReloaded;
    };
  }
  return null;
}

function isFissionEnabled() {
  return SpecialPowers.useRemoteSubframes;
}

/**
 * Open the inspector in a tab with given URL.
 * @param {string} url  The URL to open.
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return A promise that is resolved once the tab and inspector have loaded
 *         with an object: { tab, toolbox, inspector }.
 */
var openInspectorForURL = async function(url, hostType) {
  const tab = await addTab(url);
  const { inspector, toolbox, testActor } = await openInspector(hostType);
  return { tab, inspector, toolbox, testActor };
};

async function getActiveInspector() {
  const toolbox = await gDevTools.getToolboxForTab(gBrowser.selectedTab);
  return toolbox.getPanel("inspector");
}

/**
 * Simulate a key event from a <key> element.
 * @param {DOMNode} key
 */
function synthesizeKeyFromKeyTag(key) {
  is(key && key.tagName, "key", "Successfully retrieved the <key> node");

  const modifiersAttr = key.getAttribute("modifiers");

  let name = null;

  if (key.getAttribute("keycode")) {
    name = key.getAttribute("keycode");
  } else if (key.getAttribute("key")) {
    name = key.getAttribute("key");
  }

  isnot(name, null, "Successfully retrieved keycode/key");

  const modifiers = {
    shiftKey: !!modifiersAttr.match("shift"),
    ctrlKey: !!modifiersAttr.match("control"),
    altKey: !!modifiersAttr.match("alt"),
    metaKey: !!modifiersAttr.match("meta"),
    accelKey: !!modifiersAttr.match("accel"),
  };

  info("Synthesizing key " + name + " " + JSON.stringify(modifiers));
  EventUtils.synthesizeKey(name, modifiers);
}

/**
 * Simulate a key event from an electron key shortcut string:
 * https://github.com/electron/electron/blob/master/docs/api/accelerator.md
 *
 * @param {String} key
 * @param {DOMWindow} target
 *        Optional window where to fire the key event
 */
function synthesizeKeyShortcut(key, target) {
  // parseElectronKey requires any window, just to access `KeyboardEvent`
  const window = Services.appShell.hiddenDOMWindow;
  const shortcut = KeyShortcuts.parseElectronKey(window, key);
  const keyEvent = {
    altKey: shortcut.alt,
    ctrlKey: shortcut.ctrl,
    metaKey: shortcut.meta,
    shiftKey: shortcut.shift,
  };
  if (shortcut.keyCode) {
    keyEvent.keyCode = shortcut.keyCode;
  }

  info("Synthesizing key shortcut: " + key);
  EventUtils.synthesizeKey(shortcut.key || "", keyEvent, target);
}

var waitForTime = DevToolsUtils.waitForTime;

/**
 * Wait for a tick.
 * @return {Promise}
 */
function waitForTick() {
  return new Promise(resolve => DevToolsUtils.executeSoon(resolve));
}

/**
 * This shouldn't be used in the tests, but is useful when writing new tests or
 * debugging existing tests in order to introduce delays in the test steps
 *
 * @param {Number} ms
 *        The time to wait
 * @return A promise that resolves when the time is passed
 */
function wait(ms) {
  return new Promise(resolve => {
    setTimeout(resolve, ms);
    info("Waiting " + ms / 1000 + " seconds.");
  });
}

/**
 * Wait for a predicate to return a result.
 *
 * @param function condition
 *        Invoked once in a while until it returns a truthy value. This should be an
 *        idempotent function, since we have to run it a second time after it returns
 *        true in order to return the value.
 * @param string message [optional]
 *        A message to output if the condition fails.
 * @param number interval [optional]
 *        How often the predicate is invoked, in milliseconds.
 * @return object
 *         A promise that is resolved with the result of the condition.
 */
async function waitFor(condition, message = "", interval = 10, maxTries = 500) {
  try {
    const value = await BrowserTestUtils.waitForCondition(
      condition,
      message,
      interval,
      maxTries
    );
    return value;
  } catch (e) {
    const errorMessage =
      "Failed waitFor(): " +
      message +
      "\n" +
      "Failed condition: " +
      condition +
      "\n";
    throw new Error(errorMessage);
  }
}

/**
 * Wait for eventName on target to be delivered a number of times.
 *
 * @param {Object} target
 *        An observable object that either supports on/off or
 *        addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Number} numTimes
 *        Number of deliveries to wait for.
 * @param {Boolean} useCapture
 *        Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function waitForNEvents(target, eventName, numTimes, useCapture = false) {
  info("Waiting for event: '" + eventName + "' on " + target + ".");

  let count = 0;

  return new Promise(resolve => {
    for (const [add, remove] of [
      ["on", "off"],
      ["addEventListener", "removeEventListener"],
      ["addListener", "removeListener"],
      ["addMessageListener", "removeMessageListener"],
    ]) {
      if (add in target && remove in target) {
        target[add](
          eventName,
          function onEvent(...args) {
            if (typeof info === "function") {
              info("Got event: '" + eventName + "' on " + target + ".");
            }

            if (++count == numTimes) {
              target[remove](eventName, onEvent, useCapture);
              resolve(...args);
            }
          },
          useCapture
        );
        break;
      }
    }
  });
}

/**
 * Wait for DOM change on target.
 *
 * @param {Object} target
 *        The Node on which to observe DOM mutations.
 * @param {String} selector
 *        Given a selector to watch whether the expected element is changed
 *        on target.
 * @param {Number} expectedLength
 *        Optional, default set to 1
 *        There may be more than one element match an array match the selector,
 *        give an expected length to wait for more elements.
 * @return A promise that resolves when the event has been handled
 */
function waitForDOM(target, selector, expectedLength = 1) {
  return new Promise(resolve => {
    const observer = new MutationObserver(mutations => {
      mutations.forEach(mutation => {
        const elements = mutation.target.querySelectorAll(selector);

        if (elements.length === expectedLength) {
          observer.disconnect();
          resolve(elements);
        }
      });
    });

    observer.observe(target, {
      attributes: true,
      childList: true,
      subtree: true,
    });
  });
}

/**
 * Wait for eventName on target.
 *
 * @param {Object} target
 *        An observable object that either supports on/off or
 *        addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} useCapture
 *        Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function once(target, eventName, useCapture = false) {
  return waitForNEvents(target, eventName, 1, useCapture);
}

/**
 * Some tests may need to import one or more of the test helper scripts.
 * A test helper script is simply a js file that contains common test code that
 * is either not common-enough to be in head.js, or that is located in a
 * separate directory.
 * The script will be loaded synchronously and in the test's scope.
 * @param {String} filePath The file path, relative to the current directory.
 *                 Examples:
 *                 - "helper_attributes_test_runner.js"
 */
function loadHelperScript(filePath) {
  const testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  Services.scriptloader.loadSubScript(testDir + "/" + filePath, this);
}

/**
 * Open the toolbox in a given tab.
 * @param {XULNode} tab The tab the toolbox should be opened in.
 * @param {String} toolId Optional. The ID of the tool to be selected.
 * @param {String} hostType Optional. The type of toolbox host to be used.
 * @return {Promise} Resolves with the toolbox, when it has been opened.
 */
var openToolboxForTab = async function(tab, toolId, hostType) {
  info("Opening the toolbox");

  let toolbox;

  // Check if the toolbox is already loaded.
  toolbox = await gDevTools.getToolboxForTab(tab);
  if (toolbox) {
    if (!toolId || (toolId && toolbox.getPanel(toolId))) {
      info("Toolbox is already opened");
      return toolbox;
    }
  }

  // If not, load it now.
  toolbox = await gDevTools.showToolboxForTab(tab, { toolId, hostType });

  // Make sure that the toolbox frame is focused.
  await new Promise(resolve => waitForFocus(resolve, toolbox.win));

  info("Toolbox opened and focused");

  return toolbox;
};

/**
 * Add a new tab and open the toolbox in it.
 * @param {String} url The URL for the tab to be opened.
 * @param {String} toolId Optional. The ID of the tool to be selected.
 * @param {String} hostType Optional. The type of toolbox host to be used.
 * @return {Promise} Resolves when the tab has been added, loaded and the
 * toolbox has been opened. Resolves to the toolbox.
 */
var openNewTabAndToolbox = async function(url, toolId, hostType) {
  const tab = await addTab(url);
  return openToolboxForTab(tab, toolId, hostType);
};

/**
 * Close a tab and if necessary, the toolbox that belongs to it
 * @param {Tab} tab The tab to close.
 * @return {Promise} Resolves when the toolbox and tab have been destroyed and
 * closed.
 */
var closeTabAndToolbox = async function(tab = gBrowser.selectedTab) {
  if (TabTargetFactory.isKnownTab(tab)) {
    await gDevTools.closeToolboxForTab(tab);
  }

  await removeTab(tab);

  await new Promise(resolve => setTimeout(resolve, 0));
};

/**
 * Close a toolbox and the current tab.
 * @param {Toolbox} toolbox The toolbox to close.
 * @return {Promise} Resolves when the toolbox and tab have been destroyed and
 * closed.
 */
var closeToolboxAndTab = async function(toolbox) {
  await toolbox.destroy();
  await removeTab(gBrowser.selectedTab);
};

/**
 * Retrieve all tool ids compatible with a target created for the provided tab.
 *
 * @param {XULTab} tab
 *        The tab for which we want to get the list of supported toolIds
 * @return {Array<String>} array of tool ids
 */
async function getSupportedToolIds(tab) {
  info("Getting the entire list of tools supported in this tab");

  let shouldDestroyToolbox = false;

  // Get the toolbox for this tab, or create one if needed.
  let toolbox = await gDevTools.getToolboxForTab(tab);
  if (!toolbox) {
    toolbox = await gDevTools.showToolboxForTab(tab);
    shouldDestroyToolbox = true;
  }

  const toolIds = gDevTools
    .getToolDefinitionArray()
    .filter(def => def.isTargetSupported(toolbox.target))
    .map(def => def.id);

  if (shouldDestroyToolbox) {
    // Only close the toolbox if it was explicitly created here.
    await toolbox.destroy();
  }

  return toolIds;
}

/**
 * Waits until a predicate returns true.
 *
 * @param function predicate
 *        Invoked once in a while until it returns true.
 * @param number interval [optional]
 *        How often the predicate is invoked, in milliseconds.
 */
function waitUntil(predicate, interval = 10) {
  if (predicate()) {
    return Promise.resolve(true);
  }
  return new Promise(resolve => {
    setTimeout(function() {
      waitUntil(predicate, interval).then(() => resolve(true));
    }, interval);
  });
}

/**
 * Variant of waitUntil that accepts a predicate returning a promise.
 */
async function asyncWaitUntil(predicate, interval = 10) {
  let success = await predicate();
  while (!success) {
    // Wait for X milliseconds.
    await new Promise(resolve => setTimeout(resolve, interval));
    // Test the predicate again.
    success = await predicate();
  }
}

/**
 * Wait for a context menu popup to open.
 *
 * @param Element popup
 *        The XUL popup you expect to open.
 * @param Element button
 *        The button/element that receives the contextmenu event. This is
 *        expected to open the popup.
 * @param function onShown
 *        Function to invoke on popupshown event.
 * @param function onHidden
 *        Function to invoke on popuphidden event.
 * @return object
 *         A Promise object that is resolved after the popuphidden event
 *         callback is invoked.
 */
function waitForContextMenu(popup, button, onShown, onHidden) {
  return new Promise(resolve => {
    function onPopupShown() {
      info("onPopupShown");
      popup.removeEventListener("popupshown", onPopupShown);

      onShown && onShown();

      // Use executeSoon() to get out of the popupshown event.
      popup.addEventListener("popuphidden", onPopupHidden);
      DevToolsUtils.executeSoon(() => popup.hidePopup());
    }
    function onPopupHidden() {
      info("onPopupHidden");
      popup.removeEventListener("popuphidden", onPopupHidden);

      onHidden && onHidden();

      resolve(popup);
    }

    popup.addEventListener("popupshown", onPopupShown);

    info("wait for the context menu to open");
    synthesizeContextMenuEvent(button);
  });
}

function synthesizeContextMenuEvent(el) {
  el.scrollIntoView();
  const eventDetails = { type: "contextmenu", button: 2 };
  EventUtils.synthesizeMouse(
    el,
    5,
    2,
    eventDetails,
    el.ownerDocument.defaultView
  );
}

/**
 * Promise wrapper around SimpleTest.waitForClipboard
 */
function waitForClipboardPromise(setup, expected) {
  return new Promise((resolve, reject) => {
    SimpleTest.waitForClipboard(expected, setup, resolve, reject);
  });
}

/**
 * Simple helper to push a temporary preference. Wrapper on SpecialPowers
 * pushPrefEnv that returns a promise resolving when the preferences have been
 * updated.
 *
 * @param {String} preferenceName
 *        The name of the preference to updated
 * @param {} value
 *        The preference value, type can vary
 * @return {Promise} resolves when the preferences have been updated
 */
function pushPref(preferenceName, value) {
  const options = { set: [[preferenceName, value]] };
  return SpecialPowers.pushPrefEnv(options);
}

/**
 * Lookup the provided dotted path ("prop1.subprop2.myProp") in the provided object.
 *
 * @param {Object} obj
 *        Object to expand.
 * @param {String} path
 *        Dotted path to use to expand the object.
 * @return {?} anything that is found at the provided path in the object.
 */
function lookupPath(obj, path) {
  const segments = path.split(".");
  return segments.reduce((prev, current) => prev[current], obj);
}

var closeToolbox = async function() {
  await gDevTools.closeToolboxForTab(gBrowser.selectedTab);
};

/**
 * Clean the logical clipboard content. This method only clears the OS clipboard on
 * Windows (see Bug 666254).
 */
function emptyClipboard() {
  const clipboard = Services.clipboard;
  clipboard.emptyClipboard(clipboard.kGlobalClipboard);
}

/**
 * Check if the current operating system is Windows.
 */
function isWindows() {
  return Services.appinfo.OS === "WINNT";
}

/**
 * Wait for a given toolbox to get its title updated.
 */
function waitForTitleChange(toolbox) {
  return new Promise(resolve => {
    toolbox.topWindow.addEventListener("message", function onmessage(event) {
      if (event.data.name == "set-host-title") {
        toolbox.topWindow.removeEventListener("message", onmessage);
        resolve();
      }
    });
  });
}

/**
 * Create an HTTP server that can be used to simulate custom requests within
 * a test.  It is automatically cleaned up when the test ends, so no need to
 * call `destroy`.
 *
 * See https://developer.mozilla.org/en-US/docs/Httpd.js/HTTP_server_for_unit_tests
 * for more information about how to register handlers.
 *
 * The server can be accessed like:
 *
 *   const server = createTestHTTPServer();
 *   let url = "http://localhost: " + server.identity.primaryPort + "/path";
 *
 * @returns {HttpServer}
 */
function createTestHTTPServer() {
  const { HttpServer } = ChromeUtils.import(
    "resource://testing-common/httpd.js"
  );
  const server = new HttpServer();

  registerCleanupFunction(async function cleanup() {
    await new Promise(resolve => server.stop(resolve));
  });

  server.start(-1);
  return server;
}

/*
 * Register an actor in the content process of the current tab.
 *
 * Calling ActorRegistry.registerModule only registers the actor in the current process.
 * As all test scripts are ran in the parent process, it is only registered here.
 * This function helps register them in the content process used for the current tab.
 *
 * @param {string} url
 *        Actor module URL or absolute require path
 * @param {json} options
 *        Arguments to be passed to DevToolsServer.registerModule
 */
async function registerActorInContentProcess(url, options) {
  function convertChromeToFile(uri) {
    return Cc["@mozilla.org/chrome/chrome-registry;1"]
      .getService(Ci.nsIChromeRegistry)
      .convertChromeURL(Services.io.newURI(uri)).spec;
  }
  // chrome://mochitests URI is registered only in the parent process, so convert these
  // URLs to file:// one in order to work in the content processes
  url = url.startsWith("chrome://mochitests") ? convertChromeToFile(url) : url;
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [{ url, options }],
    args => {
      // eslint-disable-next-line no-shadow
      const { require } = ChromeUtils.import(
        "resource://devtools/shared/Loader.jsm"
      );
      const {
        ActorRegistry,
      } = require("devtools/server/actors/utils/actor-registry");
      ActorRegistry.registerModule(args.url, args.options);
    }
  );
}

/**
 * Move the provided Window to the provided left, top coordinates and wait for
 * the window position to be updated.
 */
async function moveWindowTo(win, left, top) {
  // Check that the expected coordinates are within the window available area.
  left = Math.max(win.screen.availLeft, left);
  left = Math.min(win.screen.width, left);
  top = Math.max(win.screen.availTop, top);
  top = Math.min(win.screen.height, top);

  info(`Moving window to {${left}, ${top}}`);
  win.moveTo(left, top);

  // Bug 1600809: window move/resize can be async on Linux sometimes.
  // Wait so that the anchor's position is correctly measured.
  info("Wait for window screenLeft and screenTop to be updated");
  return waitUntil(() => win.screenLeft === left && win.screenTop === top);
}

function getCurrentTestFilePath() {
  return gTestPath.replace("chrome://mochitests/content/browser/", "");
}

/**
 * Wait for a single resource of the provided resourceType.
 *
 * @param {ResourceWatcher} resourceWatcher
 *        The ResourceWatcher instance that should emit the expected resource.
 * @param {String} resourceType
 *        One of ResourceWatcher.TYPES, type of the expected resource.
 * @param {Object} additional options
 *        - {Boolean} ignoreExistingResources: ignore existing resources or not.
 *        - {Function} predicate: if provided, will wait until a resource makes
 *          predicate(resource) return true.
 * @return {Object}
 *         - resource {Object} the resource itself
 *         - targetFront {TargetFront} the target which owns the resource
 */
function waitForNextResource(
  resourceWatcher,
  resourceType,
  { ignoreExistingResources = false, predicate } = {}
) {
  // If no predicate was provided, convert to boolean to avoid resolving for
  // empty `resources` arrays.
  predicate = predicate || (resource => !!resource);

  return new Promise(resolve => {
    const onAvailable = resources => {
      const matchingResource = resources.find(resource => predicate(resource));
      if (matchingResource) {
        resolve(matchingResource);
        resourceWatcher.unwatchResources([resourceType], { onAvailable });
      }
    };

    resourceWatcher.watchResources([resourceType], {
      ignoreExistingResources,
      onAvailable,
    });
  });
}

/**
 * Unregister all registered service workers.
 *
 * @param {DevToolsClient} client
 */
async function unregisterAllServiceWorkers(client) {
  info("Wait until all workers have a valid registrationFront");
  let workers;
  await asyncWaitUntil(async function() {
    workers = await client.mainRoot.listAllWorkers();
    const allWorkersRegistered = workers.service.every(
      worker => !!worker.registrationFront
    );
    return allWorkersRegistered;
  });

  info("Unregister all service workers");
  const promises = [];
  for (const worker of workers.service) {
    promises.push(worker.registrationFront.unregister());
  }
  await Promise.all(promises);
}

/**********************
 * Screenshot helpers *
 **********************/

/**
 * Returns an object containing the r,g and b colors of the provided image at
 * the passed position
 *
 * @param {Image} image
 * @param {Int} x
 * @param {Int} y
 * @returns Object with the following properties:
 *           - {Int} r: The red component of the pixel
 *           - {Int} g: The green component of the pixel
 *           - {Int} b: The blue component of the pixel
 */

function colorAt(image, x, y) {
  // Create a test canvas element.
  const HTML_NS = "http://www.w3.org/1999/xhtml";
  const canvas = document.createElementNS(HTML_NS, "canvas");
  canvas.width = image.width;
  canvas.height = image.height;

  // Draw the image in the canvas
  const context = canvas.getContext("2d");
  context.drawImage(image, 0, 0, image.width, image.height);

  // Return the color found at the provided x,y coordinates as a "r, g, b" string.
  const [r, g, b] = context.getImageData(x, y, 1, 1).data;
  return { r, g, b };
}

let allDownloads = [];
/**
 * Returns a Promise that resolves when a new screenshot is available in the download folder.
 */
async function waitUntilScreenshot() {
  const { Downloads } = require("resource://gre/modules/Downloads.jsm");
  const list = await Downloads.getList(Downloads.ALL);

  return new Promise(function(resolve) {
    const view = {
      onDownloadAdded: async download => {
        await download.whenSucceeded();
        if (allDownloads.includes(download)) {
          return;
        }

        allDownloads.push(download);
        resolve(download.target.path);
        list.removeView(view);
      },
    };

    list.addView(view);
  });
}

/**
 * Clear all the download references.
 */
async function resetDownloads() {
  info("Reset downloads");
  const { Downloads } = require("resource://gre/modules/Downloads.jsm");
  const publicList = await Downloads.getList(Downloads.PUBLIC);
  const downloads = await publicList.getAll();
  for (const download of downloads) {
    publicList.remove(download);
    await download.finalize(true);
  }
  allDownloads = [];
}

/**
 * Return a screenshot of the currently selected node in the inspector (using the internal
 * Inspector#screenshotNode method).
 *
 * @param {Inspector} inspector
 * @returns {Image}
 */
async function takeNodeScreenshot(inspector) {
  // Cleanup all downloads at the end of the test.
  registerCleanupFunction(resetDownloads);

  info(
    "Call screenshotNode() and wait until the screenshot is found in the Downloads"
  );
  const whenScreenshotSucceeded = waitUntilScreenshot();
  inspector.screenshotNode();
  const filePath = await whenScreenshotSucceeded;

  info("Create an image using the downloaded fileas source");
  const image = new Image();
  const onImageLoad = once(image, "load");
  image.src = OS.Path.toFileURI(filePath);
  await onImageLoad;

  info("Remove the downloaded screenshot file");
  await OS.File.remove(filePath);

  // See intermittent Bug 1508435. Even after removing the file, tests still manage to
  // reuse files from the previous test if they have the same name. Since our file name
  // is based on a timestamp that has "second" precision, wait for one second to make sure
  // screenshots will have different names.
  info(
    "Wait for one second to make sure future screenshots will use a different name"
  );
  await new Promise(r => setTimeout(r, 1000));

  return image;
}

/**
 * Check that the provided image has the expected width, height, and color.
 * NOTE: This test assumes that the image is only made of a single color and will only
 * check one pixel.
 */
async function assertSingleColorScreenshotImage(
  image,
  width,
  height,
  { r, g, b }
) {
  info(`Assert ${image.src} content`);
  const ratio = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.wrappedJSObject.devicePixelRatio
  );

  is(
    image.width,
    ratio * width,
    `node screenshot has the expected width (dpr = ${ratio})`
  );
  is(
    image.height,
    height * ratio,
    `node screenshot has the expected height (dpr = ${ratio})`
  );

  const color = colorAt(image, 0, 0);
  is(color.r, r, "node screenshot has the expected red component");
  is(color.g, g, "node screenshot has the expected green component");
  is(color.b, b, "node screenshot has the expected blue component");
}

/**
 * Check that the provided image has the expected color at a given position
 */
function checkImageColorAt({ image, x = 0, y, expectedColor, label }) {
  const color = colorAt(image, x, y);
  is(`rgb(${Object.values(color).join(", ")})`, expectedColor, label);
}

/**
 * Assert that a given parent pool has the expected number of children for
 * a given typeName.
 */
function checkPoolChildrenSize(parentPool, typeName, expected) {
  const children = [...parentPool.poolChildren()];
  const childrenByType = children.filter(pool => pool.typeName === typeName);
  is(
    childrenByType.length,
    expected,
    `${parentPool.actorID} should have ${expected} children of type ${typeName}`
  );
}

/**
 * Wait for a specific action type to be dispatched.
 *
 * If the action is async and defines a `status` property, this helper will wait
 * for the status to reach either "error" or "done".
 *
 * @param {Object} store
 *        Redux store where the action should be dispatched.
 * @param {String} actionType
 *        The actionType to wait for.
 * @param {Number} repeat
 *        Optional, number of time the action is expected to be dispatched.
 *        Defaults to 1
 * @return {Promise}
 */
function waitForDispatch(store, actionType, repeat = 1) {
  let count = 0;
  return new Promise(resolve => {
    store.dispatch({
      type: "@@service/waitUntil",
      predicate: action => {
        const isDone =
          !action.status ||
          action.status === "done" ||
          action.status === "error";

        if (action.type === actionType && isDone && ++count == repeat) {
          return true;
        }

        return false;
      },
      run: (dispatch, getState, action) => {
        resolve(action);
      },
    });
  });
}

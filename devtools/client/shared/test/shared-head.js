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
const { TargetFactory } = require("devtools/client/framework/target");
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
  return toolbox.target.getFront("test");
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

  const targetFront = await client.mainRoot.getTab({ tab });
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
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

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

var waitForTime = DevToolsUtils.waitForTime;

function loadFrameScriptUtils(browser = gBrowser.selectedBrowser) {
  let mm = browser.messageManager;
  const frameURL =
    "chrome://mochitests/content/browser/devtools/client/shared/test/frame-script-utils.js";
  info("Loading the helper frame script " + frameURL);
  mm.loadFrameScript(frameURL, false);
  SimpleTest.registerCleanupFunction(() => {
    mm = null;
  });
  return mm;
}

Services.prefs.setBoolPref("devtools.inspector.three-pane-enabled", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.dump.emit");
  Services.prefs.clearUserPref("devtools.inspector.three-pane-enabled");
  Services.prefs.clearUserPref("devtools.toolbox.host");
  Services.prefs.clearUserPref("devtools.toolbox.previousHost");
  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleHeight");
});

registerCleanupFunction(async function cleanup() {
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
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = gDevTools.getToolbox(target);

  // If we're switching origins, we need to wait for the 'switched-target'
  // event to make sure everything is ready.
  // Navigating from/to pages loaded in the parent process, like about:robots,
  // also spawn new targets.
  // (If target-switching pref is false, the toolbox will reboot)
  const onTargetSwitched = toolbox.once("switched-target");
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
  await BrowserTestUtils.loadURI(browser, uri);

  info(`Waiting for page to be loaded…`);
  await onBrowserLoaded;
  info(`→ page loaded`);

  // Compare the PIDs (and not the toolbox's targets) as PIDs are updated also immediately,
  // while target may be updated slightly later.
  const switchedToAnotherProcess =
    currentPID !== browser.browsingContext.currentWindowGlobal.osPid;

  // If we switched to another process and the target switching pref is false,
  // the toolbox will close and reopen.
  // For now, this helper doesn't support this case
  if (
    switchedToAnotherProcess &&
    !Services.prefs.getBoolPref("devtools.target-switching.enabled", false)
  ) {
    ok(
      false,
      `navigateTo(${uri}) navigated to another process, but the target-switching preference is false`
    );
    return;
  }

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

function isTargetSwitchingEnabled() {
  return (
    isFissionEnabled() &&
    Services.prefs.getBoolPref("devtools.target-switching.enabled", false)
  );
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
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  return gDevTools.getToolbox(target).getPanel("inspector");
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
 * Open the toolbox in a given tab.
 * @param {XULNode} tab The tab the toolbox should be opened in.
 * @param {String} toolId Optional. The ID of the tool to be selected.
 * @param {String} hostType Optional. The type of toolbox host to be used.
 * @return {Promise} Resolves with the toolbox, when it has been opened.
 */
var openToolboxForTab = async function(tab, toolId, hostType) {
  info("Opening the toolbox");

  let toolbox;
  const target = await TargetFactory.forTab(tab);

  // Check if the toolbox is already loaded.
  toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    if (!toolId || (toolId && toolbox.getPanel(toolId))) {
      info("Toolbox is already opened");
      return toolbox;
    }
  }

  // If not, load it now.
  toolbox = await gDevTools.showToolbox(target, toolId, hostType);

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
  if (TargetFactory.isKnownTab(tab)) {
    const target = await TargetFactory.forTab(tab);
    if (target) {
      await gDevTools.closeToolbox(target);
    }
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
 * Takes a string `script` and evaluates it directly in the content
 * in potentially a different process.
 */
let MM_INC_ID = 0;
function evalInDebuggee(script, browser = gBrowser.selectedBrowser) {
  return new Promise(resolve => {
    const id = MM_INC_ID++;
    const mm = browser.messageManager;
    mm.sendAsyncMessage("devtools:test:eval", { script, id });
    mm.addMessageListener("devtools:test:eval:response", handler);

    function handler({ data }) {
      if (id !== data.id) {
        return;
      }

      info(`Successfully evaled in debuggee: ${script}`);
      mm.removeMessageListener("devtools:test:eval:response", handler);
      resolve(data.value);
    }
  });
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
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  await gDevTools.closeToolbox(target);
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

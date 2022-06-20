/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

/* import-globals-from ../../inspector/test/shared-head.js */

"use strict";

// This shared-head.js file is used by most mochitests
// and we start using it in xpcshell tests as well.
// It contains various common helper functions.

const isMochitest = "gTestPath" in this;
const isXpcshell = !isMochitest;
if (isXpcshell) {
  // gTestPath isn't exposed to xpcshell tests
  // _TEST_FILE is an array for a unique string
  /* global _TEST_FILE */
  this.gTestPath = _TEST_FILE[0];
}

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
    "resource://devtools/shared/loader/Loader.jsm"
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
  "resource://devtools/shared/loader/Loader.jsm"
);

// When loaded from xpcshell test, this file is loaded via xpcshell.ini's head property
// and so it loaded first before anything else and isn't having access to Services global.
// Whereas many head.js files from mochitest import this file via loadSubScript
// and already expose Services as a global.
var Services = this.Services || require("Services");

const { gDevTools } = require("devtools/client/framework/devtools");
const {
  TabDescriptorFactory,
} = require("devtools/client/framework/tab-descriptor-factory");
const {
  CommandsFactory,
} = require("devtools/shared/commands/commands-factory");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

const KeyShortcuts = require("devtools/client/shared/key-shortcuts");

loader.lazyRequireGetter(
  this,
  "ResponsiveUIManager",
  "devtools/client/responsive/manager"
);
loader.lazyRequireGetter(
  this,
  "localTypes",
  "devtools/client/responsive/types"
);
loader.lazyRequireGetter(
  this,
  "ResponsiveMessageHelper",
  "devtools/client/responsive/utils/message"
);

loader.lazyRequireGetter(
  this,
  "FluentReact",
  "devtools/client/shared/vendor/fluent-react"
);

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
const URL_ROOT_NET_SSL = CHROME_URL_ROOT.replace(
  "chrome://mochitests/content/",
  "https://example.net/"
);
// mochi.test:8888 is the actual primary location where files are served.
const URL_ROOT_MOCHI_8888 = CHROME_URL_ROOT.replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

const TARGET_SWITCHING_PREF = "devtools.target-switching.server.enabled";

try {
  if (isMochitest) {
    Services.scriptloader.loadSubScript(
      "chrome://mochitests/content/browser/devtools/client/shared/test/telemetry-test-helpers.js",
      this
    );
  }
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

// All tests are asynchronous
if (isMochitest) {
  waitForExplicitFinish();
}

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
function onConsoleMessage(subject) {
  const message = subject.wrappedJSObject.arguments[0];

  if (message && /Failed propType/.test(message.toString())) {
    ok(false, message);
  }
}

const ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"].getService(
  Ci.nsIConsoleAPIStorage
);

ConsoleAPIStorage.addLogEventListener(
  onConsoleMessage,
  Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal)
);
registerCleanupFunction(() => {
  ConsoleAPIStorage.removeLogEventListener(onConsoleMessage);
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
  while (isMochitest && gBrowser.tabs.length > 1) {
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
    waitForAllTargetsToBeAttached(hud.commands.targetCommand),
    wait(1000),
  ]);

  info("Close the Browser Console");
  await BrowserConsoleManager.closeBrowserConsole();
  info("Browser Console closed");
}

/**
 * Observer code to register the test actor in every DevTools server which
 * starts registering its own actors.
 *
 * We require immediately the highlighter test actor file, because it will force to load and
 * register the front and the spec for HighlighterTestActor. Normally specs and fronts are
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
 * The test cleanup will send the async message "remove-devtools-highlightertestactor-observer",
 * we listen to this message to cleanup the observer.
 */
function highlighterTestActorBootstrap() {
  const HIGHLIGHTER_TEST_ACTOR_URL =
    "chrome://mochitests/content/browser/devtools/client/shared/test/highlighter-test-actor.js";

  const { require: _require } = ChromeUtils.import(
    "resource://devtools/shared/loader/Loader.jsm"
  );
  _require(HIGHLIGHTER_TEST_ACTOR_URL);

  /* eslint-disable-next-line no-shadow */
  const Services = _require("Services");

  const actorRegistryObserver = subject => {
    const actorRegistry = subject.wrappedJSObject;
    actorRegistry.registerModule(HIGHLIGHTER_TEST_ACTOR_URL, {
      prefix: "highlighterTest",
      constructor: "HighlighterTestActor",
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

if (isMochitest) {
  const highlighterTestActorBootstrapScript =
    "data:,(" + highlighterTestActorBootstrap + ")()";
  Services.ppmm.loadProcessScript(
    highlighterTestActorBootstrapScript,
    // Load this script in all processes (created or to be created)
    true
  );

  registerCleanupFunction(() => {
    Services.ppmm.broadcastAsyncMessage("remove-devtools-testactor-observer");
    Services.ppmm.removeDelayedProcessScript(
      highlighterTestActorBootstrapScript
    );
  });
}

/**
 * Spawn an instance of the highlighter test actor for the given toolbox
 *
 * @param {Toolbox} toolbox
 * @param {Object} options
 * @param {Function} options.target: Optional target to get the highlighterTestFront for.
 *        If not provided, the top level target will be used.
 * @returns {HighlighterTestFront}
 */
async function getHighlighterTestFront(toolbox, { target } = {}) {
  // Loading the Inspector panel in order to overwrite the TestActor getter for the
  // highlighter instance with a method that points to the currently visible
  // Box Model Highlighter managed by the Inspector panel.
  const inspector = await toolbox.loadTool("inspector");

  const highlighterTestFront = await (target || toolbox.target).getFront(
    "highlighterTest"
  );
  // Override the highligher getter with a method to return the active box model
  // highlighter. Adaptation for multi-process scenarios where there can be multiple
  // highlighters, one per process.
  highlighterTestFront.highlighter = () => {
    return inspector.highlighters.getActiveHighlighter(
      inspector.highlighters.TYPES.BOXMODEL
    );
  };
  return highlighterTestFront;
}

/**
 * Spawn an instance of the highlighter test actor for the given tab, when we need the
 * highlighter test front before opening or without a toolbox.
 *
 * @param {Tab} tab
 * @returns {HighlighterTestFront}
 */
async function getHighlighterTestFrontWithoutToolbox(tab) {
  const commands = await CommandsFactory.forTab(tab);
  // Initialize the TargetCommands which require some async stuff to be done
  // before being fully ready. This will define the `targetCommand.targetFront` attribute.
  await commands.targetCommand.startListening();

  const targetFront = commands.targetCommand.targetFront;
  return targetFront.getFront("highlighterTest");
}

/**
 * Returns a Promise that resolves when all the targets are fully attached.
 *
 * @param {TargetCommand} targetCommand
 */
function waitForAllTargetsToBeAttached(targetCommand) {
  return Promise.allSettled(
    targetCommand
      .getAllTargets(targetCommand.ALL_TYPES)
      .map(target => target.initialized)
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
async function addTab(url, options = {}) {
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
    // Waiting for presShell helps with test timeouts in webrender platforms.
    await waitForPresShell(tab.linkedBrowser);
    info("Tab added and finished loading");
  } else {
    info("Tab added");
  }

  return tab;
}

/**
 * Remove the given tab.
 * @param {Object} tab The tab to be removed.
 * @return Promise<undefined> resolved when the tab is successfully removed.
 */
async function removeTab(tab) {
  info("Removing tab.");

  const { gBrowser } = tab.ownerDocument.defaultView;
  const onClose = once(gBrowser.tabContainer, "TabClose");
  gBrowser.removeTab(tab);
  await onClose;

  info("Tab removed and finished closing");
}

/**
 * Alias for navigateTo which will reuse the current URI of the provided browser
 * to trigger a navigation.
 */
async function reloadBrowser({
  browser = gBrowser.selectedBrowser,
  isErrorPage = false,
  waitForLoad = true,
} = {}) {
  return navigateTo(browser.currentURI.spec, {
    browser,
    isErrorPage,
    waitForLoad,
  });
}

/**
 * Navigate the currently selected tab to a new URL and wait for it to load.
 * Also wait for the toolbox to attach to the new target, if we navigated
 * to a new process.
 *
 * @param {String} url The url to be loaded in the current tab.
 * @param {JSON} options Optional dictionary object with the following keys:
 *        - {XULBrowser} browser
 *          The browser element which should navigate. Defaults to the selected
 *          browser.
 *        - {Boolean} isErrorPage
 *          You may pass `true` if the URL is an error page. Otherwise
 *          BrowserTestUtils.browserLoaded will wait for 'load' event, which
 *          never fires for error pages.
 *        - {Boolean} waitForLoad
 *          You may pass `false` if the page load is expected to be blocked by
 *          a script or a breakpoint.
 *
 * @return a promise that resolves when the page has fully loaded.
 */
async function navigateTo(
  uri,
  {
    browser = gBrowser.selectedBrowser,
    isErrorPage = false,
    waitForLoad = true,
  } = {}
) {
  const waitForDevToolsReload = await watchForDevToolsReload(browser, {
    isErrorPage,
    waitForLoad,
  });

  uri = uri.replaceAll("\n", "");
  info(`Navigating to "${uri}"`);

  const onBrowserLoaded = BrowserTestUtils.browserLoaded(
    browser,
    // includeSubFrames
    false,
    // resolve on this specific page to load (if null, it would be any page load)
    loadedUrl => {
      // loadedUrl is encoded, while uri might not be.
      return loadedUrl === uri || decodeURI(loadedUrl) === uri;
    },
    isErrorPage
  );

  // if we're navigating to the same page we're already on, use reloadTab instead as the
  // behavior slightly differs from loadURI (e.g. scroll position isn't keps with the latter).
  if (uri === browser.currentURI.spec) {
    gBrowser.reloadTab(gBrowser.getTabForBrowser(browser));
  } else {
    BrowserTestUtils.loadURI(browser, uri);
  }

  if (waitForLoad) {
    info(`Waiting for page to be loaded…`);
    await onBrowserLoaded;
    info(`→ page loaded`);
  }

  await waitForDevToolsReload();
}

/**
 * This method should be used to watch for completion of any browser navigation
 * performed with a DevTools UI.
 *
 * It should watch for:
 * - Toolbox reload
 * - Toolbox commands reload
 * - RDM reload
 * - RDM commands reload
 *
 * And it should work both for target switching or old-style navigations.
 *
 * This method, similarly to all the other watch* navigation methods in this file,
 * is async but returns another method which should be called after the navigation
 * is done. Browser navigation might be monitored differently depending on the
 * situation, so it's up to the caller to handle it as needed.
 *
 * Typically, this would be used as follows:
 * ```
 *   async function someNavigationHelper(browser) {
 *     const waitForDevToolsFn = await watchForDevToolsReload(browser);
 *
 *     // This step should wait for the load to be completed from the browser's
 *     // point of view, so that waitForDevToolsFn can compare pIds, browsing
 *     // contexts etc... and check if we should expect a target switch
 *     await performBrowserNavigation(browser);
 *
 *     await waitForDevToolsFn();
 *   }
 * ```
 */
async function watchForDevToolsReload(
  browser,
  { isErrorPage = false, waitForLoad = true } = {}
) {
  const waitForToolboxReload = await _watchForToolboxReload(browser, {
    isErrorPage,
    waitForLoad,
  });
  const waitForResponsiveReload = await _watchForResponsiveReload(browser, {
    isErrorPage,
    waitForLoad,
  });

  return async function() {
    info("Wait for the toolbox to reload");
    await waitForToolboxReload();

    info("Wait for Responsive UI to reload");
    await waitForResponsiveReload();
  };
}

/**
 * Start watching for the toolbox reload to be completed:
 * - watch for the toolbox's commands to be fully reloaded
 * - watch for the toolbox's current panel to be reloaded
 */
async function _watchForToolboxReload(
  browser,
  { isErrorPage, waitForLoad } = {}
) {
  const tab = gBrowser.getTabForBrowser(browser);

  const toolbox = await gDevTools.getToolboxForTab(tab);

  if (!toolbox) {
    // No toolbox to wait for
    return function() {};
  }

  const waitForCurrentPanelReload = watchForCurrentPanelReload(toolbox);
  const waitForToolboxCommandsReload = await watchForCommandsReload(
    toolbox.commands,
    { isErrorPage, waitForLoad }
  );
  const checkTargetSwitching = await watchForTargetSwitching(
    toolbox.commands,
    browser
  );

  return async function() {
    const isTargetSwitching = checkTargetSwitching();

    info(`Waiting for toolbox commands to be reloaded…`);
    await waitForToolboxCommandsReload(isTargetSwitching);

    // TODO: We should wait for all loaded panels to reload here, because some
    // of them might still perform background updates.
    if (waitForCurrentPanelReload) {
      info(`Waiting for ${toolbox.currentToolId} to be reloaded…`);
      await waitForCurrentPanelReload();
      info(`→ panel reloaded`);
    }
  };
}

/**
 * Start watching for Responsive UI (RDM) reload to be completed:
 * - watch for the Responsive UI's commands to be fully reloaded
 * - watch for the Responsive UI's target switch to be done
 */
async function _watchForResponsiveReload(
  browser,
  { isErrorPage, waitForLoad } = {}
) {
  const tab = gBrowser.getTabForBrowser(browser);
  const ui = ResponsiveUIManager.getResponsiveUIForTab(tab);

  if (!ui) {
    // No responsive UI to wait for
    return function() {};
  }

  const onResponsiveTargetSwitch = ui.once("responsive-ui-target-switch-done");
  const waitForResponsiveCommandsReload = await watchForCommandsReload(
    ui.commands,
    { isErrorPage, waitForLoad }
  );
  const checkTargetSwitching = await watchForTargetSwitching(
    ui.commands,
    browser
  );

  return async function() {
    const isTargetSwitching = checkTargetSwitching();

    info(`Waiting for responsive ui commands to be reloaded…`);
    await waitForResponsiveCommandsReload(isTargetSwitching);

    if (isTargetSwitching) {
      await onResponsiveTargetSwitch;
    }
  };
}

/**
 * Watch for the current panel selected in the provided toolbox to be reloaded.
 * Some panels implement custom events that should be expected for every reload.
 *
 * Note about returning a method instead of a promise:
 * In general this pattern is useful so that we can check if a target switch
 * occurred or not, and decide which events to listen for. So far no panel is
 * behaving differently whether there was a target switch or not. But to remain
 * consistent with other watch* methods we still return a function here.
 *
 * @param {Toolbox}
 *        The Toolbox instance which is going to experience a reload
 * @return {function} An async method to be called and awaited after the reload
 *         started. Will return `null` for panels which don't implement any
 *         specific reload event.
 */
function watchForCurrentPanelReload(toolbox) {
  return _watchForPanelReload(toolbox, toolbox.currentToolId);
}

/**
 * Watch for all the panels loaded in the provided toolbox to be reloaded.
 * Some panels implement custom events that should be expected for every reload.
 *
 * Note about returning a method instead of a promise:
 * See comment for watchForCurrentPanelReload
 *
 * @param {Toolbox}
 *        The Toolbox instance which is going to experience a reload
 * @return {function} An async method to be called and awaited after the reload
 *         started.
 */
function watchForLoadedPanelsReload(toolbox) {
  const waitForPanels = [];
  for (const [id] of toolbox.getToolPanels()) {
    // Store a watcher method for each panel already loaded.
    waitForPanels.push(_watchForPanelReload(toolbox, id));
  }

  return function() {
    return Promise.all(
      waitForPanels.map(async watchPanel => {
        // Wait for all panels to be reloaded.
        if (watchPanel) {
          await watchPanel();
        }
      })
    );
  };
}

function _watchForPanelReload(toolbox, toolId) {
  const panel = toolbox.getPanel(toolId);

  if (toolId == "inspector") {
    const markuploaded = panel.once("markuploaded");
    const onNewRoot = panel.once("new-root");
    const onUpdated = panel.once("inspector-updated");
    const onReloaded = panel.once("reloaded");

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
  } else if (toolId == "netmonitor") {
    const onReloaded = panel.once("reloaded");
    return async function() {
      info("Waiting for netmonitor updates after page reload");
      await onReloaded;
    };
  } else if (toolId == "accessibility") {
    const onReloaded = panel.once("reloaded");
    return async function() {
      info("Waiting for accessibility updates after page reload");
      await onReloaded;
    };
  }
  return null;
}

/**
 * Watch for a Commands instance to be reloaded after a navigation.
 *
 * As for other navigation watch* methods, this should be called before the
 * navigation starts, and the function it returns should be called after the
 * navigation is done from a Browser point of view.
 *
 * !!! The wait function expects a `isTargetSwitching` argument to be provided,
 * which needs to be monitored using watchForTargetSwitching !!!
 */
async function watchForCommandsReload(
  commands,
  { isErrorPage = false, waitForLoad = true } = {}
) {
  // If we're switching origins, we need to wait for the 'switched-target'
  // event to make sure everything is ready.
  // Navigating from/to pages loaded in the parent process, like about:robots,
  // also spawn new targets.
  // (If target switching is disabled, the toolbox will reboot)
  const onTargetSwitched = commands.targetCommand.once("switched-target");

  // Wait until we received a page load resource:
  // - dom-complete if we can wait for a full page load
  // - dom-loading otherwise
  // This allows to wait for page load for consumers calling directly
  // waitForDevTools instead of navigateTo/reloadBrowser.
  // This is also useful as an alternative to target switching, when no target
  // switch is supposed to happen.
  const waitForCompleteLoad = waitForLoad && !isErrorPage;
  const documentEventName = waitForCompleteLoad
    ? "dom-complete"
    : "dom-loading";

  const {
    onResource: onTopLevelDomEvent,
  } = await commands.resourceCommand.waitForNextResource(
    commands.resourceCommand.TYPES.DOCUMENT_EVENT,
    {
      ignoreExistingResources: true,
      predicate: resource =>
        resource.targetFront.isTopLevel && resource.name === documentEventName,
    }
  );

  return async function(isTargetSwitching) {
    if (typeof isTargetSwitching === "undefined") {
      throw new Error("isTargetSwitching was not provided to the wait method");
    }

    if (isTargetSwitching) {
      info(`Waiting for target switch…`);
      await onTargetSwitched;
      info(`→ switched-target emitted`);
    }

    info(`Waiting for '${documentEventName}' resource…`);
    await onTopLevelDomEvent;
    info(`→ '${documentEventName}' resource emitted`);

    return isTargetSwitching;
  };
}

/**
 * Watch if an upcoming navigation will trigger a target switching, for the
 * provided Commands instance and the provided Browser.
 *
 * As for other navigation watch* methods, this should be called before the
 * navigation starts, and the function it returns should be called after the
 * navigation is done from a Browser point of view.
 */
async function watchForTargetSwitching(commands, browser) {
  browser = browser || gBrowser.selectedBrowser;
  const currentPID = browser.browsingContext.currentWindowGlobal.osPid;
  const currentBrowsingContextID = browser.browsingContext.id;

  // If the current top-level target follows the window global lifecycle, a
  // target switch will occur regardless of process changes.
  const targetFollowsWindowLifecycle =
    commands.targetCommand.targetFront.targetForm.followWindowGlobalLifeCycle;

  return function() {
    // Compare the PIDs (and not the toolbox's targets) as PIDs are updated also immediately,
    // while target may be updated slightly later.
    const switchedProcess =
      currentPID !== browser.browsingContext.currentWindowGlobal.osPid;
    const switchedBrowsingContext =
      currentBrowsingContextID !== browser.browsingContext.id;

    return (
      targetFollowsWindowLifecycle || switchedProcess || switchedBrowsingContext
    );
  };
}

/**
 * Create a Target for the provided tab and attach to it before resolving.
 * This should only be used for tests which don't involve the frontend or a
 * toolbox. Typically, retrieving the target and attaching to it should be
 * handled at framework level when a Toolbox is used.
 *
 * @param {XULTab} tab
 *        The tab for which a target should be created.
 * @return {WindowGlobalTargetFront} The attached target front.
 */
async function createAndAttachTargetForTab(tab) {
  info("Creating and attaching to a local tab target");

  const commands = await CommandsFactory.forTab(tab);

  // Initialize the TargetCommands which require some async stuff to be done
  // before being fully ready. This will define the `targetCommand.targetFront` attribute.
  await commands.targetCommand.startListening();

  const target = commands.targetCommand.targetFront;
  return target;
}

function isFissionEnabled() {
  return SpecialPowers.useRemoteSubframes;
}

function isServerTargetSwitchingEnabled() {
  return Services.prefs.getBoolPref(TARGET_SWITCHING_PREF);
}

/**
 * Enables server target switching
 */
async function enableTargetSwitching() {
  await pushPref(TARGET_SWITCHING_PREF, true);
}

function isEveryFrameTargetEnabled() {
  return Services.prefs.getBoolPref(
    "devtools.every-frame-target.enabled",
    false
  );
}

/**
 * Open the inspector in a tab with given URL.
 * @param {string} url  The URL to open.
 * @param {String} hostType Optional hostType, as defined in Toolbox.HostType
 * @return A promise that is resolved once the tab and inspector have loaded
 *         with an object: { tab, toolbox, inspector, highlighterTestFront }.
 */
async function openInspectorForURL(url, hostType) {
  const tab = await addTab(url);
  const { inspector, toolbox, highlighterTestFront } = await openInspector(
    hostType
  );
  return { tab, inspector, toolbox, highlighterTestFront };
}

async function getActiveInspector() {
  const toolbox = await gDevTools.getToolboxForTab(gBrowser.selectedTab);
  return toolbox.getPanel("inspector");
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
 *        Can be set globally for a test via `waitFor.overrideIntervalForTestFile = someNumber;`.
 * @param number maxTries [optional]
 *        How many times the predicate is invoked before timing out.
 *        Can be set globally for a test via `waitFor.overrideMaxTriesForTestFile = someNumber;`.
 * @return object
 *         A promise that is resolved with the result of the condition.
 */
async function waitFor(condition, message = "", interval = 10, maxTries = 500) {
  // Update interval & maxTries if overrides are defined on the waitFor object.
  interval =
    typeof waitFor.overrideIntervalForTestFile !== "undefined"
      ? waitFor.overrideIntervalForTestFile
      : interval;
  maxTries =
    typeof waitFor.overrideMaxTriesForTestFile !== "undefined"
      ? waitFor.overrideMaxTriesForTestFile
      : maxTries;

  try {
    const value = await BrowserTestUtils.waitForCondition(
      condition,
      message,
      interval,
      maxTries
    );
    return value;
  } catch (e) {
    const errorMessage = `Failed waitFor(): ${message} \nFailed condition: ${condition} \nException Message: ${e}`;
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
async function openToolboxForTab(tab, toolId, hostType) {
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
}

/**
 * Add a new tab and open the toolbox in it.
 * @param {String} url The URL for the tab to be opened.
 * @param {String} toolId Optional. The ID of the tool to be selected.
 * @param {String} hostType Optional. The type of toolbox host to be used.
 * @return {Promise} Resolves when the tab has been added, loaded and the
 * toolbox has been opened. Resolves to the toolbox.
 */
async function openNewTabAndToolbox(url, toolId, hostType) {
  const tab = await addTab(url);
  return openToolboxForTab(tab, toolId, hostType);
}

/**
 * Close a tab and if necessary, the toolbox that belongs to it
 * @param {Tab} tab The tab to close.
 * @return {Promise} Resolves when the toolbox and tab have been destroyed and
 * closed.
 */
async function closeTabAndToolbox(tab = gBrowser.selectedTab) {
  if (TabDescriptorFactory.isKnownTab(tab)) {
    await gDevTools.closeToolboxForTab(tab);
  }

  await removeTab(tab);

  await new Promise(resolve => setTimeout(resolve, 0));
}

/**
 * Close a toolbox and the current tab.
 * @param {Toolbox} toolbox The toolbox to close.
 * @return {Promise} Resolves when the toolbox and tab have been destroyed and
 * closed.
 */
async function closeToolboxAndTab(toolbox) {
  await toolbox.destroy();
  await removeTab(gBrowser.selectedTab);
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

async function closeToolbox() {
  await gDevTools.closeToolboxForTab(gBrowser.selectedTab);
}

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
        "resource://devtools/shared/loader/Loader.jsm"
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
  image.src = PathUtils.toFileURI(filePath);
  await onImageLoad;

  info("Remove the downloaded screenshot file");
  await IOUtils.remove(filePath);

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
 * Wait until the store has reached a state that matches the predicate.
 * @param Store store
 *        The Redux store being used.
 * @param function predicate
 *        A function that returns true when the store has reached the expected
 *        state.
 * @return Promise
 *         Resolved once the store reaches the expected state.
 */
function waitUntilState(store, predicate) {
  return new Promise(resolve => {
    const unsubscribe = store.subscribe(check);

    info(`Waiting for state predicate "${predicate}"`);
    function check() {
      if (predicate(store.getState())) {
        info(`Found state predicate "${predicate}"`);
        unsubscribe();
        resolve();
      }
    }

    // Fire the check immediately in case the action has already occurred
    check();
  });
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

/**
 * Retrieve a browsing context in nested frames.
 *
 * @param {BrowsingContext|XULBrowser} browsingContext
 *        The topmost browsing context under which we should search for the
 *        browsing context.
 * @param {Array<String>} selectors
 *        Array of CSS selectors that form a path to a specific nested frame.
 * @return {BrowsingContext} The nested browsing context.
 */
async function getBrowsingContextInFrames(browsingContext, selectors) {
  let context = browsingContext;

  if (!Array.isArray(selectors)) {
    throw new Error(
      "getBrowsingContextInFrames called with an invalid selectors argument"
    );
  }

  if (selectors.length === 0) {
    throw new Error(
      "getBrowsingContextInFrames called with an empty selectors array"
    );
  }

  const clonedSelectors = [...selectors];
  while (clonedSelectors.length) {
    const selector = clonedSelectors.shift();
    context = await SpecialPowers.spawn(context, [selector], _selector => {
      return content.document.querySelector(_selector).browsingContext;
    });
  }

  return context;
}

/**
 * Synthesize a mouse event on an element, after ensuring that it is visible
 * in the viewport.
 *
 * @param {String|Array} selector: The node selector to get the node target for the event.
 *        To target an element in a specific iframe, pass an array of CSS selectors
 *        (e.g. ["iframe", ".el-in-iframe"])
 * @param {number} x
 * @param {number} y
 * @param {object} options: Options that will be passed to BrowserTestUtils.synthesizeMouse
 */
async function safeSynthesizeMouseEventInContentPage(
  selector,
  x,
  y,
  options = {}
) {
  let context = gBrowser.selectedBrowser.browsingContext;

  // If an array of selector is passed, we need to retrieve the context in which the node
  // lives in.
  if (Array.isArray(selector)) {
    if (selector.length === 1) {
      selector = selector[0];
    } else {
      context = await getBrowsingContextInFrames(
        context,
        // only pass the iframe path
        selector.slice(0, -1)
      );
      // retrieve the last item of the selector, which should be the one for the node we want.
      selector = selector.at(-1);
    }
  }

  await scrollContentPageNodeIntoView(context, selector);
  BrowserTestUtils.synthesizeMouse(selector, x, y, options, context);
}

/**
 * Synthesize a mouse event at the center of an element, after ensuring that it is visible
 * in the viewport.
 *
 * @param {String|Array} selector: The node selector to get the node target for the event.
 *        To target an element in a specific iframe, pass an array of CSS selectors
 *        (e.g. ["iframe", ".el-in-iframe"])
 * @param {object} options: Options that will be passed to BrowserTestUtils.synthesizeMouse
 */
async function safeSynthesizeMouseEventAtCenterInContentPage(
  selector,
  options = {}
) {
  let context = gBrowser.selectedBrowser.browsingContext;

  // If an array of selector is passed, we need to retrieve the context in which the node
  // lives in.
  if (Array.isArray(selector)) {
    if (selector.length === 1) {
      selector = selector[0];
    } else {
      context = await getBrowsingContextInFrames(
        context,
        // only pass the iframe path
        selector.slice(0, -1)
      );
      // retrieve the last item of the selector, which should be the one for the node we want.
      selector = selector.at(-1);
    }
  }

  await scrollContentPageNodeIntoView(context, selector);
  BrowserTestUtils.synthesizeMouseAtCenter(selector, options, context);
}

/**
 * Scroll into view an element in the content page matching the passed selector
 *
 * @param {BrowsingContext} browsingContext: The browsing context the element lives in.
 * @param {String} selector: The node selector to get the node to scroll into view
 * @returns {Promise}
 */
function scrollContentPageNodeIntoView(browsingContext, selector) {
  return SpecialPowers.spawn(browsingContext, [selector], function(
    innerSelector
  ) {
    const node = content.wrappedJSObject.document.querySelector(innerSelector);
    node.scrollIntoView();
  });
}

/**
 * Change the zoom level of the selected page.
 *
 * @param {Number} zoomLevel
 */
function setContentPageZoomLevel(zoomLevel) {
  gBrowser.selectedBrowser.fullZoom = zoomLevel;
}

/**
 * Wait for the next DOCUMENT_EVENT dom-complete resource on a top-level target
 *
 * @param {Object} commands
 * @return {Promise<Object>}
 *         Return a promise which resolves once we fully settle the resource listener.
 *         You should await for its resolution before doing the action which may fire
 *         your resource.
 *         This promise will resolve with an object containing a `onDomCompleteResource` property,
 *         which is also a promise, that will resolve once a "top-level" DOCUMENT_EVENT dom-complete
 *         is received.
 */
async function waitForNextTopLevelDomCompleteResource(commands) {
  const {
    onResource: onDomCompleteResource,
  } = await commands.resourceCommand.waitForNextResource(
    commands.resourceCommand.TYPES.DOCUMENT_EVENT,
    {
      ignoreExistingResources: true,
      predicate: resource =>
        resource.name === "dom-complete" && resource.targetFront.isTopLevel,
    }
  );
  return { onDomCompleteResource };
}

/**
 * Wait for the provided context to have a valid presShell. This can be useful
 * for tests which try to create popup panels or interact with the document very
 * early.
 *
 * @param {BrowsingContext} context
 **/
function waitForPresShell(context) {
  return SpecialPowers.spawn(context, [], async () => {
    const winUtils = SpecialPowers.getDOMWindowUtils(content);
    await ContentTaskUtils.waitForCondition(() => {
      try {
        return !!winUtils.getPresShellId();
      } catch (e) {
        return false;
      }
    }, "Waiting for a valid presShell");
  });
}

/**
 * In tests using Fluent localization, it is preferable to match DOM elements using
 * a message ID rather than the raw string as:
 *
 *  1. It allows testing infrastructure to be multilingual if needed.
 *  2. It isolates the tests from localization changes.
 *
 * @param {Array<string>} resourceIds A list of .ftl files to load.
 * @returns {(id: string, args?: Record<string, FluentVariable>) => string}
 */
async function getFluentStringHelper(resourceIds) {
  const locales = Services.locale.appLocalesAsBCP47;
  const generator = L10nRegistry.getInstance().generateBundles(
    locales,
    resourceIds
  );

  const bundles = [];
  for await (const bundle of generator) {
    bundles.push(bundle);
  }

  const reactLocalization = new FluentReact.ReactLocalization(bundles);

  /**
   * Get the string from a message id. It throws when the message is not found.
   *
   * @param {string} id
   * @param {string} attributeName: attribute name if you need to access a specific attribute
   *                 defined in the fluent string, e.g. setting "title" for this param
   *                 will retrieve the `title` string in
   *                    compatibility-issue-browsers-list =
   *                      .title = This is the title
   * @param {Record<string, FluentVariable>} [args] optional
   * @returns {string}
   */
  return (id, attributeName, args) => {
    let string;

    if (!attributeName) {
      string = reactLocalization.getString(id, args);
    } else {
      for (const bundle of reactLocalization.bundles) {
        const msg = bundle.getMessage(id);
        if (msg?.attributes[attributeName]) {
          string = bundle.formatPattern(
            msg.attributes[attributeName],
            args,
            []
          );
          break;
        }
      }
    }

    if (!string) {
      throw new Error(
        `Could not find a string for "${id}"${
          attributeName ? ` and attribute "${attributeName}")` : ""
        }. Was the correct resource bundle loaded?`
      );
    }
    return string;
  };
}

/**
 * Open responsive design mode for the given tab.
 */
async function openRDM(tab, { waitForDeviceList = true } = {}) {
  info("Opening responsive design mode");
  const manager = ResponsiveUIManager;
  const ui = await manager.openIfNeeded(tab.ownerGlobal, tab, {
    trigger: "test",
  });
  info("Responsive design mode opened");

  await ResponsiveMessageHelper.wait(ui.toolWindow, "post-init");
  info("Responsive design initialized");

  await waitForRDMLoaded(ui, { waitForDeviceList });

  return { ui, manager };
}

async function waitForRDMLoaded(ui, { waitForDeviceList = true } = {}) {
  // Always wait for the viewport to be added.
  const { store } = ui.toolWindow;
  await waitUntilState(store, state => state.viewports.length == 1);

  if (waitForDeviceList) {
    // Wait until the device list has been loaded.
    await waitUntilState(
      store,
      state => state.devices.listState == localTypes.loadableState.LOADED
    );
  }
}

/**
 * Close responsive design mode for the given tab.
 */
async function closeRDM(tab, options) {
  info("Closing responsive design mode");
  const manager = ResponsiveUIManager;
  await manager.closeIfNeeded(tab.ownerGlobal, tab, options);
  info("Responsive design mode closed");
}

function getInputStream(data) {
  const BufferStream = CC(
    "@mozilla.org/io/arraybuffer-input-stream;1",
    "nsIArrayBufferInputStream",
    "setData"
  );
  const buffer = new TextEncoder().encode(data).buffer;
  return new BufferStream(buffer, 0, buffer.byteLength);
}

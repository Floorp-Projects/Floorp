/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * The Mochitest API documentation
 * @module mochitest
 */

/**
 * The mochitest API to wait for certain events.
 * @module mochitest/waits
 * @parent mochitest
 */

/**
 * The mochitest API predefined asserts.
 * @module mochitest/asserts
 * @parent mochitest
 */

/**
 * The mochitest API for interacting with the debugger.
 * @module mochitest/actions
 * @parent mochitest
 */

/**
 * Helper methods for the mochitest API.
 * @module mochitest/helpers
 * @parent mochitest
 */

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

var { Toolbox } = require("devtools/client/framework/toolbox");
var { Task } = require("devtools/shared/task");

const sourceUtils = {
  isLoaded: source => source.get("loadedState") === "loaded"
};

const EXAMPLE_URL =
  "http://example.com/browser/devtools/client/debugger/new/test/mochitest/examples/";

Services.prefs.setBoolPref("devtools.debugger.new-debugger-frontend", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.debugger.new-debugger-frontend");
  delete window.resumeTest;
});

function log(msg, data) {
  info(`${msg} ${!data ? "" : JSON.stringify(data)}`);
}

function logThreadEvents(dbg, event) {
  const thread = dbg.toolbox.threadClient;

  thread.addListener(event, function onEvent(eventName, ...args) {
    info(`Thread event '${eventName}' fired.`);
  });
}

async function waitFor(condition) {
  await BrowserTestUtils.waitForCondition(condition, "waitFor", 10, 500);
  return condition();
}

// Wait until an action of `type` is dispatched. This is different
// then `_afterDispatchDone` because it doesn't wait for async actions
// to be done/errored. Use this if you want to listen for the "start"
// action of an async operation (somewhat rare).
function waitForNextDispatch(store, type) {
  return new Promise(resolve => {
    store.dispatch({
      // Normally we would use `services.WAIT_UNTIL`, but use the
      // internal name here so tests aren't forced to always pass it
      // in
      type: "@@service/waitUntil",
      predicate: action => action.type === type,
      run: (dispatch, getState, action) => {
        resolve(action);
      }
    });
  });
}

// Wait until an action of `type` is dispatched. If it's part of an
// async operation, wait until the `status` field is "done" or "error"
function _afterDispatchDone(store, type) {
  return new Promise(resolve => {
    store.dispatch({
      // Normally we would use `services.WAIT_UNTIL`, but use the
      // internal name here so tests aren't forced to always pass it
      // in
      type: "@@service/waitUntil",
      predicate: action => {
        if (action.type === type) {
          return action.status
            ? action.status === "done" || action.status === "error"
            : true;
        }
      },
      run: (dispatch, getState, action) => {
        resolve(action);
      }
    });
  });
}

/**
 * Wait for a specific action type to be dispatch.
 * If an async action, will wait for it to be done.
 *
 * @memberof mochitest/waits
 * @param {Object} dbg
 * @param {String} type
 * @param {Number} eventRepeat
 * @return {Promise}
 * @static
 */
function waitForDispatch(dbg, type, eventRepeat = 1) {
  let count = 0;

  return Task.spawn(function*() {
    info(`Waiting for ${type} to dispatch ${eventRepeat} time(s)`);
    while (count < eventRepeat) {
      yield _afterDispatchDone(dbg.store, type);
      count++;
      info(`${type} dispatched ${count} time(s)`);
    }
  });
}

/**
 * Waits for specific thread events.
 *
 * @memberof mochitest/waits
 * @param {Object} dbg
 * @param {String} eventName
 * @return {Promise}
 * @static
 */
function waitForThreadEvents(dbg, eventName) {
  info(`Waiting for thread event '${eventName}' to fire.`);
  const thread = dbg.toolbox.threadClient;

  return new Promise(function(resolve, reject) {
    thread.addListener(eventName, function onEvent(eventName, ...args) {
      info(`Thread event '${eventName}' fired.`);
      thread.removeListener(eventName, onEvent);
      resolve.apply(resolve, args);
    });
  });
}

/**
 * Waits for `predicate(state)` to be true. `state` is the redux app state.
 *
 * @memberof mochitest/waits
 * @param {Object} dbg
 * @param {Function} predicate
 * @return {Promise}
 * @static
 */
function waitForState(dbg, predicate, msg) {
  return new Promise(resolve => {
    info(`Waiting for state change: ${msg || ""}`);
    if (predicate(dbg.store.getState())) {
      return resolve();
    }

    const unsubscribe = dbg.store.subscribe(() => {
      const result = predicate(dbg.store.getState());
      if (result) {
        info(`Finished waiting for state change: ${msg || ""}`);
        unsubscribe();
        resolve(result);
      }
    });
  });
}

/**
 * Waits for sources to be loaded.
 *
 * @memberof mochitest/waits
 * @param {Object} dbg
 * @param {Array} sources
 * @return {Promise}
 * @static
 */
async function waitForSources(dbg, ...sources) {
  const { selectors: { getSources }, store } = dbg;
  if (sources.length === 0) {
    return Promise.resolve();
  }

  info(`Waiting on sources: ${sources.join(", ")}`);
  await Promise.all(
    sources.map(url => {
      if (!sourceExists(dbg, url)) {
        return waitForState(
          dbg,
          () => sourceExists(dbg, url),
          `source ${url} exists`
        );
      }
    })
  );

  info(`Finished waiting on sources: ${sources.join(", ")}`);
}

/**
 * Waits for a source to be loaded.
 *
 * @memberof mochitest/waits
 * @param {Object} dbg
 * @param {String} source
 * @return {Promise}
 * @static
 */
function waitForSource(dbg, url) {
  return waitForState(
    dbg,
    state => {
      const sources = dbg.selectors.getSources(state);
      return sources.find(s => (s.get("url") || "").includes(url));
    },
    `source exists`
  );
}

async function waitForElement(dbg, name) {
  await waitUntil(() => findElement(dbg, name));
  return findElement(dbg, name);
}

async function waitForElementWithSelector(dbg, selector) {
  await waitUntil(() => findElementWithSelector(dbg, selector));
  return findElementWithSelector(dbg, selector);
}

function waitForSelectedSource(dbg, url) {
  return waitForState(
    dbg,
    state => {

      const source = dbg.selectors.getSelectedSource(state);
      const isLoaded = source && sourceUtils.isLoaded(source);
      if (!isLoaded) {
        return false;
      }

      if (!url) {
        return true;
      }

      const newSource = findSource(dbg, url, { silent: true });
      if (newSource.id != source.id) {
        return false;
      }

      // wait for async work to be done
      const hasSymbols = dbg.selectors.hasSymbols(state, source);
      const hasSourceMetaData = dbg.selectors.hasSourceMetaData(
        state,
        source.id
      );
      const hasPausePoints = dbg.selectors.hasPausePoints(state, source.id);
      return hasSymbols && hasSourceMetaData && hasPausePoints;
    },
    "selected source"
  );
}

/**
 * Assert that the debugger is not currently paused.
 * @memberof mochitest/asserts
 * @static
 */
function assertNotPaused(dbg) {
  ok(!isPaused(dbg), "client is not paused");
}

function getVisibleSelectedFrameLine(dbg) {
  const { selectors: { getVisibleSelectedFrame }, getState } = dbg;
  const frame = getVisibleSelectedFrame(getState());
  return frame && frame.location.line;
}

/**
 * Assert that the debugger is paused at the correct location.
 *
 * @memberof mochitest/asserts
 * @param {Object} dbg
 * @param {String} source
 * @param {Number} line
 * @static
 */
function assertPausedLocation(dbg) {
  const { selectors: { getSelectedSource }, getState } = dbg;

  ok(
    isSelectedFrameSelected(dbg, getState()),
    "top frame's source is selected"
  );

  // Check the pause location
  const pauseLine = getVisibleSelectedFrameLine(dbg);
  assertDebugLine(dbg, pauseLine);

  ok(isVisibleInEditor(dbg, getCM(dbg).display.gutters), "gutter is visible");
}

function assertDebugLine(dbg, line) {
  // Check the debug line
  const lineInfo = getCM(dbg).lineInfo(line - 1);
  const source = dbg.selectors.getSelectedSource(dbg.getState());
  if (source && source.loadedState == "loading") {
    const url = source.url;
    ok(
      false,
      `Looks like the source ${url} is still loading. Try adding waitForLoadedSource in the test.`
    );
    return;
  }

  if (!lineInfo.wrapClass) {
    const pauseLine = getVisibleSelectedFrameLine(dbg);
    ok(false, `Expected pause line on line ${line}, it is on ${pauseLine}`);
    return;
  }

  ok(
    lineInfo.wrapClass.includes("new-debug-line"),
    "Line is highlighted as paused"
  );

  const debugLine =
    findElement(dbg, "debugLine") || findElement(dbg, "debugErrorLine");

  is(
    findAllElements(dbg, "debugLine").length +
      findAllElements(dbg, "debugErrorLine").length,
    1,
    "There is only one line"
  );

  ok(isVisibleInEditor(dbg, debugLine), "debug line is visible");

  const markedSpans = lineInfo.handle.markedSpans;
  if (markedSpans && markedSpans.length > 0) {
    const marker = markedSpans[0].marker;
    ok(
      marker.className.includes("debug-expression"),
      "expression is highlighted as paused"
    );
  }
}

/**
 * Assert that the debugger is highlighting the correct location.
 *
 * @memberof mochitest/asserts
 * @param {Object} dbg
 * @param {String} source
 * @param {Number} line
 * @static
 */
function assertHighlightLocation(dbg, source, line) {
  const { selectors: { getSelectedSource }, getState } = dbg;
  source = findSource(dbg, source);

  // Check the selected source
  is(
    getSelectedSource(getState()).get("url"),
    source.url,
    "source url is correct"
  );

  // Check the highlight line
  const lineEl = findElement(dbg, "highlightLine");
  ok(lineEl, "Line is highlighted");

  is(
    findAllElements(dbg, "highlightLine").length,
    1,
    "Only 1 line is highlighted"
  );

  ok(isVisibleInEditor(dbg, lineEl), "Highlighted line is visible");
  ok(
    getCM(dbg)
      .lineInfo(line - 1)
      .wrapClass.includes("highlight-line"),
    "Line is highlighted"
  );
}

/**
 * Returns boolean for whether the debugger is paused.
 *
 * @memberof mochitest/asserts
 * @param {Object} dbg
 * @static
 */
function isPaused(dbg) {
  const { selectors: { isPaused }, getState } = dbg;
  return !!isPaused(getState());
}

async function waitForLoadedScopes(dbg) {
  const scopes = await waitForElement(dbg, "scopes");
  // Since scopes auto-expand, we can assume they are loaded when there is a tree node
  // with the aria-level attribute equal to "2".
  await waitUntil(() => scopes.querySelector(`.tree-node[aria-level="2"]`));
}

/**
 * Waits for the debugger to be fully paused.
 *
 * @memberof mochitest/waits
 * @param {Object} dbg
 * @static
 */
async function waitForPaused(dbg, url) {
  const { getSelectedScope } = dbg.selectors;

  await waitForState(
    dbg,
    state => isPaused(dbg) && !!getSelectedScope(state),
    "paused"
  );

  await waitForLoadedScopes(dbg);
  await waitForSelectedSource(dbg, url);
}

/*
 * useful for when you want to see what is happening
 * e.g await waitForever()
 */
function waitForever() {
  return new Promise(r => {});
}

/*
 * useful for waiting for a short amount of time as
 * a placeholder for a better waitForX handler.
 *
 * e.g await waitForTime(500)
 */
function waitForTime(ms) {
  return new Promise(r => setTimeout(r, ms));
}

function isSelectedFrameSelected(dbg, state) {
  const frame = dbg.selectors.getVisibleSelectedFrame(state);

  // Make sure the source text is completely loaded for the
  // source we are paused in.
  const sourceId = frame.location.sourceId;
  const source = dbg.selectors.getSelectedSource(state);

  if (!source) {
    return false;
  }

  const isLoaded = source.has("loadedState") && sourceUtils.isLoaded(source);
  if (!isLoaded) {
    return false;
  }

  return source.id == sourceId;
}

function createDebuggerContext(toolbox) {
  const panel = toolbox.getPanel("jsdebugger");
  const win = panel.panelWin;
  const { store, client, selectors, actions } = panel.getVarsForTests();

  return {
    actions: actions,
    selectors: selectors,
    getState: store.getState,
    store: store,
    client: client,
    toolbox: toolbox,
    win: win,
    panel: panel
  };
}

/**
 * Clear all the debugger related preferences.
 */
function clearDebuggerPreferences() {
  Services.prefs.clearUserPref("devtools.debugger.pause-on-exceptions");
  Services.prefs.clearUserPref("devtools.debugger.ignore-caught-exceptions");
  Services.prefs.clearUserPref("devtools.debugger.tabs");
  Services.prefs.clearUserPref("devtools.debugger.pending-selected-location");
  Services.prefs.clearUserPref("devtools.debugger.pending-breakpoints");
  Services.prefs.clearUserPref("devtools.debugger.expressions");
  Services.prefs.clearUserPref("devtools.debugger.call-stack-visible");
  Services.prefs.clearUserPref("devtools.debugger.scopes-visible");
}

/**
 * Intilializes the debugger.
 *
 * @memberof mochitest
 * @param {String} url
 * @return {Promise} dbg
 * @static
 */
async function initDebugger(url) {
  clearDebuggerPreferences();
  const toolbox = await openNewTabAndToolbox(EXAMPLE_URL + url, "jsdebugger");
  return createDebuggerContext(toolbox);
}

async function initPane(url, pane) {
  clearDebuggerPreferences();
  return openNewTabAndToolbox(EXAMPLE_URL + url, pane);
}

window.resumeTest = undefined;
/**
 * Pause the test and let you interact with the debugger.
 * The test can be resumed by invoking `resumeTest` in the console.
 *
 * @memberof mochitest
 * @static
 */
function pauseTest() {
  info("Test paused. Invoke resumeTest to continue.");
  return new Promise(resolve => (resumeTest = resolve));
}

// Actions
/**
 * Returns a source that matches the URL.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @param {String} url
 * @return {Object} source
 * @static
 */
function findSource(dbg, url, { silent } = { silent: false }) {
  if (typeof url !== "string") {
    // Support passing in a source object itelf all APIs that use this
    // function support both styles
    const source = url;
    return source;
  }

  const sources = dbg.selectors.getSources(dbg.getState());
  const source = sources.find(s => (s.get("url") || "").includes(url));

  if (!source) {
    if (silent) {
      return false;
    }

    throw new Error(`Unable to find source: ${url}`);
  }

  return source.toJS();
}

function sourceExists(dbg, url) {
  return !!findSource(dbg, url, { silent: true });
}

function waitForLoadedSource(dbg, url) {
  return waitForState(
    dbg,
    state => findSource(dbg, url, { silent: true }).loadedState == "loaded",
    "loaded source"
  );
}

function waitForLoadedSources(dbg) {
  return waitForState(
    dbg,
    state => {
      const sources = dbg.selectors
        .getSources(state)
        .valueSeq()
        .toJS();
      return !sources.some(source => source.loadedState == "loading");
    },
    "loaded source"
  );
}
/**
 * Selects the source.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @param {String} url
 * @param {Number} line
 * @return {Promise}
 * @static
 */
async function selectSource(dbg, url, line) {
  const source = findSource(dbg, url);
  await dbg.actions.selectLocation({ sourceId: source.id, line });
  return waitForSelectedSource(dbg, url);
}

function closeTab(dbg, url) {
  const source = findSource(dbg, url);
  return dbg.actions.closeTab(source.url);
}

/**
 * Steps over.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @return {Promise}
 * @static
 */
async function stepOver(dbg) {
  const pauseLine = getVisibleSelectedFrameLine(dbg);
  info(`Stepping over from ${pauseLine}`);
  await dbg.actions.stepOver();
  return waitForPaused(dbg);
}

/**
 * Steps in.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @return {Promise}
 * @static
 */
async function stepIn(dbg) {
  const pauseLine = getVisibleSelectedFrameLine(dbg);
  info(`Stepping in from ${pauseLine}`);
  await dbg.actions.stepIn();
  return waitForPaused(dbg);
}

/**
 * Steps out.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @return {Promise}
 * @static
 */
async function stepOut(dbg) {
  const pauseLine = getVisibleSelectedFrameLine(dbg);
  info(`Stepping out from ${pauseLine}`);
  await dbg.actions.stepOut();
  return waitForPaused(dbg);
}

/**
 * Resumes.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @return {Promise}
 * @static
 */
function resume(dbg) {
  const pauseLine = getVisibleSelectedFrameLine(dbg);
  info(`Resuming from ${pauseLine}`);
  return dbg.actions.resume();
}

function deleteExpression(dbg, input) {
  info(`Delete expression "${input}"`);
  return dbg.actions.deleteExpression({ input });
}

/**
 * Reloads the debuggee.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @param {Array} sources
 * @return {Promise}
 * @static
 */
async function reload(dbg, ...sources) {
  const navigated = waitForDispatch(dbg, "NAVIGATE");
  await dbg.client.reload();
  await navigated;
  return waitForSources(dbg, ...sources);
}

/**
 * Navigates the debuggee to another url.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @param {String} url
 * @param {Array} sources
 * @return {Promise}
 * @static
 */
async function navigate(dbg, url, ...sources) {
  info(`Navigating to ${url}`)
  const navigated =  waitForDispatch(dbg, "NAVIGATE");
  await dbg.client.navigate(url);
  await navigated;
  return waitForSources(dbg, ...sources);
}

/**
 * Adds a breakpoint to a source at line/col.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @param {String} source
 * @param {Number} line
 * @param {Number} col
 * @return {Promise}
 * @static
 */
function addBreakpoint(dbg, source, line, column) {
  source = findSource(dbg, source);
  const sourceId = source.id;
  dbg.actions.addBreakpoint({ sourceId, line, column });
  return waitForDispatch(dbg, "ADD_BREAKPOINT");
}

function disableBreakpoint(dbg, source, line, column) {
  source = findSource(dbg, source);
  const sourceId = source.id;
  dbg.actions.disableBreakpoint({ sourceId, line, column });
  return waitForDispatch(dbg, "DISABLE_BREAKPOINT");
}

async function loadAndAddBreakpoint(dbg, filename, line, column) {
  const { selectors: { getBreakpoint, getBreakpoints }, getState } = dbg;

  await waitForSources(dbg, filename);

  ok(true, "Original sources exist");
  const source = findSource(dbg, filename);

  await selectSource(dbg, source);

  // Test that breakpoint is not off by a line.
  await addBreakpoint(dbg, source, line);

  is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
  ok(
    getBreakpoint(getState(), { sourceId: source.id, line, column }),
    "Breakpoint has correct line"
  );

  return source;
}

async function invokeWithBreakpoint(dbg, fnName, filename, { line, column }, handler) {
  const { selectors: { getBreakpoints }, getState } = dbg;

  const source = await loadAndAddBreakpoint(dbg, filename, line, column);

  const invokeResult = invokeInTab(fnName);

  let invokeFailed = await Promise.race([
    waitForPaused(dbg),
    invokeResult.then(() => new Promise(() => {}), () => true)
  ]);

  if (invokeFailed) {
    return invokeResult;
  }

  assertPausedLocation(dbg);

  await removeBreakpoint(dbg, source.id, line, column);

  is(getBreakpoints(getState()).size, 0, "Breakpoint reverted");

  await handler(source);

  await resume(dbg);

  // If the invoke errored later somehow, capture here so the error is reported nicely.
  await invokeResult;
}

async function expandAllScopes(dbg) {
  const scopes = await waitForElement(dbg, "scopes");
  const scopeElements = scopes.querySelectorAll(
    `.tree-node[aria-level="1"][data-expandable="true"]:not([aria-expanded="true"])`
  );
  const indices = Array.from(scopeElements, el => {
    return Array.prototype.indexOf.call(el.parentNode.childNodes, el);
  }).reverse();

  for (const index of indices) {
    await toggleScopeNode(dbg, index + 1);
  }
}

async function assertScopes(dbg, items) {
  await expandAllScopes(dbg);

  for (const [i, val] of items.entries()) {
    if (Array.isArray(val)) {
      is(getScopeLabel(dbg, i + 1), val[0]);
      is(
        getScopeValue(dbg, i + 1),
        val[1],
        `"${val[0]}" has the expected "${val[1]}" value`
      );
    } else {
      is(getScopeLabel(dbg, i + 1), val);
    }
  }

  is(getScopeLabel(dbg, items.length + 1), "Window");
}

/**
 * Removes a breakpoint from a source at line/col.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @param {String} source
 * @param {Number} line
 * @param {Number} col
 * @return {Promise}
 * @static
 */
function removeBreakpoint(dbg, sourceId, line, column) {
  dbg.actions.removeBreakpoint({ sourceId, line, column });
  return waitForDispatch(dbg, "REMOVE_BREAKPOINT");
}

/**
 * Toggles the Pause on exceptions feature in the debugger.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @param {Boolean} pauseOnExceptions
 * @param {Boolean} ignoreCaughtExceptions
 * @return {Promise}
 * @static
 */
async function togglePauseOnExceptions(
  dbg,
  pauseOnExceptions,
  ignoreCaughtExceptions
) {
  const command = dbg.actions.pauseOnExceptions(
    pauseOnExceptions,
    ignoreCaughtExceptions
  );

  if (!isPaused(dbg)) {
    await waitForThreadEvents(dbg, "resumed");
  }

  return command;
}

function waitForActive(dbg) {
  return waitForState(dbg, state => !dbg.selectors.isPaused(state), "active");
}

// Helpers

/**
 * Invokes a global function in the debuggee tab.
 *
 * @memberof mochitest/helpers
 * @param {String} fnc The name of a global function on the content window to
 *                     call. This is applied to structured clones of the
 *                     remaining arguments to invokeInTab.
 * @param {Any} ...args Remaining args to serialize and pass to fnc.
 * @return {Promise}
 * @static
 */
function invokeInTab(fnc, ...args) {
  info(`Invoking in tab: ${fnc}(${args.map(uneval).join(",")})`);
  return ContentTask.spawn(gBrowser.selectedBrowser, { fnc, args }, function*({
    fnc,
    args
  }) {
    content.wrappedJSObject[fnc](...args);
  });
}

const isLinux = Services.appinfo.OS === "Linux";
const isMac = Services.appinfo.OS === "Darwin";
const cmdOrCtrl = isLinux ? { ctrlKey: true } : { metaKey: true };
const shiftOrAlt = isMac
  ? { accelKey: true, shiftKey: true }
  : { accelKey: true, altKey: true };

const cmdShift = isMac
  ? { accelKey: true, shiftKey: true, metaKey: true }
  : { accelKey: true, shiftKey: true, ctrlKey: true };

// On Mac, going to beginning/end only works with meta+left/right.  On
// Windows, it only works with home/end.  On Linux, apparently, either
// ctrl+left/right or home/end work.
const endKey = isMac
  ? { code: "VK_RIGHT", modifiers: cmdOrCtrl }
  : { code: "VK_END" };
const startKey = isMac
  ? { code: "VK_LEFT", modifiers: cmdOrCtrl }
  : { code: "VK_HOME" };

const keyMappings = {
  debugger: { code: "s", modifiers: shiftOrAlt },
  inspector: { code: "c", modifiers: shiftOrAlt },
  quickOpen: { code: "p", modifiers: cmdOrCtrl },
  quickOpenFunc: { code: "o", modifiers: cmdShift },
  quickOpenLine: { code: ":", modifiers: cmdOrCtrl },
  fileSearch: { code: "f", modifiers: cmdOrCtrl },
  Enter: { code: "VK_RETURN" },
  ShiftEnter: { code: "VK_RETURN", modifiers: shiftOrAlt },
  Up: { code: "VK_UP" },
  Down: { code: "VK_DOWN" },
  Right: { code: "VK_RIGHT" },
  Left: { code: "VK_LEFT" },
  End: endKey,
  Start: startKey,
  Tab: { code: "VK_TAB" },
  Escape: { code: "VK_ESCAPE" },
  Delete: { code: "VK_DELETE" },
  pauseKey: { code: "VK_F8" },
  resumeKey: { code: "VK_F8" },
  stepOverKey: { code: "VK_F10" },
  stepInKey: { code: "VK_F11", modifiers: { ctrlKey: isLinux } },
  stepOutKey: {
    code: "VK_F11",
    modifiers: { ctrlKey: isLinux, shiftKey: true }
  }
};

/**
 * Simulates a key press in the debugger window.
 *
 * @memberof mochitest/helpers
 * @param {Object} dbg
 * @param {String} keyName
 * @return {Promise}
 * @static
 */
function pressKey(dbg, keyName) {
  const keyEvent = keyMappings[keyName];

  const { code, modifiers } = keyEvent;
  return EventUtils.synthesizeKey(code, modifiers || {}, dbg.win);
}

function type(dbg, string) {
  string.split("").forEach(char => EventUtils.synthesizeKey(char, {}, dbg.win));
}

/*
 * Checks to see if the inner element is visible inside the editor.
 *
 * @memberof mochitest/helpers
 * @param {Object} dbg
 * @param {HTMLElement} inner element
 * @return {boolean}
 * @static
 */

function isVisibleInEditor(dbg, element) {
  return isVisible(findElement(dbg, "codeMirror"), element);
}

/*
 * Checks to see if the inner element is visible inside the
 * outer element.
 *
 * Note, the inner element does not need to be entirely visible,
 * it is possible for it to be somewhat clipped by the outer element's
 * bounding element or for it to span the entire length, starting before the
 * outer element and ending after.
 *
 * @memberof mochitest/helpers
 * @param {HTMLElement} outer element
 * @param {HTMLElement} inner element
 * @return {boolean}
 * @static
 */
function isVisible(outerEl, innerEl) {
  if (!innerEl || !outerEl) {
    return false;
  }

  const innerRect = innerEl.getBoundingClientRect();
  const outerRect = outerEl.getBoundingClientRect();

  const verticallyVisible =
    innerRect.top >= outerRect.top ||
    innerRect.bottom <= outerRect.bottom ||
    (innerRect.top < outerRect.top && innerRect.bottom > outerRect.bottom);

  const horizontallyVisible =
    innerRect.left >= outerRect.left ||
    innerRect.right <= outerRect.right ||
    (innerRect.left < outerRect.left && innerRect.right > outerRect.right);

  const visible = verticallyVisible && horizontallyVisible;
  return visible;
}

const selectors = {
  callStackHeader: ".call-stack-pane ._header",
  callStackBody: ".call-stack-pane .pane",
  expressionNode: i =>
    `.expressions-list .expression-container:nth-child(${i}) .object-label`,
  expressionValue: i =>
    `.expressions-list .expression-container:nth-child(${i}) .object-delimiter + *`,
  expressionClose: i =>
    `.expressions-list .expression-container:nth-child(${i}) .close`,
  expressionInput: '.expressions-list  input.input-expression',
  expressionNodes: ".expressions-list .tree-node",
  scopesHeader: ".scopes-pane ._header",
  breakpointItem: i => `.breakpoints-list .breakpoint:nth-of-type(${i})`,
  breakpointItems: `.breakpoints-list .breakpoint`,
  scopes: ".scopes-list",
  scopeNode: i => `.scopes-list .tree-node:nth-child(${i}) .object-label`,
  scopeValue: i =>
    `.scopes-list .tree-node:nth-child(${i}) .object-delimiter + *`,
  frame: i => `.frames ul li:nth-child(${i})`,
  frames: ".frames ul li",
  gutter: i => `.CodeMirror-code *:nth-child(${i}) .CodeMirror-linenumber`,
  menuitem: i => `menupopup menuitem:nth-child(${i})`,
  pauseOnExceptions: ".pause-exceptions",
  breakpoint: ".CodeMirror-code > .new-breakpoint",
  highlightLine: ".CodeMirror-code > .highlight-line",
  debugLine: ".new-debug-line",
  debugErrorLine: ".new-debug-line-error",
  codeMirror: ".CodeMirror",
  resume: ".resume.active",
  sourceTabs: ".source-tabs",
  stepOver: ".stepOver.active",
  stepOut: ".stepOut.active",
  stepIn: ".stepIn.active",
  replayPrevious: ".replay-previous.active",
  replayNext: ".replay-next.active",
  toggleBreakpoints: ".breakpoints-toggle",
  prettyPrintButton: ".source-footer .prettyPrint",
  sourceMapLink: ".source-footer .mapped-source",
  sourcesFooter: ".sources-panel .source-footer",
  editorFooter: ".editor-pane .source-footer",
  sourceNode: i => `.sources-list .tree-node:nth-child(${i}) .node`,
  sourceNodes: ".sources-list .tree-node",
  sourceDirectoryLabel: i => `.sources-list .tree-node:nth-child(${i}) .label`,
  resultItems: ".result-list .result-item",
  fileMatch: ".managed-tree .result",
  popup: ".popover",
  tooltip: ".tooltip",
  previewPopup: ".preview-popup",
  outlineItem: i =>
    `.outline-list__element:nth-child(${i}) .function-signature`,
  outlineItems: ".outline-list__element",
  conditionalPanelInput: ".conditional-breakpoint-panel input"
};

function getSelector(elementName, ...args) {
  let selector = selectors[elementName];
  if (!selector) {
    throw new Error(`The selector ${elementName} is not defined`);
  }

  if (typeof selector == "function") {
    selector = selector(...args);
  }

  return selector;
}

function findElement(dbg, elementName, ...args) {
  const selector = getSelector(elementName, ...args);
  return findElementWithSelector(dbg, selector);
}

function findElementWithSelector(dbg, selector) {
  return dbg.win.document.querySelector(selector);
}

function findAllElements(dbg, elementName, ...args) {
  const selector = getSelector(elementName, ...args);
  return dbg.win.document.querySelectorAll(selector);
}

/**
 * Simulates a mouse click in the debugger DOM.
 *
 * @memberof mochitest/helpers
 * @param {Object} dbg
 * @param {String} elementName
 * @param {Array} args
 * @return {Promise}
 * @static
 */
async function clickElement(dbg, elementName, ...args) {
  const selector = getSelector(elementName, ...args);
  const el = await waitForElementWithSelector(dbg, selector);

  el.scrollIntoView();

  return clickElementWithSelector(dbg, selector);
}

function clickElementWithSelector(dbg, selector) {
  EventUtils.synthesizeMouseAtCenter(
    findElementWithSelector(dbg, selector),
    {},
    dbg.win
  );
}

function dblClickElement(dbg, elementName, ...args) {
  const selector = getSelector(elementName, ...args);

  return EventUtils.synthesizeMouseAtCenter(
    findElementWithSelector(dbg, selector),
    { clickCount: 2 },
    dbg.win
  );
}

function rightClickElement(dbg, elementName, ...args) {
  const selector = getSelector(elementName, ...args);
  const doc = dbg.win.document;
  return EventUtils.synthesizeMouseAtCenter(
    doc.querySelector(selector),
    { type: "contextmenu" },
    dbg.win
  );
}

function selectMenuItem(dbg, index) {
  // the context menu is in the toolbox window
  const doc = dbg.toolbox.win.document;

  // there are several context menus, we want the one with the menu-api
  const popup = doc.querySelector('menupopup[menu-api="true"]');

  const item = popup.querySelector(`menuitem:nth-child(${index})`);
  return EventUtils.synthesizeMouseAtCenter(item, {}, dbg.toolbox.win);
}

/**
 * Toggles the debugger call stack accordian.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @return {Promise}
 * @static
 */
function toggleCallStack(dbg) {
  return findElement(dbg, "callStackHeader").click();
}

function toggleScopes(dbg) {
  return findElement(dbg, "scopesHeader").click();
}

function toggleExpressionNode(dbg, index) {
  return toggleObjectInspectorNode(findElement(dbg, "expressionNode", index));
}

function toggleScopeNode(dbg, index) {
  return toggleObjectInspectorNode(findElement(dbg, "scopeNode", index));
}

function getScopeLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}

function getScopeValue(dbg, index) {
  return findElement(dbg, "scopeValue", index).innerText;
}

function toggleObjectInspectorNode(node) {

  const objectInspector = node.closest(".object-inspector");
  const properties = objectInspector.querySelectorAll(".node").length;

  log(`Toggling node ${node.innerText}`)
  node.click();
  return waitUntil(
    () => objectInspector.querySelectorAll(".node").length !== properties
  );
}

function getCM(dbg) {
  const el = dbg.win.document.querySelector(".CodeMirror");
  return el.CodeMirror;
}

function getCoordsFromPosition(cm, { line, ch }) {
  return cm.charCoords({ line: ~~line, ch: ~~ch });
}

function hoverAtPos(dbg, { line, ch }) {
  info(`Hovering at ${line}, ${ch}`);
  const cm = getCM(dbg);

  // Ensure the line is visible with margin because the bar at the bottom of
  // the editor overlaps into what the editor things is its own space, blocking
  // the click event below.
  cm.scrollIntoView({ line: line - 1, ch }, 100);

  const coords = getCoordsFromPosition(cm, { line: line - 1, ch });
  const tokenEl = dbg.win.document.elementFromPoint(coords.left, coords.top);
  tokenEl.dispatchEvent(
    new MouseEvent("mouseover", {
      bubbles: true,
      cancelable: true,
      view: dbg.win
    })
  );
}

async function assertPreviewTextValue(dbg, { text, expression }) {
  const previewEl = await waitForElement(dbg, "previewPopup");

  is(previewEl.innerText, text, "Preview text shown to user");

  const preview = dbg.selectors.getPreview(dbg.getState());
  is(preview.updating, false, "Preview.updating");
  is(preview.expression, expression, "Preview.expression");
}

async function assertPreviewTooltip(dbg, { result, expression }) {
  const previewEl = await waitForElement(dbg, "tooltip");
  is(previewEl.innerText, result, "Preview text shown to user");

  const preview = dbg.selectors.getPreview(dbg.getState());
  is(`${preview.result}`, result, "Preview.result");
  is(preview.updating, false, "Preview.updating");
  is(preview.expression, expression, "Preview.expression");
}

async function assertPreviewPopup(dbg, { field, value, expression }) {
  const previewEl = await waitForElement(dbg, "popup");
  const preview = dbg.selectors.getPreview(dbg.getState());

  const properties =
    preview.result.preview.ownProperties || preview.result.preview.items;
  const property = properties[field];

  is(`${property.value || property}`, value, "Preview.result");
  is(preview.updating, false, "Preview.updating");
  is(preview.expression, expression, "Preview.expression");
}

async function assertPreviews(dbg, previews) {
  for (const { line, column, expression, result, fields } of previews) {
    hoverAtPos(dbg, { line, ch: column - 1 });

    if (fields && result) {
      throw new Error("Invalid test fixture");
    }

    if (fields) {
      for (const [field, value] of fields) {
        await assertPreviewPopup(dbg, { expression, field, value });
      }
    } else {
      await assertPreviewTextValue(dbg, { expression, text: result });
    }

    // Move to column 0 after to make sure that the preview created by this
    // test does not affect later attempts to hover and preview.
    hoverAtPos(dbg, { line: line, ch: 0 });
  }
}

// NOTE: still experimental, the screenshots might not be exactly correct
async function takeScreenshot(dbg) {
  let canvas = dbg.win.document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "html:canvas"
  );
  let context = canvas.getContext("2d");
  canvas.width = dbg.win.innerWidth;
  canvas.height = dbg.win.innerHeight;
  context.drawWindow(dbg.win, 0, 0, canvas.width, canvas.height, "white");
  await waitForTime(1000);
  dump(`[SCREENSHOT] ${canvas.toDataURL()}\n`);
}

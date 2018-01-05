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
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this
);
var { Toolbox } = require("devtools/client/framework/toolbox");
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
      if (predicate(dbg.store.getState())) {
        info(`Finished waiting for state change: ${msg || ""}`);
        unsubscribe();
        resolve();
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
function waitForSources(dbg, ...sources) {
  if (sources.length === 0) {
    return Promise.resolve();
  }

  info(`Waiting on sources: ${sources.join(", ")}`);
  const { selectors: { getSources }, store } = dbg;
  return Promise.all(
    sources.map(url => {
      function sourceExists(state) {
        return getSources(state).some(s => {
          return (s.get("url") || "").includes(url);
        });
      }

      if (!sourceExists(store.getState())) {
        return waitForState(dbg, sourceExists, `source ${url}`);
      }
    })
  );
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
  return waitForState(dbg, state => {
    const sources = dbg.selectors.getSources(state);
    return sources.find(s => (s.get("url") || "").includes(url));
  });
}

async function waitForElement(dbg, selector) {
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
      if (newSource.id != source.get("id")) {
        return false;
      }

      // wait for async work to be done
      return dbg.selectors.hasSymbols(state, source.toJS());
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
  const { selectors: { getSelectedSource, getPause }, getState } = dbg;

  ok(isTopFrameSelected(dbg, getState()), "top frame's source is selected");

  // Check the pause location
  const pause = getPause(getState());
  const pauseLine = pause && pause.frame && pause.frame.location.line;
  assertDebugLine(dbg, pauseLine);

  ok(isVisibleInEditor(dbg, getCM(dbg).display.gutters), "gutter is visible");
}

function assertDebugLine(dbg, line) {
  // Check the debug line
  const lineInfo = getCM(dbg).lineInfo(line - 1);
  const source = dbg.selectors.getSelectedSource(dbg.getState());
  if (source && source.get("loadedState") == "loading") {
    const url = source.get("url");
    ok(
      false,
      `Looks like the source ${
        url
      } is still loading. Try adding waitForLoadedSource in the test.`
    );
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
  const { selectors: { getSelectedSource, getPause }, getState } = dbg;
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
  const { selectors: { getPause }, getState } = dbg;
  return !!getPause(getState());
}

async function waitForLoadedObjects(dbg) {
  const { hasLoadingObjects } = dbg.selectors;
  return waitForState(
    dbg,
    state => !hasLoadingObjects(state),
    "loaded objects"
  );
}
/**
 * Waits for the debugger to be fully paused.
 *
 * @memberof mochitest/waits
 * @param {Object} dbg
 * @static
 */
async function waitForPaused(dbg) {
  const { getSelectedScope, hasLoadingObjects } = dbg.selectors;

  return waitForState(
    dbg,
    state => {
      const paused = isPaused(dbg);
      const scope = !!getSelectedScope(state);
      const loaded = !hasLoadingObjects(state);
      return paused && scope && loaded;
    },
    "paused"
  );
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

/**
 * Waits for the debugger to be fully paused.
 *
 * @memberof mochitest/waits
 * @param {Object} dbg
 * @static
 */
async function waitForMappedScopes(dbg) {
  await waitForState(
    dbg,
    state => {
      const scopes = dbg.selectors.getScopes(state);
      return scopes && scopes.sourceBindings;
    },
    "mapped scopes"
  );
}

function isTopFrameSelected(dbg, state) {
  const pause = dbg.selectors.getPause(state);

  // Make sure we have the paused state.
  if (!pause) {
    return false;
  }

  // Make sure the source text is completely loaded for the
  // source we are paused in.
  const sourceId = pause.frame && pause.frame.location.sourceId;
  const source = dbg.selectors.getSelectedSource(state);

  if (!source) {
    return false;
  }

  const isLoaded = source.has("loadedState") && sourceUtils.isLoaded(source);
  if (!isLoaded) {
    return false;
  }

  return source.get("id") == sourceId;
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
function selectSource(dbg, url, line) {
  info(`Selecting source: ${url}`);
  const source = findSource(dbg, url);
  return dbg.actions.selectLocation({ sourceId: source.id, line });
}

function closeTab(dbg, url) {
  info(`Closing tab: ${url}`);
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
  info("Stepping over");
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
  info("Stepping in");
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
  info("Stepping out");
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
  info("Resuming");
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
function reload(dbg, ...sources) {
  return dbg.client
    .reload()
    .then(() => waitForSources(dbg, ...sources), "reloaded");
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
function navigate(dbg, url, ...sources) {
  dbg.client.navigate(url);
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
function addBreakpoint(dbg, source, line, col) {
  source = findSource(dbg, source);
  const sourceId = source.id;
  dbg.actions.addBreakpoint({ sourceId, line, col });
  return waitForDispatch(dbg, "ADD_BREAKPOINT");
}

function disableBreakpoint(dbg, source, line, col) {
  source = findSource(dbg, source);
  const sourceId = source.id;
  dbg.actions.disableBreakpoint({ sourceId, line, col });
  return waitForDispatch(dbg, "DISABLE_BREAKPOINT");
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
function removeBreakpoint(dbg, sourceId, line, col) {
  return dbg.actions.removeBreakpoint({ sourceId, line, col });
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
    await waitForLoadedObjects(dbg);
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
    content.wrappedJSObject[fnc](...args); // eslint-disable-line mozilla/no-cpows-in-tests, max-len
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
    `.expressions-list .expression-container:nth-child(${
      i
    }) .object-delimiter + *`,
  expressionClose: i =>
    `.expressions-list .expression-container:nth-child(${i}) .close`,
  expressionNodes: ".expressions-list .tree-node",
  scopesHeader: ".scopes-pane ._header",
  breakpointItem: i => `.breakpoints-list .breakpoint:nth-child(${i})`,
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
  toggleBreakpoints: ".breakpoints-toggle",
  prettyPrintButton: ".source-footer .prettyPrint",
  sourceMapLink: ".source-footer .mapped-source",
  sourcesFooter: ".sources-panel .source-footer",
  editorFooter: ".editor-pane .source-footer",
  sourceNode: i => `.sources-list .tree-node:nth-child(${i})`,
  sourceNodes: ".sources-list .tree-node",
  sourceArrow: i => `.sources-list .tree-node:nth-child(${i}) .arrow`,
  resultItems: ".result-list .result-item",
  fileMatch: ".managed-tree .result",
  popup: ".popover",
  tooltip: ".tooltip",
  outlineItem: i => `.outline-list__element:nth-child(${i})`,
  outlineItems: ".outline-list__element"
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
  const el = await waitForElement(dbg, selector);

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

function getCM(dbg) {
  const el = dbg.win.document.querySelector(".CodeMirror");
  return el.CodeMirror;
}

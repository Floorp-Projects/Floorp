/* eslint-disable no-unused-vars */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Helper methods to drive with the debugger during mochitests. This file can be safely
 * required from other panel test files.
 */

// Import helpers for the new debugger
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/helpers/context.js",
  this
);

var { Toolbox } = require("devtools/client/framework/toolbox");
var { Task } = require("devtools/shared/task");
var asyncStorage = require("devtools/shared/async-storage");

const {
  getSelectedLocation
} = require("devtools/client/debugger/src/utils/selected-location");

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
// then `waitForDispatch` because it doesn't wait for async actions
// to be done/errored. Use this if you want to listen for the "start"
// action of an async operation (somewhat rare).
function waitForNextDispatch(store, actionType) {
  return new Promise(resolve => {
    store.dispatch({
      // Normally we would use `services.WAIT_UNTIL`, but use the
      // internal name here so tests aren't forced to always pass it
      // in
      type: "@@service/waitUntil",
      predicate: action => action.type === actionType,
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
function waitForDispatch(dbg, actionType, eventRepeat = 1) {
  let count = 0;
  return new Promise(resolve => {
    dbg.store.dispatch({
      // Normally we would use `services.WAIT_UNTIL`, but use the
      // internal name here so tests aren't forced to always pass it
      // in
      type: "@@service/waitUntil",
      predicate: action => {
        const isDone =
          !action.status ||
          action.status === "done" ||
          action.status === "error";

        if (action.type === actionType && isDone && ++count == eventRepeat) {
          return true;
        }
      },
      run: (dispatch, getState, action) => {
        resolve(action);
      }
    });
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
 * Waits for `predicate()` to be true. `state` is the redux app state.
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
      info(`Finished waiting for state change: ${msg || ""}`);
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
    state => findSource(dbg, url, { silent: true }),
    "source exists"
  );
}

async function waitForElement(dbg, name, ...args) {
  await waitUntil(() => findElement(dbg, name, ...args));
  return findElement(dbg, name, ...args);
}

async function waitForAllElements(dbg, name, count = 1) {
  await waitUntil(() => findAllElements(dbg, name).length >= count);
  return findAllElements(dbg, name);
}

async function waitForElementWithSelector(dbg, selector) {
  await waitUntil(() => findElementWithSelector(dbg, selector));
  return findElementWithSelector(dbg, selector);
}

function assertClass(el, className, exists = true) {
  if (exists) {
    ok(el.classList.contains(className), `${className} class exists`);
  } else {
    ok(!el.classList.contains(className), `${className} class does not exist`);
  }
}

function waitForSelectedLocation(dbg, line) {
  return waitForState(dbg, state => {
    const location = dbg.selectors.getSelectedLocation();
    return location && location.line == line;
  });
}

function waitForSelectedSource(dbg, url) {
  const {
    getSelectedSourceWithContent,
    hasSymbols,
    getBreakableLines
  } = dbg.selectors;

  return waitForState(
    dbg,
    state => {
      const { source, content } = getSelectedSourceWithContent() || {};
      if (!content) {
        return false;
      }

      if (!url) {
        return true;
      }

      const newSource = findSource(dbg, url, { silent: true });
      if (newSource.id != source.id) {
        return false;
      }

      return hasSymbols(source) && getBreakableLines(source.id);
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
 * Assert that the debugger is currently paused.
 * @memberof mochitest/asserts
 * @static
 */
function assertPaused(dbg) {
  ok(isPaused(dbg), "client is paused");
}

function assertEmptyLines(dbg, lines) {
  const sourceId = dbg.selectors.getSelectedSourceId();
  const breakableLines = dbg.selectors.getBreakableLines(sourceId);
  ok(
    lines.every(line => !breakableLines.includes(line)),
    "empty lines should match"
  );
}

function getVisibleSelectedFrameLine(dbg) {
  const {
    selectors: { getVisibleSelectedFrame }
  } = dbg;
  const frame = getVisibleSelectedFrame();
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
  ok(isSelectedFrameSelected(dbg), "top frame's source is selected");

  // Check the pause location
  const pauseLine = getVisibleSelectedFrameLine(dbg);
  assertDebugLine(dbg, pauseLine);

  ok(isVisibleInEditor(dbg, getCM(dbg).display.gutters), "gutter is visible");
}

function assertDebugLine(dbg, line) {
  // Check the debug line
  const lineInfo = getCM(dbg).lineInfo(line - 1);
  const { source, content } =
    dbg.selectors.getSelectedSourceWithContent() || {};
  if (source && !content) {
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
    const classMatch =
      markedSpans.filter(
        span =>
          span.marker.className &&
          span.marker.className.includes("debug-expression")
      ).length > 0;

    ok(classMatch, "expression is highlighted as paused");
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
  source = findSource(dbg, source);

  // Check the selected source
  is(
    dbg.selectors.getSelectedSource().url,
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

  const cm = getCM(dbg);
  const lineInfo = cm.lineInfo(line - 1);
  ok(lineInfo.wrapClass.includes("highlight-line"), "Line is highlighted");
}

/**
 * Returns boolean for whether the debugger is paused.
 *
 * @memberof mochitest/asserts
 * @param {Object} dbg
 * @static
 */
function isPaused(dbg) {
  return dbg.selectors.getIsPaused(dbg.selectors.getCurrentThread());
}

// Make sure the debugger is paused at a certain source ID and line.
function assertPausedAtSourceAndLine(dbg, expectedSourceId, expectedLine) {
  assertPaused(dbg);

  const frames = dbg.selectors.getCurrentThreadFrames();
  ok(frames.length >= 1, "Got at least one frame");
  const { sourceId, line } = frames[0].location;
  ok(sourceId == expectedSourceId, "Frame has correct source");
  ok(line == expectedLine, "Frame has correct line");
}

// Get any workers associated with the debugger.
async function getWorkers(dbg) {
  await dbg.actions.updateWorkers();
  return dbg.selectors.getWorkers();
}

async function waitForLoadedScopes(dbg) {
  const scopes = await waitForElement(dbg, "scopes");
  // Since scopes auto-expand, we can assume they are loaded when there is a tree node
  // with the aria-level attribute equal to "2".
  await waitUntil(() => scopes.querySelector('.tree-node[aria-level="2"]'));
}

function waitForBreakpointCount(dbg, count) {
  return waitForState(
    dbg,
    state => dbg.selectors.getBreakpointCount() == count
  );
}

function waitForBreakpoint(dbg, url, line) {
  return waitForState(dbg, () => findBreakpoint(dbg, url, line));
}

function waitForBreakpointRemoved(dbg, url, line) {
  return waitForState(dbg, () => !findBreakpoint(dbg, url, line));
}

/**
 * Waits for the debugger to be fully paused.
 *
 * @memberof mochitest/waits
 * @param {Object} dbg
 * @static
 */
async function waitForPaused(dbg, url) {
  const { getSelectedScope, getCurrentThread } = dbg.selectors;

  await waitForState(
    dbg,
    state => isPaused(dbg) && !!getSelectedScope(getCurrentThread()),
    "paused"
  );

  await waitForLoadedScopes(dbg);
  await waitForSelectedSource(dbg, url);
}

function waitForCondition(dbg, condition) {
  return waitForState(dbg, state =>
    dbg.selectors
      .getBreakpointsList()
      .find(bp => bp.options.condition == condition)
  );
}

function waitForLog(dbg, logValue) {
  return waitForState(dbg, state =>
    dbg.selectors
      .getBreakpointsList()
      .find(bp => bp.options.logValue == logValue)
  );
}

async function waitForPausedThread(dbg, thread) {
  return waitForState(dbg, state => dbg.selectors.getIsPaused(thread));
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
  const frame = dbg.selectors.getVisibleSelectedFrame();

  // Make sure the source text is completely loaded for the
  // source we are paused in.
  const sourceId = frame.location.sourceId;
  const { source, content } =
    dbg.selectors.getSelectedSourceWithContent() || {};

  if (!source) {
    return false;
  }

  if (!content) {
    return false;
  }

  return source.id == sourceId;
}

/**
 * Clear all the debugger related preferences.
 */
async function clearDebuggerPreferences() {
  asyncStorage.clear();
  Services.prefs.clearUserPref("devtools.recordreplay.enabled");
  Services.prefs.clearUserPref("devtools.debugger.pause-on-exceptions");
  Services.prefs.clearUserPref("devtools.debugger.pause-on-caught-exceptions");
  Services.prefs.clearUserPref("devtools.debugger.ignore-caught-exceptions");
  Services.prefs.clearUserPref("devtools.debugger.pending-selected-location");
  Services.prefs.clearUserPref("devtools.debugger.expressions");
  Services.prefs.clearUserPref("devtools.debugger.call-stack-visible");
  Services.prefs.clearUserPref("devtools.debugger.scopes-visible");
  Services.prefs.clearUserPref("devtools.debugger.skip-pausing");
  Services.prefs.clearUserPref("devtools.debugger.map-scopes-enabled");
  await pushPref("devtools.debugger.log-actions", true);
}

/**
 * Intilializes the debugger.
 *
 * @memberof mochitest
 * @param {String} url
 * @return {Promise} dbg
 * @static
 */
async function initDebugger(url, ...sources) {
  await clearDebuggerPreferences();
  const toolbox = await openNewTabAndToolbox(EXAMPLE_URL + url, "jsdebugger");
  const dbg = createDebuggerContext(toolbox);
  dbg.client.waitForWorkers(false);

  await waitForSources(dbg, ...sources);
  return dbg;
}

async function initPane(url, pane) {
  await clearDebuggerPreferences();
  return openNewTabAndToolbox(EXAMPLE_URL + url, pane);
}

window.resumeTest = undefined;
registerCleanupFunction(() => {
  delete window.resumeTest;
});

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

  const sources = dbg.selectors.getSourceList();
  const source = sources.find(s => (s.url || "").includes(url));

  if (!source) {
    if (silent) {
      return false;
    }

    throw new Error(`Unable to find source: ${url}`);
  }

  return source;
}

function findSourceContent(dbg, url, opts) {
  const source = findSource(dbg, url, opts);

  if (!source) {
    return null;
  }

  const content = dbg.selectors.getSourceContent(source.id);

  if (!content) {
    return null;
  }

  if (content.state !== "fulfilled") {
    throw new Error("Expected loaded source, got" + content.value);
  }

  return content.value;
}

function sourceExists(dbg, url) {
  return !!findSource(dbg, url, { silent: true });
}

function waitForLoadedSource(dbg, url) {
  return waitForState(
    dbg,
    state => {
      const source = findSource(dbg, url, { silent: true });
      return source && dbg.selectors.getSourceContent(source.id);
    },
    "loaded source"
  );
}

function waitForLoadedSources(dbg) {
  return waitForState(
    dbg,
    state => {
      const sources = dbg.selectors.getSourceList();
      return sources.every(
        source => !!dbg.selectors.getSourceContent(source.id)
      );
    },
    "loaded source"
  );
}

function getContext(dbg) {
  return dbg.selectors.getContext();
}

function getThreadContext(dbg) {
  return dbg.selectors.getThreadContext();
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
  await dbg.actions.selectLocation(
    getContext(dbg),
    { sourceId: source.id, line },
    { keepContext: false }
  );
  return waitForSelectedSource(dbg, url);
}

async function closeTab(dbg, url) {
  await dbg.actions.closeTab(getContext(dbg), findSource(dbg, url));
}

function countTabs(dbg) {
  return findElement(dbg, "sourceTabs").children.length;
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
  await dbg.actions.stepOver(getThreadContext(dbg));
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
  await dbg.actions.stepIn(getThreadContext(dbg));
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
  await dbg.actions.stepOut(getThreadContext(dbg));
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
  return dbg.actions.resume(getThreadContext(dbg));
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
  info(`Navigating to ${url}`);
  const navigated = waitForDispatch(dbg, "NAVIGATE");
  await dbg.client.navigate(url);
  await navigated;
  return waitForSources(dbg, ...sources);
}

function getFirstBreakpointColumn(dbg, { line, sourceId }) {
  const { getSource, getFirstBreakpointPosition } = dbg.selectors;
  const source = getSource(sourceId);
  const position = getFirstBreakpointPosition({
    line,
    sourceId
  });

  return getSelectedLocation(position, source).column;
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
async function addBreakpoint(dbg, source, line, column, options) {
  source = findSource(dbg, source);
  const sourceId = source.id;
  const bpCount = dbg.selectors.getBreakpointCount();
  await dbg.actions.addBreakpoint(
    getContext(dbg),
    { sourceId, line, column },
    options
  );
  is(
    dbg.selectors.getBreakpointCount(),
    bpCount + 1,
    "a new breakpoint was created"
  );
}

function disableBreakpoint(dbg, source, line, column) {
  column =
    column || getFirstBreakpointColumn(dbg, { line, sourceId: source.id });
  const location = { sourceId: source.id, sourceUrl: source.url, line, column };
  const bp = dbg.selectors.getBreakpointForLocation(location);
  return dbg.actions.disableBreakpoint(getContext(dbg), bp);
}

function setBreakpointOptions(dbg, source, line, column, options) {
  source = findSource(dbg, source);
  const sourceId = source.id;
  column = column || getFirstBreakpointColumn(dbg, { line, sourceId });
  return dbg.actions.setBreakpointOptions(
    getContext(dbg),
    { sourceId, line, column },
    options
  );
}

function findBreakpoint(dbg, url, line) {
  const source = findSource(dbg, url);
  return dbg.selectors.getBreakpointsForSource(source.id, line)[0];
}

async function loadAndAddBreakpoint(dbg, filename, line, column) {
  const {
    selectors: { getBreakpoint, getBreakpointCount, getBreakpointsMap }
  } = dbg;

  await waitForSources(dbg, filename);

  ok(true, "Original sources exist");
  const source = findSource(dbg, filename);

  await selectSource(dbg, source);

  // Test that breakpoint is not off by a line.
  await addBreakpoint(dbg, source, line, column);

  is(getBreakpointCount(), 1, "One breakpoint exists");
  if (!getBreakpoint({ sourceId: source.id, line, column })) {
    const breakpoints = getBreakpointsMap();
    const id = Object.keys(breakpoints).pop();
    const loc = breakpoints[id].location;
    ok(
      false,
      `Breakpoint has correct line ${line}, column ${column}, but was line ${
        loc.line
      } column ${loc.column}`
    );
  }

  return source;
}

async function invokeWithBreakpoint(
  dbg,
  fnName,
  filename,
  { line, column },
  handler
) {
  const source = await loadAndAddBreakpoint(dbg, filename, line, column);

  const invokeResult = invokeInTab(fnName);

  const invokeFailed = await Promise.race([
    waitForPaused(dbg),
    invokeResult.then(() => new Promise(() => {}), () => true)
  ]);

  if (invokeFailed) {
    return invokeResult;
  }

  assertPausedLocation(dbg);

  await removeBreakpoint(dbg, source.id, line, column);

  is(dbg.selectors.getBreakpointCount(), 0, "Breakpoint reverted");

  await handler(source);

  await resume(dbg);

  // eslint-disable-next-line max-len
  // If the invoke errored later somehow, capture here so the error is reported nicely.
  await invokeResult;
}

function prettyPrint(dbg) {
  const sourceId = dbg.selectors.getSelectedSourceId();
  return dbg.actions.togglePrettyPrint(getContext(dbg), sourceId);
}

async function expandAllScopes(dbg) {
  const scopes = await waitForElement(dbg, "scopes");
  const scopeElements = scopes.querySelectorAll(
    '.tree-node[aria-level="1"][data-expandable="true"]:not([aria-expanded="true"])'
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
  const source = dbg.selectors.getSource(sourceId);
  column = column || getFirstBreakpointColumn(dbg, { line, sourceId });
  const location = { sourceId, sourceUrl: source.url, line, column };
  const bp = dbg.selectors.getBreakpointForLocation(location);
  return dbg.actions.removeBreakpoint(getContext(dbg), bp);
}

/**
 * Toggles the Pause on exceptions feature in the debugger.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @param {Boolean} pauseOnExceptions
 * @param {Boolean} pauseOnCaughtExceptions
 * @return {Promise}
 * @static
 */
async function togglePauseOnExceptions(
  dbg,
  pauseOnExceptions,
  pauseOnCaughtExceptions
) {
  return dbg.actions.pauseOnExceptions(
    pauseOnExceptions,
    pauseOnCaughtExceptions
  );
}

function waitForActive(dbg) {
  const {
    selectors: { getIsPaused, getCurrentThread }
  } = dbg;
  return waitForState(dbg, state => !getIsPaused(getCurrentThread()), "active");
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
    return content.wrappedJSObject[fnc](...args);
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
  close: { code: "w", modifiers: cmdOrCtrl },
  debugger: { code: "s", modifiers: shiftOrAlt },
  inspector: { code: "c", modifiers: shiftOrAlt },
  quickOpen: { code: "p", modifiers: cmdOrCtrl },
  quickOpenFunc: { code: "o", modifiers: cmdShift },
  quickOpenLine: { code: ":", modifiers: cmdOrCtrl },
  fileSearch: { code: "f", modifiers: cmdOrCtrl },
  fileSearchNext: { code: "g", modifiers: cmdOrCtrl },
  fileSearchPrev: { code: "g", modifiers: cmdShift },
  Enter: { code: "VK_RETURN" },
  ShiftEnter: { code: "VK_RETURN", modifiers: shiftOrAlt },
  AltEnter: {
    code: "VK_RETURN",
    modifiers: { altKey: true }
  },
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

async function getEditorLineGutter(dbg, line) {
  const lineEl = await getEditorLineEl(dbg, line);
  return lineEl.firstChild;
}

async function getEditorLineEl(dbg, line) {
  let el = await codeMirrorGutterElement(dbg, line);
  while (el && !el.matches(".CodeMirror-code > div")) {
    el = el.parentElement;
  }

  return el;
}

async function assertEditorBreakpoint(dbg, line, shouldExist) {
  const el = await getEditorLineEl(dbg, line);

  const exists = !!el.querySelector(".new-breakpoint");
  const existsStr = shouldExist ? "exists" : "does not exist";
  ok(exists === shouldExist, `Breakpoint ${existsStr} on line ${line}`);
}

function assertBreakpointSnippet(dbg, index, snippet) {
  const actualSnippet = findElement(dbg, "breakpointLabel", 2).innerText;
  is(snippet, actualSnippet, `Breakpoint ${index} snippet`);
}

const selectors = {
  callStackHeader: ".call-stack-pane ._header",
  callStackBody: ".call-stack-pane .pane",
  expressionNode: i =>
    `.expressions-list .expression-container:nth-child(${i}) .object-label`,
  expressionValue: i =>
    // eslint-disable-next-line max-len
    `.expressions-list .expression-container:nth-child(${i}) .object-delimiter + *`,
  expressionClose: i =>
    `.expressions-list .expression-container:nth-child(${i}) .close`,
  expressionInput: ".expressions-list  input.input-expression",
  expressionNodes: ".expressions-list .tree-node",
  expressionPlus: ".watch-expressions-pane button.plus",
  scopesHeader: ".scopes-pane ._header",
  breakpointItem: i => `.breakpoints-list div:nth-of-type(${i})`,
  breakpointLabel: i => `${selectors.breakpointItem(i)} .breakpoint-label`,
  breakpointItems: ".breakpoints-list .breakpoint",
  breakpointContextMenu: {
    disableSelf: "#node-menu-disable-self",
    disableAll: "#node-menu-disable-all",
    disableOthers: "#node-menu-disable-others",
    enableSelf: "#node-menu-enable-self",
    enableOthers: "#node-menu-enable-others",
    remove: "#node-menu-delete-self",
    removeOthers: "#node-menu-delete-other",
    removeCondition: "#node-menu-remove-condition"
  },
  columnBreakpoints: ".column-breakpoint",
  scopes: ".scopes-list",
  scopeNode: i => `.scopes-list .tree-node:nth-child(${i}) .object-label`,
  scopeValue: i =>
    `.scopes-list .tree-node:nth-child(${i}) .object-delimiter + *`,
  mapScopesCheckbox: ".map-scopes-header input",
  frame: i => `.frames [role="list"] [role="listitem"]:nth-child(${i})`,
  frames: '.frames [role="list"] [role="listitem"]',
  gutter: i => `.CodeMirror-code *:nth-child(${i}) .CodeMirror-linenumber`,
  addConditionItem:
    "#node-menu-add-condition, #node-menu-add-conditional-breakpoint",
  editConditionItem:
    "#node-menu-edit-condition, #node-menu-edit-conditional-breakpoint",
  addLogItem: "#node-menu-add-log-point",
  editLogItem: "#node-menu-edit-log-point",
  disableItem: "#node-menu-disable-breakpoint",
  menuitem: i => `menupopup menuitem:nth-child(${i})`,
  pauseOnExceptions: ".pause-exceptions",
  breakpoint: ".CodeMirror-code > .new-breakpoint",
  highlightLine: ".CodeMirror-code > .highlight-line",
  debugLine: ".new-debug-line",
  debugErrorLine: ".new-debug-line-error",
  codeMirror: ".CodeMirror",
  resume: ".resume.active",
  pause: ".pause.active",
  sourceTabs: ".source-tabs",
  activeTab: ".source-tab.active",
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
  threadSourceTree: i => `.threads-list .sources-pane:nth-child(${i})`,
  threadSourceTreeHeader: i =>
    `${selectors.threadSourceTree(i)} .thread-header`,
  threadSourceTreeSourceNode: (i, j) =>
    `${selectors.threadSourceTree(i)} .tree-node:nth-child(${j}) .node`,
  sourceDirectoryLabel: i => `.sources-list .tree-node:nth-child(${i}) .label`,
  resultItems: ".result-list .result-item",
  fileMatch: ".project-text-search .line-value",
  popup: ".popover",
  tooltip: ".tooltip",
  previewPopup: ".preview-popup",
  openInspector: "button.open-inspector",
  outlineItem: i =>
    `.outline-list__element:nth-child(${i}) .function-signature`,
  outlineItems: ".outline-list__element",
  conditionalPanel: ".conditional-breakpoint-panel",
  conditionalPanelInput: ".conditional-breakpoint-panel textarea",
  conditionalBreakpointInSecPane: ".breakpoint.is-conditional",
  logPointPanel: ".conditional-breakpoint-panel.log-point",
  logPointInSecPane: ".breakpoint.is-log",
  searchField: ".search-field",
  blackbox: ".action.black-box",
  projectSearchCollapsed: ".project-text-search .arrow:not(.expanded)",
  projectSerchExpandedResults: ".project-text-search .result",
  threadsPaneItems: ".workers-pane .worker",
  threadsPaneItem: i => `.workers-pane .worker:nth-child(${i})`,
  threadsPaneItemPause: i => `${selectors.threadsPaneItem(i)} .pause-badge`,
  CodeMirrorLines: ".CodeMirror-lines"
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
  return findAllElementsWithSelector(dbg, selector);
}

function findAllElementsWithSelector(dbg, selector) {
  return dbg.win.document.querySelectorAll(selector);
}

function getSourceNodeLabel(dbg, index) {
  return findElement(dbg, "sourceNode", index)
    .textContent.trim()
    .replace(/^[\s\u200b]*/g, "");
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
  clickDOMElement(dbg, findElementWithSelector(dbg, selector));
}

function clickDOMElement(dbg, element) {
  EventUtils.synthesizeMouseAtCenter(element, {}, dbg.win);
}

function dblClickElement(dbg, elementName, ...args) {
  const selector = getSelector(elementName, ...args);

  return EventUtils.synthesizeMouseAtCenter(
    findElementWithSelector(dbg, selector),
    { clickCount: 2 },
    dbg.win
  );
}

function clickElementWithOptions(dbg, elementName, options, ...args) {
  const selector = getSelector(elementName, ...args);
  const el = findElementWithSelector(dbg, selector);
  el.scrollIntoView();

  return EventUtils.synthesizeMouseAtCenter(el, options, dbg.win);
}

function altClickElement(dbg, elementName, ...args) {
  return clickElementWithOptions(dbg, elementName, { altKey: true }, ...args);
}

function shiftClickElement(dbg, elementName, ...args) {
  return clickElementWithOptions(dbg, elementName, { shiftKey: true }, ...args);
}

function rightClickElement(dbg, elementName, ...args) {
  const selector = getSelector(elementName, ...args);
  const doc = dbg.win.document;
  return rightClickEl(dbg, doc.querySelector(selector));
}

function rightClickEl(dbg, el) {
  const doc = dbg.win.document;
  EventUtils.synthesizeMouseAtCenter(el, { type: "contextmenu" }, dbg.win);
}

async function clickGutter(dbg, line) {
  const el = await codeMirrorGutterElement(dbg, line);
  clickDOMElement(dbg, el);
}

function selectContextMenuItem(dbg, selector) {
  // the context menu is in the toolbox window
  const doc = dbg.toolbox.topDoc;

  // there are several context menus, we want the one with the menu-api
  const popup = doc.querySelector('menupopup[menu-api="true"]');

  const item = popup.querySelector(selector);
  return EventUtils.synthesizeMouseAtCenter(item, {}, dbg.toolbox.topWindow);
}

async function typeInPanel(dbg, text) {
  await waitForElement(dbg, "conditionalPanelInput");

  // Position cursor reliably at the end of the text.
  pressKey(dbg, "End");
  type(dbg, text);
  pressKey(dbg, "Enter");
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

  log(`Toggling node ${node.innerText}`);
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

async function waitForScrolling(codeMirror) {
  return new Promise(resolve => {
    codeMirror.on("scroll", resolve);
    setTimeout(resolve, 500);
  });
}

async function codeMirrorGutterElement(dbg, line) {
  info(`CodeMirror line ${line}`);
  const cm = getCM(dbg);

  const position = { line: line - 1, ch: 0 };
  cm.scrollIntoView(position, 0);
  await waitForScrolling(cm);

  const coords = getCoordsFromPosition(cm, position);

  const { left, top } = coords;

  // Adds a vertical offset due to increased line height
  // https://github.com/firefox-devtools/debugger/pull/7934
  const lineHeightOffset = 3;

  // Click in the center of the line/breakpoint
  const leftOffset = 10;

  const tokenEl = dbg.win.document.elementFromPoint(
    left - leftOffset,
    top + lineHeightOffset
  );

  if (!tokenEl) {
    throw new Error(`Failed to find element for line ${line}`);
  }
  return tokenEl;
}

async function hoverAtPos(dbg, { line, ch }) {
  info(`Hovering at ${line}, ${ch}`);
  const cm = getCM(dbg);

  // Ensure the line is visible with margin because the bar at the bottom of
  // the editor overlaps into what the editor things is its own space, blocking
  // the click event below.
  cm.scrollIntoView({ line: line - 1, ch }, 0);
  await waitForScrolling(cm);

  const coords = getCoordsFromPosition(cm, { line: line - 1, ch });

  const { left, top } = coords;

  // Adds a vertical offset due to increased line height
  // https://github.com/firefox-devtools/debugger/pull/7934
  const lineHeightOffset = 3;

  const tokenEl = dbg.win.document.elementFromPoint(
    left,
    top + lineHeightOffset
  );

  if (!tokenEl) {
    return false;
  }

  tokenEl.dispatchEvent(
    new MouseEvent("mouseover", {
      bubbles: true,
      cancelable: true,
      view: dbg.win
    })
  );
}

// tryHovering will hover at a position every second until we
// see a preview element (popup, tooltip) appear. Once it appears,
// it considers it a success.
function tryHovering(dbg, line, column, elementName) {
  return new Promise((resolve, reject) => {
    const element = waitForElement(dbg, elementName);
    let count = 0;

    element.then(() => {
      clearInterval(interval);
      resolve(element);
    });

    const interval = setInterval(() => {
      if (count++ == 5) {
        clearInterval(interval);
        reject("failed to preview");
      }

      hoverAtPos(dbg, { line, ch: column - 1 });
    }, 1000);
  });
}

async function assertPreviewTextValue(dbg, line, column, { text, expression }) {
  const previewEl = await tryHovering(dbg, line, column, "previewPopup");

  ok(previewEl.innerText.includes(text), "Preview text shown to user");

  const preview = dbg.selectors.getPreview();
  is(preview.expression, expression, "Preview.expression");
}

async function assertPreviewTooltip(dbg, line, column, { result, expression }) {
  const previewEl = await tryHovering(dbg, line, column, "tooltip");

  is(previewEl.innerText, result, "Preview text shown to user");

  const preview = dbg.selectors.getPreview();
  is(`${preview.result}`, result, "Preview.result");
  is(preview.expression, expression, "Preview.expression");
}

async function hoverOnToken(dbg, line, column, selector) {
  await tryHovering(dbg, line, column, selector);
  return dbg.selectors.getPreview();
}

function getPreviewProperty(preview, field) {
  const properties =
    preview.result.preview.ownProperties || preview.result.preview.items;
  const property = properties[field];
  return property.value || property;
}

async function assertPreviewPopup(
  dbg,
  line,
  column,
  { field, value, expression }
) {
  const preview = await hoverOnToken(dbg, line, column, "popup");
  is(`${getPreviewProperty(preview, field)}`, value, "Preview.result");

  is(preview.expression, expression, "Preview.expression");
}

async function assertPreviews(dbg, previews) {
  for (const { line, column, expression, result, fields } of previews) {
    if (fields && result) {
      throw new Error("Invalid test fixture");
    }

    if (fields) {
      for (const [field, value] of fields) {
        await assertPreviewPopup(dbg, line, column, {
          expression,
          field,
          value
        });
      }
    } else {
      await assertPreviewTextValue(dbg, line, column, {
        expression,
        text: result
      });
    }

    dbg.actions.clearPreview(getContext(dbg));
  }
}

async function waitForBreakableLine(dbg, source, lineNumber) {
  await waitForState(
    dbg,
    state => {
      const currentSource = findSource(dbg, source);

      const breakableLines =
        currentSource && dbg.selectors.getBreakableLines(currentSource.id);

      return breakableLines && breakableLines.includes(lineNumber);
    },
    `waiting for breakable line ${lineNumber}`
  );
}

async function waitForSourceCount(dbg, i) {
  // We are forced to wait until the DOM nodes appear because the
  // source tree batches its rendering.
  await waitUntil(() => {
    return findAllElements(dbg, "sourceNodes").length === i;
  }, `waiting for ${i} sources`);
}

async function assertSourceCount(dbg, count) {
  await waitForSourceCount(dbg, count);
  is(findAllElements(dbg, "sourceNodes").length, count, `${count} sources`);
}

async function waitForNodeToGainFocus(dbg, index) {
  await waitUntil(() => {
    const element = findElement(dbg, "sourceNode", index);

    if (element) {
      return element.classList.contains("focused");
    }

    return false;
  }, `waiting for source node ${index} to be focused`);
}

async function assertNodeIsFocused(dbg, index) {
  await waitForNodeToGainFocus(dbg, index);
  const node = findElement(dbg, "sourceNode", index);
  ok(node.classList.contains("focused"), `node ${index} is focused`);
}

async function addExpression(dbg, input) {
  info("Adding an expression");

  const plusIcon = findElementWithSelector(dbg, selectors.expressionPlus);
  if (plusIcon) {
    plusIcon.click();
  }
  findElementWithSelector(dbg, selectors.expressionInput).focus();
  type(dbg, input);
  pressKey(dbg, "Enter");

  await waitForDispatch(dbg, "EVALUATE_EXPRESSION");
}

async function editExpression(dbg, input) {
  info("Updating the expression");
  dblClickElement(dbg, "expressionNode", 1);
  // Position cursor reliably at the end of the text.
  pressKey(dbg, "End");
  type(dbg, input);
  const evaluated = waitForDispatch(dbg, "EVALUATE_EXPRESSIONS");
  pressKey(dbg, "Enter");
  await evaluated;
}

async function waitUntilPredicate(predicate) {
  let result;
  await waitUntil(() => {
    result = predicate();
    return result;
  });

  return result;
}

// Return a promise with a reference to jsterm, opening the split
// console if necessary.  This cleans up the split console pref so
// it won't pollute other tests.
async function getDebuggerSplitConsole(dbg) {
  let { toolbox, win } = dbg;

  if (!win) {
    win = toolbox.win;
  }

  if (!toolbox.splitConsole) {
    pressKey(dbg, "Escape");
  }

  await toolbox.openSplitConsole();
  return toolbox.getPanel("webconsole");
}

async function openConsoleContextMenu(hud, element) {
  const onConsoleMenuOpened = hud.ui.wrapper.once("menu-open");
  synthesizeContextMenuEvent(element);
  await onConsoleMenuOpened;
  const toolbox = gDevTools.getToolbox(hud.target);
  return toolbox.topDoc.getElementById("webconsole-menu");
}

function hideConsoleContextMenu(hud) {
  const toolbox = gDevTools.getToolbox(hud.target);
  const popup = toolbox.topDoc.getElementById("webconsole-menu");
  if (!popup) {
    return Promise.resolve();
  }

  const onPopupHidden = once(popup, "popuphidden");
  popup.hidePopup();
  return onPopupHidden;
}

// Return a promise that resolves with the result of a thread evaluating a
// string in the topmost frame.
async function evaluateInTopFrame(target, text) {
  const threadClient = target.threadClient;
  const consoleFront = await target.getFront("console");
  const { frames } = await threadClient.getFrames(0, 1);
  ok(frames.length == 1, "Got one frame");
  const options = { thread: threadClient.actor, frameActor: frames[0].actor };
  const response = await consoleFront.evaluateJS(text, options);
  return response.result.type == "undefined" ? undefined : response.result;
}

// Return a promise that resolves when a thread evaluates a string in the
// topmost frame, ensuring the result matches the expected value.
async function checkEvaluateInTopFrame(target, text, expected) {
  const rval = await evaluateInTopFrame(target, text);
  ok(rval == expected, `Eval returned ${expected}`);
}

async function findConsoleMessage(dbg, query) {
  const [message] = await findConsoleMessages(dbg, query);
  const value = message.querySelector(".message-body").innerText;
  const link = message.querySelector(".frame-link-source-inner").innerText;
  return { value, link };
}

async function findConsoleMessages(dbg, query) {
  const webConsole = await dbg.toolbox.getPanel("webconsole");
  const win = webConsole._frameWindow;
  return Array.prototype.filter.call(
    win.document.querySelectorAll(".message"),
    e => e.innerText.includes(query)
  );
}

async function hasConsoleMessage(dbg, msg) {
  return waitFor(async () => {
    const messages = await findConsoleMessages(dbg, msg);
    return messages.length > 0;
  });
}

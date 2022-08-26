/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Helper methods to drive with the debugger during mochitests. This file can be safely
 * required from other panel test files.
 */

"use strict";

/* eslint-disable no-unused-vars */

// We can't use "import globals from head.js" because of bug 1395426.
// So workaround by manually importing the few symbols we are using from it.
// (Note that only ./mach eslint devtools/client fails while devtools/client/debugger passes)
/* global EXAMPLE_URL, ContentTask */

// Assume that shared-head is always imported before this file
/* import-globals-from ../../../shared/test/shared-head.js */

/**
 * Helper method to create a "dbg" context for other tools to use
 */
function createDebuggerContext(toolbox) {
  const panel = toolbox.getPanel("jsdebugger");
  const win = panel.panelWin;

  return {
    ...win.dbg,
    commands: toolbox.commands,
    toolbox,
    win,
    panel,
  };
}

var { Toolbox } = require("devtools/client/framework/toolbox");
const asyncStorage = require("devtools/shared/async-storage");

const {
  getSelectedLocation,
} = require("devtools/client/debugger/src/utils/selected-location");

const {
  resetSchemaVersion,
} = require("devtools/client/debugger/src/utils/prefs");

const {
  isGeneratedId,
  isOriginalId,
  originalToGeneratedId,
} = require("devtools/client/shared/source-map/index");

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
      resolve();
      return;
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
    return;
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
      return Promise.resolve();
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

/**
 * Wait for a count of given elements to be rendered on screen.
 *
 * @param {DebuggerPanel} dbg
 * @param {String} name
 * @param {Integer} count: Number of elements to match. Defaults to 1.
 * @param {Boolean} countStrictlyEqual: When set to true, will wait until the exact number
 *                  of elements is displayed on screen. When undefined or false, will wait
 *                  until there's at least `${count}` elements on screen (e.g. if count
 *                  is 1, it will resolve if there are 2 elements rendered).
 */
async function waitForAllElements(
  dbg,
  name,
  count = 1,
  countStrictlyEqual = false
) {
  await waitUntil(() => {
    const elsCount = findAllElements(dbg, name).length;
    return countStrictlyEqual ? elsCount === count : elsCount >= count;
  });
  return findAllElements(dbg, name);
}

async function waitForElementWithSelector(dbg, selector) {
  await waitUntil(() => findElementWithSelector(dbg, selector));
  return findElementWithSelector(dbg, selector);
}

function waitForRequestsToSettle(dbg) {
  return dbg.commands.client.waitForRequestsToSettle();
}

function assertClass(el, className, exists = true) {
  if (exists) {
    ok(el.classList.contains(className), `${className} class exists`);
  } else {
    ok(!el.classList.contains(className), `${className} class does not exist`);
  }
}

function waitForSelectedLocation(dbg, line, column) {
  return waitForState(dbg, state => {
    const location = dbg.selectors.getSelectedLocation();
    return (
      location &&
      (line ? location.line == line : true) &&
      (column ? location.column == column : true)
    );
  });
}

/**
 * Wait for a given source to be selected and ready.
 *
 * @memberof mochitest/waits
 * @param {Object} dbg
 * @param {null|string|Source} sourceOrUrl Optional. Either a source URL (string) or a source object (typically fetched via `findSource`)
 * @return {Promise}
 * @static
 */
function waitForSelectedSource(dbg, sourceOrUrl) {
  const {
    getSelectedSource,
    getSelectedSourceTextContent,
    getSymbols,
    getBreakableLines,
  } = dbg.selectors;

  return waitForState(
    dbg,
    state => {
      const source = getSelectedSource() || {};
      const sourceTextContent = getSelectedSourceTextContent();
      if (!sourceTextContent) {
        return false;
      }

      if (sourceOrUrl) {
        // Second argument is either a source URL (string)
        // or a Source object.
        if (typeof sourceOrUrl == "string") {
          if (!source.url.includes(encodeURI(sourceOrUrl))) {
            return false;
          }
        } else if (source.id != sourceOrUrl.id) {
          return false;
        }
      }

      return getSymbols(source) && getBreakableLines(source.id);
    },
    "selected source"
  );
}

function getVisibleSelectedFrameLine(dbg) {
  const frame = dbg.selectors.getVisibleSelectedFrame();
  return frame?.location.line;
}

function getVisibleSelectedFrameColumn(dbg) {
  const frame = dbg.selectors.getVisibleSelectedFrame();
  return frame?.location.column;
}

/**
 * Assert that a given line is breakable or not.
 * Verify that CodeMirror gutter is grayed out via the empty line classname if not breakable.
 */
function assertLineIsBreakable(dbg, file, line, shouldBeBreakable) {
  const lineInfo = getCM(dbg).lineInfo(line - 1);
  // When a line is not breakable, the "empty-line" class is added
  // and the line is greyed out
  if (shouldBeBreakable) {
    ok(
      !lineInfo.wrapClass?.includes("empty-line"),
      `${file}:${line} should be breakable`
    );
  } else {
    ok(
      lineInfo?.wrapClass?.includes("empty-line"),
      `${file}:${line} should NOT be breakable`
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
 * Helper function for assertPausedAtSourceAndLine.
 *
 * Assert that CodeMirror reports to be paused at the given line/column.
 */
function _assertDebugLine(dbg, line, column) {
  // Check the debug line
  const lineInfo = getCM(dbg).lineInfo(line - 1);
  const source = dbg.selectors.getSelectedSource();
  const sourceTextContent = dbg.selectors.getSelectedSourceTextContent();
  if (source && !sourceTextContent) {
    const url = source.url;
    ok(
      false,
      `Looks like the source ${url} is still loading. Try adding waitForLoadedSource in the test.`
    );
    return;
  }

  // Scroll the line into view to make sure the content
  // on the line is rendered and in the dom.
  getCM(dbg).scrollIntoView({ line, ch: 0 });

  if (!lineInfo.wrapClass) {
    const pauseLine = getVisibleSelectedFrameLine(dbg);
    ok(false, `Expected pause line on line ${line}, it is on ${pauseLine}`);
    return;
  }

  ok(
    lineInfo?.wrapClass.includes("new-debug-line"),
    `Line ${line} is not highlighted as paused`
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
  if (markedSpans && markedSpans.length) {
    const hasExpectedDebugLine = markedSpans.some(
      span =>
        span.marker.className?.includes("debug-expression") &&
        // When a precise column is expected, ensure that we have at least
        // one "debug line" for the column we expect.
        // (See the React Component: DebugLine.setDebugLine)
        (!column || span.from == column)
    );
    ok(
      hasExpectedDebugLine,
      "Got the expected DebugLine. i.e. got the right marker in codemirror visualizing the breakpoint"
    );
  }
  info(`Paused on line ${line}`);
}

/**
 * Make sure the debugger is paused at a certain source ID and line.
 *
 * @param {Object} dbg
 * @param {String} expectedSourceId
 * @param {Number} expectedLine
 * @param {Number} [expectedColumn]
 */
function assertPausedAtSourceAndLine(
  dbg,
  expectedSourceId,
  expectedLine,
  expectedColumn
) {
  // Check that the debugger is paused.
  assertPaused(dbg);

  // Check that the paused location is correctly rendered.
  ok(isSelectedFrameSelected(dbg), "top frame's source is selected");

  // Check the pause location
  const pauseLine = getVisibleSelectedFrameLine(dbg);
  is(
    pauseLine,
    expectedLine,
    "Redux state for currently selected frame's line is correct"
  );
  const pauseColumn = getVisibleSelectedFrameColumn(dbg);
  if (expectedColumn) {
    is(
      pauseColumn,
      expectedColumn,
      "Redux state for currently selected frame's column is correct"
    );
  }
  _assertDebugLine(dbg, pauseLine, pauseColumn);

  ok(isVisibleInEditor(dbg, getCM(dbg).display.gutters), "gutter is visible");

  const frames = dbg.selectors.getCurrentThreadFrames();
  ok(frames.length >= 1, "Got at least one frame");

  // Lets make sure we can assert both original and generated file locations when needed
  const { sourceId, line, column } = isGeneratedId(expectedSourceId)
    ? frames[0].generatedLocation
    : frames[0].location;
  is(sourceId, expectedSourceId, "Frame has correct source");
  is(
    line,
    expectedLine,
    `Frame paused at line ${line}, but expected line ${expectedLine}`
  );

  if (expectedColumn) {
    is(
      column,
      expectedColumn,
      `Frame paused at column ${column}, but expected column ${expectedColumn}`
    );
  }
}

async function waitForThreadCount(dbg, count) {
  return waitForState(
    dbg,
    state => dbg.selectors.getThreads(state).length == count
  );
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
 * Returns boolean for whether the debugger is paused.
 *
 * @param {Object} dbg
 */
function isPaused(dbg) {
  return dbg.selectors.getIsCurrentThreadPaused();
}

/**
 * Assert that the debugger is not currently paused.
 *
 * @param {Object} dbg
 * @param {String} msg
 *        Optional assertion message
 */
function assertNotPaused(dbg, msg = "client is not paused") {
  ok(!isPaused(dbg), msg);
}

/**
 * Assert that the debugger is currently paused.
 *
 * @param {Object} dbg
 */
function assertPaused(dbg, msg = "client is paused") {
  ok(isPaused(dbg), msg);
}

/**
 * Waits for the debugger to be fully paused.
 *
 * @param {Object} dbg
 * @param {String} url
 *        Optional URL of the script we should be pausing on.
 */
async function waitForPaused(dbg, url) {
  info("Waiting for the debugger to pause");
  const {
    getSelectedScope,
    getCurrentThread,
    getCurrentThreadFrames,
  } = dbg.selectors;

  await waitForState(
    dbg,
    state => isPaused(dbg) && !!getSelectedScope(getCurrentThread()),
    "paused"
  );

  await waitForState(dbg, getCurrentThreadFrames, "fetched frames");
  await waitForLoadedScopes(dbg);
  await waitForSelectedSource(dbg, url);
}

/**
 * Waits for the debugger to resume.
 *
 * @param {Objeect} dbg
 */
function waitForResumed(dbg) {
  info("Waiting for the debugger to resume");
  return waitForState(dbg, state => !dbg.selectors.getIsCurrentThreadPaused());
}

function waitForInlinePreviews(dbg) {
  return waitForState(dbg, () => dbg.selectors.getSelectedInlinePreviews());
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

function isSelectedFrameSelected(dbg, state) {
  const frame = dbg.selectors.getVisibleSelectedFrame();

  // Make sure the source text is completely loaded for the
  // source we are paused in.
  const sourceId = frame.location.sourceId;
  const source = dbg.selectors.getSelectedSource();
  const sourceTextContent = dbg.selectors.getSelectedSourceTextContent();

  if (!source || !sourceTextContent) {
    return false;
  }

  return source.id == sourceId;
}

/**
 * Checks to see if the frame is selected and the title is correct.
 *
 * @param {Object} dbg
 * @param {Integer} index
 * @param {String} title
 */
function isFrameSelected(dbg, index, title) {
  const $frame = findElement(dbg, "frame", index);

  const {
    selectors: { getSelectedFrame, getCurrentThread },
  } = dbg;

  const frame = getSelectedFrame(getCurrentThread());

  const elSelected = $frame.classList.contains("selected");
  const titleSelected = frame.displayName == title;

  return elSelected && titleSelected;
}

/**
 * Clear all the debugger related preferences.
 */
async function clearDebuggerPreferences(prefs = []) {
  resetSchemaVersion();
  asyncStorage.clear();
  Services.prefs.clearUserPref("devtools.debugger.alphabetize-outline");
  Services.prefs.clearUserPref("devtools.debugger.pause-on-exceptions");
  Services.prefs.clearUserPref("devtools.debugger.pause-on-caught-exceptions");
  Services.prefs.clearUserPref("devtools.debugger.ignore-caught-exceptions");
  Services.prefs.clearUserPref("devtools.debugger.pending-selected-location");
  Services.prefs.clearUserPref("devtools.debugger.expressions");
  Services.prefs.clearUserPref("devtools.debugger.breakpoints-visible");
  Services.prefs.clearUserPref("devtools.debugger.call-stack-visible");
  Services.prefs.clearUserPref("devtools.debugger.scopes-visible");
  Services.prefs.clearUserPref("devtools.debugger.skip-pausing");
  Services.prefs.clearUserPref("devtools.debugger.map-scopes-enabled");

  for (const pref of prefs) {
    await pushPref(...pref);
  }
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
  // We depend on EXAMPLE_URLs origin to do cross origin/process iframes via
  // EXAMPLE_REMOTE_URL. If the top level document origin changes,
  // we may break this. So be careful if you want to change EXAMPLE_URL.
  return initDebuggerWithAbsoluteURL(EXAMPLE_URL + url, ...sources);
}

async function initDebuggerWithAbsoluteURL(url, ...sources) {
  await clearDebuggerPreferences();
  const toolbox = await openNewTabAndToolbox(url, "jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  await waitForSources(dbg, ...sources);
  return dbg;
}

async function initPane(url, pane, prefs) {
  await clearDebuggerPreferences(prefs);
  return openNewTabAndToolbox(EXAMPLE_URL + url, pane);
}

/**
 * Returns a source that matches a given filename, or a URL.
 * This also accept a source as input argument, in such case it just returns it.
 *
 * @param {Object} dbg
 * @param {String} filenameOrUrlOrSource
 *        The typical case will be to pass only a filename,
 *        but you may also pass a full URL to match sources without filesnames like data: URL
 *        or pass the source itself, which is just returned.
 * @param {Object} options
 * @param {Boolean} options.silent
 *        If true, won't throw if the source is missing.
 * @return {Object} source
 */
function findSource(
  dbg,
  filenameOrUrlOrSource,
  { silent } = { silent: false }
) {
  if (typeof filenameOrUrlOrSource !== "string") {
    // Support passing in a source object itself all APIs that use this
    // function support both styles
    return filenameOrUrlOrSource;
  }

  const sources = dbg.selectors.getSourceList();
  const source = sources.find(s => {
    // Sources don't have a file name attribute, we need to compute it here:
    const sourceFileName = s.url
      ? decodeURI(s.url.substring(s.url.lastIndexOf("/") + 1))
      : "";
    // The input argument may either be only the filename, or the complete URL
    // This helps match sources whose URL doesn't contain a filename, like data: URLs
    return (
      sourceFileName == filenameOrUrlOrSource || s.url == filenameOrUrlOrSource
    );
  });

  if (!source) {
    if (silent) {
      return false;
    }

    throw new Error(`Unable to find source: ${filenameOrUrlOrSource}`);
  }

  return source;
}

/**
 * Find the source in specied thread. Useful when there exists
 * same named sources in different targets (threads)
 *
 * @param {Object} dbg
 * @param {String} filenameOrUrl - The source file name of the full source url
 * @param {String} threadName - The name of the thread the source belongs to
 */
function findSourceInThread(dbg, filenameOrUrl, threadName) {
  const sources = dbg.selectors.getSourceList();
  return sources.find(s => {
    const sourceFileName = s.url
      ? s.url.substring(s.url.lastIndexOf("/") + 1)
      : "";
    if (sourceFileName == filenameOrUrl || s.url == filenameOrUrl) {
      const thread = dbg.selectors.getThread(s.thread);
      return thread.name == threadName;
    }
    return false;
  });
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
    throw new Error(`Expected loaded source, got${content.value}`);
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
 * @param {Number} column
 * @return {Promise}
 * @static
 */
async function selectSource(dbg, url, line, column) {
  const source = findSource(dbg, url);

  await dbg.actions.selectLocation(
    getContext(dbg),
    { sourceId: source.id, line, column },
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
async function resume(dbg) {
  const pauseLine = getVisibleSelectedFrameLine(dbg);
  info(`Resuming from ${pauseLine}`);
  const onResumed = waitForResumed(dbg);
  await dbg.actions.resume();
  return onResumed;
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
  // We aren't waiting for load as the page may not load because of a breakpoint
  await reloadBrowser({ waitForLoad: false });
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
  return navigateToAbsoluteURL(dbg, EXAMPLE_URL + url, ...sources);
}

/**
 * Navigates the debuggee to another absolute url.
 *
 * @memberof mochitest/actions
 * @param {Object} dbg
 * @param {String} url
 * @param {Array} sources
 * @return {Promise}
 * @static
 */
async function navigateToAbsoluteURL(dbg, url, ...sources) {
  await navigateTo(url);
  return waitForSources(dbg, ...sources);
}

function getFirstBreakpointColumn(dbg, { line, sourceId }) {
  const { getSource, getFirstBreakpointPosition } = dbg.selectors;
  const source = getSource(sourceId);
  const position = getFirstBreakpointPosition({
    line,
    sourceId,
  });

  return getSelectedLocation(position, source).column;
}

function isMatchingLocation(location1, location2) {
  return (
    location1?.sourceId == location2?.sourceId &&
    location1?.line == location2?.line &&
    location1?.column == location2?.column
  );
}

function getBreakpointForLocation(dbg, location) {
  if (!location) {
    return undefined;
  }

  const isGeneratedSource = isGeneratedId(location.sourceId);
  return dbg.selectors.getBreakpointsList().find(bp => {
    const loc = isGeneratedSource ? bp.generatedLocation : bp.location;
    return isMatchingLocation(loc, location);
  });
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
  const onBreakpoint = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  await dbg.actions.addBreakpoint(
    getContext(dbg),
    { sourceId, line, column },
    options
  );
  await onBreakpoint;
  is(
    dbg.selectors.getBreakpointCount(),
    bpCount + 1,
    "a new breakpoint was created"
  );
}

/**
 * Similar to `addBreakpoint`, but uses the UI instead or calling
 * the actions directly. This only support breakpoint on lines,
 * not on a specific column.
 */
async function addBreakpointViaGutter(dbg, line) {
  info(`Add breakpoint via the editor on line ${line}`);
  await clickGutter(dbg, line);
  return waitForDispatch(dbg.store, "SET_BREAKPOINT");
}

function disableBreakpoint(dbg, source, line, column) {
  column =
    column || getFirstBreakpointColumn(dbg, { line, sourceId: source.id });
  const location = { sourceId: source.id, sourceUrl: source.url, line, column };
  const bp = getBreakpointForLocation(dbg, location);
  return dbg.actions.disableBreakpoint(getContext(dbg), bp);
}

function findBreakpoint(dbg, url, line) {
  const source = findSource(dbg, url);
  return dbg.selectors.getBreakpointsForSource(source.id, line)[0];
}

// helper for finding column breakpoints.
function findColumnBreakpoint(dbg, url, line, column) {
  const source = findSource(dbg, url);
  const lineBreakpoints = dbg.selectors.getBreakpointsForSource(
    source.id,
    line
  );
  return lineBreakpoints.find(bp => {
    return bp.generatedLocation.column === column;
  });
}

async function loadAndAddBreakpoint(dbg, filename, line, column) {
  const {
    selectors: { getBreakpoint, getBreakpointCount, getBreakpointsMap },
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
      `Breakpoint has correct line ${line}, column ${column}, but was line ${loc.line} column ${loc.column}`
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
    invokeResult.then(
      () => new Promise(() => {}),
      () => true
    ),
  ]);

  if (invokeFailed) {
    await invokeResult;
    return;
  }

  assertPausedAtSourceAndLine(dbg, findSource(dbg, filename).id, line, column);

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

function findSourceTreeThreadByName(dbg, name) {
  return [...findAllElements(dbg, "sourceTreeThreads")].find(el => {
    return el.textContent.includes(name);
  });
}

function findSourceNodeWithText(dbg, text) {
  return [...findAllElements(dbg, "sourceNodes")].find(el => {
    return el.textContent.includes(text);
  });
}

/**
 * Assert the icon type used in the SourceTree for a given source
 *
 * @param {Object} dbg
 * @param {String} sourceName
 *        Name of the source displayed in the source tree
 * @param {String} icon
 *        Expected icon CSS classname
 */
function assertSourceIcon(dbg, sourceName, icon) {
  const sourceItem = findSourceNodeWithText(dbg, sourceName);
  ok(sourceItem, `Found the source item for ${sourceName}`);
  is(
    sourceItem.querySelector(".source-icon").className,
    `img source-icon ${icon}`,
    `The icon for ${sourceName} is correct`
  );
}

async function expandSourceTree(dbg) {
  // Click on expand all context menu for all top level "expandable items".
  // If there is no project root, it will be thread items.
  // But when there is a project root, it can be directory or group items.
  // Select only expandable in order to ignore source items.
  for (const rootNode of dbg.win.document.querySelectorAll(
    ".sources-list > .managed-tree > .tree > .tree-node[data-expandable=true]"
  )) {
    await expandAllSourceNodes(dbg, rootNode);
  }
}

async function expandAllSourceNodes(dbg, treeNode) {
  const onContextMenu = waitForContextMenu(dbg);
  rightClickEl(dbg, treeNode);
  const menupopup = await onContextMenu;
  const onHidden = new Promise(resolve => {
    menupopup.addEventListener("popuphidden", resolve, { once: true });
  });
  selectContextMenuItem(dbg, "#node-menu-expand-all");
  await onHidden;
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
  const bp = getBreakpointForLocation(dbg, location);
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
  return ContentTask.spawn(gBrowser.selectedBrowser, { fnc, args }, options =>
    content.wrappedJSObject[options.fnc](...options.args)
  );
}

function clickElementInTab(selector) {
  info(`click element ${selector} in tab`);

  return SpecialPowers.spawn(gBrowser.selectedBrowser, [selector], function(
    _selector
  ) {
    const element = content.document.querySelector(_selector);
    // Run the click in another event loop in order to immediately resolve spawn's promise.
    // Otherwise if we pause on click and navigate, the JSWindowActor used by spawn will
    // be destroyed while its query is still pending. And this would reject the promise.
    content.setTimeout(() => {
      element.click();
    });
  });
}

const isLinux = Services.appinfo.OS === "Linux";
const isMac = Services.appinfo.OS === "Darwin";
const cmdOrCtrl = isMac ? { metaKey: true } : { ctrlKey: true };
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
  commandKeyDown: { code: "VK_META", modifiers: { type: "keydown" } },
  commandKeyUp: { code: "VK_META", modifiers: { type: "keyup" } },
  debugger: { code: "s", modifiers: shiftOrAlt },
  // test conditional panel shortcut
  toggleCondPanel: { code: "b", modifiers: cmdShift },
  inspector: { code: "c", modifiers: shiftOrAlt },
  quickOpen: { code: "p", modifiers: cmdOrCtrl },
  quickOpenFunc: { code: "o", modifiers: cmdShift },
  quickOpenLine: { code: ":", modifiers: cmdOrCtrl },
  fileSearch: { code: "f", modifiers: cmdOrCtrl },
  projectSearch: { code: "f", modifiers: cmdShift },
  fileSearchNext: { code: "g", modifiers: { metaKey: true } },
  fileSearchPrev: { code: "g", modifiers: cmdShift },
  goToLine: { code: "g", modifiers: { ctrlKey: true } },
  Enter: { code: "VK_RETURN" },
  ShiftEnter: { code: "VK_RETURN", modifiers: { shiftKey: true } },
  AltEnter: {
    code: "VK_RETURN",
    modifiers: { altKey: true },
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
  stepInKey: { code: "VK_F11" },
  stepOutKey: {
    code: "VK_F11",
    modifiers: { shiftKey: true },
  },
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

/**
 * Assert the text content on the line matches what is
 * expected.
 *
 * @param {Object} dbg
 * @param {Number} line
 * @param {String} expectedTextContent
 */
function assertTextContentOnLine(dbg, line, expectedTextContent) {
  const lineInfo = getCM(dbg).lineInfo(line - 1);
  const textContent = lineInfo.text.trim();
  is(textContent, expectedTextContent, `Expected text content on line ${line}`);
}

/*
 * Assert that no breakpoint is set on a given line of
 * the currently selected source in the editor.
 *
 * @memberof mochitest/helpers
 * @param {Object} dbg
 * @param {Number} line Line where to check for a breakpoint in the editor
 * @static
 */
async function assertNoBreakpoint(dbg, line) {
  const el = await getEditorLineEl(dbg, line);

  const exists = !!el.querySelector(".new-breakpoint");
  ok(!exists, `Breakpoint doesn't exists on line ${line}`);
}

/*
 * Assert that a regular breakpoint is set in the currently
 * selected source in the editor. (no conditional, nor log breakpoint)
 *
 * @memberof mochitest/helpers
 * @param {Object} dbg
 * @param {Number} line Line where to check for a breakpoint
 * @static
 */
async function assertBreakpoint(dbg, line) {
  const el = await getEditorLineEl(dbg, line);

  const exists = !!el.querySelector(".new-breakpoint");
  ok(exists, `Breakpoint exists on line ${line}`);

  const hasConditionClass = el.classList.contains("has-condition");

  ok(
    !hasConditionClass,
    `Regular breakpoint doesn't have condition on line ${line}`
  );

  const hasLogClass = el.classList.contains("has-log");

  ok(!hasLogClass, `Regular breakpoint doesn't have log on line ${line}`);
}

/*
 * Assert that a conditionnal breakpoint is set.
 *
 * @memberof mochitest/helpers
 * @param {Object} dbg
 * @param {Number} line Line where to check for a breakpoint
 * @static
 */
async function assertConditionBreakpoint(dbg, line) {
  const el = await getEditorLineEl(dbg, line);

  const exists = !!el.querySelector(".new-breakpoint");
  ok(exists, `Breakpoint exists on line ${line}`);

  const hasConditionClass = el.classList.contains("has-condition");

  ok(hasConditionClass, `Conditional breakpoint on line ${line}`);

  const hasLogClass = el.classList.contains("has-log");

  ok(
    !hasLogClass,
    `Conditional breakpoint doesn't have log breakpoint on line ${line}`
  );
}

/*
 * Assert that a log breakpoint is set.
 *
 * @memberof mochitest/helpers
 * @param {Object} dbg
 * @param {Number} line Line where to check for a breakpoint
 * @static
 */
async function assertLogBreakpoint(dbg, line) {
  const el = await getEditorLineEl(dbg, line);

  const exists = !!el.querySelector(".new-breakpoint");
  ok(exists, `Breakpoint exists on line ${line}`);

  const hasConditionClass = el.classList.contains("has-condition");

  ok(
    !hasConditionClass,
    `Log breakpoint doesn't have condition on line ${line}`
  );

  const hasLogClass = el.classList.contains("has-log");

  ok(hasLogClass, `Log breakpoint on line ${line}`);
}

function assertBreakpointSnippet(dbg, index, snippet) {
  const actualSnippet = findElement(dbg, "breakpointLabel", 2).innerText;
  is(snippet, actualSnippet, `Breakpoint ${index} snippet`);
}

const selectors = {
  callStackHeader: ".call-stack-pane ._header",
  callStackBody: ".call-stack-pane .pane",
  domMutationItem: ".dom-mutation-list li",
  expressionNode: i =>
    `.expressions-list .expression-container:nth-child(${i}) .object-label`,
  expressionValue: i =>
    // eslint-disable-next-line max-len
    `.expressions-list .expression-container:nth-child(${i}) .object-delimiter + *`,
  expressionClose: i =>
    `.expressions-list .expression-container:nth-child(${i}) .close`,
  expressionInput: ".watch-expressions-pane input.input-expression",
  expressionNodes: ".expressions-list .tree-node",
  expressionPlus: ".watch-expressions-pane button.plus",
  expressionRefresh: ".watch-expressions-pane button.refresh",
  scopesHeader: ".scopes-pane ._header",
  breakpointItem: i => `.breakpoints-list div:nth-of-type(${i})`,
  breakpointLabel: i => `${selectors.breakpointItem(i)} .breakpoint-label`,
  breakpointHeadings: ".breakpoints-list .breakpoint-heading",
  breakpointItems: ".breakpoints-list .breakpoint",
  breakpointContextMenu: {
    disableSelf: "#node-menu-disable-self",
    disableAll: "#node-menu-disable-all",
    disableOthers: "#node-menu-disable-others",
    enableSelf: "#node-menu-enable-self",
    enableOthers: "#node-menu-enable-others",
    disableDbgStatement: "#node-menu-disable-dbgStatement",
    enableDbgStatement: "#node-menu-enable-dbgStatement",
    remove: "#node-menu-delete-self",
    removeOthers: "#node-menu-delete-other",
    removeCondition: "#node-menu-remove-condition",
  },
  editorContextMenu: {
    continueToHere: "#node-menu-continue-to-here",
  },
  columnBreakpoints: ".column-breakpoint",
  scopes: ".scopes-list",
  scopeNodes: ".scopes-list .object-label",
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
  prettyPrintButton: ".source-footer .prettyPrint",
  sourceMapLink: ".source-footer .mapped-source",
  sourcesFooter: ".sources-panel .source-footer",
  editorFooter: ".editor-pane .source-footer",
  sourceNode: i => `.sources-list .tree-node:nth-child(${i}) .node`,
  sourceNodes: ".sources-list .tree-node",
  sourceTreeThreads: '.sources-list .tree-node[aria-level="1"]',
  sourceTreeThreadsNodes:
    '.sources-list .tree-node[aria-level="1"] > .node > span:nth-child(1)',
  sourceTreeFiles: ".sources-list .tree-node[data-expandable=false]",
  threadSourceTree: i => `.threads-list .sources-pane:nth-child(${i})`,
  threadSourceTreeHeader: i =>
    `${selectors.threadSourceTree(i)} .thread-header`,
  threadSourceTreeSourceNode: (i, j) =>
    `${selectors.threadSourceTree(i)} .tree-node:nth-child(${j}) .node`,
  sourceDirectoryLabel: i => `.sources-list .tree-node:nth-child(${i}) .label`,
  resultItems: ".result-list .result-item",
  resultItemName: (name, i) =>
    `${selectors.resultItems}:nth-child(${i})[title$="${name}"]`,
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
  projectSearchExpandedResults: ".project-text-search .result",
  projectSearchFileResults: ".project-text-search .file-result",
  threadsPaneItems: ".threads-pane .thread",
  threadsPaneItem: i => `.threads-pane .thread:nth-child(${i})`,
  threadsPaneItemPause: i => `${selectors.threadsPaneItem(i)} .pause-badge`,
  CodeMirrorLines: ".CodeMirror-lines",
  inlinePreviewLabels: ".inline-preview .inline-preview-label",
  inlinePreviewValues: ".inline-preview .inline-preview-value",
  inlinePreviewOpenInspector: ".inline-preview-value button.open-inspector",
  watchpointsSubmenu: "#node-menu-watchpoints",
  addGetWatchpoint: "#node-menu-add-get-watchpoint",
  addSetWatchpoint: "#node-menu-add-set-watchpoint",
  removeWatchpoint: "#node-menu-remove-watchpoint",
  logEventsCheckbox: ".events-header input",
  previewPopupInvokeGetterButton: ".preview-popup .invoke-getter",
  previewPopupObjectNumber: ".preview-popup .objectBox-number",
  previewPopupObjectObject: ".preview-popup .objectBox-object",
  sourceTreeRootNode: ".sources-panel .node .window",
  sourceTreeFolderNode: ".sources-panel .node .folder",
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

function clickDOMElement(dbg, element, options = {}) {
  EventUtils.synthesizeMouseAtCenter(element, options, dbg.win);
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
  el.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(el, { type: "contextmenu" }, dbg.win);
}

async function clickGutter(dbg, line) {
  const el = await codeMirrorGutterElement(dbg, line);
  clickDOMElement(dbg, el);
}

async function cmdClickGutter(dbg, line) {
  const el = await codeMirrorGutterElement(dbg, line);
  clickDOMElement(dbg, el, cmdOrCtrl);
}

function findContextMenu(dbg, selector) {
  // the context menu is in the toolbox window
  const doc = dbg.toolbox.topDoc;

  // there are several context menus, we want the one with the menu-api
  const popup = doc.querySelector('menupopup[menu-api="true"]');

  return popup.querySelector(selector);
}

// Waits for the context menu to exist and to fully open. Once this function
// completes, selectContextMenuItem can be called.
// waitForContextMenu must be called after menu opening has been triggered, e.g.
// after synthesizing a right click / contextmenu event.
async function waitForContextMenu(dbg) {
  // the context menu is in the toolbox window
  const doc = dbg.toolbox.topDoc;

  // there are several context menus, we want the one with the menu-api
  const popup = await waitFor(() =>
    doc.querySelector('menupopup[menu-api="true"]')
  );

  if (popup.state == "open") {
    return popup;
  }

  await new Promise(resolve => {
    popup.addEventListener("popupshown", () => resolve(), { once: true });
  });

  return popup;
}

function selectContextMenuItem(dbg, selector) {
  const item = findContextMenu(dbg, selector);
  item.closest("menupopup").activateItem(item);
}

async function openContextMenuSubmenu(dbg, selector) {
  const item = findContextMenu(dbg, selector);
  const popup = item.menupopup;
  const popupshown = new Promise(resolve => {
    popup.addEventListener("popupshown", () => resolve(), { once: true });
  });
  item.openMenu(true);
  await popupshown;
  return popup;
}

async function assertContextMenuLabel(dbg, selector, label) {
  const item = await waitFor(() => findContextMenu(dbg, selector));
  is(item.label, label, "The label of the context menu item shown to the user");
}

async function typeInPanel(dbg, text) {
  await waitForElement(dbg, "conditionalPanelInput");
  // Position cursor reliably at the end of the text.
  pressKey(dbg, "End");
  type(dbg, text);
  pressKey(dbg, "Enter");
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

function rightClickScopeNode(dbg, index) {
  rightClickObjectInspectorNode(dbg, findElement(dbg, "scopeNode", index));
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

  info(`Toggling node ${node.innerText}`);
  node.click();
  return waitUntil(
    () => objectInspector.querySelectorAll(".node").length !== properties
  );
}

function rightClickObjectInspectorNode(dbg, node) {
  const objectInspector = node.closest(".object-inspector");
  const properties = objectInspector.querySelectorAll(".node").length;

  info(`Right clicking node ${node.innerText}`);
  rightClickEl(dbg, node);

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

async function getTokenFromPosition(dbg, { line, ch }) {
  info(`Get token at ${line}, ${ch}`);
  const cm = getCM(dbg);
  cm.scrollIntoView({ line: line - 1, ch }, 0);

  // Ensure the line is visible with margin because the bar at the bottom of
  // the editor overlaps into what the editor thinks is its own space, blocking
  // the click event below.
  await waitForScrolling(cm);

  const coords = getCoordsFromPosition(cm, { line: line - 1, ch });

  const { left, top } = coords;

  // Adds a vertical offset due to increased line height
  // https://github.com/firefox-devtools/debugger/pull/7934
  const lineHeightOffset = 3;

  return dbg.win.document.elementFromPoint(left, top + lineHeightOffset);
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

async function clickAtPos(dbg, pos) {
  const tokenEl = await getTokenFromPosition(dbg, pos);

  if (!tokenEl) {
    return;
  }

  const { top, left } = tokenEl.getBoundingClientRect();
  info(
    `Clicking on token ${tokenEl.innerText} in line ${tokenEl.parentNode.innerText}`
  );
  tokenEl.dispatchEvent(
    new MouseEvent("click", {
      bubbles: true,
      cancelable: true,
      view: dbg.win,
      clientX: left,
      clientY: top,
    })
  );
}

async function rightClickAtPos(dbg, pos) {
  const el = await getTokenFromPosition(dbg, pos);
  if (!el) {
    return;
  }

  EventUtils.synthesizeMouseAtCenter(el, { type: "contextmenu" }, dbg.win);
}

async function hoverAtPos(dbg, pos) {
  const tokenEl = await getTokenFromPosition(dbg, pos);

  if (!tokenEl) {
    return;
  }

  info(`Hovering on token ${tokenEl.innerText}`);
  tokenEl.dispatchEvent(
    new MouseEvent("mouseover", {
      bubbles: true,
      cancelable: true,
      view: dbg.win,
    })
  );

  InspectorUtils.addPseudoClassLock(tokenEl, ":hover");
}

async function closePreviewAtPos(dbg, line, column) {
  const pos = { line, ch: column - 1 };
  const tokenEl = await getTokenFromPosition(dbg, pos);

  if (!tokenEl) {
    return;
  }

  InspectorUtils.removePseudoClassLock(tokenEl, ":hover");

  const gutterEl = await getEditorLineGutter(dbg, line);
  EventUtils.synthesizeMouseAtCenter(gutterEl, { type: "mousemove" }, dbg.win);
  await waitUntil(() => findElement(dbg, "previewPopup") == null);
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
  is(`${preview.resultGrip}`, result, "Preview.result");
  is(preview.expression, expression, "Preview.expression");
}

async function assertPreviews(dbg, previews) {
  for (const { line, column, expression, result, fields } of previews) {
    if (fields && result) {
      throw new Error("Invalid test fixture");
    }

    if (fields) {
      const popupEl = await tryHovering(dbg, line, column, "popup");
      const oiNodes = Array.from(
        popupEl.querySelectorAll(".preview-popup .node")
      );

      for (const [field, value] of fields) {
        const node = oiNodes.find(
          oiNode => oiNode.querySelector(".object-label")?.textContent === field
        );
        if (!node) {
          ok(false, `The "${field}" property is not displayed in the popup`);
        } else {
          is(
            node.querySelector(".objectBox").textContent,
            value,
            `The "${field}" property has the expected value`
          );
        }
      }
    } else {
      await assertPreviewTextValue(dbg, line, column, {
        expression,
        text: result,
      });
    }

    const { target } = dbg.selectors.getPreview(getContext(dbg));
    InspectorUtils.removePseudoClassLock(target, ":hover");
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

async function waitForSourceTreeThreadsCount(dbg, i) {
  info(`waiting for ${i} threads in the source tree`);
  await waitUntil(() => {
    return findAllElements(dbg, "sourceTreeThreads").length === i;
  });
}

async function waitForSourcesInSourceTree(
  dbg,
  sources,
  { noExpand = false } = {}
) {
  info(`waiting for ${sources.length} files in the source tree`);
  function getDisplayedSources() {
    // Replace some non visible space characters that prevents Array.includes from working correctly
    return [...findAllElements(dbg, "sourceTreeFiles")].map(e => {
      return e.textContent.trim().replace(/^[\s\u200b]*/g, "");
    });
  }
  try {
    // Use custom timeout and retry count for waitFor as the test method is slow to resolve
    // and default value makes the timeout unecessarily long
    await waitFor(
      async () => {
        if (!noExpand) {
          await expandSourceTree(dbg);
        }
        const displayedSources = getDisplayedSources();
        return (
          displayedSources.length == sources.length &&
          sources.every(source => displayedSources.includes(source))
        );
      },
      null,
      100,
      50
    );
  } catch (e) {
    // Craft a custom error message to help understand what's wrong with the Source Tree content
    const displayedSources = getDisplayedSources();
    let msg = "Invalid Source Tree Content.\n";
    const missingElements = [];
    for (const source of sources) {
      const idx = displayedSources.indexOf(source);
      if (idx != -1) {
        displayedSources.splice(idx, 1);
      } else {
        missingElements.push(source);
      }
    }
    if (missingElements.length) {
      msg += "Missing elements: " + missingElements.join(", ") + "\n";
    }
    if (displayedSources.length) {
      msg += "Unexpected elements: " + displayedSources.join(", ");
    }
    throw new Error(msg);
  }
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

/**
 * Asserts that the debugger is paused and the debugger tab is
 * highlighted.
 * @param {*} toolbox
 * @returns
 */
async function assertDebuggerIsHighlightedAndPaused(toolbox) {
  info("Wait for the debugger to be automatically selected on pause");
  await waitUntil(() => toolbox.currentToolId == "jsdebugger");
  ok(true, "Debugger selected");

  // Wait for the debugger to finish loading.
  await toolbox.getPanelWhenReady("jsdebugger");

  // And to be fully paused
  const dbg = createDebuggerContext(toolbox);
  await waitForPaused(dbg);

  ok(toolbox.isHighlighted("jsdebugger"), "Debugger is highlighted");

  return dbg;
}

async function addExpression(dbg, input) {
  info("Adding an expression");

  const plusIcon = findElementWithSelector(dbg, selectors.expressionPlus);
  if (plusIcon) {
    plusIcon.click();
  }
  findElementWithSelector(dbg, selectors.expressionInput).focus();
  type(dbg, input);
  const evaluated = waitForDispatch(dbg.store, "EVALUATE_EXPRESSION");
  pressKey(dbg, "Enter");
  await evaluated;
}

async function editExpression(dbg, input) {
  info("Updating the expression");
  dblClickElement(dbg, "expressionNode", 1);
  // Position cursor reliably at the end of the text.
  pressKey(dbg, "End");
  type(dbg, input);
  const evaluated = waitForDispatch(dbg.store, "EVALUATE_EXPRESSIONS");
  pressKey(dbg, "Enter");
  await evaluated;
}

/**
 * Get the text representation of a watch expression label given its position in the panel
 *
 * @param {Object} dbg
 * @param {Number} index: Position in the panel of the expression we want the label of
 * @returns {String}
 */
function getWatchExpressionLabel(dbg, index) {
  return findElement(dbg, "expressionNode", index).innerText;
}

/**
 * Get the text representation of a watch expression value given its position in the panel
 *
 * @param {Object} dbg
 * @param {Number} index: Position in the panel of the expression we want the value of
 * @returns {String}
 */
function getWatchExpressionValue(dbg, index) {
  return findElement(dbg, "expressionValue", index).innerText;
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

// Return a promise that resolves with the result of a thread evaluating a
// string in the topmost frame.
async function evaluateInTopFrame(dbg, text) {
  const threadFront = dbg.toolbox.target.threadFront;
  const { frames } = await threadFront.getFrames(0, 1);
  ok(frames.length == 1, "Got one frame");
  const response = await dbg.commands.scriptCommand.execute(text, {
    frameActor: frames[0].actorID,
  });
  return response.result.type == "undefined" ? undefined : response.result;
}

// Return a promise that resolves when a thread evaluates a string in the
// topmost frame, ensuring the result matches the expected value.
async function checkEvaluateInTopFrame(dbg, text, expected) {
  const rval = await evaluateInTopFrame(dbg, text);
  ok(rval == expected, `Eval returned ${expected}`);
}

async function findConsoleMessage({ toolbox }, query) {
  const [message] = await findConsoleMessages(toolbox, query);
  const value = message.querySelector(".message-body").innerText;
  const link = message.querySelector(".frame-link-source").innerText;
  return { value, link };
}

async function findConsoleMessages(toolbox, query) {
  const webConsole = await toolbox.getPanel("webconsole");
  const win = webConsole._frameWindow;
  return Array.prototype.filter.call(
    win.document.querySelectorAll(".message"),
    e => e.innerText.includes(query)
  );
}

async function hasConsoleMessage({ toolbox }, msg) {
  return waitFor(async () => {
    const messages = await findConsoleMessages(toolbox, msg);
    return !!messages.length;
  });
}

function evaluateExpressionInConsole(hud, expression) {
  const seenMessages = new Set(
    JSON.parse(
      hud.ui.outputNode
        .querySelector("[data-visible-messages]")
        .getAttribute("data-visible-messages")
    )
  );
  const onResult = new Promise(res => {
    const onNewMessage = messages => {
      for (const message of messages) {
        if (
          message.node.classList.contains("result") &&
          !seenMessages.has(message.node.getAttribute("data-message-id"))
        ) {
          hud.ui.off("new-messages", onNewMessage);
          res(message.node);
        }
      }
    };
    hud.ui.on("new-messages", onNewMessage);
  });
  hud.ui.wrapper.dispatchEvaluateExpression(expression);
  return onResult;
}

function waitForInspectorPanelChange(dbg) {
  return dbg.toolbox.getPanelWhenReady("inspector");
}

function getEagerEvaluationElement(hud) {
  return hud.ui.outputNode.querySelector(".eager-evaluation-result");
}

async function waitForEagerEvaluationResult(hud, text) {
  await waitUntil(() => {
    const elem = getEagerEvaluationElement(hud);
    if (elem) {
      if (text instanceof RegExp) {
        return text.test(elem.innerText);
      }
      return elem.innerText == text;
    }
    return false;
  });
  ok(true, `Got eager evaluation result ${text}`);
}

function setInputValue(hud, value) {
  const onValueSet = hud.jsterm.once("set-input-value");
  hud.jsterm._setValue(value);
  return onValueSet;
}

function assertMenuItemChecked(menuItem, isChecked) {
  is(
    !!menuItem.getAttribute("aria-checked"),
    isChecked,
    `Item has expected state: ${isChecked ? "checked" : "unchecked"}`
  );
}

async function toggleDebbuggerSettingsMenuItem(dbg, { className, isChecked }) {
  const menuButton = findElementWithSelector(
    dbg,
    ".debugger-settings-menu-button"
  );
  const { parent } = dbg.panel.panelWin;
  const { document } = parent;

  menuButton.click();
  // Waits for the debugger settings panel to appear.
  await waitFor(() => document.querySelector("#debugger-settings-menu-list"));

  const menuItem = document.querySelector(className);

  assertMenuItemChecked(menuItem, isChecked);

  menuItem.click();

  // Waits for the debugger settings panel to disappear.
  await waitFor(() => menuButton.getAttribute("aria-expanded") === "false");
}

async function setLogPoint(dbg, index, value) {
  rightClickElement(dbg, "gutter", index);
  await waitForContextMenu(dbg);
  selectContextMenuItem(
    dbg,
    `${selectors.addLogItem},${selectors.editLogItem}`
  );
  const onBreakpointSet = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  await typeInPanel(dbg, value);
  await onBreakpointSet;
}

/**
 * Instantiate a HTTP Server that serves files from a given test folder.
 * The test folder should be made of multiple sub folder named: v1, v2, v3,...
 * We will serve the content from one of these sub folder
 * and switch to the next one, each time `httpServer.switchToNextVersion()`
 * is called.
 *
 * @return Object Test server with two functions:
 *   - urlFor(path)
 *     Returns the absolute url for a given file.
 *   - switchToNextVersion()
 *     Start serving files from the next available sub folder.
 *   - backToFirstVersion()
 *     When running more than one test, helps restart from the first folder.
 */
function createVersionizedHttpTestServer(testFolderName) {
  const httpServer = createTestHTTPServer();

  let currentVersion = 1;

  httpServer.registerPrefixHandler("/", async (request, response) => {
    response.processAsync();
    response.setStatusLine(request.httpVersion, 200, "OK");
    if (request.path.endsWith(".js")) {
      response.setHeader("Content-Type", "application/javascript");
    } else if (request.path.endsWith(".js.map")) {
      response.setHeader("Content-Type", "application/json");
    }
    if (request.path == "/" || request.path == "/index.html") {
      response.setHeader("Content-Type", "text/html");
    }
    // If a query string is passed, lookup with a matching file, if available
    // The '?' is replaced by '.'
    let fetchResponse;
    if (request.queryString) {
      const url = `${URL_ROOT}${testFolderName}/v${currentVersion}${request.path}.${request.queryString}`;
      try {
        fetchResponse = await fetch(url);
        // Log this only if the request succeed
        info(`[test-http-server] serving: ${url}`);
      } catch (e) {
        // Ignore any error and proceed without the query string
        fetchResponse = null;
      }
    }
    if (!fetchResponse) {
      const url = `${URL_ROOT}${testFolderName}/v${currentVersion}${request.path}`;
      info(`[test-http-server] serving: ${url}`);
      fetchResponse = await fetch(url);
    }

    // Ensure forwarding the response headers generated by the other http server
    // (this can be especially useful when query .sjs files)
    for (const [name, value] of fetchResponse.headers.entries()) {
      response.setHeader(name, value);
    }

    // Override cache settings so that versionized requests are never cached
    // and we get brand new content for any request.
    response.setHeader("Cache-Control", "no-store");

    const text = await fetchResponse.text();
    response.write(text);
    response.finish();
  });

  return {
    switchToNextVersion() {
      currentVersion++;
    },
    backToFirstVersion() {
      currentVersion = 1;
    },
    urlFor(path) {
      const port = httpServer.identity.primaryPort;
      return `http://localhost:${port}/${path}`;
    },
  };
}

// This module is also loaded for Browser Toolbox tests, within the browser toolbox process
// which doesn't contain mochitests resource://testing-common URL.
// This isn't important to allow rejections in the context of the browser toolbox tests.
const protocolHandler = Services.io
  .getProtocolHandler("resource")
  .QueryInterface(Ci.nsIResProtocolHandler);
if (protocolHandler.hasSubstitution("testing-common")) {
  const { PromiseTestUtils } = ChromeUtils.import(
    "resource://testing-common/PromiseTestUtils.jsm"
  );

  // Debugger operations that are canceled because they were rendered obsolete by
  // a navigation or pause/resume end up as uncaught rejections. These never
  // indicate errors and are allowed in all debugger tests.
  PromiseTestUtils.allowMatchingRejectionsGlobally(/Page has navigated/);
  PromiseTestUtils.allowMatchingRejectionsGlobally(
    /Current thread has changed/
  );
  PromiseTestUtils.allowMatchingRejectionsGlobally(
    /Current thread has paused or resumed/
  );
  PromiseTestUtils.allowMatchingRejectionsGlobally(/Connection closed/);
  this.PromiseTestUtils = PromiseTestUtils;
}

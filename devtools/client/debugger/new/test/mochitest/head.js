/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js", this);
var { Toolbox } = require("devtools/client/framework/toolbox");
const EXAMPLE_URL = "http://example.com/browser/devtools/client/debugger/new/test/mochitest/examples/";

Services.prefs.setBoolPref("devtools.debugger.new-debugger-frontend", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.debugger.new-debugger-frontend");
  delete window.resumeTest;
})

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
          return action.status ?
            (action.status === "done" || action.status === "error") :
            true;
        }
      },
      run: (dispatch, getState, action) => {
        resolve(action);
      }
    });
  });
}

function waitForDispatch(dbg, type, eventRepeat = 1) {
  let count = 0;

  return Task.spawn(function* () {
    info("Waiting for " + type + " to dispatch " + eventRepeat + " time(s)");
    while (count < eventRepeat) {
      yield _afterDispatchDone(dbg.store, type);
      count++;
      info(type + " dispatched " + count + " time(s)");
    }
  });
}

function waitForThreadEvents(dbg, eventName) {
  info("Waiting for thread event '" + eventName + "' to fire.");
  const thread = dbg.toolbox.threadClient;

  return new Promise(function(resolve, reject) {
    thread.addListener(eventName, function onEvent(eventName, ...args) {
      info("Thread event '" + eventName + "' fired.");
      thread.removeListener(eventName, onEvent);
      resolve.apply(resolve, args);
    });
  });
}

function waitForState(dbg, predicate) {
  return new Promise(resolve => {
    const unsubscribe = dbg.store.subscribe(() => {
      if (predicate(dbg.store.getState())) {
        unsubscribe();
        resolve();
      }
    });
  });
}

function waitForMs(time) {
  return new Promise(resolve => setTimeout(resolve, time));
}

function assertPausedLocation(dbg, source, line) {
  const { selectors: { getSelectedSource, getPause }, getState } = dbg;

  // support passing in a partial url and fetching the source
  if (typeof source == "string") {
    source = findSource(dbg, source);
  }

  // check the selected source
  is(getSelectedSource(getState()).get("url"), source.url);

  // check the pause location
  const location = getPause(getState()).getIn(["frame", "location"]);
  is(location.get("sourceId"), source.id);
  is(location.get("line"), line);

  // check the debug line
  ok(dbg.win.cm.lineInfo(line - 1).wrapClass.includes("debug-line"),
     "Line is highlighted");
}

function isPaused(dbg) {
  const { selectors: { getPause }, getState } = dbg;
  return !!getPause(getState());
}

const waitForPaused = Task.async(function* (dbg) {
  // We want to make sure that we get both a real paused event and
  // that the state is fully populated. The client may do some more
  // work (call other client methods) before populating the state.
  return Promise.all([
    yield waitForThreadEvents(dbg, "paused"),
    yield waitForState(dbg, state => {
      const pause = dbg.selectors.getPause(state);
      // Make sure we have the paused state.
      if(!pause) {
        return false;
      }
      // Make sure the source text is completely loaded for the
      // source we are paused in.
      const sourceId = pause.getIn(["frame", "location", "sourceId"]);
      const sourceText = dbg.selectors.getSourceText(dbg.getState(), sourceId);
      return sourceText && !sourceText.get("loading");
    })
  ]);
});

const initDebugger = Task.async(function* (url, ...sources) {
  const toolbox = yield openNewTabAndToolbox(EXAMPLE_URL + url, "jsdebugger");
  const win = toolbox.getPanel("jsdebugger").panelWin;
  const store = win.Debugger.store;
  const { getSources } = win.Debugger.selectors;

  const dbg = {
    actions: win.Debugger.actions,
    selectors: win.Debugger.selectors,
    getState: store.getState,
    store: store,
    client: win.Debugger.client,
    toolbox: toolbox,
    win: win
  };

  if(sources.length) {
    // TODO: Extract this out to a utility function
    info("Waiting on sources: " + sources.join(", "));
    yield Promise.all(sources.map(url => {
      function sourceExists(state) {
        return getSources(state).some(s => s.get("url").includes(url));
      }

      if(!sourceExists(store.getState())) {
        return waitForState(dbg, sourceExists);
      }
    }));
  }

  return dbg;
});

window.resumeTest = undefined;
function pauseTest() {
  info("Test paused. Invoke resumeTest to continue.");
  return new Promise(resolve => resumeTest = resolve);
}

// Actions

function findSource(dbg, url) {
  const sources = dbg.selectors.getSources(dbg.getState());
  const source = sources.find(s => s.get("url").includes(url));

  if(!source) {
    throw new Error("Unable to find source: " + url);
  }

  return source.toJS();
}

function selectSource(dbg, url) {
  info("Selecting source: " + url);
  const source = findSource(dbg, url);
  dbg.actions.selectSource(source.id);

  return waitForDispatch(dbg, "LOAD_SOURCE_TEXT");
}

function stepOver(dbg) {
  info("Stepping over");
  dbg.actions.stepOver();
  return waitForPaused(dbg);
}

function stepIn(dbg) {
  info("Stepping in");
  dbg.actions.stepIn();
  return waitForPaused(dbg);
}

function stepOut(dbg) {
  info("Stepping out");
  dbg.actions.stepOut();
  return waitForPaused(dbg);
}

function resume(dbg) {
  info("Resuming");
  dbg.actions.resume();
  return waitForThreadEvents(dbg, "resumed");
}

function reload(dbg) {
  return dbg.client.reload();
}

function addBreakpoint(dbg, sourceId, line, col) {
  return dbg.actions.addBreakpoint({ sourceId, line, col });
}

function togglePauseOnExceptions(dbg,
  pauseOnExceptions, ignoreCaughtExceptions) {

  const command = dbg.actions.pauseOnExceptions(
    pauseOnExceptions,
    ignoreCaughtExceptions
  );

  if (!isPaused(dbg)) {
    return waitForThreadEvents(dbg, "resumed");
  }

  return command;
}

// Helpers
// invoke a global function in the debugged tab
function invokeInTab(fnc) {
  return ContentTask.spawn(gBrowser.selectedBrowser, fnc, function* (fnc) {
    content.wrappedJSObject[fnc](); // eslint-disable-line mozilla/no-cpows-in-tests, max-len
  });
}


function isVisibleWithin(outerEl, innerEl) {
  const innerRect = innerEl.getBoundingClientRect();
  const outerRect = outerEl.getBoundingClientRect();
  return innerRect.top > outerRect.top &&
    innerRect.bottom < outerRect.bottom;
}

const selectors = {
  callStackHeader: ".call-stack-pane ._header",
  frame: index => `.frames ul li:nth-child(${index})`,
  gutter: i => `.CodeMirror-code *:nth-child(${i}) .CodeMirror-linenumber`,
  pauseOnExceptions: ".pause-exceptions",
  breakpoint: ".CodeMirror-code > .new-breakpoint",
  codeMirror: ".CodeMirror",
  resume: ".resume.active",
  stepOver: ".stepOver.active",
  stepOut: ".stepOut.active",
  stepIn: ".stepIn.active"
}

function getSelector(elementName, ...args) {
  let selector = selectors[elementName];
  if (!selector) {
    throw new Error(`The selector ${elementName} is not defined`);
  }

  if (typeof selector == "function") {
    selector = selector(...args)
  }

  return selector;
}

function findElement(dbg, elementName, ...args) {
  const selector = getSelector(elementName, ...args);
  return dbg.win.document.querySelector(selector);
}

// click an element in the debugger
function clickElement(dbg, elementName, ...args) {
  const selector = getSelector(elementName, ...args);
  const doc = dbg.win.document;
  return EventUtils.synthesizeMouseAtCenter(
    doc.querySelector(selector),
    {},
    dbg.win
  );
}

function toggleCallStack(dbg) {
  return findElement(dbg, "callStackHeader").click()
}

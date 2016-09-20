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

function waitForSources(dbg, ...sources) {
  if(sources.length === 0) {
    return Promise.resolve();
  }

  info("Waiting on sources: " + sources.join(", "));
  const {selectors: {getSources}, store} = dbg;
  return Promise.all(sources.map(url => {
    function sourceExists(state) {
      return getSources(state).some(s => s.get("url").includes(url));
    }

    if(!sourceExists(store.getState())) {
      return waitForState(dbg, sourceExists);
    }
  }));
}

function assertPausedLocation(dbg, source, line) {
  const { selectors: { getSelectedSource, getPause }, getState } = dbg;
  source = findSource(dbg, source);

  // Check the selected source
  is(getSelectedSource(getState()).get("id"), source.id);

  // Check the pause location
  const location = getPause(getState()).getIn(["frame", "location"]);
  is(location.get("sourceId"), source.id);
  is(location.get("line"), line);

  // Check the debug line
  ok(dbg.win.cm.lineInfo(line - 1).wrapClass.includes("debug-line"),
     "Line is highlighted as paused");
}

function assertHighlightLocation(dbg, source, line) {
  const { selectors: { getSelectedSource, getPause }, getState } = dbg;
  source = findSource(dbg, source);

  // Check the selected source
  is(getSelectedSource(getState()).get("url"), source.url);

  // Check the highlight line
  const lineEl = findElement(dbg, "highlightLine");
  ok(lineEl, "Line is highlighted");
  ok(isVisibleWithin(findElement(dbg, "codeMirror"), lineEl),
     "Highlighted line is visible");
  ok(dbg.win.cm.lineInfo(line - 1).wrapClass.includes("highlight-line"),
     "Line is highlighted");
}

function isPaused(dbg) {
  const { selectors: { getPause }, getState } = dbg;
  return !!getPause(getState());
}

function waitForPaused(dbg) {
  return Task.spawn(function* () {
    // We want to make sure that we get both a real paused event and
    // that the state is fully populated. The client may do some more
    // work (call other client methods) before populating the state.
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
    });
  });
};

function createDebuggerContext(toolbox) {
  const win = toolbox.getPanel("jsdebugger").panelWin;
  const store = win.Debugger.store;

  return {
    actions: win.Debugger.actions,
    selectors: win.Debugger.selectors,
    getState: store.getState,
    store: store,
    client: win.Debugger.client,
    toolbox: toolbox,
    win: win
  };
}

function initDebugger(url, ...sources) {
  return Task.spawn(function* () {
    const toolbox = yield openNewTabAndToolbox(EXAMPLE_URL + url, "jsdebugger");
    const dbg = createDebuggerContext(toolbox);
    yield waitForSources(dbg, ...sources);
    return dbg;
  });
};

window.resumeTest = undefined;
function pauseTest() {
  info("Test paused. Invoke resumeTest to continue.");
  return new Promise(resolve => resumeTest = resolve);
}

// Actions

function findSource(dbg, url) {
  if(typeof url !== "string") {
    // Support passing in a source object itelf all APIs that use this
    // function support both styles
    const source = url;
    return source;
  }

  const sources = dbg.selectors.getSources(dbg.getState());
  const source = sources.find(s => s.get("url").includes(url));

  if(!source) {
    throw new Error("Unable to find source: " + url);
  }

  return source.toJS();
}

function selectSource(dbg, url, line) {
  info("Selecting source: " + url);
  const source = findSource(dbg, url);
  const hasText = !!dbg.selectors.getSourceText(dbg.getState(), source.id);
  dbg.actions.selectSource(source.id, { line });

  if(!hasText) {
    return waitForDispatch(dbg, "LOAD_SOURCE_TEXT");
  }
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

function reload(dbg, ...sources) {
  return dbg.client.reload().then(() => waitForSources(...sources));
}

function navigate(dbg, url, ...sources) {
  dbg.client.navigate(url);
  return waitForSources(dbg, ...sources)
}

function addBreakpoint(dbg, source, line, col) {
  source = findSource(dbg, source);
  const sourceId = source.id;
  return dbg.actions.addBreakpoint({ sourceId, line, col });
}

function removeBreakpoint(dbg, sourceId, line, col) {
  return dbg.actions.removeBreakpoint({ sourceId, line, col });
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
  info(`Invoking function ${fnc} in tab`);
  return ContentTask.spawn(gBrowser.selectedBrowser, fnc, function* (fnc) {
    content.wrappedJSObject[fnc](); // eslint-disable-line mozilla/no-cpows-in-tests, max-len
  });
}

const isLinux = Services.appinfo.OS === "Linux";
const keyMappings = {
  pauseKey: { code: "VK_F8" },
  resumeKey: { code: "VK_F8" },
  stepOverKey: { code: "VK_F10" },
  stepInKey: { code: "VK_F11", modifiers: { ctrlKey: isLinux } },
  stepOutKey: { code: "VK_F11", modifiers: { ctrlKey: isLinux, shiftKey: true } }
};

function pressKey(dbg, keyName) {
  let keyEvent = keyMappings[keyName];
  const { code, modifiers } = keyEvent;
  return EventUtils.synthesizeKey(
    code,
    modifiers || {},
    dbg.win
  );
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
  highlightLine: ".CodeMirror-code > .highlight-line",
  codeMirror: ".CodeMirror",
  resume: ".resume.active",
  stepOver: ".stepOver.active",
  stepOut: ".stepOut.active",
  stepIn: ".stepIn.active"
};

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

function findAllElements(dbg, elementName, ...args) {
  const selector = getSelector(elementName, ...args);
  return dbg.win.document.querySelectorAll(selector);
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

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const dbginfo = new WeakMap();

// These functions implement search within the debugger. Since
// search in the debugger is different from other components,
// we can't use search.js CodeMirror addon. This is a slightly
// modified version of that addon. Depends on searchcursor.js.

function SearchState() {
  this.posFrom = this.posTo = this.query = null;
}

function getSearchState(cm) {
  return cm.state.search || (cm.state.search = new SearchState());
}

function getSearchCursor(cm, query, pos) {
  // If the query string is all lowercase, do a case insensitive search.
  return cm.getSearchCursor(query, pos,
    typeof query == "string" && query == query.toLowerCase());
}

/**
 * If there's a saved search, selects the next results.
 * Otherwise, creates a new search and selects the first
 * result.
 */
function doSearch(ctx, rev, query) {
  const { cm } = ctx;
  const state = getSearchState(cm);

  if (state.query) {
    searchNext(ctx, rev);
    return;
  }

  cm.operation(function() {
    if (state.query) {
      return;
    }

    state.query = query;
    state.posFrom = state.posTo = { line: 0, ch: 0 };
    searchNext(ctx, rev);
  });
}

/**
 * Selects the next result of a saved search.
 */
function searchNext(ctx, rev) {
  const { cm, ed } = ctx;
  cm.operation(function() {
    const state = getSearchState(cm);
    let cursor = getSearchCursor(cm, state.query,
                                 rev ? state.posFrom : state.posTo);

    if (!cursor.find(rev)) {
      cursor = getSearchCursor(cm, state.query, rev ?
        { line: cm.lastLine(), ch: null } : { line: cm.firstLine(), ch: 0 });
      if (!cursor.find(rev)) {
        return;
      }
    }

    ed.alignLine(cursor.from().line, "center");
    cm.setSelection(cursor.from(), cursor.to());
    state.posFrom = cursor.from();
    state.posTo = cursor.to();
  });
}

/**
 * Clears the currently saved search.
 */
function clearSearch(cm) {
  const state = getSearchState(cm);

  if (!state.query) {
    return;
  }

  state.query = null;
}

// Exported functions

/**
 * This function is called whenever Editor is extended with functions
 * from this module. See Editor.extend for more info.
 */
function initialize(ctx) {
  const { ed } = ctx;

  dbginfo.set(ed, {
    breakpoints: {},
    debugLocation: null
  });
}

/**
 * True if editor has a visual breakpoint at that line, false
 * otherwise.
 */
function hasBreakpoint(ctx, line) {
  const { ed } = ctx;
  // In some rare occasions CodeMirror might not be properly initialized yet, so
  // return an exceptional value in that case.
  if (ed.lineInfo(line) === null) {
    return null;
  }
  const markers = ed.lineInfo(line).wrapClass;

  return markers != null &&
         markers.includes("breakpoint");
}

/**
 * Adds a visual breakpoint for a specified line. Third
 * parameter 'cond' can hold any object.
 *
 * After adding a breakpoint, this function makes Editor to
 * emit a breakpointAdded event.
 */
function addBreakpoint(ctx, line, cond) {
  if (hasBreakpoint(ctx, line)) {
    return null;
  }

  return new Promise(resolve => {
    function _addBreakpoint() {
      const { ed } = ctx;
      const meta = dbginfo.get(ed);
      const info = ed.lineInfo(line);

      // The line does not exist in the editor. This is harmless, the
      // architecture calling this assumes the editor will handle this
      // gracefully, and make sure breakpoints exist when they need to.
      if (!info) {
        return;
      }

      ed.addLineClass(line, "breakpoint");
      meta.breakpoints[line] = { condition: cond };

      // TODO(jwl): why is `info` null when breaking on page reload?
      info.handle.on("delete", function onDelete() {
        info.handle.off("delete", onDelete);
        meta.breakpoints[info.line] = null;
      });

      if (cond) {
        setBreakpointCondition(ctx, line);
      }
      ed.emit("breakpointAdded", line);
      resolve();
    }

    // If lineInfo() returns null, wait a tick to give the editor a chance to
    // initialize properly.
    if (ctx.ed.lineInfo(line) === null) {
      DevToolsUtils.executeSoon(() => _addBreakpoint());
    } else {
      _addBreakpoint();
    }
  });
}

/**
 * Helps reset the debugger's breakpoint state
 * - removes the breakpoints in the editor
 * - cleares the debugger's breakpoint state
 *
 * Note, does not *actually* remove a source's breakpoints.
 * The canonical state is kept in the app state.
 *
 */
function removeBreakpoints(ctx) {
  const { ed, cm } = ctx;

  const meta = dbginfo.get(ed);
  if (meta.breakpoints != null) {
    meta.breakpoints = {};
  }

  cm.doc.iter((line) => {
    // The hasBreakpoint is a slow operation: checks the line type, whether cm
    // is initialized and creates several new objects. Inlining the line's
    // wrapClass property check directly.
    if (line.wrapClass == null || !line.wrapClass.includes("breakpoint")) {
      return;
    }
    removeBreakpoint(ctx, line);
  });
}

/**
 * Removes a visual breakpoint from a specified line and
 * makes Editor emit a breakpointRemoved event.
 */
function removeBreakpoint(ctx, line) {
  if (!hasBreakpoint(ctx, line)) {
    return;
  }

  const { ed } = ctx;
  const meta = dbginfo.get(ed);
  const info = ed.lineInfo(line);
  const lineOrOffset = ed.getLineOrOffset(info.line);

  meta.breakpoints[lineOrOffset] = null;
  ed.removeLineClass(lineOrOffset, "breakpoint");
  ed.removeLineClass(lineOrOffset, "conditional");
  ed.emit("breakpointRemoved", line);
}

function moveBreakpoint(ctx, fromLine, toLine) {
  const { ed } = ctx;

  ed.removeBreakpoint(fromLine);
  ed.addBreakpoint(toLine);
}

function setBreakpointCondition(ctx, line) {
  const { ed } = ctx;
  const info = ed.lineInfo(line);

  // The line does not exist in the editor. This is harmless, the
  // architecture calling this assumes the editor will handle this
  // gracefully, and make sure breakpoints exist when they need to.
  if (!info) {
    return;
  }

  ed.addLineClass(line, "conditional");
}

function removeBreakpointCondition(ctx, line) {
  const { ed } = ctx;

  ed.removeLineClass(line, "conditional");
}

/**
 * Returns a list of all breakpoints in the current Editor.
 */
function getBreakpoints(ctx) {
  const { ed } = ctx;
  const meta = dbginfo.get(ed);

  return Object.keys(meta.breakpoints).reduce((acc, line) => {
    if (meta.breakpoints[line] != null) {
      acc.push({ line: line, condition: meta.breakpoints[line].condition });
    }
    return acc;
  }, []);
}

/**
 * Saves a debug location information and adds a visual anchor to
 * the breakpoints gutter. This is used by the debugger UI to
 * display the line on which the Debugger is currently paused.
 */
function setDebugLocation(ctx, lineOrOffset) {
  const { ed } = ctx;
  const meta = dbginfo.get(ed);
  const line = ed.getLineOrOffset(lineOrOffset);

  clearDebugLocation(ctx);

  meta.debugLocation = line;
  ed.addLineClass(line, "debug-line");
}

/**
 * Returns a line number that corresponds to the current debug
 * location.
 */
function getDebugLocation(ctx) {
  const { ed } = ctx;
  const meta = dbginfo.get(ed);

  return meta.debugLocation;
}

/**
 * Clears the debug location. Clearing the debug location
 * also removes a visual anchor from the breakpoints gutter.
 */
function clearDebugLocation(ctx) {
  const { ed } = ctx;
  const meta = dbginfo.get(ed);

  if (meta.debugLocation != null) {
    ed.removeLineClass(meta.debugLocation, "debug-line");
    meta.debugLocation = null;
  }
}

/**
 * Starts a new search.
 */
function find(ctx, query) {
  clearSearch(ctx.cm);
  doSearch(ctx, false, query);
}

/**
 * Finds the next item based on the currently saved search.
 */
function findNext(ctx, query) {
  doSearch(ctx, false, query);
}

/**
 * Finds the previous item based on the currently saved search.
 */
function findPrev(ctx, query) {
  doSearch(ctx, true, query);
}

// Export functions

[
  initialize, hasBreakpoint, addBreakpoint, removeBreakpoint, moveBreakpoint,
  setBreakpointCondition, removeBreakpointCondition, getBreakpoints, removeBreakpoints,
  setDebugLocation, getDebugLocation, clearDebugLocation, find, findNext,
  findPrev
].forEach(func => {
  module.exports[func.name] = func;
});

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 "use strict";

const {Cu} = require("chrome");
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
const promise = require("promise");
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
  let { cm } = ctx;
  let state = getSearchState(cm);

  if (state.query) {
    searchNext(ctx, rev);
    return;
  }

  cm.operation(function () {
    if (state.query) return;

    state.query = query;
    state.posFrom = state.posTo = { line: 0, ch: 0 };
    searchNext(ctx, rev);
  });
}

/**
 * Selects the next result of a saved search.
 */
function searchNext(ctx, rev) {
  let { cm, ed } = ctx;
  cm.operation(function () {
    let state = getSearchState(cm)
    let cursor = getSearchCursor(cm, state.query, rev ? state.posFrom : state.posTo);

    if (!cursor.find(rev)) {
      cursor = getSearchCursor(cm, state.query, rev ?
        { line: cm.lastLine(), ch: null } : { line: cm.firstLine(), ch: 0 });
      if (!cursor.find(rev))
        return;
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
  let state = getSearchState(cm);

  if (!state.query)
    return;

  state.query = null;
}

// Exported functions

/**
 * This function is called whenever Editor is extended with functions
 * from this module. See Editor.extend for more info.
 */
function initialize(ctx) {
  let { ed } = ctx;

  dbginfo.set(ed, {
    breakpoints:   {},
    debugLocation: null
  });
}

/**
 * True if editor has a visual breakpoint at that line, false
 * otherwise.
 */
function hasBreakpoint(ctx, line) {
  let { cm } = ctx;
  // In some rare occasions CodeMirror might not be properly initialized yet, so
  // return an exceptional value in that case.
  if (cm.lineInfo(line) === null) {
    return null;
  }
  let markers = cm.lineInfo(line).gutterMarkers;

  return markers != null &&
    markers.breakpoints &&
    markers.breakpoints.classList.contains("breakpoint");
}

/**
 * Adds a visual breakpoint for a specified line. Third
 * parameter 'cond' can hold any object.
 *
 * After adding a breakpoint, this function makes Editor to
 * emit a breakpointAdded event.
 */
function addBreakpoint(ctx, line, cond) {
  function _addBreakpoint(ctx, line, cond) {
    let { ed, cm } = ctx;
    let meta = dbginfo.get(ed);
    let info = cm.lineInfo(line);

    ed.addMarker(line, "breakpoints", "breakpoint");
    meta.breakpoints[line] = { condition: cond };

    info.handle.on("delete", function onDelete() {
      info.handle.off("delete", onDelete);
      meta.breakpoints[info.line] = null;
    });

    ed.emit("breakpointAdded", line);
    deferred.resolve();
  }

  if (hasBreakpoint(ctx, line))
    return;

  let deferred = promise.defer();
  // If lineInfo() returns null, wait a tick to give the editor a chance to
  // initialize properly.
  if (ctx.cm.lineInfo(line) === null) {
    DevToolsUtils.executeSoon(() => _addBreakpoint(ctx, line, cond));
  } else {
    _addBreakpoint(ctx, line, cond);
  }
  return deferred.promise;
}

/**
 * Removes a visual breakpoint from a specified line and
 * makes Editor to emit a breakpointRemoved event.
 */
function removeBreakpoint(ctx, line) {
  if (!hasBreakpoint(ctx, line))
    return;

  let { ed, cm } = ctx;
  let meta = dbginfo.get(ed);
  let info = cm.lineInfo(line);

  meta.breakpoints[info.line] = null;
  ed.removeMarker(info.line, "breakpoints", "breakpoint");
  ed.emit("breakpointRemoved", line);
}

function moveBreakpoint(ctx, fromLine, toLine) {
  let { ed, cm } = ctx;
  let info = cm.lineInfo(fromLine);

  var fromTop = cm.cursorCoords({ line: fromLine }).top;
  var toTop = cm.cursorCoords({ line: toLine }).top;

  var marker = ed.getMarker(info.line, "breakpoints", "breakpoint");
  if (marker) {
    marker.setAttribute("adding", "");
    marker.style.transform = "translateY(" + (toTop - fromTop) + "px)";
    marker.addEventListener('transitionend', function(e) {
      ed.removeBreakpoint(info.line);
      ed.addBreakpoint(toLine);

      // For some reason, we have to reset the styles after the marker
      // is already removed, not before.
      e.target.removeAttribute("adding");
      e.target.style.transform = "none";
    });
  }
}

/**
 * Returns a list of all breakpoints in the current Editor.
 */
function getBreakpoints(ctx) {
  let { ed } = ctx;
  let meta = dbginfo.get(ed);

  return Object.keys(meta.breakpoints).reduce((acc, line) => {
    if (meta.breakpoints[line] != null)
      acc.push({ line: line, condition: meta.breakpoints[line].condition });
    return acc;
  }, []);
}

/**
 * Saves a debug location information and adds a visual anchor to
 * the breakpoints gutter. This is used by the debugger UI to
 * display the line on which the Debugger is currently paused.
 */
function setDebugLocation(ctx, line) {
  let { ed } = ctx;
  let meta = dbginfo.get(ed);

  clearDebugLocation(ctx);

  meta.debugLocation = line;
  ed.addMarker(line, "breakpoints", "debugLocation");
  ed.addLineClass(line, "debug-line");
}

/**
 * Returns a line number that corresponds to the current debug
 * location.
 */
function getDebugLocation(ctx) {
  let { ed } = ctx;
  let meta = dbginfo.get(ed);

  return meta.debugLocation;
}

/**
 * Clears the debug location. Clearing the debug location
 * also removes a visual anchor from the breakpoints gutter.
 */
function clearDebugLocation(ctx) {
  let { ed } = ctx;
  let meta = dbginfo.get(ed);

  if (meta.debugLocation != null) {
    ed.removeMarker(meta.debugLocation, "breakpoints", "debugLocation");
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
  initialize, hasBreakpoint, addBreakpoint, removeBreakpoint,
  moveBreakpoint, getBreakpoints, setDebugLocation, getDebugLocation,
  clearDebugLocation, find, findNext, findPrev
].forEach(function (func) { module.exports[func.name] = func; });

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import buildQuery from "../build-query";

/**
 * @memberof utils/source-search
 * @static
 */
function getSearchCursor(cm, query, pos, modifiers) {
  const regexQuery = buildQuery(query, modifiers, { isGlobal: true });
  return cm.getSearchCursor(regexQuery, pos);
}

/**
 * @memberof utils/source-search
 * @static
 */
function SearchState() {
  this.posFrom = this.posTo = this.query = null;
  this.overlay = null;
  this.results = [];
}

/**
 * @memberof utils/source-search
 * @static
 */
function getSearchState(cm, query) {
  const state = cm.state.search || (cm.state.search = new SearchState());
  return state;
}

function isWhitespace(query) {
  return !query.match(/\S/);
}

/**
 * This returns a mode object used by CodeMirror's addOverlay function
 * to parse and style tokens in the file.
 * The mode object contains a tokenizer function (token) which takes
 * a character stream as input, advances it a character at a time,
 * and returns style(s) for that token. For more details see
 * https://codemirror.net/5/doc/manual.html#modeapi
 *
 * @memberof utils/source-search
 * @static
 */
function searchOverlay(query, modifiers) {
  const regexQuery = buildQuery(query, modifiers, {
    ignoreSpaces: true,
    // regex must be global for the overlay
    isGlobal: true,
  });

  return {
    token: function(stream, state) {
      // set the last index to be the current stream position
      // this acts as an offset
      regexQuery.lastIndex = stream.pos;
      const match = regexQuery.exec(stream.string);
      if (match && match.index === stream.pos) {
        // if we have a match at the current stream position
        // set the class for a match
        stream.pos += match[0].length || 1;
        return "highlight highlight-full";
      }

      if (match) {
        // if we have a match somewhere in the line, go to that point in the
        // stream
        stream.pos = match.index;
      } else {
        // if we have no matches in this line, skip to the end of the line
        stream.skipToEnd();
      }

      return null;
    },
  };
}

/**
 * @memberof utils/source-search
 * @static
 */
function updateOverlay(cm, state, query, modifiers) {
  cm.removeOverlay(state.overlay);
  state.overlay = searchOverlay(query, modifiers);
  cm.addOverlay(state.overlay, { opaque: false });
}

function updateCursor(cm, state, keepSelection) {
  state.posTo = cm.getCursor("anchor");
  state.posFrom = cm.getCursor("head");

  if (!keepSelection) {
    state.posTo = { line: 0, ch: 0 };
    state.posFrom = { line: 0, ch: 0 };
  }
}

export function getMatchIndex(count, currentIndex, rev) {
  if (!rev) {
    if (currentIndex == count - 1) {
      return 0;
    }

    return currentIndex + 1;
  }

  if (currentIndex == 0) {
    return count - 1;
  }

  return currentIndex - 1;
}

/**
 * If there's a saved search, selects the next results.
 * Otherwise, creates a new search and selects the first
 * result.
 *
 * @memberof utils/source-search
 * @static
 */
function doSearch(
  ctx,
  rev,
  query,
  keepSelection,
  modifiers,
  focusFirstResult = true
) {
  const { cm, ed } = ctx;
  if (!cm) {
    return null;
  }
  const defaultIndex = { line: -1, ch: -1 };

  return cm.operation(function() {
    if (!query || isWhitespace(query)) {
      clearSearch(cm, query);
      return null;
    }

    const state = getSearchState(cm, query);
    const isNewQuery = state.query !== query;
    state.query = query;

    updateOverlay(cm, state, query, modifiers);
    updateCursor(cm, state, keepSelection);
    const searchLocation = searchNext(ctx, rev, query, isNewQuery, modifiers);

    // We don't want to jump the editor
    // when we're selecting text
    if (!cm.state.selectingText && searchLocation && focusFirstResult) {
      ed.alignLine(searchLocation.from.line, "center");
      cm.setSelection(searchLocation.from, searchLocation.to);
    }

    return searchLocation ? searchLocation.from : defaultIndex;
  });
}

export function searchSourceForHighlight(
  ctx,
  rev,
  query,
  keepSelection,
  modifiers,
  line,
  ch
) {
  const { cm } = ctx;
  if (!cm) {
    return;
  }

  cm.operation(function() {
    const state = getSearchState(cm, query);
    const isNewQuery = state.query !== query;
    state.query = query;

    updateOverlay(cm, state, query, modifiers);
    updateCursor(cm, state, keepSelection);
    findNextOnLine(ctx, rev, query, isNewQuery, modifiers, line, ch);
  });
}

function getCursorPos(newQuery, rev, state) {
  if (newQuery) {
    return rev ? state.posFrom : state.posTo;
  }

  return rev ? state.posTo : state.posFrom;
}

/**
 * Selects the next result of a saved search.
 *
 * @memberof utils/source-search
 * @static
 */
function searchNext(ctx, rev, query, newQuery, modifiers) {
  const { cm } = ctx;
  let nextMatch;
  cm.operation(function() {
    const state = getSearchState(cm, query);
    const pos = getCursorPos(newQuery, rev, state);

    if (!state.query) {
      return;
    }

    let cursor = getSearchCursor(cm, state.query, pos, modifiers);

    const location = rev
      ? { line: cm.lastLine(), ch: null }
      : { line: cm.firstLine(), ch: 0 };

    if (!cursor.find(rev) && state.query) {
      cursor = getSearchCursor(cm, state.query, location, modifiers);
      if (!cursor.find(rev)) {
        return;
      }
    }

    nextMatch = { from: cursor.from(), to: cursor.to() };
  });

  return nextMatch;
}

function findNextOnLine(ctx, rev, query, newQuery, modifiers, line, ch) {
  const { cm, ed } = ctx;
  cm.operation(function() {
    const pos = { line: line - 1, ch };
    let cursor = getSearchCursor(cm, query, pos, modifiers);

    if (!cursor.find(rev) && query) {
      cursor = getSearchCursor(cm, query, pos, modifiers);
      if (!cursor.find(rev)) {
        return;
      }
    }

    // We don't want to jump the editor
    // when we're selecting text
    if (!cm.state.selectingText) {
      ed.alignLine(cursor.from().line, "center");
      cm.setSelection(cursor.from(), cursor.to());
    }
  });
}

/**
 * Remove overlay.
 *
 * @memberof utils/source-search
 * @static
 */
export function removeOverlay(ctx, query) {
  const state = getSearchState(ctx.cm, query);
  ctx.cm.removeOverlay(state.overlay);
  const { line, ch } = ctx.cm.getCursor();
  ctx.cm.doc.setSelection({ line, ch }, { line, ch }, { scroll: false });
}

/**
 * Clears the currently saved search.
 *
 * @memberof utils/source-search
 * @static
 */
export function clearSearch(cm, query) {
  const state = getSearchState(cm, query);

  state.results = [];

  if (!state.query) {
    return;
  }
  cm.removeOverlay(state.overlay);
  state.query = null;
}

/**
 * Starts a new search.
 *
 * @memberof utils/source-search
 * @static
 */
export function find(ctx, query, keepSelection, modifiers, focusFirstResult) {
  clearSearch(ctx.cm, query);
  return doSearch(
    ctx,
    false,
    query,
    keepSelection,
    modifiers,
    focusFirstResult
  );
}

/**
 * Finds the next item based on the currently saved search.
 *
 * @memberof utils/source-search
 * @static
 */
export function findNext(ctx, query, keepSelection, modifiers) {
  return doSearch(ctx, false, query, keepSelection, modifiers);
}

/**
 * Finds the previous item based on the currently saved search.
 *
 * @memberof utils/source-search
 * @static
 */
export function findPrev(ctx, query, keepSelection, modifiers) {
  return doSearch(ctx, true, query, keepSelection, modifiers);
}

export { buildQuery };

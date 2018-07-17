"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.doSearch = doSearch;
exports.doSearchForHighlight = doSearchForHighlight;
exports.setFileSearchQuery = setFileSearchQuery;
exports.toggleFileSearchModifier = toggleFileSearchModifier;
exports.updateSearchResults = updateSearchResults;
exports.searchContents = searchContents;
exports.searchContentsForHighlight = searchContentsForHighlight;
exports.traverseResults = traverseResults;
exports.closeFileSearch = closeFileSearch;

var _editor = require("../utils/editor/index");

var _search = require("../workers/search/index");

var _selectors = require("../selectors/index");

var _ui = require("./ui");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function doSearch(query, editor) {
  return ({
    getState,
    dispatch
  }) => {
    const selectedSource = (0, _selectors.getSelectedSource)(getState());

    if (!selectedSource || !selectedSource.text) {
      return;
    }

    dispatch(setFileSearchQuery(query));
    dispatch(searchContents(query, editor));
  };
}

function doSearchForHighlight(query, editor, line, ch) {
  return async ({
    getState,
    dispatch
  }) => {
    const selectedSource = (0, _selectors.getSelectedSource)(getState());

    if (!selectedSource || !selectedSource.text) {
      return;
    }

    dispatch(searchContentsForHighlight(query, editor, line, ch));
  };
}

function setFileSearchQuery(query) {
  return {
    type: "UPDATE_FILE_SEARCH_QUERY",
    query
  };
}

function toggleFileSearchModifier(modifier) {
  return {
    type: "TOGGLE_FILE_SEARCH_MODIFIER",
    modifier
  };
}

function updateSearchResults(characterIndex, line, matches) {
  const matchIndex = matches.findIndex(elm => elm.line === line && elm.ch === characterIndex);
  return {
    type: "UPDATE_SEARCH_RESULTS",
    results: {
      matches,
      matchIndex,
      count: matches.length,
      index: characterIndex
    }
  };
}

function searchContents(query, editor) {
  return async ({
    getState,
    dispatch
  }) => {
    const modifiers = (0, _selectors.getFileSearchModifiers)(getState());
    const selectedSource = (0, _selectors.getSelectedSource)(getState());

    if (!editor || !selectedSource || !selectedSource.text || !modifiers) {
      return;
    }

    const ctx = {
      ed: editor,
      cm: editor.codeMirror
    };

    if (!query) {
      (0, _editor.clearSearch)(ctx.cm, query);
      return;
    }

    const _modifiers = modifiers.toJS();

    const matches = await (0, _search.getMatches)(query, selectedSource.text, _modifiers);
    const res = (0, _editor.find)(ctx, query, true, _modifiers);

    if (!res) {
      return;
    }

    const {
      ch,
      line
    } = res;
    dispatch(updateSearchResults(ch, line, matches));
  };
}

function searchContentsForHighlight(query, editor, line, ch) {
  return async ({
    getState,
    dispatch
  }) => {
    const modifiers = (0, _selectors.getFileSearchModifiers)(getState());
    const selectedSource = (0, _selectors.getSelectedSource)(getState());

    if (!query || !editor || !selectedSource || !selectedSource.text || !modifiers) {
      return;
    }

    const ctx = {
      ed: editor,
      cm: editor.codeMirror
    };

    const _modifiers = modifiers.toJS();

    (0, _editor.searchSourceForHighlight)(ctx, false, query, true, _modifiers, line, ch);
  };
}

function traverseResults(rev, editor) {
  return async ({
    getState,
    dispatch
  }) => {
    if (!editor) {
      return;
    }

    const ctx = {
      ed: editor,
      cm: editor.codeMirror
    };
    const query = (0, _selectors.getFileSearchQuery)(getState());
    const modifiers = (0, _selectors.getFileSearchModifiers)(getState());
    const {
      matches
    } = (0, _selectors.getFileSearchResults)(getState());

    if (query === "") {
      dispatch((0, _ui.setActiveSearch)("file"));
    }

    if (modifiers) {
      const matchedLocations = matches || [];
      const results = rev ? (0, _editor.findPrev)(ctx, query, true, modifiers.toJS()) : (0, _editor.findNext)(ctx, query, true, modifiers.toJS());

      if (!results) {
        return;
      }

      const {
        ch,
        line
      } = results;
      dispatch(updateSearchResults(ch, line, matchedLocations));
    }
  };
}

function closeFileSearch(editor) {
  return ({
    getState,
    dispatch
  }) => {
    const query = (0, _selectors.getFileSearchQuery)(getState());

    if (editor) {
      const ctx = {
        ed: editor,
        cm: editor.codeMirror
      };
      (0, _editor.removeOverlay)(ctx, query);
    }

    dispatch(setFileSearchQuery(""));
    dispatch((0, _ui.closeActiveSearch)());
    dispatch((0, _ui.clearHighlightLineRange)());
  };
}
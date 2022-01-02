/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  clearSearch,
  find,
  findNext,
  findPrev,
  removeOverlay,
  searchSourceForHighlight,
} from "../utils/editor";
import { renderWasmText } from "../utils/wasm";
import { getMatches } from "../workers/search";

import {
  getSelectedSourceWithContent,
  getFileSearchModifiers,
  getFileSearchQuery,
  getFileSearchResults,
} from "../selectors";

import {
  closeActiveSearch,
  clearHighlightLineRange,
  setActiveSearch,
} from "./ui";
import { isFulfilled } from "../utils/async-value";

export function doSearch(cx, query, editor) {
  return ({ getState, dispatch }) => {
    const selectedSource = getSelectedSourceWithContent(getState());
    if (!selectedSource || !selectedSource.content) {
      return;
    }

    dispatch(setFileSearchQuery(cx, query));
    dispatch(searchContents(cx, query, editor));
  };
}

export function doSearchForHighlight(query, editor, line, ch) {
  return async ({ getState, dispatch }) => {
    const selectedSource = getSelectedSourceWithContent(getState());
    if (!selectedSource?.content) {
      return;
    }
    dispatch(searchContentsForHighlight(query, editor, line, ch));
  };
}

export function setFileSearchQuery(cx, query) {
  return {
    type: "UPDATE_FILE_SEARCH_QUERY",
    cx,
    query,
  };
}

export function toggleFileSearchModifier(cx, modifier) {
  return { type: "TOGGLE_FILE_SEARCH_MODIFIER", cx, modifier };
}

export function updateSearchResults(cx, characterIndex, line, matches) {
  const matchIndex = matches.findIndex(
    elm => elm.line === line && elm.ch === characterIndex
  );

  return {
    type: "UPDATE_SEARCH_RESULTS",
    cx,
    results: {
      matches,
      matchIndex,
      count: matches.length,
      index: characterIndex,
    },
  };
}

export function searchContents(cx, query, editor, focusFirstResult = true) {
  return async ({ getState, dispatch }) => {
    const modifiers = getFileSearchModifiers(getState());
    const selectedSource = getSelectedSourceWithContent(getState());

    if (
      !editor ||
      !selectedSource ||
      !selectedSource.content ||
      !isFulfilled(selectedSource.content) ||
      !modifiers
    ) {
      return;
    }
    const selectedContent = selectedSource.content.value;

    const ctx = { ed: editor, cm: editor.codeMirror };

    if (!query) {
      clearSearch(ctx.cm, query);
      return;
    }

    let text;
    if (selectedContent.type === "wasm") {
      text = renderWasmText(selectedSource.id, selectedContent).join("\n");
    } else {
      text = selectedContent.value;
    }

    const matches = await getMatches(query, text, modifiers);

    const res = find(ctx, query, true, modifiers, focusFirstResult);
    if (!res) {
      return;
    }

    const { ch, line } = res;

    dispatch(updateSearchResults(cx, ch, line, matches));
  };
}

export function searchContentsForHighlight(query, editor, line, ch) {
  return async ({ getState, dispatch }) => {
    const modifiers = getFileSearchModifiers(getState());
    const selectedSource = getSelectedSourceWithContent(getState());

    if (
      !query ||
      !editor ||
      !selectedSource ||
      !selectedSource.content ||
      !modifiers
    ) {
      return;
    }

    const ctx = { ed: editor, cm: editor.codeMirror };
    searchSourceForHighlight(ctx, false, query, true, modifiers, line, ch);
  };
}

export function traverseResults(cx, rev, editor) {
  return async ({ getState, dispatch }) => {
    if (!editor) {
      return;
    }

    const ctx = { ed: editor, cm: editor.codeMirror };

    const query = getFileSearchQuery(getState());
    const modifiers = getFileSearchModifiers(getState());
    const { matches } = getFileSearchResults(getState());

    if (query === "") {
      dispatch(setActiveSearch("file"));
    }

    if (modifiers) {
      const matchedLocations = matches || [];
      const findArgs = [ctx, query, true, modifiers];
      const results = rev ? findPrev(...findArgs) : findNext(...findArgs);

      if (!results) {
        return;
      }
      const { ch, line } = results;
      dispatch(updateSearchResults(cx, ch, line, matchedLocations));
    }
  };
}

export function closeFileSearch(cx, editor) {
  return ({ getState, dispatch }) => {
    if (editor) {
      const query = getFileSearchQuery(getState());
      const ctx = { ed: editor, cm: editor.codeMirror };
      removeOverlay(ctx, query);
    }

    dispatch(setFileSearchQuery(cx, ""));
    dispatch(closeActiveSearch());
    dispatch(clearHighlightLineRange());
  };
}

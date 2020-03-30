/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

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
import type { Action, FileTextSearchModifier, ThunkArgs } from "./types";
import type { Context } from "../types";

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
type Editor = Object;
type Match = Object;

export function doSearch(cx: Context, query: string, editor: Editor) {
  return ({ getState, dispatch }: ThunkArgs) => {
    const selectedSource = getSelectedSourceWithContent(getState());
    if (!selectedSource || !selectedSource.content) {
      return;
    }

    dispatch(setFileSearchQuery(cx, query));
    dispatch(searchContents(cx, query, editor));
  };
}

export function doSearchForHighlight(
  query: string,
  editor: Editor,
  line: number,
  ch: number
) {
  return async ({ getState, dispatch }: ThunkArgs) => {
    const selectedSource = getSelectedSourceWithContent(getState());
    if (!selectedSource?.content) {
      return;
    }
    dispatch(searchContentsForHighlight(query, editor, line, ch));
  };
}

export function setFileSearchQuery(cx: Context, query: string): Action {
  return {
    type: "UPDATE_FILE_SEARCH_QUERY",
    cx,
    query,
  };
}

export function toggleFileSearchModifier(
  cx: Context,
  modifier: FileTextSearchModifier
): Action {
  return { type: "TOGGLE_FILE_SEARCH_MODIFIER", cx, modifier };
}

export function updateSearchResults(
  cx: Context,
  characterIndex: number,
  line: number,
  matches: Match[]
): Action {
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

export function searchContents(
  cx: Context,
  query: string,
  editor: Object,
  focusFirstResult?: boolean = true
) {
  return async ({ getState, dispatch }: ThunkArgs) => {
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

export function searchContentsForHighlight(
  query: string,
  editor: Object,
  line: number,
  ch: number
) {
  return async ({ getState, dispatch }: ThunkArgs) => {
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

export function traverseResults(cx: Context, rev: boolean, editor: Editor) {
  return async ({ getState, dispatch }: ThunkArgs) => {
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

export function closeFileSearch(cx: Context, editor: Editor) {
  return ({ getState, dispatch }: ThunkArgs) => {
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

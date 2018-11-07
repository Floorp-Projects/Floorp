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
  searchSourceForHighlight
} from "../utils/editor";
import { isWasm, renderWasmText } from "../utils/wasm";
import { getMatches } from "../workers/search";
import type { Action, FileTextSearchModifier, ThunkArgs } from "./types";
import type { WasmSource } from "../types";

import {
  getSelectedSource,
  getFileSearchModifiers,
  getFileSearchQuery,
  getFileSearchResults
} from "../selectors";

import {
  closeActiveSearch,
  clearHighlightLineRange,
  setActiveSearch
} from "./ui";
type Editor = Object;
type Match = Object;

export function doSearch(query: string, editor: Editor) {
  return ({ getState, dispatch }: ThunkArgs) => {
    const selectedSource = getSelectedSource(getState());
    if (!selectedSource || !selectedSource.text) {
      return;
    }

    dispatch(setFileSearchQuery(query));
    dispatch(searchContents(query, editor));
  };
}

export function doSearchForHighlight(
  query: string,
  editor: Editor,
  line: number,
  ch: number
) {
  return async ({ getState, dispatch }: ThunkArgs) => {
    const selectedSource = getSelectedSource(getState());
    if (!selectedSource || !selectedSource.text) {
      return;
    }
    dispatch(searchContentsForHighlight(query, editor, line, ch));
  };
}

export function setFileSearchQuery(query: string): Action {
  return {
    type: "UPDATE_FILE_SEARCH_QUERY",
    query
  };
}

export function toggleFileSearchModifier(
  modifier: FileTextSearchModifier
): Action {
  return { type: "TOGGLE_FILE_SEARCH_MODIFIER", modifier };
}

export function updateSearchResults(
  characterIndex: number,
  line: number,
  matches: Match[]
): Action {
  const matchIndex = matches.findIndex(
    elm => elm.line === line && elm.ch === characterIndex
  );

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

export function searchContents(query: string, editor: Object) {
  return async ({ getState, dispatch }: ThunkArgs) => {
    const modifiers = getFileSearchModifiers(getState());
    const selectedSource = getSelectedSource(getState());

    if (!editor || !selectedSource || !selectedSource.text || !modifiers) {
      return;
    }

    const ctx = { ed: editor, cm: editor.codeMirror };

    if (!query) {
      clearSearch(ctx.cm, query);
      return;
    }

    const _modifiers = modifiers.toJS();
    let text = selectedSource.text;

    if (isWasm(selectedSource.id)) {
      text = renderWasmText(((selectedSource: any): WasmSource)).join("\n");
    }

    const matches = await getMatches(query, text, _modifiers);

    const res = find(ctx, query, true, _modifiers);
    if (!res) {
      return;
    }

    const { ch, line } = res;

    dispatch(updateSearchResults(ch, line, matches));
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
    const selectedSource = getSelectedSource(getState());

    if (
      !query ||
      !editor ||
      !selectedSource ||
      !selectedSource.text ||
      !modifiers
    ) {
      return;
    }

    const ctx = { ed: editor, cm: editor.codeMirror };
    const _modifiers = modifiers.toJS();

    searchSourceForHighlight(ctx, false, query, true, _modifiers, line, ch);
  };
}

export function traverseResults(rev: boolean, editor: Editor) {
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
      const results = rev
        ? findPrev(ctx, query, true, modifiers.toJS())
        : findNext(ctx, query, true, modifiers.toJS());

      if (!results) {
        return;
      }
      const { ch, line } = results;
      dispatch(updateSearchResults(ch, line, matchedLocations));
    }
  };
}

export function closeFileSearch(editor: Editor) {
  return ({ getState, dispatch }: ThunkArgs) => {
    if (editor) {
      const query = getFileSearchQuery(getState());
      const ctx = { ed: editor, cm: editor.codeMirror };
      removeOverlay(ctx, query);
    }

    dispatch(setFileSearchQuery(""));
    dispatch(closeActiveSearch());
    dispatch(clearHighlightLineRange());
  };
}

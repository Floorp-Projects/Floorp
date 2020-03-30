/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Redux actions for the editor tabs
 * @module actions/tabs
 */

import { removeDocument } from "../utils/editor";
import { selectSource } from "./sources";

import {
  getSourceByURL,
  getSourceTabs,
  getNewSelectedSourceId,
} from "../selectors";

import type { Action, ThunkArgs } from "./types";
import type { Source, Context, SourceId, URL } from "../types";

export function updateTab(source: Source, framework: string): Action {
  const { url, id: sourceId, isOriginal } = source;

  return {
    type: "UPDATE_TAB",
    url,
    framework,
    isOriginal,
    sourceId,
  };
}

export function addTab(source: Source): Action {
  const { url, id: sourceId, isOriginal } = source;

  return {
    type: "ADD_TAB",
    url,
    isOriginal,
    sourceId,
  };
}

export function moveTab(url: URL, tabIndex: number): Action {
  return {
    type: "MOVE_TAB",
    url,
    tabIndex,
  };
}

export function moveTabBySourceId(
  sourceId: SourceId,
  tabIndex: number
): Action {
  return {
    type: "MOVE_TAB_BY_SOURCE_ID",
    sourceId,
    tabIndex,
  };
}

/**
 * @memberof actions/tabs
 * @static
 */
export function closeTab(cx: Context, source: Source) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    removeDocument(source.id);

    const tabs = getSourceTabs(getState());
    dispatch(({ type: "CLOSE_TAB", source }: Action));

    const sourceId = getNewSelectedSourceId(getState(), tabs);
    dispatch(selectSource(cx, sourceId));
  };
}

/**
 * @memberof actions/tabs
 * @static
 */
export function closeTabs(cx: Context, urls: string[]) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const sources = urls
      .map(url => getSourceByURL(getState(), url))
      .filter(Boolean);

    const tabs = getSourceTabs(getState());
    sources.map(source => removeDocument(source.id));
    dispatch(({ type: "CLOSE_TABS", sources }: Action));

    const sourceId = getNewSelectedSourceId(getState(), tabs);
    dispatch(selectSource(cx, sourceId));
  };
}

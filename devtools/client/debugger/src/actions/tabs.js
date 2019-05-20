/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Redux actions for the editor tabs
 * @module actions/tabs
 */

import { isOriginalId } from "devtools-source-map";

import { removeDocument } from "../utils/editor";
import { selectSource } from "./sources";

import {
  getSourceTabs,
  getSourceByURL,
  getNewSelectedSourceId,
  removeSourceFromTabList,
  removeSourcesFromTabList,
} from "../selectors";

import type { Action, ThunkArgs } from "./types";
import type { Source, Context } from "../types";

export function updateTab(source: Source, framework: string): Action {
  const { url, id: sourceId } = source;
  const isOriginal = isOriginalId(source.id);

  return {
    type: "UPDATE_TAB",
    url,
    framework,
    isOriginal,
    sourceId,
  };
}

export function addTab(source: Source): Action {
  const { url, id: sourceId } = source;
  const isOriginal = isOriginalId(source.id);

  return {
    type: "ADD_TAB",
    url,
    isOriginal,
    sourceId,
  };
}

export function moveTab(url: string, tabIndex: number): Action {
  return {
    type: "MOVE_TAB",
    url,
    tabIndex,
  };
}

/**
 * @memberof actions/tabs
 * @static
 */
export function closeTab(cx: Context, source: Source) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const { id, url } = source;

    removeDocument(id);

    const tabs = removeSourceFromTabList(getSourceTabs(getState()), source);
    const sourceId = getNewSelectedSourceId(getState(), tabs);
    dispatch(({ type: "CLOSE_TAB", url, tabs }: Action));
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
    sources.map(source => removeDocument(source.id));

    const tabs = removeSourcesFromTabList(getSourceTabs(getState()), sources);
    dispatch(({ type: "CLOSE_TABS", sources, tabs }: Action));

    const sourceId = getNewSelectedSourceId(getState(), tabs);
    dispatch(selectSource(cx, sourceId));
  };
}

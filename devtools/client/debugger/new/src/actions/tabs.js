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
  getSourcesByURLs,
  getSourceTabs,
  getSourceFromId,
  getNewSelectedSourceId,
  removeSourceFromTabList,
  removeSourcesFromTabList
} from "../selectors";

import { getMainThread } from "../reducers/debuggee";

import type { Action, ThunkArgs } from "./types";
import type { Source, Worker } from "../types";

export function updateTab(source: Source, framework: string): Action {
  const { url, id: sourceId, thread } = source;
  const isOriginal = isOriginalId(source.id);

  return {
    type: "UPDATE_TAB",
    url,
    framework,
    isOriginal,
    sourceId,
    thread
  };
}

export function addTab(source: Source): Action {
  const { url, id: sourceId, thread } = source;
  const isOriginal = isOriginalId(source.id);

  return {
    type: "ADD_TAB",
    url,
    isOriginal,
    sourceId,
    thread
  };
}

export function moveTab(url: string, tabIndex: number): Action {
  return {
    type: "MOVE_TAB",
    url,
    tabIndex
  };
}

/**
 * @memberof actions/tabs
 * @static
 */
export function closeTab(source: Source) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const { id, url } = source;

    removeDocument(id);

    const tabs = removeSourceFromTabList(getSourceTabs(getState()), source);
    const sourceId = getNewSelectedSourceId(getState(), tabs);
    dispatch(({ type: "CLOSE_TAB", url, tabs }: Action));
    dispatch(selectSource(sourceId));
  };
}

/**
 * @memberof actions/tabs
 * @static
 */
export function closeTabs(urls: string[]) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const sources = getSourcesByURLs(getState(), urls);
    sources.map(source => removeDocument(source.id));

    const tabs = removeSourcesFromTabList(getSourceTabs(getState()), sources);
    dispatch(({ type: "CLOSE_TABS", sources, tabs }: Action));

    const sourceId = getNewSelectedSourceId(getState(), tabs);
    dispatch(selectSource(sourceId));
  };
}

export function closeTabsForMissingThreads(workers: Worker[]) {
  return ({ dispatch, getState }: ThunkArgs) => {
    const oldTabs = getSourceTabs(getState());
    const mainThread = getMainThread(getState());
    const removed = [];
    for (const { sourceId } of oldTabs) {
      if (sourceId) {
        const source = getSourceFromId(getState(), sourceId);
        if (
          source.thread != mainThread.actor &&
          !workers.some(({ actor }) => actor == source.thread)
        ) {
          removed.push(source);
        }
      }
    }
    const tabs = removeSourcesFromTabList(oldTabs, removed);
    dispatch({ type: "CLOSE_TABS", removed, tabs });

    const sourceId = getNewSelectedSourceId(getState(), tabs);
    dispatch(selectSource(sourceId));
  };
}

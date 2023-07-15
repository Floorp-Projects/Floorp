/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { showMenu, buildMenu } from "../../context-menu/menu";
import { getTabMenuItems } from "../../utils/tabs";

import {
  getSelectedLocation,
  getSourcesForTabs,
  isSourceBlackBoxed,
  isSourceMapIgnoreListEnabled,
  isSourceOnSourceMapIgnoreList,
} from "../../selectors";

import { toggleBlackBox } from "../sources/blackbox";
import { togglePrettyPrint } from "../sources/prettyPrint";
import { copyToClipboard, showSource } from "../ui";
import { closeTab, closeTabs } from "../tabs";

import { getRawSourceURL, isPretty, shouldBlackbox } from "../../utils/source";
import { copyToTheClipboard } from "../../utils/clipboard";

/**
 * Show the context menu of Tab.
 *
 * @param {object} event
 *        The context-menu DOM event.
 * @param {object} source
 *        Source object of the related Tab.
 */
export function showTabContextMenu(event, source) {
  return async ({ dispatch, getState }) => {
    const sourceId = source.id;

    const state = getState();
    const tabSources = getSourcesForTabs(state);
    const isBlackBoxed = isSourceBlackBoxed(state, source);
    const isSourceOnIgnoreList =
      isSourceMapIgnoreListEnabled(state) &&
      isSourceOnSourceMapIgnoreList(state, source);
    const selectedLocation = getSelectedLocation(state);

    const tabCount = tabSources.length;
    const otherTabs = tabSources.filter(t => t.id !== sourceId);
    const sourceTab = tabSources.find(t => t.id == sourceId);
    const tabURLs = tabSources.map(t => t.url);
    const otherTabURLs = otherTabs.map(t => t.url);

    if (!sourceTab || !selectedLocation || !selectedLocation.source.id) {
      return;
    }

    const tabMenuItems = getTabMenuItems();
    const items = [
      {
        item: {
          ...tabMenuItems.closeTab,
          click: () => dispatch(closeTab(sourceTab)),
        },
      },
      {
        item: {
          ...tabMenuItems.closeOtherTabs,
          disabled: otherTabURLs.length === 0,
          click: () => dispatch(closeTabs(otherTabURLs)),
        },
      },
      {
        item: {
          ...tabMenuItems.closeTabsToEnd,
          disabled:
            tabCount === 1 ||
            tabSources.some((t, i) => t.id === sourceId && tabCount - 1 === i),
          click: () => {
            const tabIndex = tabSources.findIndex(t => t.id == sourceId);
            dispatch(closeTabs(tabURLs.filter((t, i) => i > tabIndex)));
          },
        },
      },
      {
        item: {
          ...tabMenuItems.closeAllTabs,
          click: () => dispatch(closeTabs(tabURLs)),
        },
      },
      { item: { type: "separator" } },
      {
        item: {
          ...tabMenuItems.copySource,
          // Only enable when this is the selected source as this requires the source to be loaded,
          // which may not be the case if the tab wasn't ever selected.
          disabled: selectedLocation.source.id !== source.id,
          click: () => {
            dispatch(copyToClipboard(selectedLocation));
          },
        },
      },
      {
        item: {
          ...tabMenuItems.copySourceUri2,
          disabled: !source.url,
          click: () => copyToTheClipboard(getRawSourceURL(source.url)),
        },
      },
      {
        item: {
          ...tabMenuItems.showSource,
          // Source Tree only shows sources with URL
          disabled: !source.url,
          click: () => dispatch(showSource(sourceId)),
        },
      },
      {
        item: {
          ...tabMenuItems.toggleBlackBox,
          label: isBlackBoxed
            ? L10N.getStr("ignoreContextItem.unignore")
            : L10N.getStr("ignoreContextItem.ignore"),
          disabled: isSourceOnIgnoreList || !shouldBlackbox(source),
          click: () => dispatch(toggleBlackBox(source)),
        },
      },
      {
        item: {
          ...tabMenuItems.prettyPrint,
          disabled: isPretty(sourceTab),
          click: () => dispatch(togglePrettyPrint(sourceId)),
        },
      },
    ];

    showMenu(event, buildMenu(items));
  };
}

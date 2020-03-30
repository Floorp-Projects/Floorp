/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { PersistedTab, VisibleTab } from "../reducers/tabs";
import type { TabList, Tab, TabsSources } from "../reducers/types";
import type { URL } from "../types";

/*
 * Finds the hidden tabs by comparing the tabs' top offset.
 * hidden tabs will have a great top offset.
 *
 * @param sourceTabs Array
 * @param sourceTabEls HTMLCollection
 *
 * @returns Array
 */

export function getHiddenTabs(
  sourceTabs: TabsSources,
  sourceTabEls: Array<any>
): TabsSources {
  sourceTabEls = [].slice.call(sourceTabEls);
  function getTopOffset() {
    const topOffsets = sourceTabEls.map(t => t.getBoundingClientRect().top);
    return Math.min(...topOffsets);
  }

  function hasTopOffset(el) {
    // adding 10px helps account for cases where the tab might be offset by
    // styling such as selected tabs which don't have a border.
    const tabTopOffset = getTopOffset();
    return el.getBoundingClientRect().top > tabTopOffset + 10;
  }

  return sourceTabs.filter((tab, index) => {
    const element = sourceTabEls[index];
    return element && hasTopOffset(element);
  });
}

export function getFramework(tabs: TabList, url: URL) {
  const tab = tabs.find(t => t.url === url);

  if (tab) {
    return tab.framework;
  }

  return "";
}

export function getTabMenuItems() {
  return {
    closeTab: {
      id: "node-menu-close-tab",
      label: L10N.getStr("sourceTabs.closeTab"),
      accesskey: L10N.getStr("sourceTabs.closeTab.accesskey"),
      disabled: false,
    },
    closeOtherTabs: {
      id: "node-menu-close-other-tabs",
      label: L10N.getStr("sourceTabs.closeOtherTabs"),
      accesskey: L10N.getStr("sourceTabs.closeOtherTabs.accesskey"),
      disabled: false,
    },
    closeTabsToEnd: {
      id: "node-menu-close-tabs-to-end",
      label: L10N.getStr("sourceTabs.closeTabsToEnd"),
      accesskey: L10N.getStr("sourceTabs.closeTabsToEnd.accesskey"),
      disabled: false,
    },
    closeAllTabs: {
      id: "node-menu-close-all-tabs",
      label: L10N.getStr("sourceTabs.closeAllTabs"),
      accesskey: L10N.getStr("sourceTabs.closeAllTabs.accesskey"),
      disabled: false,
    },
    showSource: {
      id: "node-menu-show-source",
      label: L10N.getStr("sourceTabs.revealInTree"),
      accesskey: L10N.getStr("sourceTabs.revealInTree.accesskey"),
      disabled: false,
    },
    copyToClipboard: {
      id: "node-menu-copy-to-clipboard",
      label: L10N.getStr("copyToClipboard.label"),
      accesskey: L10N.getStr("copyToClipboard.accesskey"),
      disabled: false,
    },
    copySourceUri2: {
      id: "node-menu-copy-source-url",
      label: L10N.getStr("copySourceUri2"),
      accesskey: L10N.getStr("copySourceUri2.accesskey"),
      disabled: false,
    },
    toggleBlackBox: {
      id: "node-menu-blackbox",
      label: L10N.getStr("blackboxContextItem.blackbox"),
      accesskey: L10N.getStr("blackboxContextItem.blackbox.accesskey"),
      disabled: false,
    },
    prettyPrint: {
      id: "node-menu-pretty-print",
      label: L10N.getStr("sourceTabs.prettyPrint"),
      accesskey: L10N.getStr("sourceTabs.prettyPrint.accesskey"),
      disabled: false,
    },
  };
}

export function isSimilarTab(tab: Tab, url: URL, isOriginal: boolean) {
  return tab.url === url && tab.isOriginal === isOriginal;
}

export function persistTabs(tabs: VisibleTab[]): PersistedTab[] {
  return [...tabs]
    .filter(tab => tab.url)
    .map(tab => {
      const newTab = { ...tab };
      newTab.sourceId = null;
      return newTab;
    });
}

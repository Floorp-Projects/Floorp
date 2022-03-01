/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  dumpTab,
  configs
} from './common.js';

import * as TabsStore from './tabs-store.js';

import EventListenerManager from '/extlib/EventListenerManager.js';

function log(...args) {
  internalLogger('common/Window', ...args);
}

export default class Window {
  constructor(windowId) {
    const alreadyTracked = TabsStore.windows.get(windowId);
    if (alreadyTracked)
      return alreadyTracked;

    log(`window ${windowId} is newly tracked`);

    this.id    = windowId;
    this.tabs  = new Map();
    this.order = [];

    this.element = null;
    this.classList = null;

    this.internalMovingTabs  = new Set();
    this.alreadyMovedTabs    = new Set();
    this.internalClosingTabs = new Set();
    this.keepDescendantsTabs = new Set();
    this.tabsToBeHighlightedAlone = new Set();

    this.subTreeMovingCount =
      this.subTreeChildrenMovingCount =
      this.doingIntelligentlyCollapseExpandCount =
      this.internalFocusCount =
      this.internalSilentlyFocusCount =
      this.internalByMouseFocusCount =
      this.duplicatingTabsCount = 0;

    this.preventToDetectTabBunchesUntil = Date.now() + configs.tabBunchesDetectionDelayOnNewWindow;

    this.openingTabs   = new Set();

    this.openedNewTabs = new Map();

    this.bypassTabControlCount       = 0;
    this.toBeOpenedTabsWithPositions = 0;
    this.toBeOpenedOrphanTabs        = 0;

    this.toBeAttachedTabs = new Set();
    this.toBeDetachedTabs = new Set();

    this.lastRelatedTabs = new Map();
    this.previousLastRelatedTabs = new Map();

    TabsStore.windows.set(windowId, this);
    TabsStore.prepareIndexesForWindow(windowId);

    // We should initialize private properties with blank value for better performance with a fixed shape.
    this.delayedDestroy = null;
  }

  destroy() {
    for (const tab of this.tabs.values()) {
      if (tab.$TST)
        tab.$TST.destroy();
    }
    this.tabs.clear();
    TabsStore.windows.delete(this.id);
    TabsStore.unprepareIndexesForWindow(this.id);

    if (this.element) {
      const element = this.element;
      if (element.parentNode && !element.hasChildNodes()) {
        element.parentNode.removeChild(element);
        this.unbindElement();
      }
    }

    this.tabs = null;
    this.order = null;
    this.id = null;
  }

  clear() {
    this.tabs.clear();
    this.order = [];
    TabsStore.unprepareIndexesForWindow(this.id);
    TabsStore.prepareIndexesForWindow(this.id);
    this.clearLastRelatedTabs();
  }

  clearLastRelatedTabs() {
    this.lastRelatedTabs.clear();
    this.previousLastRelatedTabs.clear();
  }

  bindElement(element) {
    this.element = element;
    this.classList = element.classList;
  }

  unbindElement() {
    this.element = null;
    this.classList = null;
  }

  getOrderedTabs(startId, endId, tabs) {
    const orderedIds = this.sliceOrder(startId, endId);
    tabs = tabs || this.tabs;
    return (function*() {
      for (const id of orderedIds) {
        const tab = tabs.get(id);
        if (tab)
          yield tab;
      }
    }).call(this);
  }

  getReversedOrderedTabs(startId, endId, tabs) {
    const orderedIds = this.sliceOrder(startId, endId, this.order.slice(0).reverse());
    tabs = tabs || this.tabs;
    return (function*() {
      for (const id of orderedIds) {
        const tab = tabs.get(id);
        if (tab)
          yield tab;
      }
    }).call(this);
  }

  sliceOrder(startId, endId, orderedIds) {
    if (!orderedIds)
      orderedIds = this.order;
    if (startId) {
      if (!this.tabs.has(startId))
        return [];
      orderedIds = orderedIds.slice(orderedIds.indexOf(startId));
    }
    if (endId) {
      if (!this.tabs.has(endId))
        return [];
      orderedIds = orderedIds.slice(0, orderedIds.indexOf(endId) + 1);
    }
    return orderedIds;
  }

  trackTab(tab) {
    const alreadyTracked = TabsStore.tabs.get(tab.id);
    if (alreadyTracked)
      tab = alreadyTracked;

    if (this.delayedDestroy) {
      clearTimeout(this.delayedDestroy);
      this.delayedDestroy = null;
    }

    const order = this.order;
    if (this.tabs.has(tab.id)) { // already tracked: update
      const index = order.indexOf(tab.id);
      order.splice(index, 1);
      order.splice(tab.index, 0, tab.id);
      for (let i = Math.min(index, tab.index), maxi = Math.min(Math.max(index + 1, tab.index + 1), order.length); i < maxi; i++) {
        const tab = this.tabs.get(order[i]);
        if (!tab)
          throw new Error(`Unknown tab: ${i}/${order[i]} (${order.join(', ')})`);
        tab.index = i;
        tab.reindexedBy = `Window.property.trackTab/update (${tab.index})`;
      }
      const parent = tab.$TST.parent;
      if (parent) {
        parent.$TST.sortChildren();
        parent.$TST.invalidateCachedAncestors();
      }
      log(`tab ${dumpTab(tab)} is re-tracked under the window ${this.id}: `, order);
    }
    else { // not tracked yet: add
      this.tabs.set(tab.id, tab);
      order.splice(tab.index, 0, tab.id);
      for (let i = tab.index + 1, maxi = order.length; i < maxi; i++) {
        const tab = this.tabs.get(order[i]);
        if (!tab)
          throw new Error(`Unknown tab: ${i}/${order[i]} (${order.join(', ')})`);
        tab.index = i;
        tab.reindexedBy = `Window.property.trackTab/new (${tab.index})`;
      }
      log(`tab ${dumpTab(tab)} is newly tracked under the window ${this.id}: `, order);
    }
    TabsStore.updateIndexesForTab(tab);
    return tab;
  }

  detachTab(tabId) {
    const tab = TabsStore.tabs.get(tabId);
    if (!tab)
      return;

    TabsStore.removeTabFromIndexes(tab);

    tab.$TST.detach();
    this.tabs.delete(tabId);
    const order = this.order;
    const index = order.indexOf(tab.id);
    if (index < 0) // the tab is not tracked yet!
      return;
    order.splice(index, 1);
    if (this.tabs.size == 0) {
      if (!TabsStore.getCurrentWindowId()) { // only in the background page - the sidebar has no need to destroy itself manually.
        // the last tab can be removed with browser.tabs.closeWindowWithLastTab=false,
        // so we should not destroy the window immediately.
        if (this.delayedDestroy)
          clearTimeout(this.delayedDestroy);
        this.delayedDestroy = setTimeout(() => {
          if (this.tabs &&
            this.tabs.size == 0)
            this.destroy();
        }, (configs.collapseDuration, 1000) * 5);
      }
    }
    else {
      for (let i = index, maxi = order.length; i < maxi; i++) {
        this.tabs.get(order[i]).index = i;
      }
    }
    return tab;
  }

  untrackTab(tabId) {
    const tab = this.detachTab(tabId);
    if (tab)
      tab.$TST.destroy();
  }

  export(full) {
    const tabs = [];
    for (const tab of this.getOrderedTabs()) {
      tabs.push(tab.$TST.export(full));
    }
    return tabs;
  }
}

Window.onInitialized = new EventListenerManager();

Window.init = windowId => {
  const window = TabsStore.windows.get(windowId) || new Window(windowId);
  Window.onInitialized.dispatch(window);
  return window;
}

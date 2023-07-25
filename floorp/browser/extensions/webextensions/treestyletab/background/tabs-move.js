/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Tree Style Tab.
 *
 * The Initial Developer of the Original Code is YUKI "Piro" Hiroshi.
 * Portions created by the Initial Developer are Copyright (C) 2011-2019
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): YUKI "Piro" Hiroshi <piro.outsider.reflex@gmail.com>
 *                 wanabe <https://github.com/wanabe>
 *                 Tetsuharu OHZEKI <https://github.com/saneyuki>
 *                 Xidorn Quan <https://github.com/upsuper> (Firefox 40+ support)
 *                 lv7777 (https://github.com/lv7777)
 *
 * ***** END LICENSE BLOCK ******/
'use strict';

import {
  log as internalLogger,
  wait,
  toLines,
  configs
} from '/common/common.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as Constants from '/common/constants.js';
import { SequenceMatcher } from '/extlib/diff.js';
import * as SidebarConnection from '/common/sidebar-connection.js';
import * as TabsStore from '/common/tabs-store.js';

import Tab from '/common/Tab.js';

function log(...args) {
  internalLogger('background/tabs-move', ...args);
}
function logApiTabs(...args) {
  internalLogger('common/api-tabs', ...args);
}


// ========================================================
// primitive methods for internal use

export async function moveTabsBefore(tabs, referenceTab, options = {}) {
  log('moveTabsBefore: ', tabs, referenceTab, options);
  if (!tabs.length ||
      !TabsStore.ensureLivingTab(referenceTab))
    return [];

  if (referenceTab.$TST.isAllPlacedBeforeSelf(tabs)) {
    log('moveTabsBefore:no need to move');
    return [];
  }
  return moveTabsInternallyBefore(tabs, referenceTab, options);
}
export async function moveTabBefore(tab, referenceTab, options = {}) {
  return moveTabsBefore([tab], referenceTab, options).then(moved => moved.length > 0);
}

async function moveTabsInternallyBefore(tabs, referenceTab, options = {}) {
  if (!tabs.length ||
      !TabsStore.ensureLivingTab(referenceTab))
    return [];

  const window = TabsStore.windows.get(tabs[0].windowId);

  log('moveTabsInternallyBefore: ', tabs, referenceTab, options);

  const precedingReferenceTab = referenceTab.$TST.previousTab;
  if (referenceTab.pinned) {
    // unpinned tab cannot be moved before any pinned tab
    tabs = tabs.filter(tab => tab.pinned);
  }
  else if (precedingReferenceTab &&
           !precedingReferenceTab.pinned) {
    // pinned tab cannot be moved after any unpinned tab
    tabs = tabs.filter(tab => !tab.pinned);
  }
  if (!tabs.length)
    return [];

  const movedTabs = [];
  try {
    /*
      Tab elements are moved by tabs.onMoved automatically, but
      the operation is asynchronous. To help synchronous operations
      following to this operation, we need to move tabs immediately.
    */
    for (const tab of tabs) {
      const oldPreviousTab = tab.$TST.unsafePreviousTab;
      const oldNextTab     = tab.$TST.unsafeNextTab;
      if (oldNextTab && oldNextTab.id == referenceTab.id) // no move case
        continue;
      window.internalMovingTabs.add(tab.id);
      window.alreadyMovedTabs.add(tab.id);
      if (referenceTab.index > tab.index)
        tab.index = referenceTab.index - 1;
      else
        tab.index = referenceTab.index;
      tab.reindexedBy = `moveTabsInternallyBefore (${tab.index})`;
      Tab.track(tab);
      movedTabs.push(tab);
      Tab.onTabInternallyMoved.dispatch(tab, {
        nextTab: referenceTab,
        oldPreviousTab,
        oldNextTab,
        broadcasted: !!options.broadcasted
      });
      SidebarConnection.sendMessage({
        type:        Constants.kCOMMAND_NOTIFY_TAB_INTERNALLY_MOVED,
        windowId:    tab.windowId,
        tabId:       tab.id,
        newIndex:    tab.index,
        nextTabId:   referenceTab && referenceTab.id,
        broadcasted: !!options.broadcasted
      });
    }
    if (movedTabs.length == 0) {
      log(' => actually nothing moved');
    }
    else {
      log('Tab nodes rearranged by moveTabsInternallyBefore:\n'+(!configs.debug ? '' :
        () => toLines(Array.from(window.getOrderedTabs()),
                      tab => ` - ${tab.index}: ${tab.id}${tabs.includes(tab) ? '[MOVED]' : ''}`)));
    }
    if (SidebarConnection.isInitialized()) { // only on the background page
      if (options.delayedMove) // Wait until opening animation is finished.
        await wait(configs.newTabAnimationDuration);
      syncToNativeTabs(tabs);
    }
  }
  catch(e) {
    ApiTabs.handleMissingTabError(e);
    log('moveTabsInternallyBefore failed: ', String(e));
  }
  return movedTabs;
}
export async function moveTabInternallyBefore(tab, referenceTab, options = {}) {
  return moveTabsInternallyBefore([tab], referenceTab, options);
}

export async function moveTabsAfter(tabs, referenceTab, options = {}) {
  log('moveTabsAfter: ', tabs, referenceTab, options);
  if (!tabs.length ||
      !TabsStore.ensureLivingTab(referenceTab))
    return [];

  if (referenceTab.$TST.isAllPlacedAfterSelf(tabs)) {
    log('moveTabsAfter:no need to move');
    return [];
  }
  return moveTabsInternallyAfter(tabs, referenceTab, options);
}
export async function moveTabAfter(tab, referenceTab, options = {}) {
  return moveTabsAfter([tab], referenceTab, options).then(moved => moved.length > 0);
}

async function moveTabsInternallyAfter(tabs, referenceTab, options = {}) {
  if (!tabs.length ||
      !TabsStore.ensureLivingTab(referenceTab))
    return [];

  const window = TabsStore.windows.get(tabs[0].windowId);

  log('moveTabsInternallyAfter: ', tabs, referenceTab, options);

  const followingReferenceTab = referenceTab.$TST.nextTab;
  if (followingReferenceTab &&
      followingReferenceTab.pinned) {
    // unpinned tab cannot be moved before any pinned tab
    tabs = tabs.filter(tab => tab.pinned);
  }
  else if (!referenceTab.pinned) {
    // pinned tab cannot be moved after any unpinned tab
    tabs = tabs.filter(tab => !tab.pinned);
  }
  if (!tabs.length)
    return [];

  const movedTabs = [];
  try {
    /*
      Tab elements are moved by tabs.onMoved automatically, but
      the operation is asynchronous. To help synchronous operations
      following to this operation, we need to move tabs immediately.
    */
    let nextTab = referenceTab.$TST.unsafeNextTab;
    if (nextTab && tabs.find(tab => tab.id == nextTab.id))
      nextTab = null;
    for (const tab of tabs) {
      const oldPreviousTab = tab.$TST.unsafePreviousTab;
      const oldNextTab     = tab.$TST.unsafeNextTab;
      if ((!oldNextTab && !nextTab) ||
          (oldNextTab && nextTab && oldNextTab.id == nextTab.id)) // no move case
        continue;
      window.internalMovingTabs.add(tab.id);
      window.alreadyMovedTabs.add(tab.id);
      if (nextTab) {
        if (nextTab.index > tab.index)
          tab.index = nextTab.index - 1;
        else
          tab.index = nextTab.index;
      }
      else {
        tab.index = window.tabs.size - 1
      }
      tab.reindexedBy = `moveTabsInternallyAfter (${tab.index})`;
      Tab.track(tab);
      movedTabs.push(tab);
      Tab.onTabInternallyMoved.dispatch(tab, {
        nextTab,
        oldPreviousTab,
        oldNextTab,
        broadcasted: !!options.broadcasted
      });
      SidebarConnection.sendMessage({
        type:        Constants.kCOMMAND_NOTIFY_TAB_INTERNALLY_MOVED,
        windowId:    tab.windowId,
        tabId:       tab.id,
        newIndex:    tab.index,
        nextTabId:   nextTab && nextTab.id,
        broadcasted: !!options.broadcasted
      });
    }
    if (movedTabs.length == 0) {
      log(' => actually nothing moved');
    }
    else {
      log('Tab nodes rearranged by moveTabsInternallyAfter:\n'+(!configs.debug ? '' :
        () => toLines(Array.from(window.getOrderedTabs()),
                      tab => ` - ${tab.index}: ${tab.id}${tabs.includes(tab) ? '[MOVED]' : ''}`)));
    }
    if (SidebarConnection.isInitialized()) { // only on the background page
      if (options.delayedMove) // Wait until opening animation is finished.
        await wait(configs.newTabAnimationDuration);
      syncToNativeTabs(tabs);
    }
  }
  catch(e) {
    ApiTabs.handleMissingTabError(e);
    log('moveTabsInternallyAfter failed: ', String(e));
  }
  return movedTabs;
}
export async function moveTabInternallyAfter(tab, referenceTab, options = {}) {
  return moveTabsInternallyAfter([tab], referenceTab, options);
}


// ========================================================
// Synchronize order of tab elements to browser's tabs

const mMovedTabs        = new Map();
const mPreviousSync     = new Map();
const mDelayedSync      = new Map();
const mDelayedSyncTimer = new Map();

export async function waitUntilSynchronized(windowId) {
  const previous = mPreviousSync.get(windowId);
  if (previous)
    return previous.then(() => waitUntilSynchronized(windowId));
  return Promise.resolve(mDelayedSync.get(windowId)).then(() => {
    const previous = mPreviousSync.get(windowId);
    if (previous)
      return waitUntilSynchronized(windowId);
  });
}

function syncToNativeTabs(tabs) {
  const windowId = tabs[0].windowId;
  //log(`syncToNativeTabs(${windowId})`);
  const movedTabs = mMovedTabs.get(windowId) || [];
  mMovedTabs.set(windowId, movedTabs.concat(tabs));
  if (mDelayedSyncTimer.has(windowId))
    clearTimeout(mDelayedSyncTimer.get(windowId));
  const delayedSync = new Promise((resolve, _reject) => {
    mDelayedSyncTimer.set(windowId, setTimeout(() => {
      mDelayedSync.delete(windowId);
      let previousSync = mPreviousSync.get(windowId);
      if (previousSync)
        previousSync = previousSync.then(() => syncToNativeTabsInternal(windowId));
      else
        previousSync = syncToNativeTabsInternal(windowId);
      previousSync = previousSync.then(resolve);
      mPreviousSync.set(windowId, previousSync);
    }, 250));
  }).then(() => {
    mPreviousSync.delete(windowId);
  });
  mDelayedSync.set(windowId, delayedSync);
  return delayedSync;
}
async function syncToNativeTabsInternal(windowId) {
  mDelayedSyncTimer.delete(windowId);

  const oldMovedTabs = mMovedTabs.get(windowId) || [];
  mMovedTabs.delete(windowId);

  if (Tab.needToWaitTracked(windowId))
    await Tab.waitUntilTrackedAll(windowId);
  if (Tab.needToWaitMoved(windowId))
    await Tab.waitUntilMovedAll(windowId);

  const window = TabsStore.windows.get(windowId);
  if (!window) // already destroyed
    return;

  for (const tab of oldMovedTabs) {
    window.internalMovingTabs.delete(tab.id);
    window.alreadyMovedTabs.delete(tab.id);
  }

  // Tabs may be removed while waiting.
  const internalOrder   = TabsStore.windows.get(windowId).order;
  const nativeTabsOrder = (await browser.tabs.query({ windowId }).catch(ApiTabs.createErrorHandler())).map(tab => tab.id);
  log(`syncToNativeTabs(${windowId}): rearrange `, { internalOrder:internalOrder.join(','), nativeTabsOrder:nativeTabsOrder.join(',') });

  log(`syncToNativeTabs(${windowId}): step1, internalOrder => nativeTabsOrder`);
  let tabIdsForUpdatedIndices = Array.from(nativeTabsOrder);

  const moveOperations = (new SequenceMatcher(nativeTabsOrder, internalOrder)).operations();
  const movedTabs = new Set();
  for (const operation of moveOperations) {
    const [tag, fromStart, fromEnd, toStart, toEnd] = operation;
    log(`syncToNativeTabs(${windowId}): operation `, { tag, fromStart, fromEnd, toStart, toEnd });
    switch (tag) {
      case 'equal':
      case 'delete':
        break;

      case 'insert':
      case 'replace':
        let moveTabIds = internalOrder.slice(toStart, toEnd);
        const referenceId = nativeTabsOrder[fromStart] || null;
        let toIndex = -1;
        let fromIndices = moveTabIds.map(id => tabIdsForUpdatedIndices.indexOf(id));
        if (referenceId) {
          toIndex = tabIdsForUpdatedIndices.indexOf(referenceId);
        }
        if (toIndex < 0)
          toIndex = internalOrder.length;
        // ignore already removed tabs!
        moveTabIds = moveTabIds.filter((id, index) => fromIndices[index] > -1);
        if (moveTabIds.length == 0)
          continue;
        fromIndices = fromIndices.filter(index => index > -1);
        const fromIndex = fromIndices[0];
        if (fromIndex < toIndex)
          toIndex--;
        log(`syncToNativeTabs(${windowId}): step1, move ${moveTabIds.join(',')} before ${referenceId} / from = ${fromIndex}, to = ${toIndex}`);
        for (const movedId of moveTabIds) {
          window.internalMovingTabs.add(movedId);
          window.alreadyMovedTabs.add(movedId);
          movedTabs.add(movedId);
        }
        logApiTabs(`tabs-move:syncToNativeTabs(${windowId}): step1, browser.tabs.move() `, moveTabIds, {
          windowId,
          index: toIndex
        });
        browser.tabs.move(moveTabIds, {
          windowId,
          index: toIndex
        }).catch(ApiTabs.createErrorHandler(e => {
          log(`syncToNativeTabs(${windowId}): step1, failed to move: `, String(e), e.stack);
          throw e;
        }));
        tabIdsForUpdatedIndices = tabIdsForUpdatedIndices.filter(id => !moveTabIds.includes(id));
        tabIdsForUpdatedIndices.splice(toIndex, 0, ...moveTabIds);
        break;
    }
  }
  log(`syncToNativeTabs(${windowId}): step1, rearrange completed.`);

  if (movedTabs.size > 0) {
    // tabs.onMoved produced by this operation can break the order of tabs
    // in the sidebar, so we need to synchronize complete order of tabs after
    // all.
    SidebarConnection.sendMessage({
      type: Constants.kCOMMAND_SYNC_TABS_ORDER,
      windowId
    });

    // Multiple times asynchronous tab move is unstable, so we retry again
    // for safety until all tabs are completely synchronized.
    syncToNativeTabs([{ windowId }]);
  }
}

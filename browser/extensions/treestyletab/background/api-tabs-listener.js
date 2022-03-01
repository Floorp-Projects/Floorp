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
 * Portions created by the Initial Developer are Copyright (C) 2011-2021
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
  dumpTab,
  toLines,
  configs
} from '/common/common.js';
import * as Constants from '/common/constants.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TabsUpdate from '/common/tabs-update.js';
import * as TabsInternalOperation from '/common/tabs-internal-operation.js';
import * as TreeBehavior from '/common/tree-behavior.js';
import * as SidebarConnection from '/common/sidebar-connection.js';

import Tab from '/common/Tab.js';
import Window from '/common/Window.js';

import * as Tree from './tree.js';

function log(...args) {
  internalLogger('background/api-tabs-listener', ...args);
}
function logUpdated(...args) {
  internalLogger('common/tabs-update', ...args);
}

let mAppIsActive = false;

export function init() {
  browser.tabs.onActivated.addListener(onActivated);
  browser.tabs.onUpdated.addListener(onUpdated);
  browser.tabs.onHighlighted.addListener(onHighlighted);
  browser.tabs.onCreated.addListener(onCreated);
  browser.tabs.onRemoved.addListener(onRemoved);
  browser.tabs.onMoved.addListener(onMoved);
  browser.tabs.onAttached.addListener(onAttached);
  browser.tabs.onDetached.addListener(onDetached);
  browser.windows.onCreated.addListener(onWindowCreated);
  browser.windows.onRemoved.addListener(onWindowRemoved);

  browser.windows.getAll({}).then(windows => {
    mAppIsActive = windows.some(window => window.focused);
  });
}

let mPromisedStartedResolver;
let mPromisedStarted = new Promise((resolve, _reject) => {
  mPromisedStartedResolver = resolve;
});

export function destroy() {
  mPromisedStartedResolver = undefined;
  mPromisedStarted = undefined;
  browser.tabs.onActivated.removeListener(onActivated);
  browser.tabs.onUpdated.removeListener(onUpdated);
  browser.tabs.onHighlighted.removeListener(onHighlighted);
  browser.tabs.onCreated.removeListener(onCreated);
  browser.tabs.onRemoved.removeListener(onRemoved);
  browser.tabs.onMoved.removeListener(onMoved);
  browser.tabs.onAttached.removeListener(onAttached);
  browser.tabs.onDetached.removeListener(onDetached);
  browser.windows.onCreated.removeListener(onWindowCreated);
  browser.windows.onRemoved.removeListener(onWindowRemoved);
}

export function start() {
  if (!mPromisedStartedResolver)
    return;
  mPromisedStartedResolver();
  mPromisedStartedResolver = undefined;
  mPromisedStarted = undefined;
}


const mTabOperationQueue = [];

function addTabOperationQueue() {
  let onCompleted;
  const previous = mTabOperationQueue[mTabOperationQueue.length - 1];
  const queue = new Promise((resolve, _aReject) => {
    onCompleted = resolve;
  });
  queue.then(() => {
    mTabOperationQueue.splice(mTabOperationQueue.indexOf(queue), 1);
  });
  mTabOperationQueue.push(queue);
  return [onCompleted, previous];
}

function warnTabDestroyedWhileWaiting(tabId, tab) {
  if (configs.debug)
    console.log(`WARNING: tab ${tabId} is destroyed while waiting. `, tab, new Error().stack);
}


async function onActivated(activeInfo) {
  if (mPromisedStarted)
    await mPromisedStarted;

  TabsStore.activeTabInWindow.set(activeInfo.windowId, Tab.get(activeInfo.tabId));

  const [onCompleted, previous] = addTabOperationQueue();
  if (!configs.acceleratedTabOperations && previous)
    await previous;

  try {
    const window = Window.init(activeInfo.windowId);

    const byInternalOperation = window.internalFocusCount > 0;
    if (byInternalOperation)
      window.internalFocusCount--;
    const byMouseOperation = window.internalByMouseFocusCount > 0;
    if (byMouseOperation)
      window.internalByMouseFocusCount--;
    const silently = window.internalSilentlyFocusCount > 0;
    if (silently)
      window.internalSilentlyFocusCount--;
    const byTabDuplication = parseInt(window.duplicatingTabsCount) > 0;

    if (!Tab.isTracked(activeInfo.tabId))
      await Tab.waitUntilTracked(activeInfo.tabId, { element: !!TabsStore.getCurrentWindowId() });

    const newActiveTab = Tab.get(activeInfo.tabId);
    if (!newActiveTab ||
        !TabsStore.ensureLivingTab(newActiveTab)) {
      warnTabDestroyedWhileWaiting(activeInfo.tabId);
      onCompleted();
      return;
    }

    log('tabs.onActivated: ', newActiveTab);
    const oldActiveTabs = TabsInternalOperation.setTabActive(newActiveTab);
    const byActiveTabRemove = !activeInfo.previousTabId;

    if (!TabsStore.ensureLivingTab(newActiveTab)) { // it can be removed while waiting
      onCompleted();
      warnTabDestroyedWhileWaiting(activeInfo.tabId);
      return;
    }

    let focusOverridden = Tab.onActivating.dispatch(newActiveTab, {
      ...activeInfo,
      byActiveTabRemove,
      byTabDuplication,
      byInternalOperation,
      byMouseOperation,
      silently
    });
    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_TAB_ACTIVATING,
      windowId: activeInfo.windowId,
      tabId:    activeInfo.tabId,
      byActiveTabRemove,
      byTabDuplication,
      byInternalOperation,
      byMouseOperation,
      silently
    });
    // don't do await if not needed, to process things synchronously
    if (focusOverridden instanceof Promise)
      focusOverridden = await focusOverridden;
    focusOverridden = focusOverridden === false;
    if (focusOverridden) {
      onCompleted();
      return;
    }

    if (!TabsStore.ensureLivingTab(newActiveTab)) { // it can be removed while waiting
      onCompleted();
      warnTabDestroyedWhileWaiting(activeInfo.tabId);
      return;
    }

    const onActivatedReuslt = Tab.onActivated.dispatch(newActiveTab, {
      ...activeInfo,
      oldActiveTabs,
      byActiveTabRemove,
      byTabDuplication,
      byInternalOperation,
      byMouseOperation,
      silently
    });
    // don't do await if not needed, to process things synchronously
    if (onActivatedReuslt instanceof Promise)
      await onActivatedReuslt;

    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_TAB_ACTIVATED,
      windowId: activeInfo.windowId,
      tabId:    activeInfo.tabId,
      byActiveTabRemove,
      byTabDuplication,
      byInternalOperation,
      byMouseOperation,
      silently
    });
    onCompleted();
  }
  catch(e) {
    console.log(e);
    onCompleted();
  }
}

async function onUpdated(tabId, changeInfo, tab) {
  if (mPromisedStarted)
    await mPromisedStarted;

  if (!Tab.isTracked(tabId))
    await Tab.waitUntilTracked(tabId, { element: !!TabsStore.getCurrentWindowId() });

  const [onCompleted, previous] = addTabOperationQueue();
  if (!configs.acceleratedTabOperations && previous)
    await previous;

  try {
    const updatedTab = Tab.get(tabId);
    if (!updatedTab ||
        !TabsStore.ensureLivingTab(updatedTab)) {
      onCompleted();
      warnTabDestroyedWhileWaiting(tabId, updatedTab);
      return;
    }

    logUpdated('tabs.onUpdated ', tabId, changeInfo, tab, updatedTab);

    const oldState = {};

    if ('url' in changeInfo) {
      changeInfo.previousUrl = updatedTab.url;
      // On Linux (and possibly on some other environments) the initial page load
      // sometimes produces "onUpdated" event with unchanged URL unexpectedly,
      // so we should ignure such invalid (uneffective) URL changes.
      // See also: https://github.com/piroor/treestyletab/issues/3078
      if (changeInfo.url == 'about:blank' &&
          changeInfo.previousUrl == changeInfo.url &&
          changeInfo.status == 'loading') {
        delete changeInfo.url;
        delete changeInfo.previousUrl;
      }
    }
    /*
      Updated openerTabId is not notified via tabs.onUpdated due to
      https://bugzilla.mozilla.org/show_bug.cgi?id=1409262 , so it can be
      notified with delay as a part of the complete tabs.Tab object,
      "tab" given to this handler. To prevent unexpected tree brekage,
      we should apply updated openerTabId only when it is modified at
      outside of TST (in other words, by any other addon.)
    */
    for (const key of Object.keys(changeInfo)) {
      if (key == 'index')
        continue;
      if ('key' in updatedTab)
        oldState[key] = updatedTab[key];
      if (key == 'openerTabId')
        log(`openerTabId of ${tabId} is changed by someone (notified via changeInfo)!: ${updatedTab.openerTabId} (original) => ${changeInfo[key]} (changed by someone)`, configs.debug && new Error().stack);
      updatedTab[key] = changeInfo[key];
    }
    if (changeInfo.url ||
        changeInfo.status == 'complete') {
      // On some edge cases internally changed "favIconUrl" is not
      // notified, so we need to check actual favIconUrl manually.
      // Known cases are:
      //  * Transition from "about:privatebrowsing" to "about:blank"
      //    https://github.com/piroor/treestyletab/issues/1916
      //  * Reopen tab by Ctrl-Shift-T
      browser.tabs.get(tabId).then(tab => {
        if (tab.favIconUrl != updatedTab.favIconUrl)
          onUpdated(tabId, { favIconUrl: tab.favIconUrl }, tab);
      }).catch(ApiTabs.createErrorSuppressor());
    }
    if (configs.enableWorkaroundForBug1409262 &&
        tab.openerTabId != updatedTab.$TST.updatedOpenerTabId) {
      log(`openerTabId of ${tabId} is changed by someone!: ${updatedTab.$TST.updatedOpenerTabId} (original) => ${tab.openerTabId} (changed by someone) `, configs.debug && new Error().stack);
      updatedTab.$TST.updatedOpenerTabId = updatedTab.openerTabId = changeInfo.openerTabId = tab.openerTabId;
    }

    TabsUpdate.updateTab(updatedTab, changeInfo, { tab, old: oldState });

    const onUpdatedResult = Tab.onUpdated.dispatch(updatedTab, changeInfo);
    // don't do await if not needed, to process things synchronously
    if (onUpdatedResult instanceof Promise)
      await onUpdatedResult;

    onCompleted();
  }
  catch(e) {
    console.log(e);
    onCompleted();
  }
}

const mTabsHighlightedTimers = new Map();
const mLastHighlightedCount  = new Map();
async function onHighlighted(highlightInfo) {
  if (mPromisedStarted)
    await mPromisedStarted;

  let timer = mTabsHighlightedTimers.get(highlightInfo.windowId);
  if (timer)
    clearTimeout(timer);
  if ((mLastHighlightedCount.get(highlightInfo.windowId) || 0) <= 1 &&
      highlightInfo.tabIds.length == 1) {
    // simple active tab switching
    TabsUpdate.updateTabsHighlighted(highlightInfo);
    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_HIGHLIGHTED_TABS_CHANGED,
      windowId: highlightInfo.windowId,
      tabIds:   highlightInfo.tabIds
    });
    return;
  }
  timer = setTimeout(() => {
    mTabsHighlightedTimers.delete(highlightInfo.windowId);
    TabsUpdate.updateTabsHighlighted(highlightInfo);
    mLastHighlightedCount.set(highlightInfo.windowId, highlightInfo.tabIds.length);
    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_HIGHLIGHTED_TABS_CHANGED,
      windowId: highlightInfo.windowId,
      tabIds:   highlightInfo.tabIds
    });
  }, configs.delayToApplyHighlightedState);
  mTabsHighlightedTimers.set(highlightInfo.windowId, timer);
}

async function onCreated(tab) {
  if (mPromisedStarted)
    await mPromisedStarted;

  log('tabs.onCreated: ', dumpTab(tab));

  // Cache the initial index for areTabsFromOtherDeviceWithInsertAfterCurrent()@handle-tab-bunches.js
  // See also: https://github.com/piroor/treestyletab/issues/2419
  tab.$indexOnCreated = tab.index;

  return onNewTabTracked(tab, { trigger: 'tabs.onCreated' });
}

async function onNewTabTracked(tab, info) {
  const window               = Window.init(tab.windowId);
  const bypassTabControl     = window.bypassTabControlCount > 0;
  const positionedBySelf     = window.toBeOpenedTabsWithPositions > 0;
  const duplicatedInternally = window.duplicatingTabsCount > 0;
  const maybeOrphan          = window.toBeOpenedOrphanTabs > 0;
  const activeTab            = Tab.getActiveTab(window.id);
  const fromExternal         = !mAppIsActive && !tab.openerTabId;
  const initialOpenerTabId   = tab.openerTabId;

  // New tab's index can become invalid because the value of "index" is same to
  // the one given to browser.tabs.create() (new tab) or the original index
  // (restored tab) instead of its actual index.
  // (By the way, any pinned tab won't be opened after the first unpinned tab,
  // and any unpinned tab won't be opened before the last pinned tab. On such
  // cases Firefox automatically fixup the index regardless they are newly
  // opened ore restored, so we don't need to care such cases.)
  // See also:
  //   https://github.com/piroor/treestyletab/issues/2131
  //   https://github.com/piroor/treestyletab/issues/2216
  //   https://bugzilla.mozilla.org/show_bug.cgi?id=1541748
  tab.index = Math.max(0, Math.min(tab.index, window.tabs.size));
  tab.reindexedBy = `onNewTabTracked (${tab.index})`;

  // New tab from a bookmark or external apps always have its URL as the title
  // (but the scheme part is missing.)
  tab.$possibleInitialUrl = tab.title;

  // We need to track new tab after getting old active tab. Otherwise, this
  // operation updates the latest active tab in the window amd it becomes
  // impossible to know which tab was previously active.
  tab = Tab.track(tab);

  if (info.trigger == 'tabs.onCreated')
    tab.$TST.addState(Constants.kTAB_STATE_CREATING);

  const mayBeReplacedWithContainer = tab.$TST.mayBeReplacedWithContainer;
  log(`onNewTabTracked(${dumpTab(tab)}): `, tab, { window, positionedBySelf, mayBeReplacedWithContainer, duplicatedInternally, maybeOrphan, activeTab });

  Tab.onBeforeCreate.dispatch(tab, {
    positionedBySelf,
    mayBeReplacedWithContainer,
    maybeOrphan,
    activeTab,
    fromExternal
  });

  if (Tab.needToWaitTracked(tab.windowId, { exceptionTabId: tab.id }))
    await Tab.waitUntilTrackedAll(tab.windowId, { exceptionTabId: tab.id });

  const [onCompleted, previous] = addTabOperationQueue();
  if (!configs.acceleratedTabOperations && previous)
    await previous;

  log(`onNewTabTracked(${dumpTab(tab)}): start to create tab element`);

  // Cached tree information may be expired when there are multiple new tabs
  // opened at just same time and some of others are attached on listeners of
  // "onCreating" and other points. Thus we need to refresh cached information
  // dynamically.
  // See also: https://github.com/piroor/treestyletab/issues/2419
  let treeForActionDetection;
  const onTreeModified = (_child, _info) => {
    if (!treeForActionDetection ||
        !TabsStore.ensureLivingTab(tab))
      return;
    treeForActionDetection = Tree.snapshotForActionDetection(tab);
    log('Tree modification is detected while waiting. Cached tree for action detection is updated: ', treeForActionDetection);
  };
  // We should refresh ceched information only when tabs are creaetd and
  // attached, because the cacheed information was originally introduced for
  // failsafe around problems from tabs closed while waiting.
  Tree.onAttached.addListener(onTreeModified);

  try {
    tab = Tab.init(tab, { inBackground: false });

    const nextTab = Tab.getTabAt(window.id, tab.index);

    // We need to update "active" state of a new active tab immediately.
    // Attaching of initial child tab (this new tab may become it) to an
    // existing tab may produce collapsing of existing tree, and a
    // collapsing tree may have the old active tab. On such cases TST
    // tries to move focus to a nearest visible ancestor, instead of this
    // new active tab.
    // See also: https://github.com/piroor/treestyletab/issues/2155
    if (tab.active)
      TabsInternalOperation.setTabActive(tab);

    const uniqueId = await tab.$TST.promisedUniqueId;

    if (!TabsStore.ensureLivingTab(tab)) { // it can be removed while waiting
      onCompleted(uniqueId);
      tab.$TST.rejectOpened();
      Tab.untrack(tab.id);
      warnTabDestroyedWhileWaiting(tab.id, tab);
      return;
    }

    TabsUpdate.updateTab(tab, tab, {
      forceApply: true
    });

    const duplicated = duplicatedInternally || uniqueId.duplicated;
    const restored   = uniqueId.restored;
    const skipFixupTree = !nextTab;
    log(`onNewTabTracked(${dumpTab(tab)}): `, { duplicated, restored, skipFixupTree });

    const maybeNeedToFixupTree = (
      (info.mayBeReplacedWithContainer ||
       (!duplicated &&
        !restored &&
        !skipFixupTree)) &&
      !info.positionedBySelf
    );
    // Tabs can be removed and detached while waiting, so cache them here for `detectTabActionFromNewPosition()`.
    // This operation takes too much time so it should be skipped if unnecessary.
    // See also: https://github.com/piroor/treestyletab/issues/2278#issuecomment-521534290
    treeForActionDetection = maybeNeedToFixupTree ? Tree.snapshotForActionDetection(tab) : null;

    if (bypassTabControl)
      window.bypassTabControlCount--;
    if (positionedBySelf)
      window.toBeOpenedTabsWithPositions--;
    if (maybeOrphan)
      window.toBeOpenedOrphanTabs--;
    if (duplicatedInternally)
      window.duplicatingTabsCount--;

    if (restored) {
      window.restoredCount = window.restoredCount || 0;
      window.restoredCount++;
      if (!window.allTabsRestored) {
        log(`onNewTabTracked(${dumpTab(tab)}): Maybe starting to restore window`);
        window.allTabsRestored = new Promise((resolve, _aReject) => {
          let lastCount = window.restoredCount;
          const timer = setInterval(() => {
            if (lastCount != window.restoredCount) {
              lastCount = window.restoredCount;
              return;
            }
            clearTimeout(timer);
            window.allTabsRestored = null;
            window.restoredCount   = 0;
            log('All tabs are restored');
            resolve(lastCount);
          }, 200);
        });
        window.allTabsRestored = Tab.onWindowRestoring.dispatch(tab.windowId);
      }
      SidebarConnection.sendMessage({
        type:     Constants.kCOMMAND_NOTIFY_TAB_RESTORING,
        tabId:    tab.id,
        windowId: tab.windowId
      });
      await window.allTabsRestored;
      log(`onNewTabTracked(${dumpTab(tab)}): continued for restored tab`);
    }
    if (!TabsStore.ensureLivingTab(tab)) {
      log(`onNewTabTracked(${dumpTab(tab)}):  => aborted`);
      onCompleted(uniqueId);
      tab.$TST.rejectOpened();
      Tab.untrack(tab.id);
      warnTabDestroyedWhileWaiting(tab.id, tab);
      Tree.onAttached.removeListener(onTreeModified);
      return;
    }

    let moved = Tab.onCreating.dispatch(tab, {
      bypassTabControl,
      positionedBySelf,
      mayBeReplacedWithContainer,
      maybeOrphan,
      restored,
      duplicated,
      duplicatedInternally,
      activeTab,
      fromExternal
    });
    // don't do await if not needed, to process things synchronously
    if (moved instanceof Promise)
      moved = await moved;
    moved = moved === false;

    if (!TabsStore.ensureLivingTab(tab)) {
      log(`onNewTabTracked(${dumpTab(tab)}):  => aborted`);
      onCompleted(uniqueId);
      tab.$TST.rejectOpened();
      Tab.untrack(tab.id);
      warnTabDestroyedWhileWaiting(tab.id, tab);
      Tree.onAttached.removeListener(onTreeModified);
      return;
    }

    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_TAB_CREATING,
      windowId: tab.windowId,
      tabId:    tab.id,
      tab:      tab.$TST.sanitized,
      order:    window.order,
      maybeMoved: moved
    });
    log(`onNewTabTracked(${dumpTab(tab)}): moved = `, moved);

    if (TabsStore.ensureLivingTab(tab)) { // it can be removed while waiting
      window.openingTabs.add(tab.id);
      setTimeout(() => {
        if (!TabsStore.windows.get(tab.windowId)) // it can be removed while waiting
          return;
        window.openingTabs.delete(tab.id);
      }, 0);
    }

    if (!TabsStore.ensureLivingTab(tab)) { // it can be removed while waiting
      onCompleted(uniqueId);
      tab.$TST.rejectOpened();
      Tab.untrack(tab.id);
      warnTabDestroyedWhileWaiting(tab.id, tab);
      Tree.onAttached.removeListener(onTreeModified);
      return;
    }

    log(`onNewTabTracked(${dumpTab(tab)}): uniqueId = `, uniqueId);

    Tab.onCreated.dispatch(tab, {
      bypassTabControl,
      positionedBySelf,
      mayBeReplacedWithContainer,
      movedBySelfWhileCreation: moved,
      skipFixupTree,
      restored,
      duplicated,
      duplicatedInternally,
      originalTab: duplicated && Tab.get(uniqueId.originalTabId),
      treeForActionDetection,
      fromExternal
    });
    tab.$TST.resolveOpened();

    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_TAB_CREATED,
      windowId: tab.windowId,
      tabId:    tab.id,
      active:   tab.active,
      maybeMoved: moved
    });

    if (!duplicated &&
        restored) {
      tab.$TST.addState(Constants.kTAB_STATE_RESTORED);
      Tab.onRestored.dispatch(tab);
      checkRecycledTab(window.id);
    }

    onCompleted(uniqueId);
    tab.$TST.removeState(Constants.kTAB_STATE_CREATING);

    // tab can be changed while creating!
    const renewedTab = await browser.tabs.get(tab.id).catch(ApiTabs.createErrorHandler());
    if (!renewedTab)
      throw new Error(`tab ${tab.id} is closed while tracking`);

    const updatedOpenerTabId = tab.openerTabId;
    const changedProps = {};
    for (const key of Object.keys(renewedTab)) {
      const value = renewedTab[key];
      if (tab[key] == value)
        continue;
      if (key == 'openerTabId' &&
          info.trigger == 'tabs.onAttached' &&
          value != tab.openerTabId &&
          tab.openerTabId == tab.$TST.updatedOpenerTabId) {
        log(`openerTabId of ${tab.id} is different from the raw value but it has been updated by TST while attaching, so don't detect as updated for now`);
        continue;
      }
      changedProps[key] = value;
    }

    // When the active tab is duplicated, Firefox creates a duplicated tab
    // with its `openerTabId` filled with the ID of the source tab.
    // It is the `initialOpenerTabId`.
    // On the other hand, TST may attach the duplicated tab to any other
    // parent while it is initializing, based on a configuration
    // `configs.autoAttachOnDuplicated`. It is the `updatedOpenerTabId`.
    // At this scenario `renewedTab.openerTabId` becomes `initialOpenerTabId`
    // and `updatedOpenerTabId` is lost.
    // Thus we need to re-apply `updatedOpenerTabId` as the `openerTabId` of
    // the tab again, to keep the tree structure managed by TST.
    // See also: https://github.com/piroor/treestyletab/issues/2388
    if ('openerTabId' in changedProps) {
      log(`openerTabId of ${tab.id} is changed while creating: ${tab.openerTabId} (changed by someone) => ${changedProps.openerTabId} (original) `, configs.debug && new Error().stack);
      if (duplicated &&
          tab.active &&
          changedProps.openerTabId == initialOpenerTabId &&
          changedProps.openerTabId != updatedOpenerTabId) {
        log(`restore original openerTabId of ${tab.id} for duplicated active tab: ${updatedOpenerTabId}`);
        delete changedProps.openerTabId;
        browser.tabs.update(tab.id, { openerTabId: updatedOpenerTabId });
      }
    }

    if (Object.keys(renewedTab).length > 0)
      onUpdated(tab.id, changedProps, renewedTab);

    const currentActiveTab = Tab.getActiveTab(tab.windowId);
    if (renewedTab.active &&
        currentActiveTab.id != tab.id)
      onActivated({
        tabId:         tab.id,
        windowId:      tab.windowId,
        previousTabId: currentActiveTab.id
      });

    tab.$TST.memorizeNeighbors('newly tracked');
    if (tab.$TST.unsafePreviousTab)
      tab.$TST.unsafePreviousTab.$TST.memorizeNeighbors('unsafePreviousTab');
    if (tab.$TST.unsafeNextTab)
      tab.$TST.unsafeNextTab.$TST.memorizeNeighbors('unsafeNextTab');

    Tree.onAttached.removeListener(onTreeModified);

    return tab;
  }
  catch(e) {
    console.log(e, e.stack);
    onCompleted();
    tab.$TST.removeState(Constants.kTAB_STATE_CREATING);
    Tree.onAttached.removeListener(onTreeModified);
  }
}

// "Recycled tab" is an existing but reused tab for session restoration.
function checkRecycledTab(windowId) {
  const possibleRecycledTabs = Tab.getRecycledTabs(windowId);
  log(`Detecting recycled tabs`);
  for (const tab of possibleRecycledTabs) {
    if (!TabsStore.ensureLivingTab(tab))
      continue;
    const currentId = tab.$TST.uniqueId.id;
    tab.$TST.updateUniqueId().then(uniqueId => {
      if (!TabsStore.ensureLivingTab(tab) ||
          !uniqueId.restored ||
          uniqueId.id == currentId ||
          Constants.kTAB_STATE_RESTORED in tab.$TST.states)
        return;
      log('A recycled tab is detected: ', dumpTab(tab));
      tab.$TST.addState(Constants.kTAB_STATE_RESTORED);
      Tab.onRestored.dispatch(tab);
    });
  }
}

async function onRemoved(tabId, removeInfo) {
  Tree.markTabIdAsUnattachable(tabId);

  if (mPromisedStarted)
    await mPromisedStarted;

  log('tabs.onRemoved: ', tabId, removeInfo);
  const window              = Window.init(removeInfo.windowId);
  const byInternalOperation = window.internalClosingTabs.has(tabId);
  if (byInternalOperation)
    window.internalClosingTabs.delete(tabId);
  const preventEntireTreeBehavior = window.keepDescendantsTabs.has(tabId);
  if (preventEntireTreeBehavior)
    window.keepDescendantsTabs.delete(tabId);

  if (Tab.needToWaitTracked(removeInfo.windowId))
    await Tab.waitUntilTrackedAll(removeInfo.windowId);

  const [onCompleted, previous] = addTabOperationQueue();
  if (!configs.acceleratedTabOperations && previous)
    await previous;

  try {
    const oldTab = Tab.get(tabId);
    if (!oldTab) {
      onCompleted();
      return;
    }

    log('tabs.onRemoved, tab is found: ', oldTab, `openerTabId=${oldTab.openerTabId}`);

    const nearestTabs = [oldTab.$TST.unsafePreviousTab, oldTab.$TST.unsafeNextTab];

    // remove from "highlighted tabs" cache immediately, to prevent misdetection for "multiple highlighted".
    TabsStore.removeHighlightedTab(oldTab);
    TabsStore.removeGroupTab(oldTab);

    TabsStore.addRemovedTab(oldTab);

    removeInfo = {
      ...removeInfo,
      byInternalOperation,
      preventEntireTreeBehavior,
      oldChildren: oldTab.$TST.children,
      oldParent:   oldTab.$TST.parent,
      context: Constants.kPARENT_TAB_OPERATION_CONTEXT_CLOSE
    };

    if (!removeInfo.isWindowClosing) {
      SidebarConnection.sendMessage({
        type:            Constants.kCOMMAND_NOTIFY_TAB_REMOVING,
        windowId:        oldTab.windowId,
        tabId:           oldTab.id,
        isWindowClosing: removeInfo.isWindowClosing,
        byInternalOperation,
        preventEntireTreeBehavior,
      });
    }

    const onRemovingResult = Tab.onRemoving.dispatch(oldTab, {
      ...removeInfo,
      byInternalOperation,
      preventEntireTreeBehavior,
    });
    // don't do await if not needed, to process things synchronously
    if (onRemovingResult instanceof Promise)
      await onRemovingResult;

    // The removing tab may be attached to tree/someone attached to the removing tab.
    // We need to clear them by onRemoved handlers.
    removeInfo.oldChildren = oldTab.$TST.children;
    removeInfo.oldParent   = oldTab.$TST.parent;
    oldTab.$TST.addState(Constants.kTAB_STATE_REMOVING);
    TabsStore.addRemovingTab(oldTab);

    TabsStore.windows.get(removeInfo.windowId).detachTab(oldTab.id, {
      toBeRemoved: true
    });

    const onRemovedReuslt = Tab.onRemoved.dispatch(oldTab, removeInfo);
    // don't do await if not needed, to process things synchronously
    if (onRemovedReuslt instanceof Promise)
      await onRemovedReuslt;

    SidebarConnection.sendMessage({
      type:            Constants.kCOMMAND_NOTIFY_TAB_REMOVED,
      windowId:        oldTab.windowId,
      tabId:           oldTab.id,
      isWindowClosing: removeInfo.isWindowClosing,
      byInternalOperation,
      preventEntireTreeBehavior,
    });
    oldTab.$TST.destroy();

    for (const tab of nearestTabs) {
      if (!tab || !tab.$TST)
        continue;
      tab.$TST.memorizeNeighbors('neighbor of closed tab');
    }

    onCompleted();
  }
  catch(e) {
    console.log(e);
    onCompleted();
  }
  finally {
    Tree.clearUnattachableTabId(tabId);
  }
}

async function onMoved(tabId, moveInfo) {
  if (mPromisedStarted)
    await mPromisedStarted;

  const window = Window.init(moveInfo.windowId);

  // Firefox may move the tab between TabsMove.moveTabsInternallyBefore/After()
  // and TabsMove.syncTabsPositionToApiTabs(). We should treat such a movement
  // as an "internal" operation also, because we need to suppress "move back"
  // and other fixup operations around tabs moved by foreign triggers, on such
  // cases. Don't mind, the tab will be rearranged again by delayed
  // TabsMove.syncTabsPositionToApiTabs() anyway!
  const maybeInternalOperation = window.internalMovingTabs.has(tabId);

  if (!Tab.isTracked(tabId))
    await Tab.waitUntilTracked(tabId, { element: !!TabsStore.getCurrentWindowId() });
  if (Tab.needToWaitMoved(moveInfo.windowId))
    await Tab.waitUntilMovedAll(moveInfo.windowId);

  const [onCompleted, previous] = addTabOperationQueue();
  if (!configs.acceleratedTabOperations && previous)
    await previous;

  try {
    const finishMoving = Tab.get(tabId).$TST.startMoving();
    const completelyMoved = () => { finishMoving(); onCompleted() };

    /* When a tab is pinned, tabs.onMoved may be notified before
       tabs.onUpdated(pinned=true) is notified. As the result,
       descendant tabs are unexpectedly moved to the top of the
       tab bar to follow their parent pinning tab. To avoid this
       problem, we have to wait for a while with this "async" and
       do following processes after the tab is completely pinned. */
    const movedTab = Tab.get(tabId);
    if (!movedTab) {
      if (maybeInternalOperation)
        window.internalMovingTabs.delete(tabId);
      completelyMoved();
      warnTabDestroyedWhileWaiting(tabId, movedTab);
      return;
    }

    let oldPreviousTab = movedTab.hidden ? movedTab.$TST.unsafePreviousTab : movedTab.$TST.previousTab;
    let oldNextTab     = movedTab.hidden ? movedTab.$TST.unsafeNextTab : movedTab.$TST.nextTab;
    if (movedTab.index != moveInfo.toIndex ||
        (oldPreviousTab && oldPreviousTab.index == movedTab.index - 1) ||
        (oldNextTab && oldNextTab.index == movedTab.index + 1)) {
      // already moved
      oldPreviousTab = Tab.getTabAt(moveInfo.windowId, moveInfo.toIndex < moveInfo.fromIndex ? moveInfo.fromIndex : moveInfo.fromIndex - 1);
      oldNextTab     = Tab.getTabAt(moveInfo.windowId, moveInfo.toIndex < moveInfo.fromIndex ? moveInfo.fromIndex + 1 : moveInfo.fromIndex);
      if (oldPreviousTab && oldPreviousTab.id == movedTab.id)
        oldPreviousTab = Tab.getTabAt(moveInfo.windowId, moveInfo.toIndex < moveInfo.fromIndex ? moveInfo.fromIndex - 1 : moveInfo.fromIndex - 2);
      if (oldNextTab && oldNextTab.id == movedTab.id)
        oldNextTab = Tab.getTabAt(moveInfo.windowId, moveInfo.toIndex < moveInfo.fromIndex ? moveInfo.fromIndex : moveInfo.fromIndex - 1);
    }

    let alreadyMoved = false;
    if (window.alreadyMovedTabs.has(tabId)) {
      window.alreadyMovedTabs.delete(tabId);
      alreadyMoved = true;
    }

    const extendedMoveInfo = {
      ...moveInfo,
      byInternalOperation: maybeInternalOperation,
      alreadyMoved,
      oldPreviousTab,
      oldNextTab,
      isSubstantiallyMoved: movedTab.$TST.isSubstantiallyMoved
    };
    log('tabs.onMoved: ', movedTab, extendedMoveInfo);

    let canceled = Tab.onMoving.dispatch(movedTab, extendedMoveInfo);
    // don't do await if not needed, to process things synchronously
    if (canceled instanceof Promise)
      await canceled;
    canceled = canceled === false;
    if (!canceled &&
        TabsStore.ensureLivingTab(movedTab)) { // it is removed while waiting
      let newNextIndex = extendedMoveInfo.toIndex;
      if (extendedMoveInfo.fromIndex < newNextIndex)
        newNextIndex++;
      const nextTab = Tab.getTabAt(moveInfo.windowId, newNextIndex);
      extendedMoveInfo.nextTab = nextTab;
      if (!alreadyMoved &&
          movedTab.$TST.nextTab != nextTab) {
        if (nextTab) {
          if (nextTab.index > movedTab.index)
            movedTab.index = nextTab.index - 1;
          else
            movedTab.index = nextTab.index;
        }
        else {
          movedTab.index = window.tabs.size - 1
        }
        movedTab.reindexedBy = `tabs.onMoved (${movedTab.index})`;
        window.trackTab(movedTab);
        log('Tab nodes rearranged by tabs.onMoved listener:\n'+(!configs.debug ? '' :
          toLines(Array.from(window.getOrderedTabs()),
                  tab => ` - ${tab.index}: ${tab.id}${tab.id == movedTab.id ? '[MOVED]' : ''}`)),
            { moveInfo });
      }
      const onMovedResult = Tab.onMoved.dispatch(movedTab, extendedMoveInfo);
      // don't do await if not needed, to process things synchronously
      if (onMovedResult instanceof Promise)
        await onMovedResult;
      if (!alreadyMoved)
        SidebarConnection.sendMessage({
          type:      Constants.kCOMMAND_NOTIFY_TAB_MOVED,
          windowId:  movedTab.windowId,
          tabId:     movedTab.id,
          newIndex:  movedTab.index,
          nextTabId: nextTab && nextTab.id
        });
    }
    if (maybeInternalOperation)
      window.internalMovingTabs.delete(tabId);
    completelyMoved();

    movedTab.$TST.memorizeNeighbors('moved');
    if (movedTab.$TST.unsafePreviousTab)
      movedTab.$TST.unsafePreviousTab.$TST.memorizeNeighbors('unsafePreviousTab');
    if (movedTab.$TST.unsafeNextTab)
      movedTab.$TST.unsafeNextTab.$TST.memorizeNeighbors('unsafeNextTab');

    if (oldPreviousTab)
      oldPreviousTab.$TST.memorizeNeighbors('oldPreviousTab');
    if (oldNextTab)
      oldNextTab.$TST.memorizeNeighbors('oldNextTab');
  }
  catch(e) {
    console.log(e);
    onCompleted();
  }
}

const mTreeInfoForTabsMovingAcrossWindows = new Map();

async function onAttached(tabId, attachInfo) {
  if (mPromisedStarted)
    await mPromisedStarted;

  const [onCompleted, previous] = addTabOperationQueue();
  if (!configs.acceleratedTabOperations && previous)
    await previous;

  try {
    log('tabs.onAttached, id: ', tabId, attachInfo);
    const tab = Tab.get(tabId);
    const attachedTab = await browser.tabs.get(tabId).catch(ApiTabs.createErrorHandler());
    if (!attachedTab) {
      onCompleted();
      return;
    }

    if (tab) {
      tab.windowId = attachInfo.newWindowId
      tab.index    = attachedTab.index;
      tab.reindexedBy = `tabs.onAttached (${tab.index})`;
    }

    TabsInternalOperation.clearOldActiveStateInWindow(attachInfo.newWindowId);
    const info = {
      ...attachInfo,
      ...mTreeInfoForTabsMovingAcrossWindows.get(tabId)
    };
    mTreeInfoForTabsMovingAcrossWindows.delete(tabId);

    const window = TabsStore.windows.get(attachInfo.newWindowId);
    await onNewTabTracked(tab, { trigger: 'tabs.onAttached' });
    const byInternalOperation = window.toBeAttachedTabs.has(tab.id);
    if (byInternalOperation)
      window.toBeAttachedTabs.delete(tab.id);
    info.byInternalOperation = info.byInternalOperation || byInternalOperation;

    if (!byInternalOperation) { // we should process only tabs attached by others.
      const onAttachedResult = Tab.onAttached.dispatch(tab, info);
      // don't do await if not needed, to process things synchronously
      if (onAttachedResult instanceof Promise)
        await onAttachedResult;
    }

    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_TAB_ATTACHED_TO_WINDOW,
      windowId: attachInfo.newWindowId,
      tabId
    });

    onCompleted();
  }
  catch(e) {
    console.log(e);
    onCompleted();
  }
}

async function onDetached(tabId, detachInfo) {
  if (mPromisedStarted)
    await mPromisedStarted;

  const [onCompleted, previous] = addTabOperationQueue();
  if (!configs.acceleratedTabOperations && previous)
    await previous;

  try {
    log('tabs.onDetached, id: ', tabId, detachInfo);
    const oldTab = Tab.get(tabId);
    if (!oldTab) {
      onCompleted();
      return;
    }

    const oldWindow           = TabsStore.windows.get(detachInfo.oldWindowId);
    const byInternalOperation = oldWindow.toBeDetachedTabs.has(tabId);
    if (byInternalOperation)
      oldWindow.toBeDetachedTabs.delete(tabId);

    const descendants = oldTab.$TST.descendants;
    const info = {
      ...detachInfo,
      byInternalOperation,
      trigger:     'tabs.onDetached',
      windowId:    detachInfo.oldWindowId,
      structure:   TreeBehavior.getTreeStructureFromTabs([oldTab, ...descendants]),
      descendants
    };
    const alreadyMovedAcrossWindows = Array.from(mTreeInfoForTabsMovingAcrossWindows.values(), info => info.descendants.map(tab => tab.id)).some(tabIds => tabIds.includes(tabId));
    if (!alreadyMovedAcrossWindows)
      mTreeInfoForTabsMovingAcrossWindows.set(tabId, info);

    if (!byInternalOperation) // we should process only tabs detached by others.
      Tab.onDetached.dispatch(oldTab, info);

    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_TAB_DETACHED_FROM_WINDOW,
      windowId: detachInfo.oldWindowId,
      tabId,
      wasPinned: oldTab.pinned
    });

    TabsStore.addRemovedTab(oldTab);
    oldWindow.detachTab(oldTab.id, {
      toBeDetached: true
    });
    if (!TabsStore.getCurrentWindowId() && // only in the background page - the sidebar has no need to destroy itself manually.
        oldWindow.tabs &&
        oldWindow.tabs.size == 0) { // not destroyed yet case
      if (oldWindow.delayedDestroy)
        clearTimeout(oldWindow.delayedDestroy);
      oldWindow.delayedDestroy = setTimeout(() => {
        // the last tab can be removed with browser.tabs.closeWindowWithLastTab=false,
        // so we should not destroy the window immediately.
        if (oldWindow.tabs &&
            oldWindow.tabs.size == 0)
          oldWindow.destroy();
      }, (configs.collapseDuration, 1000) * 5);
    }

    onCompleted();
  }
  catch(e) {
    console.log(e);
    onCompleted();
  }
}

async function onWindowCreated(window) {
  const trackedWindow = TabsStore.windows.get(window.id) || new Window(window.id);
  trackedWindow.incognito = window.incognito;
}

async function onWindowRemoved(windowId) {
  if (mPromisedStarted)
    await mPromisedStarted;

  mTabsHighlightedTimers.delete(windowId);
  mLastHighlightedCount.delete(windowId);

  const [onCompleted, previous] = addTabOperationQueue();
  if (!configs.acceleratedTabOperations && previous)
    await previous;

  try {
    log('onWindowRemoved ', windowId);
    const window = TabsStore.windows.get(windowId);
    if (window &&
        !TabsStore.getCurrentWindowId()) // skip destructor on sidebar
      window.destroy();

    onCompleted();
  }
  catch(e) {
    console.log(e);
    onCompleted();
  }
}


browser.windows.onFocusChanged.addListener(windowId => {
  mAppIsActive = windowId > 0;
});

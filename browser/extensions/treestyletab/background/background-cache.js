/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  dumpTab,
  wait,
  mapAndFilter,
  configs
} from '/common/common.js';
import * as Constants from '/common/constants.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TabsUpdate from '/common/tabs-update.js';
import * as TabsInternalOperation from '/common/tabs-internal-operation.js';
import * as MetricsData from '/common/metrics-data.js';

import Tab from '/common/Tab.js';

import * as Tree from './tree.js';

function log(...args) {
  internalLogger('background/background-cache', ...args);
}

const kCONTENTS_VERSION = 5;

let mActivated = false;

export function activate() {
  mActivated = true;
  configs.$addObserver(onConfigChange);
}


// ===================================================================
// restoring tabs from cache
// ===================================================================

export async function restoreWindowFromEffectiveWindowCache(windowId, options = {}) {
  MetricsData.add('restoreWindowFromEffectiveWindowCache: start');
  log(`restoreWindowFromEffectiveWindowCache for ${windowId} start`);
  const owner = options.owner || getWindowCacheOwner(windowId);
  if (!owner) {
    log(`restoreWindowFromEffectiveWindowCache for ${windowId} fail: no owner`);
    return false;
  }
  cancelReservedCacheTree(windowId); // prevent to break cache before loading
  const tabs = options.tabs || await browser.tabs.query({ windowId }).catch(ApiTabs.createErrorHandler());
  if (configs.debug)
    log(`restoreWindowFromEffectiveWindowCache for ${windowId} tabs: `, () => tabs.map(dumpTab));
  const actualSignature = getWindowSignature(tabs);
  let cache = options.caches && options.caches.get(`window-${owner.windowId}`) || await MetricsData.addAsync('restoreWindowFromEffectiveWindowCache: window cache', getWindowCache(owner, Constants.kWINDOW_STATE_CACHED_TABS));
  if (!cache) {
    log(`restoreWindowFromEffectiveWindowCache for ${windowId} fail: no cache`);
    return false;
  }
  const promisedPermanentStates = Promise.all(tabs.map(tab => Tab.get(tab.id).$TST.getPermanentStates())); // don't await at here for better performance
  MetricsData.add('restoreWindowFromEffectiveWindowCache: validity check: start');
  let cachedSignature = cache && cache.signature;
  log(`restoreWindowFromEffectiveWindowCache for ${windowId}: got from the owner `, {
    owner, cachedSignature, cache
  });
  const signatureGeneratedFromCache = signatureFromTabsCache(cache.tabs).join('\n');
  if (cache &&
      cache.tabs &&
      cachedSignature &&
      cachedSignature.join('\n') != signatureGeneratedFromCache) {
    log(`restoreWindowFromEffectiveWindowCache for ${windowId}: cache is broken.`, {
      cachedSignature: cachedSignature.join('\n'),
      signatureGeneratedFromCache
    });
    cache = cachedSignature = null;
    TabsInternalOperation.clearCache(owner);
    MetricsData.add('restoreWindowFromEffectiveWindowCache: validity check: signature failed.');
  }
  else {
    MetricsData.add('restoreWindowFromEffectiveWindowCache: validity check: signature passed.');
  }
  if (options.ignorePinnedTabs &&
      cache &&
      cache.tabs &&
      cachedSignature) {
    cache.tabs      = trimTabsCache(cache.tabs, cache.pinnedTabsCount);
    cachedSignature = trimSignature(cachedSignature, cache.pinnedTabsCount);
  }
  MetricsData.add('restoreWindowFromEffectiveWindowCache: validity check: matching actual signature of got cache');
  const signatureMatched = matcheSignatures({
    actual: actualSignature,
    cached: cachedSignature
  });
  log(`restoreWindowFromEffectiveWindowCache for ${windowId}: verify cache`, {
    cache, actualSignature, cachedSignature, signatureMatched
  });
  if (!cache ||
      cache.version != kCONTENTS_VERSION ||
      !signatureMatched) {
    log(`restoreWindowFromEffectiveWindowCache for ${windowId}: no effective cache`);
    TabsInternalOperation.clearCache(owner);
    MetricsData.add('restoreWindowFromEffectiveWindowCache: validity check: actual signature failed.');
    return false;
  }
  MetricsData.add('restoreWindowFromEffectiveWindowCache: validity check: actual signature passed.');
  cache.offset = actualSignature.length - cachedSignature.length;

  log(`restoreWindowFromEffectiveWindowCache for ${windowId}: restore from cache`);

  const permanentStates = await MetricsData.addAsync('restoreWindowFromEffectiveWindowCache: permanentStatus', promisedPermanentStates); // await at here for better performance
  const restored = await MetricsData.addAsync('restoreWindowFromEffectiveWindowCache: restoreTabsFromCache', restoreTabsFromCache(windowId, { cache, tabs, permanentStates }));
  if (restored)
    MetricsData.add(`restoreWindowFromEffectiveWindowCache: window ${windowId} succeeded`);
  else
    MetricsData.add(`restoreWindowFromEffectiveWindowCache: window ${windowId} failed`);

  log(`restoreWindowFromEffectiveWindowCache for ${windowId}: restored = ${restored}`);
  return restored;
}

function getWindowSignature(tabs) {
  const tabIds = tabs.map(tab => tab.id);
  return tabs.map(tab => `${tab.openerTabId ? tabIds.indexOf(tab.$TST && tab.$TST.parentId || tab.openerTabId) : -1 },${tab.cookieStoreId},${tab.incognito},${tab.pinned}`);
}

function trimSignature(signature, ignoreCount) {
  if (!ignoreCount || ignoreCount < 0)
    return signature;
  return signature.slice(ignoreCount);
}

function trimTabsCache(cache, ignoreCount) {
  if (!ignoreCount || ignoreCount < 0)
    return cache;
  return cache.slice(ignoreCount);
}

function matcheSignatures(signatures) {
  return (
    signatures.actual &&
    signatures.cached &&
    signatures.actual.slice(-signatures.cached.length).join('\n') == signatures.cached.join('\n')
  );
}

function signatureFromTabsCache(cachedTabs) {
  const cachedTabIds = cachedTabs.map(tab => tab.id);
  return cachedTabs.map(tab => `${tab.$TST.parentId ? cachedTabIds.indexOf(tab.$TST.parentId) : -1 },${tab.cookieStoreId},${tab.incognito},${tab.pinned}`);
}


async function restoreTabsFromCache(windowId, params = {}) {
  if (!params.cache ||
      params.cache.version != kCONTENTS_VERSION)
    return false;

  return (await restoreTabsFromCacheInternal({
    windowId:     windowId,
    tabs:         params.tabs,
    permanentStates: params.permanentStates,
    offset:       params.cache.offset || 0,
    cache:        params.cache.tabs
  })).length > 0;
}

async function restoreTabsFromCacheInternal(params) {
  MetricsData.add('restoreTabsFromCacheInternal: start');
  log(`restoreTabsFromCacheInternal: restore tabs for ${params.windowId} from cache`);
  const offset = params.offset || 0;
  const window = TabsStore.windows.get(params.windowId);
  const tabs   = params.tabs.slice(offset).map(tab => Tab.get(tab.id));
  if (offset > 0 &&
      tabs.length <= offset) {
    log('restoreTabsFromCacheInternal: missing window');
    return [];
  }
  log(`restoreTabsFromCacheInternal: there is ${window.tabs.size} tabs`);
  if (params.cache.length != tabs.length) {
    log('restoreTabsFromCacheInternal: Mismatched number of restored tabs?');
    return [];
  }
  try {
    await MetricsData.addAsync('rebuildAll: fixupTabsRestoredFromCache', fixupTabsRestoredFromCache(tabs, params.permanentStates, params.cache));
  }
  catch(e) {
    log(String(e), e.stack);
    throw e;
  }
  log('restoreTabsFromCacheInternal: done');
  if (configs.debug)
    Tab.dumpAll();
  return tabs;
}

async function fixupTabsRestoredFromCache(tabs, permanentStates, cachedTabs) {
  MetricsData.add('fixupTabsRestoredFromCache: start');
  if (tabs.length != cachedTabs.length)
    throw new Error(`fixupTabsRestoredFromCache: Mismatched number of tabs restored from cache, tabs=${tabs.length}, cachedTabs=${cachedTabs.length}`);
  log('fixupTabsRestoredFromCache start ', () => ({ tabs: tabs.map(dumpTab), cachedTabs }));
  const idMap = new Map();
  // step 1: build a map from old id to new id
  tabs = tabs.map((tab, index) => {
    const cachedTab = cachedTabs[index];
    const oldId     = cachedTab.id;
    tab = Tab.get(tab.id);
    log(`fixupTabsRestoredFromCache: remap ${oldId} => ${tab.id}`);
    idMap.set(oldId, tab);
    return tab;
  });
  MetricsData.add('fixupTabsRestoredFromCache: step 1 done.');
  // step 2: restore information of tabs
  // Do this from bottom to top, to reduce post operations for modified trees.
  // (Attaching a tab to an existing tree will trigger "update" task for
  // existing ancestors, but attaching existing subtree to a solo tab won't
  // trigger such tasks.)
  // See also: https://github.com/piroor/treestyletab/issues/2278#issuecomment-519387792
  for (let i = tabs.length - 1; i > -1; i--) {
    fixupTabRestoredFromCache(tabs[i], permanentStates[i], cachedTabs[i], idMap);
  }
  // step 3: restore collapsed/expanded state of tabs and finalize the
  // restoration process
  // Do this from top to bottom, because a tab can be placed under an
  // expanded parent but the parent can be placed under a collapsed parent.
  for (const tab of tabs) {
    fixupTabRestoredFromCachePostProcess(tab);
  }
  MetricsData.add('fixupTabsRestoredFromCache: step 2 done.');
}

function fixupTabRestoredFromCache(tab, permanentStates, cachedTab, idMap) {
  tab.$TST.clear();
  const tabStates = new Set([...cachedTab.$TST.states, ...permanentStates]);
  for (const state of Constants.kTAB_TEMPORARY_STATES) {
    tabStates.delete(state);
  }
  tab.$TST.states = tabStates;
  tab.$TST.attributes = cachedTab.$TST.attributes;

  log('fixupTabRestoredFromCache children: ', cachedTab.$TST.childIds);
  const childIds = mapAndFilter(cachedTab.$TST.childIds, oldId => {
    const tab = idMap.get(oldId);
    return tab && tab.id || undefined;
  });
  tab.$TST.children = childIds;
  if (childIds.length > 0)
    tab.$TST.setAttribute(Constants.kCHILDREN, `|${childIds.join('|')}|`);
  else
    tab.$TST.removeAttribute(Constants.kCHILDREN);
  log('fixupTabRestoredFromCache children: => ', tab.$TST.childIds);

  log('fixupTabRestoredFromCache parent: ', cachedTab.$TST.parentId);
  const parentTab = idMap.get(cachedTab.$TST.parentId) || null;
  tab.$TST.parent = parentTab;
  if (parentTab)
    tab.$TST.setAttribute(Constants.kPARENT, parentTab.id);
  else
    tab.$TST.removeAttribute(Constants.kPARENT);
  log('fixupTabRestoredFromCache parent: => ', tab.$TST.parentId);

  tab.$TST.treeStructureAlreadyRestoredFromSessionData = true;
}

function fixupTabRestoredFromCachePostProcess(tab) {
  const parentTab = tab.$TST.parent;
  if (parentTab &&
      (parentTab.$TST.collapsed ||
       parentTab.$TST.subtreeCollapsed)) {
    tab.$TST.addState(Constants.kTAB_STATE_COLLAPSED);
    tab.$TST.addState(Constants.kTAB_STATE_COLLAPSED_DONE);
  }
  else {
    tab.$TST.removeState(Constants.kTAB_STATE_COLLAPSED);
    tab.$TST.removeState(Constants.kTAB_STATE_COLLAPSED_DONE);
  }

  TabsStore.updateIndexesForTab(tab);
  TabsUpdate.updateTab(tab, tab, { forceApply: true, onlyApply: true });
}


// ===================================================================
// updating cache
// ===================================================================

async function updateWindowCache(owner, key, value) {
  if (!owner)
    return;
  if (value === undefined) {
    try {
      return browser.sessions.removeWindowValue(owner.windowId, key).catch(ApiTabs.createErrorSuppressor());
    }
    catch(e) {
      console.log(new Error('fatal error: failed to delete window cache'), e, owner, key, value);
    }
  }
  else {
    try {
      return browser.sessions.setWindowValue(owner.windowId, key, value).catch(ApiTabs.createErrorSuppressor());
    }
    catch(e) {
      console.log(new Error('fatal error: failed to update window cache'), e, owner, key, value);
    }
  }
}

export function markWindowCacheDirtyFromTab(tab, akey) {
  const window = TabsStore.windows.get(tab.windowId);
  if (!window) // the window may be closed
    return;
  if (window.markWindowCacheDirtyFromTabTimeout)
    clearTimeout(window.markWindowCacheDirtyFromTabTimeout);
  window.markWindowCacheDirtyFromTabTimeout = setTimeout(() => {
    window.markWindowCacheDirtyFromTabTimeout = null;
    updateWindowCache(window.lastWindowCacheOwner, akey, true);
  }, 100);
}

async function getWindowCache(owner, key) {
  return browser.sessions.getWindowValue(owner.windowId, key).catch(ApiTabs.createErrorHandler());
}

function getWindowCacheOwner(windowId) {
  const tab = Tab.getFirstTab(windowId);
  if (!tab)
    return null;
  return {
    id:       tab.id,
    windowId: tab.windowId
  };
}

export async function reserveToCacheTree(windowId) {
  if (!mActivated ||
      !configs.useCachedTree)
    return;

  const window = TabsStore.windows.get(windowId);
  if (!window)
    return;

  // If there is any opening (but not resolved its unique id yet) tab,
  // we are possibly restoring tabs. To avoid cache breakage before
  // restoration, we must wait until we know whether there is any other
  // restoring tab or not.
  if (Tab.needToWaitTracked(windowId))
    await Tab.waitUntilTrackedAll(windowId);

  if (window.allTabsRestored)
    return;

  log('reserveToCacheTree for window ', windowId, { stack: configs.debug && new Error().stack });
  TabsInternalOperation.clearCache(windowId.lastWindowCacheOwner);

  if (window.waitingToCacheTree)
    clearTimeout(window.waitingToCacheTree);
  window.waitingToCacheTree = setTimeout(() => {
    cacheTree(windowId);
  }, 500);
}

function cancelReservedCacheTree(windowId) {
  const window = TabsStore.windows.get(windowId);
  if (window && window.waitingToCacheTree) {
    clearTimeout(window.waitingToCacheTree);
    delete window.waitingToCacheTree;
  }
}

async function cacheTree(windowId) {
  if (Tab.needToWaitTracked(windowId))
    await Tab.waitUntilTrackedAll(windowId);
  const window = TabsStore.windows.get(windowId);
  if (!window ||
      !configs.useCachedTree)
    return;
  const signature = getWindowSignature(Tab.getAllTabs(windowId));
  if (window.allTabsRestored)
    return;
  //log('save cache for ', windowId);
  window.lastWindowCacheOwner = getWindowCacheOwner(windowId);
  if (!window.lastWindowCacheOwner)
    return;
  log('cacheTree for window ', windowId, { stack: configs.debug && new Error().stack });
  updateWindowCache(window.lastWindowCacheOwner, Constants.kWINDOW_STATE_CACHED_TABS, {
    version:         kCONTENTS_VERSION,
    tabs:            TabsStore.windows.get(windowId).export(true),
    pinnedTabsCount: Tab.getPinnedTabs(windowId).length,
    signature
  });
}


// update cache on events

Tab.onCreated.addListener((tab, _info = {}) => {
  if (!tab.$TST.previousTab) { // it is a new cache owner
    const window = TabsStore.windows.get(tab.windowId);
    if (window.lastWindowCacheOwner)
      TabsInternalOperation.clearCache(window.lastWindowCacheOwner);
  }
  reserveToCacheTree(tab.windowId);
});

// Tree restoration for "Restore Previous Session"
Tab.onWindowRestoring.addListener(async windowId => {
  if (!configs.useCachedTree)
    return;

  log('Tabs.onWindowRestoring ', windowId);
  const window = TabsStore.windows.get(windowId);
  const restoredCount = await window.allTabsRestored;
  if (restoredCount == 1) {
    log('Tabs.onWindowRestoring: single tab restored');
    return;
  }

  log('Tabs.onWindowRestoring: continue ', windowId);
  MetricsData.add('Tabs.onWindowRestoring restore start');

  const tabs = await browser.tabs.query({ windowId }).catch(ApiTabs.createErrorHandler());
  try {
    await restoreWindowFromEffectiveWindowCache(windowId, {
      ignorePinnedTabs: true,
      owner: tabs[tabs.length - 1],
      tabs
    });
    MetricsData.add('Tabs.onWindowRestoring restore end');
  }
  catch(e) {
    log('Tabs.onWindowRestoring: FATAL ERROR while restoring tree from cache', String(e), e.stack);
  }
});

Tab.onRemoved.addListener((tab, info) => {
  if (!tab.$TST.previousTab) // the tab was the cache owner
    TabsInternalOperation.clearCache(tab);
  wait(0).then(() => {
  // "Restore Previous Session" closes some tabs at first, so we should not clear the old cache yet.
  // See also: https://dxr.mozilla.org/mozilla-central/rev/5be384bcf00191f97d32b4ac3ecd1b85ec7b18e1/browser/components/sessionstore/SessionStore.jsm#3053
    reserveToCacheTree(info.windowId);
  });
});

Tab.onMoved.addListener((tab, info) => {
  if (info.fromIndex == 0) // the tab is not the cache owner anymore
    TabsInternalOperation.clearCache(tab);
  reserveToCacheTree(info.windowId);
});

Tab.onUpdated.addListener((tab, _info) => {
  markWindowCacheDirtyFromTab(tab, Constants.kWINDOW_STATE_CACHED_SIDEBAR_TABS_DIRTY);
});

Tree.onSubtreeCollapsedStateChanging.addListener(tab => {
  reserveToCacheTree(tab.windowId);
});

Tree.onAttached.addListener((tab, _info) => {
  wait(0).then(() => {
    // "Restore Previous Session" closes some tabs at first and it causes tree changes, so we should not clear the old cache yet.
    // See also: https://dxr.mozilla.org/mozilla-central/rev/5be384bcf00191f97d32b4ac3ecd1b85ec7b18e1/browser/components/sessionstore/SessionStore.jsm#3053
    reserveToCacheTree(tab.windowId);
  });
});

Tree.onDetached.addListener((tab, _info) => {
  TabsInternalOperation.clearCache(tab);
  wait(0).then(() => {
    // "Restore Previous Session" closes some tabs at first and it causes tree changes, so we should not clear the old cache yet.
    // See also: https://dxr.mozilla.org/mozilla-central/rev/5be384bcf00191f97d32b4ac3ecd1b85ec7b18e1/browser/components/sessionstore/SessionStore.jsm#3053
    reserveToCacheTree(tab.windowId);
  });
});

Tab.onPinned.addListener(tab => {
  reserveToCacheTree(tab.windowId);
});

Tab.onUnpinned.addListener(tab => {
  if (tab.$TST.previousTab) // the tab was the cache owner
    TabsInternalOperation.clearCache(tab);
  reserveToCacheTree(tab.windowId);
});

Tab.onShown.addListener(tab => {
  reserveToCacheTree(tab.windowId);
});

Tab.onHidden.addListener(tab => {
  reserveToCacheTree(tab.windowId);
});

function onConfigChange(key) {
  switch (key) {
    case 'useCachedTree':
      browser.windows.getAll({
        populate:    true,
        windowTypes: ['normal']
      }).then(windows => {
        for (const window of windows) {
          const owner = window.tabs[window.tabs.length - 1];
          if (configs[key]) {
            reserveToCacheTree(window.id);
          }
          else {
            TabsInternalOperation.clearCache(owner);
            location.reload();
          }
        }
      }).catch(ApiTabs.createErrorSuppressor());
      break;
  }
}

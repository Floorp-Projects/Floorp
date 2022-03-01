/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  nextFrame,
  wait,
  countMatched,
  toLines,
  configs,
  shouldApplyAnimation
} from '/common/common.js';
import * as Constants from '/common/constants.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TabsUpdate from '/common/tabs-update.js';
import * as MetricsData from '/common/metrics-data.js';
import * as UserOperationBlocker from '/common/user-operation-blocker.js';

import Tab from '/common/Tab.js';
import Window from '/common/Window.js';

import * as SidebarTabs from './sidebar-tabs.js';
import * as Indent from './indent.js';
import * as BackgroundConnection from './background-connection.js';
import * as CollapseExpand from './collapse-expand.js';

import EventListenerManager from '/extlib/EventListenerManager.js';

import {
  kTAB_ELEMENT_NAME,
  TabUpdateTarget,
} from './components/TabElement.js';

function log(...args) {
  internalLogger('sidebar/sidebar-cache', ...args);
}

const kCONTENTS_VERSION = 29;

export const onRestored = new EventListenerManager();

let mTracking = false;

let mLastWindowCacheOwner;
let mTargetWindow;
let mTabBar;

export function init() {
  mTargetWindow = TabsStore.getCurrentWindowId();
  mTabBar       = document.querySelector('#tabbar');
}

export function startTracking() {
  mTracking = true;
  configs.$addObserver(onConfigChange);
}


// ===================================================================
// restoring tabs from cache
// ===================================================================

const mPreloadedCaches = new Map();

export async function tryPreload(tab = null) {
  if (!tab) {
    const tabs = await browser.tabs.query({ currentWindow: true }).catch(ApiTabs.createErrorHandler());
    if (tabs)
      tab = tabs.filter(tab => !tab.pinned)[0] || tabs[0];
  }
  if (tab)
    preload(tab);
}

async function preload(tab) {
  const cache = await Promise.all([
    browser.sessions.getWindowValue(tab.windowId, Constants.kWINDOW_STATE_CACHED_SIDEBAR),
    browser.sessions.getWindowValue(tab.windowId, Constants.kWINDOW_STATE_CACHED_SIDEBAR_TABS_DIRTY),
    browser.sessions.getWindowValue(tab.windowId, Constants.kWINDOW_STATE_CACHED_SIDEBAR_COLLAPSED_DIRTY),
    browser.sessions.getTabValue(tab.id, Constants.kWINDOW_STATE_CACHED_SIDEBAR),
    browser.sessions.getTabValue(tab.id, Constants.kWINDOW_STATE_CACHED_SIDEBAR_TABS_DIRTY),
    browser.sessions.getTabValue(tab.id, Constants.kWINDOW_STATE_CACHED_SIDEBAR_COLLAPSED_DIRTY)
  ]).catch(ApiTabs.createErrorSuppressor());
  if (!cache)
    return;
  mPreloadedCaches.set(`window${tab.windowId}`, cache.slice(0, 2));
  mPreloadedCaches.set(`tab${tab.id}`, cache.slice(3, 5));
}

export async function getEffectiveWindowCache(options = {}) {
  MetricsData.add('getEffectiveWindowCache: start');
  log('getEffectiveWindowCache: start');
  cancelReservedUpdateCachedTabbar(); // prevent to break cache before loading
  let cache;
  let cachedSignature;
  let actualSignature;
  await Promise.all([
    MetricsData.addAsync('getEffectiveWindowCache: main', async () => {
      const tabs = options.tabs || await browser.tabs.query({ currentWindow: true }).catch(ApiTabs.createErrorHandler());
      mLastWindowCacheOwner = tabs[0];
      if (!mLastWindowCacheOwner)
        return;
      // We cannot define constants with variables at a time like:
      //   [cache, const tabsDirty, const collapsedDirty] = await Promise.all([
      let tabsDirty, collapsedDirty;
      const preloadedCache = mPreloadedCaches.get(`window${mLastWindowCacheOwner.windowId}`);
      [cache, tabsDirty, collapsedDirty] = preloadedCache || await MetricsData.addAsync('getEffectiveWindowCache: reading window cache', Promise.all([
        getWindowCache(Constants.kWINDOW_STATE_CACHED_SIDEBAR),
        getWindowCache(Constants.kWINDOW_STATE_CACHED_SIDEBAR_TABS_DIRTY),
        getWindowCache(Constants.kWINDOW_STATE_CACHED_SIDEBAR_COLLAPSED_DIRTY)
      ]));
      mPreloadedCaches.clear();
      cachedSignature = cache && cache.signature;
      log(`getEffectiveWindowCache: got from the owner `, mLastWindowCacheOwner, {
        cachedSignature, cache, tabsDirty, collapsedDirty
      });
      MetricsData.add('getEffectiveWindowCache: starting to test validity of the cache');
      if (cache &&
          cache.tabs &&
          cachedSignature &&
          cachedSignature != signatureFromTabsCache(cache.tabbar.contents)) {
        log('getEffectiveWindowCache: cache is broken.', {
          signature: cachedSignature,
          cache:     signatureFromTabsCache(cache.tabbar.contents)
        });
        cache = cachedSignature = null;
        clearWindowCache();
      }
      MetricsData.add('getEffectiveWindowCache: validity check: signature passed.');
      if (options.ignorePinnedTabs &&
          cache &&
          cache.tabbar &&
          cache.tabbar.contents &&
          cachedSignature) {
        cache.tabbar.contents = trimTabsCache(cache.tabbar.contents, cache.tabbar.pinnedTabsCount);
        cachedSignature       = trimSignature(cachedSignature, cache.tabbar.pinnedTabsCount);
      }
      MetricsData.add('getEffectiveWindowCache: validity check: starting detailed verification. ' + JSON.stringify({
        cache: !!cache,
        version: cache && cache.version
      }));
      log('getEffectiveWindowCache: verify cache (1)', { cache, tabsDirty, collapsedDirty });
      if (cache && cache.version == kCONTENTS_VERSION) {
        log('getEffectiveWindowCache: restore sidebar from cache');
        cache.tabbar.tabsDirty      = tabsDirty;
        cache.tabbar.collapsedDirty = collapsedDirty;
        cache.signature = cachedSignature;
        MetricsData.add('getEffectiveWindowCache: validity check: version passed.');
      }
      else {
        log('getEffectiveWindowCache: invalid cache ', cache);
        cache = null;
        MetricsData.add('getEffectiveWindowCache: validity check: version failed.');
      }
    }),
    MetricsData.addAsync('getEffectiveWindowCache: getWindowSignature', async () => {
      if (!options.tabs)
        options.tabs = await browser.runtime.sendMessage({
          type:     Constants.kCOMMAND_PULL_TABS,
          windowId: mTargetWindow
        });
      actualSignature = getWindowSignature(options.tabs);
    })
  ]);

  MetricsData.add('getEffectiveWindowCache: validity check: matching actual signature of got cache');
  const signatureMatched = matcheSignatures({
    actual: actualSignature,
    cached: cachedSignature
  });
  log('getEffectiveWindowCache: verify cache (2)', {
    cache, actualSignature, cachedSignature, signatureMatched
  });
  if (!cache ||
      !signatureMatched) {
    clearWindowCache();
    cache = null;
    log('getEffectiveWindowCache: failed');
    MetricsData.add('getEffectiveWindowCache: validity check: actual signature failed.');
  }
  else {
    cache.offset= countMatched(actualSignature.replace(cachedSignature, '').trim().split('\n'),
                               part => part);
    cache.actualSignature = actualSignature;
    log('getEffectiveWindowCache: success ');
    MetricsData.add('getEffectiveWindowCache: validity check: actual signature passed.');
  }

  return cache;
}

function getWindowSignature(tabs) {
  const tabIds = tabs.map(tab => tab.id);
  return toLines(tabs,
                 tab => `${tab.openerTabId ? tabIds.indexOf(tab.openerTabId) : -1 },${tab.cookieStoreId},${tab.incognito},${tab.pinned}`);
}

function trimSignature(signature, ignoreCount) {
  if (!ignoreCount || ignoreCount < 0)
    return signature;
  return signature.split('\n').slice(ignoreCount).join('\n');
}

function trimTabsCache(cache, ignoreCount) {
  if (!ignoreCount || ignoreCount < 0)
    return cache;
  return cache.replace(new RegExp(`(<li[^>]*>[\\w\\W]+?<\/li>){${ignoreCount}}`), '');
}

function matcheSignatures(signatures) {
  return (
    signatures.actual &&
    signatures.cached &&
    signatures.actual.indexOf(signatures.cached) + signatures.cached.length == signatures.actual.length
  );
}

function signatureFromTabsCache(cachedTabs) {
  const idMatcher = new RegExp(`\bid="tab-([^"]+)"`);
  const cookieStoreIdMatcher = new RegExp(/\bcontextual-identity-[^\s]+\b/);
  const incognitoMatcher = new RegExp(/\bincognito\b/);
  const pinnedMatcher = new RegExp(/\bpinned\b/);
  const parentMatcher = new RegExp(`${Constants.kPARENT}="([^"]+)"`);
  const classesMatcher = new RegExp('class="([^"]+)"');
  const tabIds = [];
  const tabs = (cachedTabs.match(/(<li[^>]*>[\w\W]+?<\/li>)/g) || []).map(matched => {
    const id = parseInt(matched.match(idMatcher)[1]);
    tabIds.push(id);
    const classes = matched.match(classesMatcher)[1];
    const cookieStoreId = classes.match(cookieStoreIdMatcher);
    const parentMatched = matched.match(parentMatcher);
    return {
      id,
      cookieStoreId: cookieStoreId && cookieStoreId[1] || 'contextual-identity-firefox-default',
      parentId:      parentMatched && parseInt(parentMatched[1]),
      incognito:     incognitoMatcher.test(classes),
      pinned:        pinnedMatcher.test(classes)
    };
  });
  return toLines(tabs,
                 tab => `${tab.parentId ? tabIds.indexOf(tab.parentId) : -1 },${tab.cookieStoreId},${tab.incognito},${tab.pinned}`);
}


export async function restoreTabsFromCache(cache, params = {}) {
  const offset = params.offset || 0;
  const window = TabsStore.windows.get(mTargetWindow);
  if (offset <= 0) {
    if (window.element)
      window.element.parentNode.removeChild(window.element);
  }

  const restored = (await MetricsData.addAsync('restoreTabsFromCache: restoring internally', restoreTabsFromCacheInternal({
    windowId:     mTargetWindow,
    tabs:         params.tabs,
    offset:       offset,
    cache:        cache.contents,
    shouldUpdate: cache.tabsDirty
  }))).length > 0;

  if (restored) {
    try {
      MetricsData.add('restoreTabsFromCache: updating restored tabs');
      SidebarTabs.updateAll();
      MetricsData.add('restoreTabsFromCache: dispatching onRestored');
      onRestored.dispatch();
      MetricsData.add('restoreTabsFromCache: done');
    }
    catch(e) {
      log(String(e), e.stack);
      throw e;
    }
  }

  return restored;
}

async function restoreTabsFromCacheInternal(params) {
  MetricsData.add('restoreTabsFromCacheInternal: start');
  log(`restoreTabsFromCacheInternal: restore tabs for ${params.windowId} from cache`);
  const offset  = params.offset || 0;
  const tabs    = params.tabs.slice(offset);
  let container = TabsStore.windows.get(params.windowId).element;
  let tabElements;
  if (offset > 0) {
    if (!container ||
        container.querySelectorAll(kTAB_ELEMENT_NAME).length <= offset) {
      log('restoreTabsFromCacheInternal: missing container');
      return [];
    }
    log(`restoreTabsFromCacheInternal: there is ${container.querySelectorAll(kTAB_ELEMENT_NAME).length} tabs`);
    log('restoreTabsFromCacheInternal: delete obsolete tabs, offset = ', offset, tabs[0].id);
    const insertionPoint = document.createRange();
    insertionPoint.selectNodeContents(container);
    // for safety, now I use actual ID string instead of short way.
    insertionPoint.setStartBefore(Tab.get(tabs[0].id).$TST.element);
    insertionPoint.setEndAfter(Tab.get(tabs[tabs.length - 1].id).$TST.element);
    insertionPoint.deleteContents();
    const tabsMustBeRemoved = tabs.map(tab => Tab.get(tab.id));
    log('restoreTabsFromCacheInternal: cleared?: ',
        tabsMustBeRemoved.every(tab => !tab),
        tabsMustBeRemoved.map(tab => tab.id));
    log(`restoreTabsFromCacheInternal: => ${container.querySelectorAll(kTAB_ELEMENT_NAME).length} tabs`);
    const matched = params.cache.match(/<li/g);
    if (!matched)
      throw new Error('restoreTabsFromCacheInternal: invalid cache');
    log(`restoreTabsFromCacheInternal: restore ${matched.length} tabs from cache`);
    if (configs.debug)
      dumpCache(params.cache);
    insertionPoint.selectNodeContents(container);
    insertionPoint.collapse(false);
    const source   = params.cache.replace(/^<ul[^>]+>|<\/ul>$/g, '');
    const fragment = insertionPoint.createContextualFragment(source);
    insertionPoint.insertNode(fragment);
    insertionPoint.detach();
    tabElements = Array.from(container.querySelectorAll(kTAB_ELEMENT_NAME)).slice(-matched.length);
  }
  else {
    if (container && container.parentNode)
      container.parentNode.removeChild(container);
    log('restoreTabsFromCacheInternal: restore');
    if (configs.debug)
      dumpCache(params.cache);
    const insertionPoint = params.insertionPoint || (() => {
      const range = document.createRange();
      range.selectNodeContents(SidebarTabs.wholeContainer);
      range.collapse(false);
      return range;
    })();
    const fragment = insertionPoint.createContextualFragment(params.cache);
    container = fragment.firstChild;
    insertionPoint.insertNode(fragment);
    container.id = `window-${params.windowId}`;
    container.dataset.windowId = params.windowId;
    Window.init(params.windowId);
    tabElements = Array.from(container.querySelectorAll(kTAB_ELEMENT_NAME));
    if (!params.insertionPoint)
      insertionPoint.detach();
  }
  const spacer = container.querySelector(`.${Constants.kTABBAR_SPACER}`);
  if (spacer)
    spacer.style.minHeight = '';
  MetricsData.add('restoreTabsFromCacheInternal: DOM tree restoration finished');

  log('restoreTabsFromCacheInternal: post process ', { tabElements, tabs });
  if (tabElements.length != tabs.length) {
    log('restoreTabsFromCacheInternal: Mismatched number of restored tabs?');
    container.parentNode.removeChild(container); // clear dirty tree!
    return [];
  }
  try {
    await MetricsData.addAsync('restoreTabsFromCacheInternal: fixing restored DOM tree', async () => {
      const parent = container.parentNode;
      parent.removeChild(container); // remove from DOM tree to optimize
      await fixupTabsRestoredFromCache(tabElements, tabs, {
        dirty: params.shouldUpdate
      });
      parent.appendChild(container);
    });
  }
  catch(e) {
    log(String(e), e.stack);
    throw e;
  }
  log('restoreTabsFromCacheInternal: done');
  if (configs.debug)
    Tab.dumpAll();
  return tabElements;
}

function dumpCache(cache) {
  log(cache
    .replace(new RegExp(`([^\\s=])="[^"]*(\\n[^"]*)+"`, 'g'), '$1="..."')
    .replace(/(<(li|ul))/g, '\n$1'));
}

async function fixupTabsRestoredFromCache(tabElements, tabs, options = {}) {
  MetricsData.add('fixupTabsRestoredFromCache: start');
  if (tabElements.length != tabs.length)
    throw new Error(`fixupTabsRestoredFromCache: Mismatched number of tabs restored from cache, elements=${tabElements.length}, tabs.Tab=${tabs.length}`);
  log('fixupTabsRestoredFromCache start ', { elements: tabElements.map(tabElement => tabElement.id), tabs });
  let lastDraw = Date.now();
  let count = 0;
  const maxCount = tabElements.length * 2;
  // step 1: build a map from old id to new id
  for (let index = 0, maxi = tabElements.length; index < maxi; index++) {
    const tab        = tabs[index];
    const tabElement = tabElements[index];
    tabElement.setAttribute('id', `tab-${tab.id}`); // set tab element's id before initialization, to associate the tab element correctly
    tab.$TST.bindElement(tabElement);
    tabElement.apiTab = tab;
    Tab.init(tab, { existing: true });
    tab.$TST.setAttribute('id', tabElement.id);
    tabElement.$TST = tab.$TST;
    tab.$TST.setAttribute(Constants.kAPI_TAB_ID, tab.id || -1);
    tab.$TST.setAttribute(Constants.kAPI_WINDOW_ID, tab.windowId || -1);
    if (Date.now() - lastDraw > configs.intervalToUpdateProgressForBlockedUserOperation) {
      UserOperationBlocker.setProgress(Math.round(++count / maxCount * 33) + 33); // 2/3: fixup all tabs
      await nextFrame();
      lastDraw = Date.now();
    }
  }
  MetricsData.add('fixupTabsRestoredFromCache: step 1 finished');
  // step 2: restore information of tabElements
  // Do this from bottom to top, to reduce post operations for modified trees.
  // (Attaching a tab to an existing tree will trigger "update" task for
  // existing ancestors, but attaching existing subtree to a solo tab won't
  // trigger such tasks.)
  // See also: https://github.com/piroor/treestyletab/issues/2278#issuecomment-519387792
  for (let i = tabElements.length - 1; i > -1; i--) {
    const tabElement = tabElements[i];
    const tab = tabElement.apiTab;
    tab.$TST.updateElement(TabUpdateTarget.CollapseExpandState | TabUpdateTarget.TabProperties);
    if (options.dirty)
      TabsUpdate.updateTab(tab, tab, { forceApply: true });
    if (Date.now() - lastDraw > configs.intervalToUpdateProgressForBlockedUserOperation) {
      UserOperationBlocker.setProgress(Math.round(++count / maxCount * 33) + 33); // 3/3: apply states to tabs
      await nextFrame();
      lastDraw = Date.now();
    }
  }
  MetricsData.add('fixupTabsRestoredFromCache: step 2 finished');
}


// ===================================================================
// updating cache
// ===================================================================

function updateWindowCache(key, value) {
  if (!mLastWindowCacheOwner ||
      !Tab.get(mLastWindowCacheOwner.id))
    return;
  if (value === undefined) {
    //log('updateWindowCache: delete cache from ', mLastWindowCacheOwner, key);
    return browser.sessions.removeWindowValue(mLastWindowCacheOwner.windowId, key).catch(ApiTabs.createErrorSuppressor());
  }
  else {
    //log('updateWindowCache: set cache for ', mLastWindowCacheOwner, key);
    return browser.sessions.setWindowValue(mLastWindowCacheOwner.windowId, key, value).catch(ApiTabs.createErrorSuppressor());
  }
}

function clearWindowCache() {
  log('clearWindowCache ', { stack: configs.debug && new Error().stack });
  updateWindowCache(Constants.kWINDOW_STATE_CACHED_SIDEBAR);
  updateWindowCache(Constants.kWINDOW_STATE_CACHED_SIDEBAR_TABS_DIRTY);
  updateWindowCache(Constants.kWINDOW_STATE_CACHED_SIDEBAR_COLLAPSED_DIRTY);
}

export function markWindowCacheDirty(key) {
  if (markWindowCacheDirty.timeout)
    clearTimeout(markWindowCacheDirty.timeout);
  markWindowCacheDirty.timeout = setTimeout(() => {
    markWindowCacheDirty.timeout = null;
    updateWindowCache(key, true);
  }, 250);
}

async function getWindowCache(key) {
  if (!mLastWindowCacheOwner)
    return null;
  return browser.sessions.getWindowValue(mLastWindowCacheOwner.windowId, key).catch(ApiTabs.createErrorHandler());
}

function getWindowCacheOwner() {
  return Tab.getFirstTab(mTargetWindow);
}

export async function reserveToUpdateCachedTabbar() {
  if (!mTracking ||
      !configs.useCachedTree)
    return;

  // If there is any opening (but not resolved its unique id yet) tab,
  // we are possibly restoring tabs. To avoid cache breakage before
  // restoration, we must wait until we know whether there is any other
  // restoring tab or not.
  if (Tab.needToWaitTracked(null))
    await Tab.waitUntilTrackedAll(null, { element: true });

  log('reserveToUpdateCachedTabbar ', { stack: configs.debug && new Error().stack });
  // clear dirty cache
  clearWindowCache();

  if (updateCachedTabbar.waiting)
    clearTimeout(updateCachedTabbar.waiting);
  updateCachedTabbar.waiting = setTimeout(() => {
    delete updateCachedTabbar.waiting;
    updateCachedTabbar();
  }, 500);
}

function cancelReservedUpdateCachedTabbar() {
  if (updateCachedTabbar.waiting) {
    clearTimeout(updateCachedTabbar.waiting);
    delete updateCachedTabbar.waiting;
  }
}

async function updateCachedTabbar() {
  if (!configs.useCachedTree)
    return;
  if (Tab.needToWaitTracked(mTargetWindow))
    await Tab.waitUntilTrackedAll(mTargetWindow);
  const signature = getWindowSignature(Tab.getAllTabs(mTargetWindow));
  log('updateCachedTabbar ', { stack: configs.debug && new Error().stack });
  mLastWindowCacheOwner = getWindowCacheOwner(mTargetWindow);
  updateWindowCache(Constants.kWINDOW_STATE_CACHED_SIDEBAR, {
    version: kCONTENTS_VERSION,
    tabbar: {
      contents:        SidebarTabs.wholeContainer.innerHTML,
      style:           mTabBar.getAttribute('style'),
      pinnedTabsCount: Tab.getPinnedTabs(mTargetWindow).length
    },
    indent: Indent.getCacheInfo(),
    signature
  });
}


function onConfigChange(changedKey) {
  switch (changedKey) {
    case 'useCachedTree':
      if (configs[changedKey]) {
        reserveToUpdateCachedTabbar();
      }
      else {
        clearWindowCache();
        location.reload();
      }
      break;
  }
}

CollapseExpand.onUpdated.addListener((_tab, _options) => {
  markWindowCacheDirty(Constants.kWINDOW_STATE_CACHED_SIDEBAR_COLLAPSED_DIRTY);
});

BackgroundConnection.onMessage.addListener(async message => {
  switch (message.type) {
    case Constants.kCOMMAND_NOTIFY_TAB_CREATED:
    case Constants.kCOMMAND_NOTIFY_TAB_MOVED:
    case Constants.kCOMMAND_NOTIFY_TAB_LEVEL_CHANGED:
    case Constants.kCOMMAND_NOTIFY_CHILDREN_CHANGED:
      if (message.tabId)
        await Tab.waitUntilTracked(message.tabId, { element: true });
    case Constants.kCOMMAND_NOTIFY_TAB_DETACHED_FROM_WINDOW:
      await wait(0);
      // "Restore Previous Session" closes some tabs at first and it causes tree changes, so we should not clear the old cache yet.
      // See also: https://dxr.mozilla.org/mozilla-central/rev/5be384bcf00191f97d32b4ac3ecd1b85ec7b18e1/browser/components/sessionstore/SessionStore.jsm#3053
      reserveToUpdateCachedTabbar();
      break;

    case Constants.kCOMMAND_NOTIFY_TAB_REMOVED:
      await wait(0);
      if (shouldApplyAnimation())
        await wait(configs.collapseDuration);
      reserveToUpdateCachedTabbar();
      break;

    case Constants.kCOMMAND_NOTIFY_TAB_PINNED:
    case Constants.kCOMMAND_NOTIFY_TAB_UNPINNED:
    case Constants.kCOMMAND_NOTIFY_TAB_SHOWN:
    case Constants.kCOMMAND_NOTIFY_TAB_HIDDEN:
      if (message.tabId)
        await Tab.waitUntilTracked(message.tabId, { element: true });
      reserveToUpdateCachedTabbar();
      break;

    case Constants.kCOMMAND_NOTIFY_TAB_UPDATED:
    case Constants.kCOMMAND_NOTIFY_TAB_LABEL_UPDATED:
    case Constants.kCOMMAND_NOTIFY_TAB_FAVICON_UPDATED:
    case Constants.kCOMMAND_NOTIFY_TAB_SOUND_STATE_UPDATED:
      if (message.tabId)
        await Tab.waitUntilTracked(message.tabId, { element: true });
      await wait(0);
      markWindowCacheDirty(Constants.kWINDOW_STATE_CACHED_SIDEBAR_TABS_DIRTY);
      break;
  }
});

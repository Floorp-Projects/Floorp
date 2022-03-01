/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import RichConfirm from '/extlib/RichConfirm.js';

import {
  log as internalLogger,
  nextFrame,
  mapAndFilter,
  configs,
  shouldApplyAnimation,
  loadUserStyleRules,
  isMacOS,
  notify,
} from '/common/common.js';
import * as Constants from '/common/constants.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TabsInternalOperation from '/common/tabs-internal-operation.js';
import * as TabsUpdate from '/common/tabs-update.js';
import * as TSTAPI from '/common/tst-api.js';
import * as ContextualIdentities from '/common/contextual-identities.js';
import * as Bookmark from '/common/bookmark.js';
import * as UserOperationBlocker from '/common/user-operation-blocker.js';
import * as Color from '/common/color.js';
import * as BrowserTheme from '/common/browser-theme.js';
import * as MetricsData from '/common/metrics-data.js';

import Tab from '/common/Tab.js';
import Window from '/common/Window.js';


import { TabTwistyElement } from './components/TabTwistyElement.js';
import { TabCloseBoxElement } from './components/TabCloseBoxElement.js';
import { TabFaviconElement } from './components/TabFaviconElement.js';
import { TabLabelElement } from './components/TabLabelElement.js';
import { TabCounterElement } from './components/TabCounterElement.js';
import { TabSoundButtonElement } from './components/TabSoundButtonElement.js';
import {
  kTAB_ELEMENT_NAME,
  TabElement,
  TabInvalidationTarget,
  TabUpdateTarget,
} from './components/TabElement.js';

import * as BackgroundConnection from './background-connection.js';
import * as SidebarCache from './sidebar-cache.js';
import * as SidebarTabs from './sidebar-tabs.js';
import * as PinnedTabs from './pinned-tabs.js';
import * as DragAndDrop from './drag-and-drop.js';
import * as RestoringTabCount from './restoring-tab-count.js';
import * as CollapseExpand from './collapse-expand.js';
import * as Size from './size.js';
import * as Indent from './indent.js';
import * as Scroll from './scroll.js';
import * as GapCanceller from './gap-canceller.js';
import * as TabContextMenu from './tab-context-menu.js';
import * as SubPanel from './subpanel.js';
import './tst-api-frontend.js';

import EventListenerManager from '/extlib/EventListenerManager.js';

function log(...args) {
  internalLogger('sidebar/sidebar', ...args);
}

export const onInit    = new EventListenerManager();
export const onBuilt   = new EventListenerManager();
export const onReady   = new EventListenerManager();


let mTargetWindow = null;
let mInitialized = false;

let mPromisedTargetWindowResolver;
const mPromisedTargetWindow = new Promise((resolve, _reject) => {
  mPromisedTargetWindowResolver = resolve;
});

const mTabBar                     = document.querySelector('#tabbar');
const mStyleLoader                = document.querySelector('#style-loader');
const mBrowserThemeDefinition     = document.querySelector('#browser-theme-definition');
const mUserStyleRules             = document.querySelector('#user-style-rules');
const mContextualIdentitiesStyle  = document.querySelector('#contextual-identity-styling');

// allow customiation for platform specific styles with selectors like `:root[data-user-agent*="Windows NT 10"]`
document.documentElement.dataset.userAgent = navigator.userAgent;
document.documentElement.classList.toggle('platform-mac', isMacOS());

{
  const url = new URL(location.href);

  mTargetWindow = parseInt(url.searchParams.get('windowId') || 0);
  if (isNaN(mTargetWindow) || mTargetWindow < 1)
    mTargetWindow = null;

  // apply style ASAP!
  const style = url.searchParams.get('style');
  applyTheme({ style });

  const title = url.searchParams.get('title');
  if (title)
    document.title = title;
}

applyAnimationState(shouldApplyAnimation());
UserOperationBlocker.block({ throbber: true });

export async function init() {
  MetricsData.add('init: start');
  log('initialize sidebar on load');

  // If we call `window.customElements.define(localName, constructor)`;` from a file defining a custom element,
  // it would be a side-effect and happen accidentally that defining a custom element
  // when we import a new file which defines a new custom element.
  // It causes a complex side-effect relations and usually causes a bug. It's tough to fix.
  //
  // I have not concluded the best practice about it yet,
  // but I think that it's safely to call `window.customElements.define(localName, constructor)` separately
  // in the application initialization phase.
  //
  // XXX:
  //  We define our custom elements at first to avoid a problem which calls a method of custom element
  //  which has not been defined yet.
  TabTwistyElement.define();
  TabCloseBoxElement.define();
  TabFaviconElement.define();
  TabLabelElement.define();
  TabCounterElement.define();
  TabSoundButtonElement.define();
  TabElement.define();

  // Read caches from existing tabs at first, for better performance.
  // Those promises will be resolved while waiting other operations.
  SidebarCache.tryPreload();

  let promisedAllTabsTracked;
  UserOperationBlocker.setProgress(0);
  await Promise.all([
    MetricsData.addAsync('getting native tabs', async () => {
      const window = await MetricsData.addAsync(
        'getting window',
        mTargetWindow ?
          browser.windows.get(mTargetWindow, { populate: true }) :
          browser.windows.getCurrent({ populate: true })
      ).catch(ApiTabs.createErrorHandler());
      if (window.focused)
        document.documentElement.classList.add('active');
      const trackedWindow = TabsStore.windows.get(window.id) || new Window(window.id);
      trackedWindow.incognito = window.incognito;
      if (window.incognito)
        document.documentElement.classList.add('incognito');

      const tabs = window.tabs;
      SidebarCache.tryPreload(tabs.filter(tab => !tab.pinned)[0] || tabs[0]);
      if (!mTargetWindow)
        mTargetWindow = tabs[0].windowId;
      TabsStore.setCurrentWindowId(mTargetWindow);
      mPromisedTargetWindowResolver(mTargetWindow);
      internalLogger.context   = `Sidebar-${mTargetWindow}`;

      // Track only the first tab for now, because it is required to initialize
      // the container and it will be used by the SidebarCache module.
      Tab.track(tabs[0]);

      promisedAllTabsTracked = MetricsData.addAsync('tracking all native tabs', async () => {
        let lastDraw = Date.now();
        let count = 0;
        const maxCount = tabs.length - 1;
        for (const tab of tabs.slice(1)) {
          Tab.track(tab);
          if (Date.now() - lastDraw > configs.intervalToUpdateProgressForBlockedUserOperation) {
            UserOperationBlocker.setProgress(Math.round(++count / maxCount * 16) + 16); // 2/6: track all tabs
            await nextFrame();
            lastDraw = Date.now();
          }
        }
      });

      PinnedTabs.init();
      Indent.init();

      SidebarCache.init();
      SidebarCache.onRestored.addListener(() => { DragAndDrop.clearDropPosition(); });

      return tabs;
    }),
    configs.$loaded
  ]);
  MetricsData.add('browser.tabs.query finish, SidebarCache initialized, configs are loaded.');
  EventListenerManager.debug = configs.debug;

  onConfigChange('colorScheme');
  onConfigChange('simulateSVGContextFill');
  onInit.dispatch();

  const promisedScrollPosition = browser.sessions.getWindowValue(mTargetWindow, Constants.kWINDOW_STATE_SCROLL_POSITION).catch(ApiTabs.createErrorHandler());
  const promisedInitializedContextualIdentities = ContextualIdentities.init();

  UserOperationBlocker.setProgress(16); // 1/6: wait background page
  const promisedResults = Promise.all([
    MetricsData.addAsync('importTabsFromBackground()', importTabsFromBackground()),
    MetricsData.addAsync('promisedAllTabsTracked', promisedAllTabsTracked)
  ]);
  log('Start queuing of messages from the background page');
  BackgroundConnection.connect();
  const [importedTabs] = await promisedResults;

  // we don't need await for these features
  MetricsData.addAsync('API for other addons', TSTAPI.initAsFrontend());

  let cachedContents;
  let restoredFromCache;
  await Promise.all([
    MetricsData.addAsync('parallel initialization: main', async () => {
      if (configs.useCachedTree)
        cachedContents = await MetricsData.addAsync('parallel initialization: main: read cached sidebar contents', SidebarCache.getEffectiveWindowCache({ tabs: importedTabs }));
      restoredFromCache = await MetricsData.addAsync('parallel initialization: main: rebuildAll', rebuildAll(importedTabs, cachedContents && cachedContents.tabbar));

      TabsUpdate.completeLoadingTabs(mTargetWindow);

      log('Start to process messages including queued ones');
      BackgroundConnection.start();

      SidebarTabs.onSyncFailed.addListener(() => rebuildAll());

      configs.$addObserver(onConfigChange);
      onConfigChange('debug');
      onConfigChange('sidebarPosition');
      onConfigChange('showContextualIdentitiesSelector');
      onConfigChange('showNewTabActionSelector');

      document.addEventListener('focus', onFocus);
      document.addEventListener('blur', onBlur);
      window.addEventListener('resize', onResize);
      mTabBar.addEventListener('transitionend', onTransisionEnd);

      browser.theme.onUpdated.addListener(onBrowserThemeChanged);

      // We need to re-calculate mixed colors when the system color scheme is changed.
      // See also: https://github.com/piroor/treestyletab/issues/2314
      window.matchMedia('(prefers-color-scheme: dark)').addListener(async _event => {
        const theme = await browser.theme.getCurrent(mTargetWindow);
        applyBrowserTheme(theme);
      });

      browser.runtime.onMessage.addListener(onMessage);

      onBuilt.dispatch();

      DragAndDrop.init();
    }),
    MetricsData.addAsync('parallel initialization: Size', async () => {
      Size.init();
    }),
    MetricsData.addAsync('parallel initialization: contextual identities', async () => {
      await promisedInitializedContextualIdentities;
      updateContextualIdentitiesStyle();
      updateContextualIdentitiesSelector();
      ContextualIdentities.startObserve();
    }),
    MetricsData.addAsync('parallel initialization: TabContextMenu', async () => {
      TabContextMenu.init();
    })
  ]);

  await MetricsData.addAsync('parallel initialization: post process', Promise.all([
    MetricsData.addAsync('parallel initialization: post process: main', async () => {
      SidebarCache.startTracking();
      Indent.updateRestoredTree(cachedContents && cachedContents.indent);
      if (!restoredFromCache) {
        SidebarTabs.updateAll();
        SidebarCache.reserveToUpdateCachedTabbar();
      }
      updateTabbarLayout({ justNow: true });
      SubPanel.onResized.addListener(() => {
        updateTabbarLayout();
      });
      SubPanel.init();

      SidebarTabs.init();
      Indent.reserveToUpdateVisualMaxTreeLevel();

      shouldApplyAnimation.onChanged.addListener(applyAnimationState);
      applyAnimationState(shouldApplyAnimation());

      onReady.dispatch();
    }),
    MetricsData.addAsync('parallel initialization: post process: Scroll.init', async () => {
      Scroll.init(await promisedScrollPosition);
      Scroll.onPositionUnlocked.addListener(() => {
        reserveToUpdateTabbarLayout({
          reason:  Constants.kTABBAR_UPDATE_REASON_TAB_CLOSE,
          timeout: shouldApplyAnimation() ? configs.collapseDuration : 0
        });
      });
    })
  ]));

  TabsUpdate.completeLoadingTabs(mTargetWindow); // failsafe

  // Failsafe. If the sync operation fail after retryings,
  // SidebarTabs.onSyncFailed is notified then this sidebar page will be
  // reloaded for complete retry.
  SidebarTabs.reserveToSyncTabsOrder();

  document.documentElement.classList.remove('initializing');
  mInitialized = true;
  UserOperationBlocker.unblock({ throbber: true });

  GapCanceller.init();

  MetricsData.add('init: end');
  if (configs.debug)
    log(`Startup metrics for ${Tab.getTabs(mTargetWindow).length} tabs: `, MetricsData.toString());
}

function applyAnimationState(active) {
  const rootClasses = document.documentElement.classList;
  if (active)
    rootClasses.add('animation');
  else
    rootClasses.remove('animation');
}

async function applyTheme({ style } = {}) {
  const [theme, ] = await Promise.all([
    browser.theme.getCurrent(mTargetWindow),
    style && applyOwnTheme(style),
    !style && configs.$loaded.then(() => applyOwnTheme()),
    configs.$loaded
  ]);
  applyBrowserTheme(theme);
  applyUserStyleRules();
  Size.update();
}

async function applyOwnTheme(style) {
  if (!style)
    style = configs.style;
  switch (style) {
    case 'proton':
      mStyleLoader.setAttribute('href', 'styles/proton/proton.css');
      break;
    case 'sidebar':
      mStyleLoader.setAttribute('href', 'styles/sidebar/sidebar.css');
      break;
    case 'photon':
    // for backward compatibility, fall back to plain.
    case 'plain':
    case 'flat':
    case 'vertigo':
    case 'mixed':
      mStyleLoader.setAttribute('href', 'styles/photon/photon.css');
      break;
    case 'highcontrast':
      mStyleLoader.setAttribute('href', 'styles/photon/highcontrast.css');
      break;
    default:
      // as the base of customization. see also:
      // https://github.com/piroor/treestyletab/issues/1604
      mStyleLoader.setAttribute('href', 'data:text/css,');
      break;
  }
  return new Promise((resolve, _reject) => {
    mStyleLoader.addEventListener('load', () => {
      nextFrame().then(resolve);
    }, { once: true });
  });
}

function applyUserStyleRules() {
  mUserStyleRules.textContent = loadUserStyleRules();
}

async function applyBrowserTheme(theme) {
  log('applying theme ', theme);

  const browserThemeStyle = await BrowserTheme.generateThemeDeclarations(theme);
  // Apply theme color at first, to use given colors as the base of following "face-*" colors.
  mBrowserThemeDefinition.textContent = browserThemeStyle;

  const baseColor = Color.parseCSSColor(window.getComputedStyle(document.querySelector('#dummy-tab-color-box'), null).backgroundColor);
  const highlightColor = Color.parseCSSColor(window.getComputedStyle(document.querySelector('#dummy-highlight-color-box'), null).backgroundColor);
  const defaultColors = `:root {
    --face-highlight-lighter: ${Color.mixCSSColors(baseColor, { ...highlightColor, alpha: 0.35 })};
    --face-highlight-more-lighter: ${Color.mixCSSColors(baseColor, { ...highlightColor, alpha: 0.2 })};
    --face-highlight-more-more-lighter: ${Color.mixCSSColors(baseColor, { ...highlightColor, alpha: 0.1 })};
    --face-gradient-start-active: rgba(${baseColor.red}, ${baseColor.green}, ${baseColor.blue}, 0.4);
    --face-gradient-start-inactive: rgba(${baseColor.red}, ${baseColor.green}, ${baseColor.blue}, 0.2);
    --face-gradient-end: rgba(${baseColor.red}, ${baseColor.green}, ${baseColor.blue}, 0);
  }`;

  mBrowserThemeDefinition.textContent = [
    defaultColors,
    browserThemeStyle
  ].join('\n');
}

function updateContextualIdentitiesStyle() {
  const colorInfo = ContextualIdentities.getColorInfo();
  const definitions = Object.keys(colorInfo.colors).map(id =>
    `.tab.contextual-identity-${id} .contextual-identity-marker {
       background-color: ${colorInfo.colors[id]};
     }`);

  // This is required to map different color for color names.
  // See also: https://github.com/piroor/treestyletab/issues/2296
  definitions.push(colorInfo.colorDeclarations);

  mContextualIdentitiesStyle.textContent = definitions.join('\n');
}

function updateContextualIdentitiesSelector() {
  const disabled = document.documentElement.classList.contains('incognito') || ContextualIdentities.getCount() == 0;

  const anchors = document.querySelectorAll(`.${Constants.kCONTEXTUAL_IDENTITY_SELECTOR}-marker`);
  for (const anchor of anchors) {
    if (disabled)
      anchor.setAttribute('disabled', true);
    else
      anchor.removeAttribute('disabled');
  }

  const selector = document.getElementById(Constants.kCONTEXTUAL_IDENTITY_SELECTOR);
  const range    = document.createRange();
  range.selectNodeContents(selector);
  range.deleteContents();

  if (disabled)
    return;

  const fragment = document.createDocumentFragment();
  ContextualIdentities.forEach(identity => {
    const item = document.createElement('li');
    item.dataset.value = identity.cookieStoreId;
    item.textContent = identity.name;
    item.dataset.icon = identity.iconUrl;
    fragment.appendChild(item);
  });
  if (configs.inheritContextualIdentityToChildTabMode != Constants.kCONTEXTUAL_IDENTITY_DEFAULT) {
    const defaultCotnainerItem = document.createElement('li');
    defaultCotnainerItem.dataset.value = 'firefox-default';
    defaultCotnainerItem.textContent = browser.i18n.getMessage('tabbar_newTabWithContexualIdentity_default');
    const icon = document.createElement('span');
    icon.classList.add('icon');
    defaultCotnainerItem.insertBefore(icon, defaultCotnainerItem.firstChild);
    fragment.appendChild(defaultCotnainerItem);
  }
  range.insertNode(fragment);
  range.detach();
}

export async function rebuildAll(importedTabs, cache) {
  MetricsData.add('rebuildAll: start');
  const range = document.createRange();
  range.selectNodeContents(SidebarTabs.wholeContainer);
  range.deleteContents();
  range.detach();

  const trackedWindow = TabsStore.windows.get(mTargetWindow);
  if (!trackedWindow)
    Window.init(mTargetWindow);

  if (!importedTabs)
    importedTabs = await MetricsData.addAsync('rebuildAll: import tabs', browser.runtime.sendMessage({
      type:     Constants.kCOMMAND_PING_TO_BACKGROUND,
      windowId: mTargetWindow
    }).catch(ApiTabs.createErrorHandler()));

  // Ignore tabs already closed. It can happen when the first tab is
  // immediately reopened by other addons like Temporary Container.
  const importedTabIds = new Set(importedTabs.map(tab => tab.id));
  for (const tab of Tab.getAllTabs()) {
    if (!importedTabIds.has(tab.id))
      Tab.untrack(tab.id);
  }

  let tabs = importedTabs.map(importedTab => Tab.import(importedTab));

  if (cache) {
    const restored = await SidebarCache.restoreTabsFromCache(cache, { tabs });
    if (restored) {
      TabsInternalOperation.setTabActive(Tab.getActiveTab(mTargetWindow));
      MetricsData.add('rebuildAll: end (from cache)');
      return true;
    }
  }

  // Re-get tabs before rebuilding tree, because they can be modified while
  // waiting for SidebarCache.restoreTabsFromCache().
  await MetricsData.addAsync('rebuildAll: re-import tabs before rebuilding tree', async () => {
    const [nativeTabs, importedTabs] = await Promise.all([
      browser.tabs.query({ windowId: mTargetWindow }).catch(ApiTabs.createErrorHandler()),
      browser.runtime.sendMessage({
        type:     Constants.kCOMMAND_PULL_TABS,
        windowId: mTargetWindow
      })
    ]);
    let lastDraw = Date.now();
    let count = 0;
    const maxCount = nativeTabs.length;
    tabs = []
    for (let index = 0; index < maxCount; index++) {
      let tab = nativeTabs[index];
      Tab.track(tab);
      tab = importedTabs[index] && Tab.import(importedTabs[index]) || tab;
      if (!tab.$TST) {
        console.log('FATAL ERROR: Imported tab is not untracked yet. Reload the sidebar to retry initialization. See also: https://github.com/piroor/treestyletab/issues/2986');
        location.reload();
        return;
      }
      tab.$TST.unbindElement(); // The tab object can have old element already detached from the document, so we need to forget it.
      if (Date.now() - lastDraw > configs.intervalToUpdateProgressForBlockedUserOperation) {
        UserOperationBlocker.setProgress(Math.round(++count / maxCount * 33) + 33); // 2/3: re-track all tabs
        await nextFrame();
        lastDraw = Date.now();
      }
      tabs.push(tab);
    }
  });

  const window = Window.init(mTargetWindow);
  window.element.parentNode.removeChild(window.element); // remove from the document for better pefromance
  let lastDraw = Date.now();
  let count = 0;
  const maxCount = tabs.length;
  for (const tab of tabs) {
    const trackedTab = Tab.init(tab, { existing: true, inBackground: true });
    TabsUpdate.updateTab(trackedTab, tab, { forceApply: true });
    trackedTab.$TST.updateElement(TabUpdateTarget.CollapseExpandState);
    if (tab.active)
      TabsInternalOperation.setTabActive(trackedTab);
    if (Date.now() - lastDraw > configs.intervalToUpdateProgressForBlockedUserOperation) {
      UserOperationBlocker.setProgress(Math.round(++count / maxCount * 33) + 66); // 3/3: build tab elements
      await nextFrame();
      lastDraw = Date.now();
    }
  }
  SidebarTabs.wholeContainer.appendChild(window.element);
  MetricsData.add('rebuildAll: end (from scratch)');
  return false;
}

const mImportedTabs = new Promise((resolve, _reject) => {
  log('preparing mImportedTabs');
  // This must be synchronous , to avoid blocking to other listeners.
  const onBackgroundIsReady = message => {
    // This handler may be called before mTargetWindow is initialized, so
    // we need to wait until it is resolved.
    // See also: https://github.com/piroor/treestyletab/issues/2200
    mPromisedTargetWindow.then(windowId => {
      log(`mImportedTabs (${windowId}): onBackgroundIsReady `, message && message.type, message && message.windowId);
      if (!message ||
          !message.type ||
          message.type != Constants.kCOMMAND_NOTIFY_BACKGROUND_READY ||
          message.windowId != windowId)
        return;
      browser.runtime.onMessage.removeListener(onBackgroundIsReady);
      log(`mImportedTabs is resolved with ${message.tabs.length} tabs`);
      resolve(message.tabs);
    });
  };
  browser.runtime.onMessage.addListener(onBackgroundIsReady);
});

async function importTabsFromBackground() {
  log('importTabsFromBackground: start');
  try {
    const importedTabs = await MetricsData.addAsync('importTabsFromBackground: kCOMMAND_PING_TO_BACKGROUND', browser.runtime.sendMessage({
      type:     Constants.kCOMMAND_PING_TO_BACKGROUND,
      windowId: mTargetWindow
    }).catch(ApiTabs.createErrorHandler()));
    if (importedTabs)
      return importedTabs;
  }
  catch(e) {
    log('importTabsFromBackground: error: ', e);
  }
  log('importTabsFromBackground: waiting for mImportedTabs');
  return MetricsData.addAsync('importTabsFromBackground: kCOMMAND_PING_TO_SIDEBAR', mImportedTabs);
}


export async function confirmToCloseTabs(tabs, { configKey } = {}) {
  const tabIds = [];
  if (!configKey)
    configKey = 'warnOnCloseTabs';
  tabs = tabs.filter(tab => {
    if (!configs.grantedRemovingTabIds.includes(tab.id)) {
      tabIds.push(tab.id);
      return true;
    }
    return false;
  });
  log(`confirmToCloseTabs (${configKey}): `, tabIds);
  const count = tabIds.length;
  if (count <= 1 ||
      !configs[configKey])
    return true;

  try {
    const granted = await browser.runtime.sendMessage({
      type:     Constants.kCOMMAND_CONFIRM_TO_CLOSE_TABS,
      windowId: mTargetWindow,
      configKey,
      tabs
    });
    if (granted) {
      configs.lastConfirmedToCloseTabs = Date.now();
      configs.grantedRemovingTabIds = Array.from(new Set((configs.grantedRemovingTabIds   || []).concat(tabIds)));
      log('confirmToCloseTabs: granted ', configs.grantedRemovingTabIds);
      reserveToClearGrantedRemovingTabs();
      return true;
    }
  }
  catch(error) {
    console.error(error);
  }

  return false;
}
TabContextMenu.onTabsClosing.addListener(confirmToCloseTabs);

function reserveToClearGrantedRemovingTabs() {
  const lastGranted = configs.grantedRemovingTabIds.join(',');
  setTimeout(() => {
    if (configs.grantedRemovingTabIds.join(',') == lastGranted)
      configs.grantedRemovingTabIds = [];
  }, 1000);
}


export function reserveToUpdateTabbarLayout({ reason, timeout } = {}) {
  //log('reserveToUpdateTabbarLayout');
  if (reserveToUpdateTabbarLayout.waiting)
    clearTimeout(reserveToUpdateTabbarLayout.waiting);
  if (reason && !(reserveToUpdateTabbarLayout.reasons & reason))
    reserveToUpdateTabbarLayout.reasons |= reason;
  if (typeof timeout != 'number')
    timeout = 10;
  reserveToUpdateTabbarLayout.timeout = Math.max(timeout, reserveToUpdateTabbarLayout.timeout);
  reserveToUpdateTabbarLayout.waiting = setTimeout(() => {
    delete reserveToUpdateTabbarLayout.waiting;
    const reasons = reserveToUpdateTabbarLayout.reasons;
    reserveToUpdateTabbarLayout.reasons = 0;
    reserveToUpdateTabbarLayout.timeout = 0;
    updateTabbarLayout({ reasons });
  }, reserveToUpdateTabbarLayout.timeout);
}
reserveToUpdateTabbarLayout.reasons = 0;
reserveToUpdateTabbarLayout.timeout = 0;

function updateTabbarLayout({ reasons, timeout, justNow } = {}) {
  if (RestoringTabCount.hasMultipleRestoringTabs()) {
    log('updateTabbarLayout: skip until completely restored');
    reserveToUpdateTabbarLayout({
      reason:  reasons,
      timeout: Math.max(100, timeout)
    });
    return;
  }
  const readableReasons = [];
  if (configs.debug) {
    if (reasons & Constants.kTABBAR_UPDATE_REASON_RESIZE)
      readableReasons.push('resize');
    if (reasons & Constants.kTABBAR_UPDATE_REASON_COLLAPSE)
      readableReasons.push('collapse');
    if (reasons & Constants.kTABBAR_UPDATE_REASON_EXPAND)
      readableReasons.push('expand');
    if (reasons & Constants.kTABBAR_UPDATE_REASON_ANIMATION_END)
      readableReasons.push('animation end');
    if (reasons & Constants.kTABBAR_UPDATE_REASON_TAB_OPEN)
      readableReasons.push('tab open');
    if (reasons & Constants.kTABBAR_UPDATE_REASON_TAB_CLOSE)
      readableReasons.push('tab close');
    if (reasons & Constants.kTABBAR_UPDATE_REASON_TAB_MOVE)
      readableReasons.push('tab move');
  }
  log(`updateTabbarLayout reasons: ${readableReasons.join(',')}`);

  let allTabsHeight;
  const firstTab = Tab.getFirstUnpinnedTab(mTargetWindow);
  if (firstTab) {
    const lastTab = Tab.getLastUnpinnedTab(mTargetWindow);
    if (!lastTab) {
      log('Failed to update layout: missing last visible tab, retrying with delay');
      reserveToUpdateTabbarLayout({ reasons, timeout });
      return;
    }
    const range = document.createRange();
    range.selectNodeContents(mTabBar);
    range.setStartBefore(firstTab.$TST.element);
    range.setEndAfter(lastTab.$TST.element);
    allTabsHeight   = range.getBoundingClientRect().height;
    range.detach();
  }
  else {
    allTabsHeight = 0;
  }
  const visibleNewTabButtonInTabbar = document.querySelector('#tabbar:not(.overflow) .after-tabs .newtab-button-box');
  const visibleNewTabButtonAfterTabbar = document.querySelector('#tabbar.overflow ~ .after-tabs .newtab-button-box');
  const newTabButtonSize = (visibleNewTabButtonInTabbar || visibleNewTabButtonAfterTabbar).getBoundingClientRect().height;
  const containerHeight = mTabBar.getBoundingClientRect().height - (visibleNewTabButtonInTabbar ? visibleNewTabButtonInTabbar.getBoundingClientRect().height : 0);
  log('height: ', { container: containerHeight, allTabsHeight, newTabButtonSize });

  document.documentElement.style.setProperty('--after-tabs-area-size', `${newTabButtonSize}px`);

  const windowId = TabsStore.getCurrentWindowId();
  const overflow = containerHeight < allTabsHeight;
  if (overflow && !mTabBar.classList.contains(Constants.kTABBAR_STATE_OVERFLOW)) {
    log('overflow');
    mTabBar.classList.add(Constants.kTABBAR_STATE_OVERFLOW);
    TSTAPI.sendMessage({
      type: TSTAPI.kNOTIFY_TABBAR_OVERFLOW,
      windowId,
    });
    nextFrame().then(() => {
      // Tab at the end of the tab bar can be hidden completely or
      // partially (newly opened in small tab bar, or scrolled out when
      // the window is shrunken), so we need to scroll to it explicitely.
      const activeTab = Tab.getActiveTab(windowId);
      if (activeTab && !Scroll.isTabInViewport(activeTab)) {
        log('scroll to active tab on updateTabbarLayout');
        Scroll.scrollToTab(activeTab);
        return;
      }
      const lastOpenedTab = Tab.getLastOpenedTab(windowId);
      if (reasons & Constants.kTABBAR_UPDATE_REASON_TAB_OPEN &&
          !Scroll.isTabInViewport(lastOpenedTab)) {
        log('scroll to last opened tab on updateTabbarLayout ', reasons);
        Scroll.scrollToTab(lastOpenedTab, {
          anchor:            activeTab,
          notifyOnOutOfView: true
        });
      }
    });
  }
  else if (!overflow && mTabBar.classList.contains(Constants.kTABBAR_STATE_OVERFLOW)) {
    log('underflow');
    mTabBar.classList.remove(Constants.kTABBAR_STATE_OVERFLOW);
    TSTAPI.sendMessage({
      type: TSTAPI.kNOTIFY_TABBAR_UNDERFLOW,
      windowId,
    });
  }

  if (justNow)
    PinnedTabs.reposition({ reasons, timeout, justNow });
  else
    PinnedTabs.reserveToReposition({ reasons, timeout, justNow });
}


function onFocus(_event) {
  BackgroundConnection.sendMessage({
    type: Constants.kNOTIFY_SIDEBAR_FOCUS
  });
}

function onBlur(_event) {
  BackgroundConnection.sendMessage({
    type: Constants.kNOTIFY_SIDEBAR_BLUR
  });
}

function onResize(_event) {
  reserveToUpdateTabbarLayout({
    reason: Constants.kTABBAR_UPDATE_REASON_RESIZE
  });
}

function onTransisionEnd(event) {
  if (event.pseudoElement || // ignore size change of pseudo elements because they won't change height of tabbar contents
      !event.target.parentNode ||
      !event.target.parentNode.classList.contains('tabs') || // ignore animations on elements not affect to the tab bar scroll size
      !/margin|height|border-((top|bottom)-)?/.test(event.propertyName))
    return;
  //log('transitionend ', event);
  reserveToUpdateTabbarLayout({
    reason: Constants.kTABBAR_UPDATE_REASON_ANIMATION_END
  });
}

function onBrowserThemeChanged(updateInfo) {
  if (!updateInfo.windowId || // reset to default
      updateInfo.windowId == mTargetWindow)
    applyBrowserTheme(updateInfo.theme);
}


ContextualIdentities.onUpdated.addListener(() => {
  updateContextualIdentitiesStyle();
  updateContextualIdentitiesSelector();
});


CollapseExpand.onUpdated.addListener((_tab, options) => {
  const reason = options.collapsed ? Constants.kTABBAR_UPDATE_REASON_COLLAPSE : Constants.kTABBAR_UPDATE_REASON_EXPAND ;
  reserveToUpdateTabbarLayout({ reason });
});

async function onConfigChange(changedKey) {
  const rootClasses = document.documentElement.classList;
  switch (changedKey) {
    case 'debug': {
      EventListenerManager.debug = configs.debug;
      if (mInitialized) {
        // We have no need to re-update tabs on the startup process.
        // Moreover, we should not re-update tabs at the time to avoid
        // breaking of initialized tab states.
        for (const tab of Tab.getAllTabs(mTargetWindow, { iterator: true })) {
          TabsUpdate.updateTab(tab, tab, { forceApply: true });
          tab.$TST.invalidateElement(TabInvalidationTarget.Tooltip);
        }
      }
      rootClasses.toggle('debug', configs.debug);
    }; break;

    case 'sidebarPosition': {
      const isRight = await isSidebarRightSide();
      rootClasses.toggle('right', isRight);
      rootClasses.toggle('left', !isRight);
      Indent.update({ force: true });
    }; break;

    case 'baseIndent':
    case 'minIndent':
    case 'maxTreeLevel':
    case 'indentAutoShrink':
    case 'indentAutoShrinkOnlyForVisible':
      Indent.update({ force: true });
      break;

    case 'style':
      location.reload();
      break;

    case 'colorScheme':
      document.documentElement.setAttribute('color-scheme', configs.colorScheme);
      break;

    case 'inheritContextualIdentityToChildTabMode':
      updateContextualIdentitiesSelector();
      break;

    case 'showContextualIdentitiesSelector':
      rootClasses.toggle(Constants.kTABBAR_STATE_CONTEXTUAL_IDENTITY_SELECTABLE, configs[changedKey]);
      break;

    case 'showNewTabActionSelector':
      rootClasses.toggle(Constants.kTABBAR_STATE_NEWTAB_ACTION_SELECTABLE, configs[changedKey]);
      break;

    case 'simulateSVGContextFill':
      rootClasses.toggle('simulate-svg-context-fill', configs[changedKey]);
      break;

    default:
      if (changedKey.startsWith('chunkedUserStyleRules'))
        applyUserStyleRules();
      break;
  }
}

async function isSidebarRightSide() {
  // This calculation logic is buggy for a window in a screen placed at
  // left of the primary display and scaled. As the result, a sidebar
  // placed at left can be mis-detected as placed at right. For safety
  // I ignore such cases and always treat such cases as "left side placed".
  // See also: https://github.com/piroor/treestyletab/issues/2984#issuecomment-901907503
  if (window.screenX < 0 && window.devicePixelRatio > 1)
    return false;
  const mayBeRight = window.mozInnerScreenX - window.screenX > (window.outerWidth - window.innerWidth) / 2;
  if (configs.sidebarPosition == Constants.kTABBAR_POSITION_AUTO &&
      mayBeRight &&
      !configs.sidebarPositionRighsideNotificationShown) {
    if (mTargetWindow != (await browser.windows.getLastFocused({})).id)
      return;

    let result;
    do {
      result = await RichConfirm.show({
        message: browser.i18n.getMessage('sidebarPositionRighsideNotification_message'),
        buttons: [
          browser.i18n.getMessage('sidebarPositionRighsideNotification_rightside'),
          browser.i18n.getMessage('sidebarPositionRighsideNotification_leftside'),
        ],
      });
    } while (result.buttonIndex < 0);

    const notificationParams = {
      title:   browser.i18n.getMessage('sidebarPositionOptionNotification_title'),
      message: browser.i18n.getMessage('sidebarPositionOptionNotification_message'),
      url:     `moz-extension://${location.host}/options/options.html#section-appearance`,
      timeout: configs.sidebarPositionOptionNotificationTimeout,
    };
    configs.sidebarPositionRighsideNotificationShown = true;
    switch (result.buttonIndex) {
      case 0:
        notify(notificationParams);
        break;

      case 1:
      default:
        configs.sidebarPosition = Constants.kTABBAR_POSITION_LEFT;
        notify(notificationParams);
        return;
    }
  }
  return configs.sidebarPosition == Constants.kTABBAR_POSITION_AUTO ?
    mayBeRight :
    configs.sidebarPosition == Constants.kTABBAR_POSITION_RIGHT;
}


// This must be synchronous and return Promise on demando, to avoid
// blocking to other listeners.
function onMessage(message, _sender, _respond) {
  if (!message ||
      typeof message.type != 'string' ||
      message.type.indexOf('treestyletab:') != 0)
    return;

  if (message.windowId &&
      message.windowId != mTargetWindow)
    return;

  //log('onMessage: ', message, sender);
  switch (message.type) {
    // for a vital check from SidebarConnection
    case Constants.kCOMMAND_PING_TO_SIDEBAR:
      return Promise.resolve(true);

    case Constants.kCOMMAND_RELOAD:
      location.reload();
      return;

    case Constants.kCOMMAND_SHOW_DIALOG:
      return RichConfirm.show({
        ...message.params,
        onHidden() {
          UserOperationBlocker.unblockIn(mTargetWindow, message.userOperationBlockerParams || {});
        }
      });
  }
}

const BUFFER_KEY_PREFIX = 'sidebar-';

BackgroundConnection.onMessage.addListener(async message => {
  switch (message.type) {
    case Constants.kCOMMAND_BLOCK_USER_OPERATIONS:
      UserOperationBlocker.blockIn(mTargetWindow, message);
      break;

    case Constants.kCOMMAND_UNBLOCK_USER_OPERATIONS:
      UserOperationBlocker.unblockIn(mTargetWindow, message);
      break;

    case Constants.kCOMMAND_PROGRESS_USER_OPERATIONS:
      UserOperationBlocker.setProgress(message.percentage, mTargetWindow);
      break;

    case Constants.kCOMMAND_NOTIFY_TAB_CREATED:
    case Constants.kCOMMAND_NOTIFY_TAB_MOVED:
    case Constants.kCOMMAND_NOTIFY_TAB_ATTACHED_TO_WINDOW:
      if (message.tabId)
        await Tab.waitUntilTracked(message.tabId, { element: true });
      reserveToUpdateTabbarLayout({
        reason:  Constants.kTABBAR_UPDATE_REASON_TAB_OPEN,
        timeout: configs.collapseDuration
      });
      break;

    case Constants.kCOMMAND_NOTIFY_TAB_REMOVING:
    case Constants.kCOMMAND_NOTIFY_TAB_DETACHED_FROM_WINDOW: {
      await Tab.waitUntilTracked(message.tabId, { element: true });
      reserveToUpdateTabbarLayout({
        reason:  Constants.kTABBAR_UPDATE_REASON_TAB_CLOSE,
        timeout: configs.collapseDuration
      });
    }; break;

    case Constants.kCOMMAND_NOTIFY_TAB_SHOWN:
    case Constants.kCOMMAND_NOTIFY_TAB_HIDDEN: {
      if (BackgroundConnection.handleBufferedMessage({ type: 'shown/hidden', message }, `${BUFFER_KEY_PREFIX}${message.tabId}`))
        return;
      await Tab.waitUntilTracked(message.tabId, { element: true });
      const lastMessage = BackgroundConnection.fetchBufferedMessage('shown/hidden', `${BUFFER_KEY_PREFIX}${message.tabId}`);
      if (!lastMessage)
        return;
      if (lastMessage.message.type == Constants.kCOMMAND_NOTIFY_TAB_SHOWN) {
        reserveToUpdateTabbarLayout({
          reason: Constants.kTABBAR_UPDATE_REASON_TAB_OPEN
        });
      }
      else {
        reserveToUpdateTabbarLayout({
          reason: Constants.kTABBAR_UPDATE_REASON_TAB_CLOSE
        });
      }
    }; break;

    case Constants.kCOMMAND_NOTIFY_TAB_RESTORING: {
      if (!configs.useCachedTree) // we cannot know when we should unblock on no cache case...
        return;

      const window = TabsStore.windows.get(mTargetWindow);
      // When we are restoring two or more tabs.
      // (But we don't need do this again for third, fourth, and later tabs.)
      if (window.restoredCount == 2)
        UserOperationBlocker.block({ throbber: true });
    }; break;

    case Constants.kCOMMAND_NOTIFY_TAB_RESTORED: {
      // Tree restoration for "Restore Previous Session"
      if (!configs.useCachedTree)
        return;

      await Tab.waitUntilTracked(message.tabId, { element: true });
      log('Tabs.onWindowRestoring');
      const window = TabsStore.windows.get(mTargetWindow);
      const cache = await SidebarCache.getEffectiveWindowCache({
        ignorePinnedTabs: true
      });
      if (!cache ||
          !cache.tabbar.cache ||
          (cache.offset &&
           window.element.querySelectorAll(kTAB_ELEMENT_NAME).length <= cache.offset)) {
        log('Tabs.onWindowRestoring: no effective cache');
        UserOperationBlocker.unblock({ throbber: true });
        return;
      }

      log('Tabs.onWindowRestoring restore! ', cache);
      MetricsData.add('Tabs.onWindowRestoring restore start');
      cache.tabbar.tabsDirty = true;
      const importedTabs = await browser.runtime.sendMessage({
        type:     Constants.kCOMMAND_PULL_TABS,
        windowId: message.windowId
      });
      const restored = await SidebarCache.restoreTabsFromCache(cache.tabbar, {
        offset: cache.offset || 0,
        tabs:   importedTabs.map(importedTab => Tab.import(importedTab))
      });
      if (!restored) {
        await rebuildAll();
      }
      Indent.updateRestoredTree(restored && cache.offset == 0 ? cache.indent : null);
      updateTabbarLayout({ justNow: true });
      UserOperationBlocker.unblock({ throbber: true });
      MetricsData.add('Tabs.onWindowRestoring restore end');
    }; break;

    case Constants.kCOMMAND_BOOKMARK_TAB_WITH_DIALOG: {
      Bookmark.bookmarkTab(Tab.get(message.tabId), {
        ...(message.options || {}),
        showDialog: true
      });
    }; break;

    case Constants.kCOMMAND_BOOKMARK_TABS_WITH_DIALOG: {
      Bookmark.bookmarkTabs(mapAndFilter(message.tabIds, id => Tab.get(id)), {
        ...(message.options || {}),
        showDialog: true
      });
    }; break;
  }
});


browser.windows.onFocusChanged.addListener(windowId => {
  document.documentElement.classList.toggle('active', windowId == mTargetWindow);
});

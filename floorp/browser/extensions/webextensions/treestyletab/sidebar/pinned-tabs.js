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
  configs
} from '/common/common.js';

import * as Constants from '/common/constants.js';
import * as TabsStore from '/common/tabs-store.js';

import Tab from '/common/Tab.js';

import * as BackgroundConnection from './background-connection.js';
import * as Size from './size.js';

// eslint-disable-next-line no-unused-vars
function log(...args) {
  internalLogger('sidebar/pinned-tabs', ...args);
}

let mTargetWindow;
let mTabBar;
let mAreaHeight     = 0;
let mMaxVisibleRows = 0;
let mScrollRow      = 0;

export function init() {
  mTargetWindow = TabsStore.getCurrentWindowId();
  mTabBar       = document.querySelector('#tabbar');
  configs.$addObserver(onConfigChange);

  // We need to register the lister as non-passive to cancel the event.
  // https://developer.mozilla.org/en-US/docs/Web/API/EventTarget/addEventListener#Improving_scrolling_performance_with_passive_listeners
  document.addEventListener('wheel', event => {
    if (event.clientY > mAreaHeight ||
        Tab.getPinnedTabs(mTargetWindow).length == 0)
      return;
    event.preventDefault();
    event.stopPropagation();
    const deltaRows = Math.floor(event.deltaY / Math.max(1, getTabHeight()));
    mScrollRow = Math.min(mMaxVisibleRows + 1, Math.max(0, mScrollRow + deltaRows));
    reserveToReposition();
  }, { capture: true, passive: false });
}

function getTabHeight() {
  return configs.faviconizePinnedTabs ? Size.getFavIconizedTabSize() : Size.getTabHeight() + Size.getTabYOffset();
}

export function reposition(options = {}) {
  //log('reposition');
  const pinnedTabs = Tab.getPinnedTabs(mTargetWindow);
  if (pinnedTabs.length == 0) {
    reset();
    document.documentElement.classList.remove('have-pinned-tabs');
    return;
  }

  document.documentElement.classList.add('have-pinned-tabs');

  const containerWidth = mTabBar.getBoundingClientRect().width;
  const maxWidth       = containerWidth;
  const faviconized    = configs.faviconizePinnedTabs;

  const width  = faviconized ? Size.getFavIconizedTabSize() : maxWidth + Size.getTabXOffset();
  const height = getTabHeight();
  const maxCol = faviconized ? Math.max(1, configs.maxFaviconizedPinnedTabsInOneRow > 0 ? configs.maxFaviconizedPinnedTabsInOneRow : Math.floor(maxWidth / width)) : 1;
  const maxRow = Math.ceil(pinnedTabs.length / maxCol);
  let col    = 0;
  let row    = 0;

  const pinnedTabsAreaRatio = Math.min(Math.max(0, configs.maxPinnedTabsRowsAreaPercentage), 100) / 100;
  const visualGap = parseFloat(window.getComputedStyle(mTabBar, null).getPropertyValue('--visual-gap-offset').replace(/px$/));
  const allTabsAreaHeight = mTabBar.parentNode.getBoundingClientRect().height + visualGap;
  mMaxVisibleRows = Math.max(0, Math.floor((allTabsAreaHeight * pinnedTabsAreaRatio) / height));
  mAreaHeight = Math.min(
    height * maxRow + (faviconized ? 0 : Size.getTabYOffset()),
    mMaxVisibleRows * height
  );
  document.documentElement.style.setProperty('--pinned-tabs-area-size', `${mAreaHeight}px`);
  const scrollRow = Math.max(0, Math.min(maxRow - mMaxVisibleRows, mScrollRow));
  for (const tab of pinnedTabs) {
    const style = tab.$TST.element.style;
    if (options.justNow)
      tab.$TST.removeState(Constants.kTAB_STATE_ANIMATION_READY);

    if (faviconized)
      tab.$TST.addState(Constants.kTAB_STATE_FAVICONIZED);
    else
      tab.$TST.removeState(Constants.kTAB_STATE_FAVICONIZED);

    if (row == maxRow - 1)
      tab.$TST.addState(Constants.kTAB_STATE_LAST_ROW);
    else
      tab.$TST.removeState(Constants.kTAB_STATE_LAST_ROW);

    style.setProperty('--pinned-position-bottom', 'auto');
    style.setProperty('--pinned-position-left', `${width * col}px`);
    style.setProperty('--pinned-position-right', faviconized ? 'auto' : 0 );
    style.setProperty('--pinned-position-top', `${height * row}px`);
    style.marginTop = `${height * -scrollRow}px`; // this need to pe separated from "top", because its animation can be delayed by the visual gap canceller.
    const inVisibleArea = scrollRow <= row && row - scrollRow < mMaxVisibleRows;
    tab.$TST.element.classList.toggle('in-visible-area', inVisibleArea);

    if (options.justNow)
      tab.$TST.addState(Constants.kTAB_STATE_ANIMATION_READY);

    /*
    log('pinned tab: ', {
      tab:    dumpTab(tab),
      col:    col,
      width:  width,
      height: height
    });
    */

    col++;
    if (col >= maxCol) {
      col = 0;
      row++;
      //log('=> new row');
    }
  }
}

export function reserveToReposition(options = {}) {
  if (reserveToReposition.waiting)
    clearTimeout(reserveToReposition.waiting);
  reserveToReposition.waiting = setTimeout(() => {
    delete reserveToReposition.waiting;
    reposition(options);
  }, 10);
}

function reset() {
  document.documentElement.style.setProperty('--pinned-tabs-area-size', '0px');
  for (const tab of Tab.getPinnedTabs(mTargetWindow, { iterator: true })) {
    clearStyle(tab);
  }
  mAreaHeight     = 0;
  mMaxVisibleRows = 0;
  mScrollRow      = 0;
}

function clearStyle(tab) {
  tab.$TST.removeState(Constants.kTAB_STATE_FAVICONIZED);
  tab.$TST.removeState(Constants.kTAB_STATE_LAST_ROW);
  const style = tab.$TST.element.style;
  style.setProperty('--pinned-position-bottom', '');
  style.setProperty('--pinned-position-left', '');
  style.setProperty('--pinned-position-right', '');
  style.setProperty('--pinned-position-top', '');
  style.marginTop = '';
  tab.$TST.element.classList.remove('in-visible-area');
}

const BUFFER_KEY_PREFIX = 'pinned-tabs-';

BackgroundConnection.onMessage.addListener(async message => {
  switch (message.type) {
    case Constants.kCOMMAND_NOTIFY_TAB_CREATED: {
      await Tab.waitUntilTracked(message.tabId, { element: true });
      const tab = Tab.get(message.tabId);
      if (!tab)
        return;
      if (tab.pinned)
        reserveToReposition();
    }; break;

    case Constants.kCOMMAND_NOTIFY_TAB_REMOVING:
    case Constants.kCOMMAND_NOTIFY_TAB_MOVED:
    case Constants.kCOMMAND_NOTIFY_TAB_INTERNALLY_MOVED: {
      // don't wait until tracked here, because removing or detaching tab will become untracked!
      const tab = Tab.get(message.tabId);
      if (tab && tab.pinned)
        reserveToReposition();
    }; break;

    case Constants.kCOMMAND_NOTIFY_TAB_SHOWN:
    case Constants.kCOMMAND_NOTIFY_TAB_HIDDEN:
      reserveToReposition();
      break;

    case Constants.kCOMMAND_NOTIFY_TAB_DETACHED_FROM_WINDOW:
      if (message.wasPinned)
        reserveToReposition();
      break;

    case Constants.kCOMMAND_NOTIFY_TAB_PINNED:
    case Constants.kCOMMAND_NOTIFY_TAB_UNPINNED: {
      if (BackgroundConnection.handleBufferedMessage({ type: 'pinned/unpinned', message }, `${BUFFER_KEY_PREFIX}${message.tabId}`))
        return;
      await Tab.waitUntilTracked(message.tabId, { element: true });
      const tab = Tab.get(message.tabId);
      const lastMessage = BackgroundConnection.fetchBufferedMessage('pinned/unpinned', `${BUFFER_KEY_PREFIX}${message.tabId}`);
      if (!tab ||
          !lastMessage)
        return;
      if (lastMessage.message.type == Constants.kCOMMAND_NOTIFY_TAB_UNPINNED)
        clearStyle(tab);
      reserveToReposition();
    }; break;
  }
});

function onConfigChange(key) {
  switch (key) {
    case 'faviconizePinnedTabs':
      reserveToReposition();
      break;
  }
}

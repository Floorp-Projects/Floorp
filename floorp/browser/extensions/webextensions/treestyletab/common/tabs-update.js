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
 * Portions created by the Initial Developer are Copyright (C) 2011-2022
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
  wait,
  mapAndFilter
} from './common.js';

import * as Constants from './constants.js';
import * as ContextualIdentities from './contextual-identities.js';
import * as SidebarConnection from './sidebar-connection.js';
import * as TabsStore from './tabs-store.js';

import Tab from './Tab.js';

function log(...args) {
  internalLogger('common/tabs-update', ...args);
}

export function updateTab(tab, newState = {}, options = {}) {
  const messages          = [];
  const addedAttributes   = {};
  const removedAttributes = [];
  const addedStates       = [];
  const removedStates     = [];
  const oldState          = options.old || {};

  if ('url' in newState) {
    tab.$TST.setAttribute(Constants.kCURRENT_URI, addedAttributes[Constants.kCURRENT_URI] = newState.url);
  }

  if ('url' in newState &&
      newState.url.indexOf(Constants.kGROUP_TAB_URI) == 0) {
    tab.$TST.addState(Constants.kTAB_STATE_GROUP_TAB, { permanently: true });
    addedStates.push(Constants.kTAB_STATE_GROUP_TAB);
    TabsStore.addGroupTab(tab);
    Tab.onGroupTabDetected.dispatch(tab);
    messages.push({
      type:     Constants.kCOMMAND_NOTIFY_GROUP_TAB_DETECTED,
      windowId: tab.windowId,
      tabId:    tab.id
    });
  }
  else if (tab.$TST.states.has(Constants.kTAB_STATE_GROUP_TAB) &&
           !tab.$TST.hasGroupTabURL) {
    removedStates.push(Constants.kTAB_STATE_GROUP_TAB);
    TabsStore.removeGroupTab(tab);
  }

  if (options.forceApply ||
      ('title' in newState &&
       newState.title != oldState.title)) {
    if (options.forceApply) {
      tab.$TST.getPermanentStates().then(states => {
        if (states.includes(Constants.kTAB_STATE_UNREAD) &&
            !tab.$TST.isGroupTab) {
          tab.$TST.addState(Constants.kTAB_STATE_UNREAD, { permanently: true });
          SidebarConnection.sendMessage({
            type:     Constants.kCOMMAND_NOTIFY_TAB_UPDATED,
            windowId: tab.windowId,
            tabId:    tab.id,
            addedStates: [Constants.kTAB_STATE_UNREAD]
          });
        }
        else {
          tab.$TST.removeState(Constants.kTAB_STATE_UNREAD, { permanently: true });
          SidebarConnection.sendMessage({
            type:     Constants.kCOMMAND_NOTIFY_TAB_UPDATED,
            windowId: tab.windowId,
            tabId:    tab.id,
            removedStates: [Constants.kTAB_STATE_UNREAD]
          });
        }
      });
    }
    else if (tab.$TST.isGroupTab) {
      tab.$TST.removeState(Constants.kTAB_STATE_UNREAD, { permanently: true });
      removedStates.push(Constants.kTAB_STATE_UNREAD);
    }
    else if (!tab.active) {
      tab.$TST.addState(Constants.kTAB_STATE_UNREAD, { permanently: true });
      addedStates.push(Constants.kTAB_STATE_UNREAD);
    }
    tab.$TST.label = newState.title;
    Tab.onLabelUpdated.dispatch(tab);
    messages.push({
      type:     Constants.kCOMMAND_NOTIFY_TAB_LABEL_UPDATED,
      windowId: tab.windowId,
      tabId:    tab.id,
      title:    tab.title,
      label:    tab.$TST.label
    });
  }

  const openerOfGroupTab = tab.$TST.isGroupTab && Tab.getOpenerFromGroupTab(tab);
  if (openerOfGroupTab &&
      openerOfGroupTab.favIconUrl) {
    messages.push({
      type:       Constants.kCOMMAND_NOTIFY_TAB_FAVICON_UPDATED,
      windowId:   tab.windowId,
      tabId:      tab.id,
      favIconUrl: openerOfGroupTab.favIconUrl
    });
  }
  else if (options.forceApply ||
           'favIconUrl' in newState) {
    tab.$TST.setAttribute(Constants.kCURRENT_FAVICON_URI, addedAttributes[Constants.kCURRENT_FAVICON_URI] = tab.favIconUrl);
    messages.push({
      type:       Constants.kCOMMAND_NOTIFY_TAB_FAVICON_UPDATED,
      windowId:   tab.windowId,
      tabId:      tab.id,
      favIconUrl: tab.favIconUrl
    });
  }
  else if (tab.$TST.isGroupTab) {
    // "about:treestyletab-group" can set error icon for the favicon and
    // reloading doesn't cloear that, so we need to clear favIconUrl manually.
    tab.favIconUrl = null;
    removedAttributes.push(Constants.kCURRENT_FAVICON_URI)
    tab.$TST.removeAttribute(Constants.kCURRENT_FAVICON_URI);
    messages.push({
      type:       Constants.kCOMMAND_NOTIFY_TAB_FAVICON_UPDATED,
      windowId:   tab.windowId,
      tabId:      tab.id,
      favIconUrl: null
    });
  }

  if ('status' in newState) {
    const reallyChanged = !tab.$TST.states.has(newState.status);
    const removed = newState.status == 'loading' ? 'complete' : 'loading';
    tab.$TST.removeState(removed);
    removedStates.push(removed);
    tab.$TST.addState(newState.status);
    addedStates.push(newState.status);
    if (newState.status == 'loading')
      TabsStore.addLoadingTab(tab);
    else
      TabsStore.removeLoadingTab(tab);
    if (!options.forceApply) {
      messages.push({
        type:     Constants.kCOMMAND_UPDATE_LOADING_STATE,
        windowId: tab.windowId,
        tabId:    tab.id,
        status:   tab.status,
        reallyChanged
      });
    }
  }

  if ((options.forceApply ||
       'pinned' in newState) &&
      newState.pinned != tab.$TST.states.has(Constants.kTAB_STATE_PINNED)) {
    if (newState.pinned) {
      tab.$TST.addState(Constants.kTAB_STATE_PINNED);
      addedStates.push(Constants.kTAB_STATE_PINNED);
      tab.$TST.removeAttribute(Constants.kLEVEL); // don't indent pinned tabs!
      removedAttributes.push(Constants.kLEVEL);
      TabsStore.removeUnpinnedTab(tab);
      TabsStore.addPinnedTab(tab);
      Tab.onPinned.dispatch(tab);
      messages.push({
        type:     Constants.kCOMMAND_NOTIFY_TAB_PINNED,
        windowId: tab.windowId,
        tabId:    tab.id
      });
    }
    else {
      tab.$TST.removeState(Constants.kTAB_STATE_PINNED);
      removedStates.push(Constants.kTAB_STATE_PINNED);
      TabsStore.removePinnedTab(tab);
      TabsStore.addUnpinnedTab(tab);
      Tab.onUnpinned.dispatch(tab);
      messages.push({
        type:     Constants.kCOMMAND_NOTIFY_TAB_UNPINNED,
        windowId: tab.windowId,
        tabId:    tab.id
      });
    }
  }

  if (options.forceApply ||
      'audible' in newState) {
    if (newState.audible) {
      tab.$TST.addState(Constants.kTAB_STATE_AUDIBLE);
      addedStates.push(Constants.kTAB_STATE_AUDIBLE);
    }
    else {
      tab.$TST.removeState(Constants.kTAB_STATE_AUDIBLE);
      removedStates.push(Constants.kTAB_STATE_AUDIBLE);
    }
  }

  let soundStateChanged = false;

  if (options.forceApply ||
      'mutedInfo' in newState) {
    soundStateChanged = true;
    const muted = newState.mutedInfo && newState.mutedInfo.muted;
    if (muted) {
      tab.$TST.addState(Constants.kTAB_STATE_MUTED);
      addedStates.push(Constants.kTAB_STATE_MUTED);
    }
    else {
      tab.$TST.removeState(Constants.kTAB_STATE_MUTED);
      removedStates.push(Constants.kTAB_STATE_MUTED);
    }
    Tab.onMutedStateChanged.dispatch(tab, muted);
  }

  if (options.forceApply ||
      soundStateChanged ||
      'audible' in newState) {
    soundStateChanged = true;
    if (tab.audible &&
        !tab.mutedInfo.muted) {
      tab.$TST.addState(Constants.kTAB_STATE_SOUND_PLAYING);
      addedStates.push(Constants.kTAB_STATE_SOUND_PLAYING);
    }
    else {
      tab.$TST.removeState(Constants.kTAB_STATE_SOUND_PLAYING);
      removedStates.push(Constants.kTAB_STATE_SOUND_PLAYING);
    }
  }

  if (soundStateChanged) {
    const parent = tab.$TST.parent;
    if (parent)
      parent.$TST.inheritSoundStateFromChildren();
  }

  if (options.forceApply ||
      'cookieStoreId' in newState) {
    for (const state of tab.$TST.states) {
      if (String(state).startsWith('contextual-identity-')) {
        tab.$TST.removeState(state);
        removedStates.push(state);
      }
    }
    if (newState.cookieStoreId) {
      const state = `contextual-identity-${newState.cookieStoreId}`;
      tab.$TST.addState(state);
      addedStates.push(state);
      const identity = ContextualIdentities.get(newState.cookieStoreId);
      if (identity)
        tab.$TST.setAttribute(Constants.kCONTEXTUAL_IDENTITY_NAME, identity.name);
      else
        tab.$TST.removeAttribute(Constants.kCONTEXTUAL_IDENTITY_NAME);
    }
    else {
      tab.$TST.removeAttribute(Constants.kCONTEXTUAL_IDENTITY_NAME);
    }
  }

  if (options.forceApply ||
      'incognito' in newState) {
    if (newState.incognito) {
      tab.$TST.addState(Constants.kTAB_STATE_PRIVATE_BROWSING);
      addedStates.push(Constants.kTAB_STATE_PRIVATE_BROWSING);
    }
    else {
      tab.$TST.removeState(Constants.kTAB_STATE_PRIVATE_BROWSING);
      removedStates.push(Constants.kTAB_STATE_PRIVATE_BROWSING);
    }
  }

  if (options.forceApply ||
      'hidden' in newState) {
    if (newState.hidden) {
      if (!tab.$TST.states.has(Constants.kTAB_STATE_HIDDEN)) {
        tab.$TST.addState(Constants.kTAB_STATE_HIDDEN);
        addedStates.push(Constants.kTAB_STATE_HIDDEN);
        TabsStore.removeVisibleTab(tab);
        TabsStore.removeControllableTab(tab);
        Tab.onHidden.dispatch(tab);
        messages.push({
          type:     Constants.kCOMMAND_NOTIFY_TAB_HIDDEN,
          windowId: tab.windowId,
          tabId:    tab.id
        });
      }
    }
    else if (tab.$TST.states.has(Constants.kTAB_STATE_HIDDEN)) {
      tab.$TST.removeState(Constants.kTAB_STATE_HIDDEN);
      removedStates.push(Constants.kTAB_STATE_HIDDEN);
      if (!tab.$TST.collapsed)
        TabsStore.addVisibleTab(tab);
      TabsStore.addControllableTab(tab);
      Tab.onShown.dispatch(tab);
      messages.push({
        type:     Constants.kCOMMAND_NOTIFY_TAB_SHOWN,
        windowId: tab.windowId,
        tabId:    tab.id
      });
    }
  }

  if (options.forceApply ||
      'highlighted' in newState) {
    if (newState.highlighted) {
      TabsStore.addHighlightedTab(tab);
      tab.$TST.addState(Constants.kTAB_STATE_HIGHLIGHTED);
      addedStates.push(Constants.kTAB_STATE_HIGHLIGHTED);
    }
    else {
      TabsStore.removeHighlightedTab(tab);
      tab.$TST.removeState(Constants.kTAB_STATE_HIGHLIGHTED);
      removedStates.push(Constants.kTAB_STATE_HIGHLIGHTED);
    }
  }

  if (options.forceApply ||
      'attention' in newState) {
    if (newState.attention) {
      tab.$TST.addState(Constants.kTAB_STATE_ATTENTION);
      addedStates.push(Constants.kTAB_STATE_ATTENTION);
    }
    else {
      tab.$TST.removeState(Constants.kTAB_STATE_ATTENTION);
      removedStates.push(Constants.kTAB_STATE_ATTENTION);
    }
  }

  if (options.forceApply ||
      'discarded' in newState) {
    wait(0).then(() => {
      // Don't set this class immediately, because we need to know
      // the newly active tab *was* discarded on onTabClosed handler.
      if (newState.discarded) {
        tab.$TST.addState(Constants.kTAB_STATE_DISCARDED);
        SidebarConnection.sendMessage({
          type:     Constants.kCOMMAND_NOTIFY_TAB_UPDATED,
          windowId: tab.windowId,
          tabId:    tab.id,
          addedStates: [Constants.kTAB_STATE_DISCARDED]
        });
      }
      else {
        tab.$TST.removeState(Constants.kTAB_STATE_DISCARDED);
        SidebarConnection.sendMessage({
          type:     Constants.kCOMMAND_NOTIFY_TAB_UPDATED,
          windowId: tab.windowId,
          tabId:    tab.id,
          removedStates: [Constants.kTAB_STATE_DISCARDED]
        });
      }
    });
  }

  SidebarConnection.sendMessage({
    type:     Constants.kCOMMAND_NOTIFY_TAB_UPDATED,
    windowId: tab.windowId,
    tabId:    tab.id,
    updatedProperties: newState && newState.$TST && newState.$TST.sanitized || newState,
    addedAttributes,
    removedAttributes,
    addedStates,
    removedStates,
    soundStateChanged
  });
  messages.forEach(SidebarConnection.sendMessage);
}

export async function updateTabsHighlighted(highlightInfo) {
  if (Tab.needToWaitTracked(highlightInfo.windowId))
    await Tab.waitUntilTrackedAll(highlightInfo.windowId);
  const window = TabsStore.windows.get(highlightInfo.windowId);
  if (!window)
    return;

  //const startAt = Date.now();

  const tabIds = highlightInfo.tabIds; // new Set(highlightInfo.tabIds);
  const toBeUnhighlightedTabs = Tab.getHighlightedTabs(highlightInfo.windowId, {
    ordered: false,
    '!id':   tabIds
  });
  const alreadyHighlightedTabs = TabsStore.highlightedTabsInWindow.get(highlightInfo.windowId);
  const toBeHighlightedTabs = mapAndFilter(tabIds, id => {
    const tab = window.tabs.get(id);
    return tab && !alreadyHighlightedTabs.has(tab.id) && tab || undefined;
  });

  //console.log(`updateTabsHighlighted: ${Date.now() - startAt}ms`, { toBeHighlightedTabs, toBeUnhighlightedTabs});

  //log('updateTabsHighlighted ', { toBeHighlightedTabs, toBeUnhighlightedTabs});
  for (const tab of toBeUnhighlightedTabs) {
    TabsStore.removeHighlightedTab(tab);
    updateTabHighlighted(tab, false);
  }
  for (const tab of toBeHighlightedTabs) {
    TabsStore.addHighlightedTab(tab);
    updateTabHighlighted(tab, true);
  }
}
async function updateTabHighlighted(tab, highlighted) {
  log(`highlighted status of ${dumpTab(tab)}: `, { old: tab.highlighted, new: highlighted });
  //if (tab.highlighted == highlighted)
  //  return false;
  if (highlighted)
    tab.$TST.addState(Constants.kTAB_STATE_HIGHLIGHTED);
  else
    tab.$TST.removeState(Constants.kTAB_STATE_HIGHLIGHTED);
  tab.highlighted = highlighted;
  const window = TabsStore.windows.get(tab.windowId);
  const inheritHighlighted = !window.tabsToBeHighlightedAlone.has(tab.id);
  if (!inheritHighlighted)
    window.tabsToBeHighlightedAlone.delete(tab.id);
  updateTab(tab, { highlighted });
  Tab.onUpdated.dispatch(tab, { highlighted }, { inheritHighlighted });
  return true;
}


export async function completeLoadingTabs(windowId) {
  const completedTabs = new Set((await browser.tabs.query({ windowId, status: 'complete' })).map(tab => tab.id));
  for (const tab of Tab.getLoadingTabs(windowId, { ordered: false, iterator: true })) {
    if (completedTabs.has(tab.id))
      updateTab(tab, { status: 'complete' });
  }
}

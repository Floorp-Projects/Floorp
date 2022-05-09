/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  mapAndFilter,
  configs,
  sanitizeForHTMLText
} from '/common/common.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as Constants from '/common/constants.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TabsInternalOperation from '/common/tabs-internal-operation.js';
import * as TreeBehavior from '/common/tree-behavior.js';
import * as TSTAPI from '/common/tst-api.js';
import * as ContextualIdentities from '/common/contextual-identities.js';
import * as Sync from '/common/sync.js';

import Tab from '/common/Tab.js';

import * as Commands from './commands.js';

import EventListenerManager from '/extlib/EventListenerManager.js';

function log(...args) {
  internalLogger('background/tab-context-menu', ...args);
}

export const onTSTItemClick = new EventListenerManager();
export const onTSTTabContextMenuShown = new EventListenerManager();
export const onTSTTabContextMenuHidden = new EventListenerManager();
export const onTopLevelItemAdded = new EventListenerManager();

const EXTERNAL_TOP_LEVEL_ITEM_MATCHER = /^external-top-level-item:([^:]+):(.+)$/;
function getExternalTopLevelItemId(ownerId, itemId) {
  return `external-top-level-item:${ownerId}:${itemId}`;
}

const SAFE_MENU_PROPERTIES = [
  'checked',
  'enabled',
  'icons',
  'parentId',
  'title',
  'type',
  'visible'
];

const mItemsById = {
  'context_newTab': {
    title: browser.i18n.getMessage('tabContextMenu_newTab_label'),
  },
  'context_separator:afterNewTab': {
    type: 'separator'
  },
  'context_reloadTab': {
    title:              browser.i18n.getMessage('tabContextMenu_reload_label'),
    titleMultiselected: browser.i18n.getMessage('tabContextMenu_reload_label_multiselected')
  },
  'context_topLevel_reloadTree': {
    title:              browser.i18n.getMessage('context_reloadTree_label'),
    titleMultiselected: browser.i18n.getMessage('context_reloadTree_label_multiselected')
  },
  'context_topLevel_reloadDescendants': {
    title:              browser.i18n.getMessage('context_reloadDescendants_label'),
    titleMultiselected: browser.i18n.getMessage('context_reloadDescendants_label_multiselected')
  },
  'context_toggleMuteTab-mute': {
    title:              browser.i18n.getMessage('tabContextMenu_mute_label'),
    titleMultiselected: browser.i18n.getMessage('tabContextMenu_mute_label_multiselected')
  },
  'context_toggleMuteTab-unmute': {
    title:              browser.i18n.getMessage('tabContextMenu_unmute_label'),
    titleMultiselected: browser.i18n.getMessage('tabContextMenu_unmute_label_multiselected')
  },
  'context_pinTab': {
    title:              browser.i18n.getMessage('tabContextMenu_pin_label'),
    titleMultiselected: browser.i18n.getMessage('tabContextMenu_pin_label_multiselected')
  },
  'context_unpinTab': {
    title:              browser.i18n.getMessage('tabContextMenu_unpin_label'),
    titleMultiselected: browser.i18n.getMessage('tabContextMenu_unpin_label_multiselected')
  },
  'context_duplicateTab': {
    title:              browser.i18n.getMessage('tabContextMenu_duplicate_label'),
    titleMultiselected: browser.i18n.getMessage('tabContextMenu_duplicate_label_multiselected')
  },
  'context_separator:afterDuplicate': {
    type: 'separator'
  },
  'context_bookmarkTab': {
    title:              browser.i18n.getMessage('tabContextMenu_bookmark_label'),
    titleMultiselected: browser.i18n.getMessage('tabContextMenu_bookmark_label_multiselected')
  },
  'context_topLevel_bookmarkTree': {
    title:              browser.i18n.getMessage('context_bookmarkTree_label'),
    titleMultiselected: browser.i18n.getMessage('context_bookmarkTree_label_multiselected')
  },
  'context_moveTab': {
    title:              browser.i18n.getMessage('tabContextMenu_moveTab_label'),
    titleMultiselected: browser.i18n.getMessage('tabContextMenu_moveTab_label_multiselected')
  },
  'context_moveTabToStart': {
    parentId: 'context_moveTab',
    title:    browser.i18n.getMessage('tabContextMenu_moveTabToStart_label')
  },
  'context_moveTabToEnd': {
    parentId: 'context_moveTab',
    title:    browser.i18n.getMessage('tabContextMenu_moveTabToEnd_label')
  },
  'context_openTabInWindow': {
    parentId: 'context_moveTab',
    title:    browser.i18n.getMessage('tabContextMenu_tearOff_label')
  },
  'context_shareTabURL': {
    title: browser.i18n.getMessage('tabContextMenu_shareTabURL_label'),
  },
  'context_sendTabsToDevice': {
    title:              browser.i18n.getMessage('tabContextMenu_sendTabsToDevice_label'),
    titleMultiselected: browser.i18n.getMessage('tabContextMenu_sendTabsToDevice_label_multiselected')
  },
  'context_topLevel_sendTreeToDevice': {
    title:              browser.i18n.getMessage('context_sendTreeToDevice_label'),
    titleMultiselected: browser.i18n.getMessage('context_sendTreeToDevice_label_multiselected')
  },
  'context_reopenInContainer': {
    title: browser.i18n.getMessage('tabContextMenu_reopenInContainer_label')
  },
  'context_selectAllTabs': {
    title: browser.i18n.getMessage('tabContextMenu_selectAllTabs_label')
  },
  'context_separator:afterSelectAllTabs': {
    type: 'separator'
  },
  'context_topLevel_collapseTree': {
    title:              browser.i18n.getMessage('context_collapseTree_label'),
    titleMultiselected: browser.i18n.getMessage('context_collapseTree_label_multiselected')
  },
  'context_topLevel_collapseTreeRecursively': {
    title:              browser.i18n.getMessage('context_collapseTreeRecursively_label'),
    titleMultiselected: browser.i18n.getMessage('context_collapseTreeRecursively_label_multiselected')
  },
  'context_topLevel_collapseAll': {
    title: browser.i18n.getMessage('context_collapseAll_label')
  },
  'context_topLevel_expandTree': {
    title:              browser.i18n.getMessage('context_expandTree_label'),
    titleMultiselected: browser.i18n.getMessage('context_expandTree_label_multiselected')
  },
  'context_topLevel_expandTreeRecursively': {
    title:              browser.i18n.getMessage('context_expandTreeRecursively_label'),
    titleMultiselected: browser.i18n.getMessage('context_expandTreeRecursively_label_multiselected')
  },
  'context_topLevel_expandAll': {
    title: browser.i18n.getMessage('context_expandAll_label')
  },
  'context_separator:afterCollapseExpand': {
    type: 'separator'
  },
  'context_closeTab': {
    title:              browser.i18n.getMessage('tabContextMenu_close_label'),
    titleMultiselected: browser.i18n.getMessage('tabContextMenu_close_label_multiselected')
  },
  'context_closeMultipleTabs': {
    title: browser.i18n.getMessage('tabContextMenu_closeMultipleTabs_label')
  },
  'context_closeTabsToTheStart': {
    parentId: 'context_closeMultipleTabs',
    title: browser.i18n.getMessage('tabContextMenu_closeTabsToTop_label')
  },
  'context_closeTabsToTheEnd': {
    parentId: 'context_closeMultipleTabs',
    title: browser.i18n.getMessage('tabContextMenu_closeTabsToBottom_label')
  },
  'context_closeOtherTabs': {
    parentId: 'context_closeMultipleTabs',
    title: browser.i18n.getMessage('tabContextMenu_closeOther_label')
  },
  'context_topLevel_closeTree': {
    title:              browser.i18n.getMessage('context_closeTree_label'),
    titleMultiselected: browser.i18n.getMessage('context_closeTree_label_multiselected')
  },
  'context_topLevel_closeDescendants': {
    title:              browser.i18n.getMessage('context_closeDescendants_label'),
    titleMultiselected: browser.i18n.getMessage('context_closeDescendants_label_multiselected')
  },
  'context_topLevel_closeOthers': {
    title:              browser.i18n.getMessage('context_closeOthers_label'),
    titleMultiselected: browser.i18n.getMessage('context_closeOthers_label_multiselected')
  },
  'context_undoCloseTab': {
    title: browser.i18n.getMessage('tabContextMenu_undoClose_label'),
    titleRegular: browser.i18n.getMessage('tabContextMenu_undoClose_label'),
    titleMultipleTabsRestorable: browser.i18n.getMessage('tabContextMenu_undoClose_label_multiple')
  },
  'context_separator:afterClose': {
    type: 'separator'
  },

  'noContextTab:context_reloadTab': {
    title: browser.i18n.getMessage('tabContextMenu_reload_label_multiselected')
  },
  'noContextTab:context_bookmarkSelected': {
    title: browser.i18n.getMessage('tabContextMenu_bookmarkSelected_label')
  },
  'noContextTab:context_selectAllTabs': {
    title: browser.i18n.getMessage('tabContextMenu_selectAllTabs_label')
  },
  'noContextTab:context_undoCloseTab': {
    title: browser.i18n.getMessage('tabContextMenu_undoClose_label')
  },

  'lastSeparatorBeforeExtraItems': {
    type:     'separator',
    fakeMenu: true
  }
};

const mExtraItems = new Map();

const SIDEBAR_URL_PATTERN = [`${Constants.kSHORTHAND_URIS.tabbar}*`];

function getItemPlacementSignature(item) {
  if (item.placementSignature)
    return item.placementSignature;
  return item.placementSignature = JSON.stringify({
    parentId: item.parentId
  });
}
export async function init() {
  browser.runtime.onMessage.addListener(onMessage);
  TSTAPI.onMessageExternal.addListener(onMessageExternal);

  window.addEventListener('unload', () => {
    browser.runtime.onMessage.removeListener(onMessage);
    TSTAPI.onMessageExternal.removeListener(onMessageExternal);
  }, { once: true });

  const itemIds = Object.keys(mItemsById);
  for (const id of itemIds) {
    const item = mItemsById[id];
    item.id          = id;
    item.lastTitle   = item.title;
    item.lastVisible = false;
    item.lastEnabled = true;
    if (item.type == 'separator') {
      let beforeSeparator = true;
      item.precedingItems = [];
      item.followingItems = [];
      for (const id of itemIds) {
        const possibleSibling = mItemsById[id];
        if (getItemPlacementSignature(item) != getItemPlacementSignature(possibleSibling)) {
          if (beforeSeparator)
            continue;
          else
            break;
        }
        if (id == item.id) {
          beforeSeparator = false;
          continue;
        }
        if (beforeSeparator) {
          if (possibleSibling.type == 'separator') {
            item.previousSeparator = possibleSibling;
            item.precedingItems = [];
          }
          else {
            item.precedingItems.push(id);
          }
        }
        else {
          if (possibleSibling.type == 'separator')
            break;
          else
            item.followingItems.push(id);
        }
      }
    }
    const info = {
      id,
      title:    item.title,
      type:     item.type || 'normal',
      contexts: ['tab'],
      viewTypes: ['sidebar', 'tab', 'popup'],
      visible:  false, // hide all by default
      documentUrlPatterns: SIDEBAR_URL_PATTERN
    };
    if (item.parentId)
      info.parentId = item.parentId;
    if (!item.fakeMenu)
      browser.menus.create(info);
    onMessageExternal({
      type: TSTAPI.kCONTEXT_MENU_CREATE,
      params: info
    }, browser.runtime);
  }
  browser.menus.onShown.addListener(onShown);
  browser.menus.onHidden.addListener(onHidden);
  browser.menus.onClicked.addListener(onClick);
  onTSTItemClick.addListener(onClick);

  await ContextualIdentities.init();
  updateContextualIdentities();
  ContextualIdentities.onUpdated.addListener(() => {
    updateContextualIdentities();
  });
}

let mMultipleTabsRestorable = false;
Tab.onChangeMultipleTabsRestorability.addListener(multipleTabsRestorable => {
  mMultipleTabsRestorable = multipleTabsRestorable;
});

const mContextualIdentityItems = new Set();
function updateContextualIdentities() {
  for (const item of mContextualIdentityItems) {
    const id = item.id;
    if (id in mItemsById)
      delete mItemsById[id];
    browser.menus.remove(id).catch(ApiTabs.createErrorSuppressor());
    onMessageExternal({
      type: TSTAPI.kCONTEXT_MENU_REMOVE,
      params: id
    }, browser.runtime);
  }
  mContextualIdentityItems.clear();

  const defaultItem = {
    parentId:  'context_reopenInContainer',
    id:        'context_reopenInContainer:firefox-default',
    title:     browser.i18n.getMessage('tabContextMenu_reopenInContainer_noContainer_label'),
    contexts:  ['tab'],
    viewTypes: ['sidebar', 'tab', 'popup'],
    documentUrlPatterns: SIDEBAR_URL_PATTERN
  };
  browser.menus.create(defaultItem);
  onMessageExternal({
    type: TSTAPI.kCONTEXT_MENU_CREATE,
    params: defaultItem
  }, browser.runtime);
  mContextualIdentityItems.add(defaultItem);

  const defaultSeparator = {
    parentId:  'context_reopenInContainer',
    id:        'context_reopenInContainer_separator',
    type:      'separator',
    contexts:  ['tab'],
    viewTypes: ['sidebar', 'tab', 'popup'],
    documentUrlPatterns: SIDEBAR_URL_PATTERN
  };
  browser.menus.create(defaultSeparator);
  onMessageExternal({
    type: TSTAPI.kCONTEXT_MENU_CREATE,
    params: defaultSeparator
  }, browser.runtime);
  mContextualIdentityItems.add(defaultSeparator);

  ContextualIdentities.forEach(identity => {
    const id = `context_reopenInContainer:${identity.cookieStoreId}`;
    const item = {
      parentId: 'context_reopenInContainer',
      id:       id,
      title:    identity.name.replace(/^([a-z0-9])/i, '&$1'),
      contexts: ['tab'],
      viewTypes: ['sidebar', 'tab', 'popup'],
      documentUrlPatterns: SIDEBAR_URL_PATTERN
    };
    if (identity.iconUrl)
      item.icons = { 16: identity.iconUrl };
    browser.menus.create(item);
    onMessageExternal({
      type: TSTAPI.kCONTEXT_MENU_CREATE,
      params: item
    }, browser.runtime);
    mContextualIdentityItems.add(item);
  });
  for (const item of mContextualIdentityItems) {
    mItemsById[item.id] = item;
    item.lastVisible = true;
    item.lastEnabled = true;
  }
}

const mLastDevicesSignature = new Map();
const mSendToDeviceItems    = new Map();
export async function updateSendToDeviceItems(parentId, { manage } = {}) {
  await Sync.waitUntilDeviceInfoInitialized();
  const devices = Sync.getOtherDevices();
  const signature = JSON.stringify(devices.map(device => ({ id: device.id, name: device.name })));
  if (signature == mLastDevicesSignature.get(parentId))
    return false;

  mLastDevicesSignature.set(parentId, signature);

  const items = mSendToDeviceItems.get(parentId) || new Set();
  for (const item of items) {
    const id = item.id;
    browser.menus.remove(id).catch(ApiTabs.createErrorSuppressor());
    onMessageExternal({
      type: TSTAPI.kCONTEXT_MENU_REMOVE,
      params: id
    }, browser.runtime);
  }
  items.clear();

  const baseParams = {
    parentId,
    contexts:            ['tab'],
    viewTypes:           ['sidebar', 'tab', 'popup'],
    documentUrlPatterns: SIDEBAR_URL_PATTERN
  };

  if (devices.length > 0) {
    for (const device of devices) {
      const item = {
        ...baseParams,
        type:  'normal',
        id:    `${parentId}:device:${device.id}`,
        title: device.name
      };
      if (device.icon)
        item.icons = {
          '16': `/resources/icons/${sanitizeForHTMLText(device.icon)}.svg`
        };
      browser.menus.create(item);
      onMessageExternal({
        type: TSTAPI.kCONTEXT_MENU_CREATE,
        params: item
      }, browser.runtime);
      items.add(item);
    }

    const separator = {
      ...baseParams,
      type: 'separator',
      id:   `${parentId}:separator`
    };
    browser.menus.create(separator);
    onMessageExternal({
      type: TSTAPI.kCONTEXT_MENU_CREATE,
      params: separator
    }, browser.runtime);
    items.add(separator);

    const sendToAllItem = {
      ...baseParams,
      type:  'normal',
      id:    `${parentId}:all`,
      title: browser.i18n.getMessage('tabContextMenu_sendTabsToAllDevices_label')
    };
    browser.menus.create(sendToAllItem);
    onMessageExternal({
      type: TSTAPI.kCONTEXT_MENU_CREATE,
      params: sendToAllItem
    }, browser.runtime);
    items.add(sendToAllItem);
  }

  if (manage) {
    const manageItem = {
      ...baseParams,
      type:  'normal',
      id:    `${parentId}:manage`,
      title: browser.i18n.getMessage('tabContextMenu_manageSyncDevices_label')
    };
    browser.menus.create(manageItem);
    onMessageExternal({
      type: TSTAPI.kCONTEXT_MENU_CREATE,
      params: manageItem
    }, browser.runtime);
    items.add(manageItem);
  }

  mSendToDeviceItems.set(parentId, items);
  return true;
}


function updateItem(id, state = {}) {
  let modified = false;
  const item = mItemsById[id];
  const updateInfo = {
    visible: 'visible' in state ? !!state.visible : true,
    enabled: 'enabled' in state ? !!state.enabled : true
  };
  if ('checked' in state)
    updateInfo.checked = state.checked;
  const title = String(state.title || (state.multiselected && item.titleMultiselected) || item.title).replace(/%S/g, state.count || 0);
  if (title) {
    updateInfo.title = title;
    modified = title != item.lastTitle;
    item.lastTitle = updateInfo.title;
  }
  if (!modified)
    modified = updateInfo.visible != item.lastVisible ||
                 updateInfo.enabled != item.lastEnabled;
  item.lastVisible = updateInfo.visible;
  item.lastEnabled = updateInfo.enabled;
  browser.menus.update(id, updateInfo).catch(ApiTabs.createErrorSuppressor());
  onMessageExternal({
    type: TSTAPI.kCONTEXT_MENU_UPDATE,
    params: [id, updateInfo]
  }, browser.runtime);
  return modified;
}

function updateSeparator(id, options = {}) {
  const item = mItemsById[id];
  const visible = (
    (options.hasVisiblePreceding ||
     hasVisiblePrecedingItem(item)) &&
    (options.hasVisibleFollowing ||
     item.followingItems.some(id => mItemsById[id].type != 'separator' && mItemsById[id].lastVisible))
  );
  return updateItem(id, { visible });
}
function hasVisiblePrecedingItem(separator) {
  return (
    separator.precedingItems.some(id => mItemsById[id].type != 'separator' && mItemsById[id].lastVisible) ||
    (separator.previousSeparator &&
     !separator.previousSeparator.lastVisible &&
     hasVisiblePrecedingItem(separator.previousSeparator))
  );
}

let mOverriddenContext = null;

async function onShown(info, contextTab) {
  try {
    contextTab = contextTab && Tab.get(contextTab.id);
    const windowId              = contextTab ? contextTab.windowId : (await browser.windows.getLastFocused({}).catch(ApiTabs.createErrorHandler())).id;
    const previousTab           = contextTab && contextTab.$TST.previousTab;
    const previousSiblingTab    = contextTab && contextTab.$TST.previousSiblingTab;
    const nextTab               = contextTab && contextTab.$TST.nextTab;
    const nextSiblingTab        = contextTab && contextTab.$TST.nextSiblingTab;
    const hasMultipleTabs       = Tab.hasMultipleTabs(windowId);
    const hasMultipleNormalTabs = Tab.hasMultipleTabs(windowId, { normal: true });
    const multiselected         = contextTab && contextTab.$TST.multiselected;
    const contextTabs           = multiselected ?
      Tab.getSelectedTabs(windowId) :
      contextTab ?
        [contextTab] :
        [];
    const hasChild              = contextTab && contextTabs.some(tab => tab.$TST.hasChild);

    if (mOverriddenContext)
      return onOverriddenMenuShown(info, contextTab, windowId);

    let modifiedItemsCount = cleanupOverriddenMenu();

    // ESLint reports "short circuit" error for following codes.
    //   https://eslint.org/docs/rules/no-unused-expressions#allowshortcircuit
    // To allow those usages, I disable the rule temporarily.
    /* eslint-disable no-unused-expressions */

    const emulate = configs.emulateDefaultContextMenu;

    updateItem('context_newTab', {
      visible: emulate,
    }) && modifiedItemsCount++;
    updateItem('context_separator:afterNewTab', {
      visible: emulate,
    }) && modifiedItemsCount++;

    updateItem('context_reloadTab', {
      visible: emulate && contextTab,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_topLevel_reloadTree', {
      visible: emulate && contextTab && configs.context_topLevel_reloadTree,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_topLevel_reloadDescendants', {
      visible: emulate && contextTab && configs.context_topLevel_reloadDescendants,
      enabled: hasChild,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_toggleMuteTab-mute', {
      visible: emulate && contextTab && (!contextTab.mutedInfo || !contextTab.mutedInfo.muted),
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_toggleMuteTab-unmute', {
      visible: emulate && contextTab && contextTab.mutedInfo && contextTab.mutedInfo.muted,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_pinTab', {
      visible: emulate && contextTab && !contextTab.pinned,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_unpinTab', {
      visible: emulate && contextTab && contextTab.pinned,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_duplicateTab', {
      visible: emulate && contextTab,
      multiselected
    }) && modifiedItemsCount++;

    updateItem('context_bookmarkTab', {
      visible: emulate && contextTab,
      multiselected: multiselected || !contextTab
    }) && modifiedItemsCount++;
    updateItem('context_topLevel_bookmarkTree', {
      visible: emulate && contextTab && configs.context_topLevel_bookmarkTree,
      multiselected
    }) && modifiedItemsCount++;

    updateItem('context_moveTab', {
      visible: emulate && contextTab,
      enabled: contextTab && hasMultipleTabs,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_moveTabToStart', {
      enabled: emulate && contextTab && hasMultipleTabs && (previousSiblingTab || previousTab) && ((previousSiblingTab || previousTab).pinned == contextTab.pinned),
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_moveTabToEnd', {
      enabled: emulate && contextTab && hasMultipleTabs && (nextSiblingTab || nextTab) && ((nextSiblingTab || nextTab).pinned == contextTab.pinned),
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_openTabInWindow', {
      enabled: emulate && contextTab && hasMultipleTabs,
      multiselected
    }) && modifiedItemsCount++;

    updateItem('context_shareTabURL', {
      visible: emulate && contextTab && false, // not implemented yet
    }) && modifiedItemsCount++;

    updateItem('context_sendTabsToDevice', {
      visible: emulate && contextTab,
      enabled: contextTabs.filter(Sync.isSendableTab).length > 0,
      multiselected,
      count: contextTabs.length
    }) && modifiedItemsCount++;
    await updateSendToDeviceItems('context_sendTabsToDevice', { manage: true }) && modifiedItemsCount++;
    updateItem('context_topLevel_sendTreeToDevice', {
      visible: emulate && contextTab && configs.context_topLevel_sendTreeToDevice && hasChild,
      enabled: hasChild && contextTabs.filter(Sync.isSendableTab).length > 0,
      multiselected
    }) && modifiedItemsCount++;
    mItemsById.context_topLevel_sendTreeToDevice.lastVisible && await updateSendToDeviceItems('context_topLevel_sendTreeToDevice') && modifiedItemsCount++;

    let showContextualIdentities = false;
    if (contextTab && !contextTab.incognito) {
      for (const item of mContextualIdentityItems.values()) {
        const id = item.id;
        let visible;
        if (!emulate)
          visible = false;
        else if (id == 'context_reopenInContainer_separator')
          visible = contextTab && contextTab.cookieStoreId != 'firefox-default';
        else
          visible = contextTab && id != `context_reopenInContainer:${contextTab.cookieStoreId}`;
        updateItem(id, { visible }) && modifiedItemsCount++;
        if (visible)
          showContextualIdentities = true;
      }
    }
    updateItem('context_reopenInContainer', {
      visible: emulate && contextTab && showContextualIdentities && !contextTab.incognito,
      multiselected
    }) && modifiedItemsCount++;

    updateItem('context_selectAllTabs', {
      visible: emulate && contextTab,
      enabled: contextTab && Tab.getSelectedTabs(windowId).length != Tab.getVisibleTabs(windowId).length,
      multiselected
    }) && modifiedItemsCount++;

    updateItem('context_topLevel_collapseTree', {
      visible: emulate && contextTab && configs.context_topLevel_collapseTree,
      enabled: hasChild,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_topLevel_collapseTreeRecursively', {
      visible: emulate && contextTab && configs.context_topLevel_collapseTreeRecursively,
      enabled: hasChild,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_topLevel_collapseAll', {
      visible: emulate && !multiselected && contextTab && configs.context_topLevel_collapseAll
    }) && modifiedItemsCount++;
    updateItem('context_topLevel_expandTree', {
      visible: emulate && contextTab && configs.context_topLevel_expandTree,
      enabled: hasChild,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_topLevel_expandTreeRecursively', {
      visible: emulate && contextTab && configs.context_topLevel_expandTreeRecursively,
      enabled: hasChild,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_topLevel_expandAll', {
      visible: emulate && !multiselected && contextTab && configs.context_topLevel_expandAll
    }) && modifiedItemsCount++;

    updateItem('context_closeTab', {
      visible: emulate && contextTab,
      multiselected
    }) && modifiedItemsCount++;

    updateItem('context_closeMultipleTabs', {
      visible: emulate && contextTab,
      enabled: hasMultipleNormalTabs,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_closeTabsToTheStart', {
      visible: emulate && contextTab,
      enabled: nextTab,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_closeTabsToTheEnd', {
      visible: emulate && contextTab,
      enabled: nextTab,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_closeOtherTabs', {
      visible: emulate && contextTab,
      enabled: hasMultipleNormalTabs,
      multiselected
    }) && modifiedItemsCount++;

    updateItem('context_topLevel_closeTree', {
      visible: emulate && contextTab && configs.context_topLevel_closeTree,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_topLevel_closeDescendants', {
      visible: emulate && contextTab && configs.context_topLevel_closeDescendants,
      enabled: hasChild,
      multiselected
    }) && modifiedItemsCount++;
    updateItem('context_topLevel_closeOthers', {
      visible: emulate && contextTab && configs.context_topLevel_closeOthers,
      multiselected
    }) && modifiedItemsCount++;

    const undoCloseTabLabel = mItemsById.context_undoCloseTab[configs.undoMultipleTabsClose && mMultipleTabsRestorable ? 'titleMultipleTabsRestorable' : 'titleRegular'];
    updateItem('context_undoCloseTab', {
      title:   undoCloseTabLabel,
      visible: emulate && contextTab,
      multiselected
    }) && modifiedItemsCount++;

    updateItem('noContextTab:context_reloadTab', {
      visible: emulate && !contextTab
    }) && modifiedItemsCount++;
    updateItem('noContextTab:context_bookmarkSelected', {
      visible: emulate && !contextTab
    }) && modifiedItemsCount++;
    updateItem('noContextTab:context_selectAllTabs', {
      visible: emulate && !contextTab,
      enabled: !contextTab && Tab.getSelectedTabs(windowId).length != Tab.getVisibleTabs(windowId).length
    }) && modifiedItemsCount++;
    updateItem('noContextTab:context_undoCloseTab', {
      title:   undoCloseTabLabel,
      visible: emulate && !contextTab
    }) && modifiedItemsCount++;

    updateSeparator('context_separator:afterDuplicate') && modifiedItemsCount++;
    updateSeparator('context_separator:afterSelectAllTabs') && modifiedItemsCount++;
    updateSeparator('context_separator:afterCollapseExpand') && modifiedItemsCount++;
    updateSeparator('context_separator:afterClose') && modifiedItemsCount++;

    const flattenExtraItems = Array.from(mExtraItems.values()).flat();

    updateSeparator('lastSeparatorBeforeExtraItems', {
      hasVisibleFollowing: contextTab && flattenExtraItems.some(item => !item.parentId && item.visible !== false)
    }) && modifiedItemsCount++;

    /* eslint-enable no-unused-expressions */

    if (modifiedItemsCount > 0)
      browser.menus.refresh().catch(ApiTabs.createErrorSuppressor());
  }
  catch(error) {
    console.error(error);
  }
}

let mLastOverriddenContextOwner = null;

function onOverriddenMenuShown(info, contextTab, windowId) {
  if (!mLastOverriddenContextOwner) {
    for (const itemId of Object.keys(mItemsById)) {
      if (!mItemsById[itemId].lastVisible)
        continue;
      mItemsById[itemId].lastVisible = false;
      browser.menus.update(itemId, { visible: false });
    }
    mLastOverriddenContextOwner = mOverriddenContext.owner;
  }

  for (const item of (mExtraItems.get(mLastOverriddenContextOwner) || [])) {
    if (item.$topLevel &&
        item.lastVisible) {
      browser.menus.update(
        getExternalTopLevelItemId(mOverriddenContext.owner, item.id),
        { visible: true }
      );
    }
  }

  const message = {
    type: TSTAPI.kCONTEXT_MENU_SHOWN,
    info: {
      bookmarkId:    info.bookmarkId || null,
      button:        info.button,
      checked:       info.checked,
      contexts:      contextTab ? ['tab'] : info.bookmarkId ? ['bookmark'] : [],
      editable:      false,
      frameId:       null,
      frameUrl:      null,
      linkText:      null,
      linkUrl:       null,
      mediaType:     null,
      menuIds:       [],
      menuItemId:    null,
      modifiers:     [],
      pageUrl:       null,
      parentMenuItemId: null,
      selectionText: null,
      srcUrl:        null,
      targetElementId: null,
      viewType:      'sidebar',
      wasChecked:    false
    },
    tab: contextTab && new TSTAPI.TreeItem(contextTab, { isContextTab: true }) || null,
    windowId
  }
  TSTAPI.sendMessage(message, {
    targets: [mOverriddenContext.owner],
    tabProperties: ['tab']
  });
  TSTAPI.sendMessage({
    ...message,
    type: TSTAPI.kFAKE_CONTEXT_MENU_SHOWN
  }, {
    targets: [mOverriddenContext.owner],
    tabProperties: ['tab']
  });

  reserveRefresh();
}

function cleanupOverriddenMenu() {
  if (!mLastOverriddenContextOwner)
    return 0;

  let modifiedItemsCount = 0;

  const owner = mLastOverriddenContextOwner;
  mLastOverriddenContextOwner = null;

  for (const itemId of Object.keys(mItemsById)) {
    if (!mItemsById[itemId].lastVisible)
      continue;
    mItemsById[itemId].lastVisible = true;
    browser.menus.update(itemId, { visible: true });
    modifiedItemsCount++;
  }

  for (const item of (mExtraItems.get(owner) || [])) {
    if (item.$topLevel &&
        item.lastVisible) {
      browser.menus.update(
        getExternalTopLevelItemId(owner, item.id),
        { visible: false }
      );
      modifiedItemsCount++;
    }
  }

  return modifiedItemsCount;
}

function onHidden() {
  const owner = mOverriddenContext && mOverriddenContext.owner;
  const windowId = mOverriddenContext && mOverriddenContext.windowId;
  if (mLastOverriddenContextOwner &&
      owner == mLastOverriddenContextOwner) {
    mOverriddenContext = null;
    TSTAPI.sendMessage({
      type: TSTAPI.kCONTEXT_MENU_HIDDEN,
      windowId
    }, {
      targets: [owner]
    });
    TSTAPI.sendMessage({
      type: TSTAPI.kFAKE_CONTEXT_MENU_HIDDEN,
      windowId
    }, {
      targets: [owner]
    });
  }
}

async function onClick(info, contextTab) {
  contextTab = contextTab && Tab.get(contextTab.id);
  const window    = await browser.windows.getLastFocused({ populate: true }).catch(ApiTabs.createErrorHandler());
  const windowId  = contextTab && contextTab.windowId || window.id;
  const activeTab = TabsStore.activeTabInWindow.get(windowId);

  let multiselectedTabs = Tab.getSelectedTabs(windowId);
  const isMultiselected = contextTab ? contextTab.$TST.multiselected : multiselectedTabs.length > 1;
  if (!isMultiselected)
    multiselectedTabs = null;

  switch (info.menuItemId.replace(/^noContextTab:/, '')) {
    case 'context_newTab': {
      const behavior = info.button == 1 ?
        configs.autoAttachOnNewTabButtonMiddleClick :
        (info.modifiers && (info.modifiers.includes('Ctrl') || info.modifiers.includes('Command'))) ?
          configs.autoAttachOnNewTabButtonAccelClick :
          contextTab ?
            configs.autoAttachOnContextNewTabCommand :
            configs.autoAttachOnNewTabCommand;
      Commands.openNewTabAs({
        baseTab: contextTab || activeTab,
        as:      behavior,
      });
    }; break;

    case 'context_reloadTab':
      if (multiselectedTabs) {
        for (const tab of multiselectedTabs) {
          browser.tabs.reload(tab.id)
            .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
        }
      }
      else {
        const tab = contextTab || activeTab;
        browser.tabs.reload(tab.id)
          .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
      }
      break;
    case 'context_toggleMuteTab-mute':
      if (multiselectedTabs) {
        for (const tab of multiselectedTabs) {
          browser.tabs.update(tab.id, { muted: true })
            .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
        }
      }
      else {
        browser.tabs.update(contextTab.id, { muted: true })
          .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
      }
      break;
    case 'context_toggleMuteTab-unmute':
      if (multiselectedTabs) {
        for (const tab of multiselectedTabs) {
          browser.tabs.update(tab.id, { muted: false })
            .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
        }
      }
      else {
        browser.tabs.update(contextTab.id, { muted: false })
          .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
      }
      break;
    case 'context_pinTab':
      if (multiselectedTabs) {
        for (const tab of multiselectedTabs) {
          browser.tabs.update(tab.id, { pinned: true })
            .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
        }
      }
      else {
        browser.tabs.update(contextTab.id, { pinned: true })
          .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
      }
      break;
    case 'context_unpinTab':
      if (multiselectedTabs) {
        for (const tab of multiselectedTabs) {
          browser.tabs.update(tab.id, { pinned: false })
            .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
        }
      }
      else {
        browser.tabs.update(contextTab.id, { pinned: false })
          .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
      }
      break;
    case 'context_duplicateTab':
      Commands.duplicateTab(contextTab, {
        destinationWindowId: windowId
      });
      break;
    case 'context_moveTabToStart':
      Commands.moveTabToStart(contextTab);
      break;
    case 'context_moveTabToEnd':
      Commands.moveTabToEnd(contextTab);
      break;
    case 'context_openTabInWindow':
      Commands.openTabInWindow(contextTab);
      break;
    case 'context_sendTabsToDevice:all':
      Commands.sendTabsToAllDevices(multiselectedTabs || [contextTab]);
      break;
    case 'context_sendTabsToDevice:manage':
      Commands.manageSyncDevices(windowId);
      break;
    case 'context_topLevel_sendTreeToDevice:all':
      Commands.sendTabsToAllDevices(multiselectedTabs || [contextTab], { recursively: true });
      break;
    case 'context_selectAllTabs': {
      const tabs = await browser.tabs.query({ windowId }).catch(ApiTabs.createErrorHandler());
      browser.tabs.highlight({
        windowId,
        populate: false,
        tabs:     [activeTab.index].concat(mapAndFilter(tabs,
                                                        tab => !tab.active ? tab.index : undefined))
      }).catch(ApiTabs.createErrorSuppressor());
    }; break;
    case 'context_bookmarkTab':
      Commands.bookmarkTab(contextTab);
      break;
    case 'context_bookmarkSelected':
      Commands.bookmarkTab(contextTab || activeTab);
      break;
    case 'context_closeTabsToTheStart': {
      const tabs = await browser.tabs.query({ windowId }).catch(ApiTabs.createErrorHandler());
      const closeTabs = [];
      const keptTabIds = new Set(
        multiselectedTabs ?
          multiselectedTabs.map(tab => tab.id) :
          [contextTab.id]
      );
      for (const tab of tabs) {
        if (keptTabIds.has(tab.id))
          break;
        if (!tab.pinned && !tab.hidden)
          closeTabs.push(Tab.get(tab.id));
      }
      const canceled = (await browser.runtime.sendMessage({
        type: Constants.kCOMMAND_NOTIFY_TABS_CLOSING,
        tabs: closeTabs.map(tab => tab.$TST.sanitized),
        windowId
      }).catch(ApiTabs.createErrorHandler())) === false;
      if (canceled)
        break;
      TabsInternalOperation.removeTabs(closeTabs);
    }; break;
    case 'context_closeTabsToTheEnd': {
      const tabs = await browser.tabs.query({ windowId }).catch(ApiTabs.createErrorHandler());
      tabs.reverse();
      const closeTabs = [];
      const keptTabIds = new Set(
        (multiselectedTabs ?
          multiselectedTabs :
          [contextTab]
        ).reduce((tabIds, tab, _index) => {
          if (tab.$TST.subtreeCollapsed)
            tabIds.push(tab.id, ...tab.$TST.descendants.map(tab => tab.id))
          else
            tabIds.push(tab.id);
          return tabIds;
        }, [])
      );
      for (const tab of tabs) {
        if (keptTabIds.has(tab.id))
          break;
        if (!tab.pinned && !tab.hidden)
          closeTabs.push(Tab.get(tab.id));
      }
      closeTabs.reverse();
      const canceled = (await browser.runtime.sendMessage({
        type: Constants.kCOMMAND_NOTIFY_TABS_CLOSING,
        tabs: closeTabs.map(tab => tab.$TST.sanitized),
        windowId
      }).catch(ApiTabs.createErrorHandler())) === false;
      if (canceled)
        break;
      TabsInternalOperation.removeTabs(closeTabs);
    }; break;
    case 'context_closeOtherTabs': {
      const tabs  = await browser.tabs.query({ windowId }).catch(ApiTabs.createErrorHandler());
      const keptTabIds = new Set(
        (multiselectedTabs ?
          multiselectedTabs :
          [contextTab]
        ).reduce((tabIds, tab, _index) => {
          if (tab.$TST.subtreeCollapsed)
            tabIds.push(tab.id, ...tab.$TST.descendants.map(tab => tab.id))
          else
            tabIds.push(tab.id);
          return tabIds;
        }, [])
      );
      const closeTabs = mapAndFilter(tabs,
                                     tab => !tab.pinned && !tab.hidden && !keptTabIds.has(tab.id) && Tab.get(tab.id) || undefined);
      const canceled = (await browser.runtime.sendMessage({
        type: Constants.kCOMMAND_NOTIFY_TABS_CLOSING,
        tabs: closeTabs.map(tab => tab.$TST.sanitized),
        windowId
      }).catch(ApiTabs.createErrorHandler())) === false;
      if (canceled)
        break;
      TabsInternalOperation.removeTabs(closeTabs);
    }; break;
    case 'context_undoCloseTab': {
      const sessions = await browser.sessions.getRecentlyClosed({ maxResults: 1 }).catch(ApiTabs.createErrorHandler());
      if (sessions.length && sessions[0].tab)
        browser.sessions.restore(sessions[0].tab.sessionId).catch(ApiTabs.createErrorSuppressor());
    }; break;
    case 'context_closeTab': {
      const closeTabs = (multiselectedTabs || TreeBehavior.getClosingTabsFromParent(contextTab, {
        byInternalOperation: true
      })).reverse(); // close down to top, to keep tree structure of Tree Style Tab
      const canceled = (await browser.runtime.sendMessage({
        type: Constants.kCOMMAND_NOTIFY_TABS_CLOSING,
        tabs: closeTabs.map(tab => tab.$TST.sanitized),
        windowId
      }).catch(ApiTabs.createErrorHandler())) === false;
      if (canceled)
        return;
      TabsInternalOperation.removeTabs(closeTabs);
    }; break;

    default: {
      const contextualIdentityMatch = info.menuItemId.match(/^context_reopenInContainer:(.+)$/);
      if (contextTab &&
          contextualIdentityMatch)
        Commands.reopenInContainer(contextTab, contextualIdentityMatch[1]);

      const sendTabsToDeviceMatch = info.menuItemId.match(/^context_sendTabsToDevice:device:(.+)$/);
      if (contextTab &&
          sendTabsToDeviceMatch)
        Commands.sendTabsToDevice(
          multiselectedTabs || [contextTab],
          { to: sendTabsToDeviceMatch[1] }
        );
      const sendTreeToDeviceMatch = info.menuItemId.match(/^context_topLevel_sendTreeToDevice:device:(.+)$/);
      if (contextTab &&
          sendTreeToDeviceMatch)
        Commands.sendTabsToDevice(
          multiselectedTabs || [contextTab],
          { to: sendTreeToDeviceMatch[1],
            recursively: true }
        );

      if (EXTERNAL_TOP_LEVEL_ITEM_MATCHER.test(info.menuItemId)) {
        const owner      = RegExp.$1;
        const menuItemId = RegExp.$2;
        const tab = contextTab && (await (new TSTAPI.TreeItem(contextTab, { isContextTab: true })).exportFor      (owner)) || null;
        const message = {
          type: TSTAPI.kCONTEXT_MENU_CLICK,
          info: {
            bookmarkId:    info.bookmarkId || null,
            button:        info.button,
            checked:       info.checked,
            editable:      false,
            frameId:       null,
            frameUrl:      null,
            linkText:      null,
            linkUrl:       null,
            mediaType:     null,
            menuItemId,
            modifiers:     [],
            pageUrl:       null,
            parentMenuItemId: null,
            selectionText: null,
            srcUrl:        null,
            targetElementId: null,
            viewType:      'sidebar',
            wasChecked:    info.wasChecked
          },
          tab
        };
        if (owner == browser.runtime.id) {
          browser.runtime.sendMessage(message).catch(ApiTabs.createErrorSuppressor());
        }
        else if (TSTAPI.isSafeAtIncognito(owner, { tab: contextTab, windowId: TabsStore.getCurrentWindowId() })) {
          browser.runtime.sendMessage(owner, message).catch(ApiTabs.createErrorSuppressor());
          browser.runtime.sendMessage(owner, {
            ...message,
            type: TSTAPI.kFAKE_CONTEXT_MENU_CLICK
          }).catch(ApiTabs.createErrorSuppressor());
        }
      }
    }; break;
  }
}


function getItemsFor(addonId) {
  if (mExtraItems.has(addonId)) {
    return mExtraItems.get(addonId);
  }
  const items = [];
  mExtraItems.set(addonId, items);
  return items;
}

function exportExtraItems() {
  const exported = {};
  for (const [id, items] of mExtraItems.entries()) {
    exported[id] = items;
  }
  return exported;
}

async function notifyUpdated() {
  await browser.runtime.sendMessage({
    type:  Constants.kCOMMAND_NOTIFY_CONTEXT_MENU_UPDATED,
    items: exportExtraItems()
  }).catch(ApiTabs.createErrorSuppressor());
}

let mReservedNotifyUpdate;
let mNotifyUpdatedHandlers = [];

function reserveNotifyUpdated() {
  return new Promise((resolve, _aReject) => {
    mNotifyUpdatedHandlers.push(resolve);
    if (mReservedNotifyUpdate)
      clearTimeout(mReservedNotifyUpdate);
    mReservedNotifyUpdate = setTimeout(async () => {
      mReservedNotifyUpdate = undefined;
      await notifyUpdated();
      const handlers = mNotifyUpdatedHandlers;
      mNotifyUpdatedHandlers = [];
      for (const handler of handlers) {
        handler();
      }
    }, 10);
  });
}

function reserveRefresh() {
  if (reserveRefresh.reserved)
    clearTimeout(reserveRefresh.reserved);
  reserveRefresh.reserved = setTimeout(() => {
    reserveRefresh.reserved = null;;
    browser.menus.refresh();
  }, 0);
}

function onMessage(message, _sender) {
  log('tab-context-menu: internally called:', message);
  switch (message.type) {
    case Constants.kCOMMAND_GET_CONTEXT_MENU_ITEMS:
      return Promise.resolve(exportExtraItems());

    case TSTAPI.kCONTEXT_MENU_CLICK:
      onTSTItemClick.dispatch(message.info, message.tab);
      return;

    case TSTAPI.kCONTEXT_MENU_SHOWN:
      onShown(message.info, message.tab);
      onTSTTabContextMenuShown.dispatch(message.info, message.tab);
      return;

    case TSTAPI.kCONTEXT_MENU_HIDDEN:
      onTSTTabContextMenuHidden.dispatch();
      return;

    case Constants.kCOMMAND_NOTIFY_CONTEXT_ITEM_CHECKED_STATUS_CHANGED:
      for (const itemData of mExtraItems.get(message.ownerId)) {
        if (!itemData.id != message.id)
          continue;
        itemData.checked = message.checked;
        break;
      }
      return;

    case Constants.kCOMMAND_NOTIFY_CONTEXT_OVERRIDDEN:
      mOverriddenContext = message.context || null;
      if (mOverriddenContext) {
        mOverriddenContext.owner = message.owner;
        mOverriddenContext.windowId = message.windowId;
      }
      break;
  }
}

export function onMessageExternal(message, sender) {
  switch (message.type) {
    case TSTAPI.kCONTEXT_MENU_CREATE:
    case TSTAPI.kFAKE_CONTEXT_MENU_CREATE: {
      log('TSTAPI.kCONTEXT_MENU_CREATE:', message, { id: sender.id, url: sender.url });
      const items  = getItemsFor(sender.id);
      let params = message.params;
      if (Array.isArray(params))
        params = params[0];
      const parent = params.parentId && items.filter(item => item.id == params.parentId)[0];
      if (params.parentId && !parent)
        break;
      let shouldAdd = true;
      if (params.id) {
        for (let i = 0, maxi = items.length; i < maxi; i++) {
          const item = items[i];
          if (item.id != params.id)
            continue;
          items.splice(i, 1, params);
          shouldAdd = false;
          break;
        }
      }
      if (shouldAdd) {
        items.push(params);
        if (parent && params.id) {
          parent.children = parent.children || [];
          parent.children.push(params.id);
        }
      }
      mExtraItems.set(sender.id, items);
      params.$topLevel = (
        Array.isArray(params.viewTypes) &&
        params.viewTypes.includes('sidebar')
      );
      if (sender.id != browser.runtime.id &&
          params.$topLevel) {
        params.lastVisible = params.visible !== false;
        const visible = !!(
          params.lastVisible &&
          mOverriddenContext &&
          mLastOverriddenContextOwner == sender.id
        );
        const createParams = {
          id:        getExternalTopLevelItemId(sender.id, params.id),
          type:      params.type || 'normal',
          visible,
          viewTypes: ['sidebar', 'tab', 'popup'],
          contexts:  (params.contexts || []).filter(context => context == 'tab' || context == 'bookmark'),
          documentUrlPatterns: SIDEBAR_URL_PATTERN
        };
        if (params.parentId)
          createParams.parentId = getExternalTopLevelItemId(sender.id, params.parentId);
        for (const property of SAFE_MENU_PROPERTIES) {
          if (property in params)
            createParams[property] = params[property];
        }
        browser.menus.create(createParams);
        reserveRefresh();
        onTopLevelItemAdded.dispatch();
      }
      return reserveNotifyUpdated();
    }; break;

    case TSTAPI.kCONTEXT_MENU_UPDATE:
    case TSTAPI.kFAKE_CONTEXT_MENU_UPDATE: {
      log('TSTAPI.kCONTEXT_MENU_UPDATE:', message, { id: sender.id, url: sender.url });
      const items = getItemsFor(sender.id);
      for (let i = 0, maxi = items.length; i < maxi; i++) {
        const item = items[i];
        if (item.id != message.params[0])
          continue;
        const params = message.params[1];
        const updateProperties = {};
        for (const property of SAFE_MENU_PROPERTIES) {
          if (property in params)
            updateProperties[property] = params[property];
        }
        if (sender.id != browser.runtime.id &&
            item.$topLevel) {
          if ('visible' in updateProperties)
            item.lastVisible = updateProperties.visible;
          if (!mOverriddenContext ||
              mLastOverriddenContextOwner != sender.id)
            delete updateProperties.visible;
        }
        items.splice(i, 1, {
          ...item,
          ...updateProperties
        });
        if (sender.id != browser.runtime.id &&
            item.$topLevel &&
            Object.keys(updateProperties).length > 0) {
          browser.menus.update(
            getExternalTopLevelItemId(sender.id, item.id),
            updateProperties
          );
          reserveRefresh()
        }
        break;
      }
      mExtraItems.set(sender.id, items);
      return reserveNotifyUpdated();
    }; break;

    case TSTAPI.kCONTEXT_MENU_REMOVE:
    case TSTAPI.kFAKE_CONTEXT_MENU_REMOVE: {
      log('TSTAPI.kCONTEXT_MENU_REMOVE:', message, { id: sender.id, url: sender.url });
      let items = getItemsFor(sender.id);
      let id    = message.params;
      if (Array.isArray(id))
        id = id[0];
      const item   = items.filter(item => item.id == id)[0];
      if (!item)
        break;
      const parent = item.parentId && items.filter(item => item.id == item.parentId)[0];
      items = items.filter(item => item.id != id);
      mExtraItems.set(sender.id, items);
      if (parent && parent.children)
        parent.children = parent.children.filter(childId => childId != id);
      if (item.children) {
        for (const childId of item.children) {
          onMessageExternal({ type: message.type, params: childId }, sender);
        }
      }
      if (sender.id != browser.runtime.id &&
          item.$topLevel) {
        browser.menus.remove(getExternalTopLevelItemId(sender.id, item.id));
        reserveRefresh();
      }
      return reserveNotifyUpdated();
    }; break;

    case TSTAPI.kCONTEXT_MENU_REMOVE_ALL:
    case TSTAPI.kFAKE_CONTEXT_MENU_REMOVE_ALL:
    case TSTAPI.kUNREGISTER_SELF: {
      delete mExtraItems.delete(sender.id);
      return reserveNotifyUpdated();
    }; break;
  }
}

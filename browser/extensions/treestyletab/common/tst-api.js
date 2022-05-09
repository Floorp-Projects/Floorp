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

import TabFavIconHelper from '/extlib/TabFavIconHelper.js';
import EventListenerManager from '/extlib/EventListenerManager.js';

import {
  log as internalLogger,
  wait,
  configs,
  isLinux,
} from './common.js';
import * as Constants from './constants.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as TabsStore from './tabs-store.js';
import * as SidebarConnection from './sidebar-connection.js';

import Tab from './Tab.js';

function log(...args) {
  internalLogger('common/tst-api', ...args);
}

export const onInitialized = new EventListenerManager();
export const onRegistered   = new EventListenerManager();
export const onUnregistered = new EventListenerManager();
export const onMessageExternal = {
  $listeners: new Set(),
  addListener(listener) {
    this.$listeners.add(listener);
  },
  removeListener(listener) {
    this.$listeners.delete(listener);
  },
  dispatch(...args) {
    return Array.from(this.$listeners, listener => listener(...args));
  }
};

export const kREGISTER_SELF         = 'register-self';
export const kUNREGISTER_SELF       = 'unregister-self';
export const kWAIT_FOR_SHUTDOWN     = 'wait-for-shutdown';
export const kPING                  = 'ping';
export const kNOTIFY_READY          = 'ready';
export const kNOTIFY_SHUTDOWN       = 'shutdown'; // defined but not notified for now.
export const kNOTIFY_SIDEBAR_SHOW   = 'sidebar-show';
export const kNOTIFY_SIDEBAR_HIDE   = 'sidebar-hide';
export const kNOTIFY_TAB_CLICKED    = 'tab-clicked'; // for backward compatibility
export const kNOTIFY_TAB_DBLCLICKED = 'tab-dblclicked';
export const kNOTIFY_TAB_MOUSEDOWN  = 'tab-mousedown';
export const kNOTIFY_TAB_MOUSEUP    = 'tab-mouseup';
export const kNOTIFY_TABBAR_CLICKED = 'tabbar-clicked'; // for backward compatibility
export const kNOTIFY_TABBAR_MOUSEDOWN = 'tabbar-mousedown';
export const kNOTIFY_TABBAR_MOUSEUP = 'tabbar-mouseup';
export const kNOTIFY_TABBAR_OVERFLOW  = 'tabbar-overflow';
export const kNOTIFY_TABBAR_UNDERFLOW = 'tabbar-underflow';
export const kNOTIFY_NEW_TAB_BUTTON_CLICKED   = 'new-tab-button-clicked';
export const kNOTIFY_NEW_TAB_BUTTON_MOUSEDOWN = 'new-tab-button-mousedown';
export const kNOTIFY_NEW_TAB_BUTTON_MOUSEUP   = 'new-tab-button-mouseup';
export const kNOTIFY_TAB_MOUSEMOVE  = 'tab-mousemove';
export const kNOTIFY_TAB_MOUSEOVER  = 'tab-mouseover';
export const kNOTIFY_TAB_MOUSEOUT   = 'tab-mouseout';
export const kNOTIFY_TAB_DRAGREADY  = 'tab-dragready';
export const kNOTIFY_TAB_DRAGCANCEL = 'tab-dragcancel';
export const kNOTIFY_TAB_DRAGSTART  = 'tab-dragstart';
export const kNOTIFY_TAB_DRAGENTER  = 'tab-dragenter';
export const kNOTIFY_TAB_DRAGEXIT   = 'tab-dragexit';
export const kNOTIFY_TAB_DRAGEND    = 'tab-dragend';
export const kNOTIFY_TREE_ATTACHED  = 'tree-attached';
export const kNOTIFY_TREE_DETACHED  = 'tree-detached';
export const kNOTIFY_TREE_COLLAPSED_STATE_CHANGED = 'tree-collapsed-state-changed';
export const kNOTIFY_NATIVE_TAB_DRAGSTART = 'native-tab-dragstart';
export const kNOTIFY_NATIVE_TAB_DRAGEND   = 'native-tab-dragend';
export const kNOTIFY_PERMISSIONS_CHANGED = 'permissions-changed';
export const kSTART_CUSTOM_DRAG     = 'start-custom-drag';
export const kNOTIFY_TRY_MOVE_FOCUS_FROM_COLLAPSING_TREE = 'try-move-focus-from-collapsing-tree';
export const kNOTIFY_TRY_REDIRECT_FOCUS_FROM_COLLAPSED_TAB = 'try-redirect-focus-from-collaped-tab';
export const kNOTIFY_TRY_EXPAND_TREE_FROM_FOCUSED_PARENT = 'try-expand-tree-from-focused-parent';
export const kNOTIFY_TRY_EXPAND_TREE_FROM_FOCUSED_BUNDLED_PARENT = 'try-expand-tree-from-focused-bundled-parent';
export const kNOTIFY_TRY_EXPAND_TREE_FROM_ATTACHED_CHILD = 'try-expand-tree-from-attached-child';
export const kNOTIFY_TRY_EXPAND_TREE_FROM_FOCUSED_COLLAPSED_TAB = 'try-expand-tree-from-focused-collapsed-tab';
export const kNOTIFY_TRY_EXPAND_TREE_FROM_LONG_PRESS_CTRL_KEY = 'try-expand-tree-from-long-press-ctrl-key';
export const kNOTIFY_TRY_EXPAND_TREE_FROM_END_TAB_SWITCH = 'try-expand-tree-from-end-tab-switch';
export const kNOTIFY_TRY_FIXUP_TREE_ON_TAB_MOVED = 'try-fixup-tree-on-tab-moved';
export const kGET_TREE              = 'get-tree';
export const kATTACH                = 'attach';
export const kDETACH                = 'detach';
export const kINDENT                = 'indent';
export const kDEMOTE                = 'demote';
export const kOUTDENT               = 'outdent';
export const kPROMOTE               = 'promote';
export const kMOVE_UP               = 'move-up';
export const kMOVE_TO_START         = 'move-to-start';
export const kMOVE_DOWN             = 'move-down';
export const kMOVE_TO_END           = 'move-to-end';
export const kMOVE_BEFORE           = 'move-before';
export const kMOVE_AFTER            = 'move-after';
export const kFOCUS                 = 'focus';
export const kCREATE                = 'create';
export const kDUPLICATE             = 'duplicate';
export const kGROUP_TABS            = 'group-tabs';
export const kOPEN_IN_NEW_WINDOW    = 'open-in-new-window';
export const kREOPEN_IN_CONTAINER   = 'reopen-in-container';
export const kGET_TREE_STRUCTURE    = 'get-tree-structure';
export const kSET_TREE_STRUCTURE    = 'set-tree-structure';
export const kCOLLAPSE_TREE         = 'collapse-tree';
export const kEXPAND_TREE           = 'expand-tree';
export const kTOGGLE_TREE_COLLAPSED = 'toggle-tree-collapsed';
export const kADD_TAB_STATE         = 'add-tab-state';
export const kREMOVE_TAB_STATE      = 'remove-tab-state';
export const kSCROLL                = 'scroll';
export const kSTOP_SCROLL           = 'stop-scroll';
export const kSCROLL_LOCK           = 'scroll-lock';
export const kSCROLL_UNLOCK         = 'scroll-unlock';
export const kNOTIFY_SCROLLED       = 'scrolled';
export const kBLOCK_GROUPING        = 'block-grouping';
export const kUNBLOCK_GROUPING      = 'unblock-grouping';
export const kGRANT_TO_REMOVE_TABS  = 'grant-to-remove-tabs';
export const kOPEN_ALL_BOOKMARKS_WITH_STRUCTURE = 'open-all-bookmarks-with-structure';
export const kSET_EXTRA_TAB_CONTENTS   = 'set-extra-tab-contents';
export const kCLEAR_EXTRA_TAB_CONTENTS = 'clear-extra-tab-contents';
export const kCLEAR_ALL_EXTRA_TAB_CONTENTS = 'clear-all-extra-tab-contents';
export const kSET_EXTRA_NEW_TAB_BUTTON_CONTENTS   = 'set-extra-new-tab-button-contents';
export const kCLEAR_EXTRA_NEW_TAB_BUTTON_CONTENTS = 'clear-extra-new-tab-button-contents';
export const kGET_DRAG_DATA         = 'get-drag-data';

export const kCONTEXT_MENU_OPEN       = 'contextMenu-open';
export const kCONTEXT_MENU_CREATE     = 'contextMenu-create';
export const kCONTEXT_MENU_UPDATE     = 'contextMenu-update';
export const kCONTEXT_MENU_REMOVE     = 'contextMenu-remove';
export const kCONTEXT_MENU_REMOVE_ALL = 'contextMenu-remove-all';
export const kCONTEXT_MENU_CLICK      = 'contextMenu-click';
export const kCONTEXT_MENU_SHOWN      = 'contextMenu-shown';
export const kCONTEXT_MENU_HIDDEN     = 'contextMenu-hidden';
export const kFAKE_CONTEXT_MENU_OPEN       = 'fake-contextMenu-open';
export const kFAKE_CONTEXT_MENU_CREATE     = 'fake-contextMenu-create';
export const kFAKE_CONTEXT_MENU_UPDATE     = 'fake-contextMenu-update';
export const kFAKE_CONTEXT_MENU_REMOVE     = 'fake-contextMenu-remove';
export const kFAKE_CONTEXT_MENU_REMOVE_ALL = 'fake-contextMenu-remove-all';
export const kFAKE_CONTEXT_MENU_CLICK      = 'fake-contextMenu-click';
export const kFAKE_CONTEXT_MENU_SHOWN      = 'fake-contextMenu-shown';
export const kFAKE_CONTEXT_MENU_HIDDEN     = 'fake-contextMenu-hidden';
export const kOVERRIDE_CONTEXT        = 'override-context';

export const kCOMMAND_BROADCAST_API_REGISTERED   = 'treestyletab:broadcast-registered';
export const kCOMMAND_BROADCAST_API_UNREGISTERED = 'treestyletab:broadcast-unregistered';
export const kCOMMAND_BROADCAST_API_PERMISSION_CHANGED = 'treestyletab:permission-changed';
export const kCOMMAND_REQUEST_INITIALIZE         = 'treestyletab:request-initialize';
export const kCOMMAND_REQUEST_CONTROL_STATE      = 'treestyletab:request-control-state';
export const kCOMMAND_GET_ADDONS                 = 'treestyletab:get-addons';
export const kCOMMAND_SET_API_PERMISSION         = 'treestyletab:set-api-permisssion';
export const kCOMMAND_NOTIFY_PERMISSION_CHANGED  = 'treestyletab:notify-api-permisssion-changed';
export const kCOMMAND_UNREGISTER_ADDON           = 'treestyletab:unregister-addon';

// https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions
// https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs/Tab
const kPERMISSION_ACTIVE_TAB = 'activeTab';
const kPERMISSION_TABS       = 'tabs';
const kPERMISSION_COOKIES    = 'cookies';
const kPERMISSION_INCOGNITO  = 'incognito'; // only for internal use
const kPERMISSIONS_ALL = new Set([
  kPERMISSION_TABS,
  kPERMISSION_COOKIES,
  kPERMISSION_INCOGNITO
]);

const mAddons = new Map();
let mScrollLockedBy    = {};
let mGroupingBlockedBy = {};

const mIsBackend  = location.href.startsWith(browser.runtime.getURL('background/background.html'));
const mIsFrontend = location.href.startsWith(browser.runtime.getURL('sidebar/sidebar.html'));

export class TreeItem {
  constructor(tab, options = {}) {
    this.tab          = tab;
    this.isContextTab = !!options.isContextTab;
    this.interval     = options.interval || 0;
    this.cache        = options.cache || {};
    if (!this.cache.tabs)
      this.cache.tabs = {};
    if (!this.cache.effectiveFavIconUrls)
      this.cache.effectiveFavIconUrls = {};

    this.exportTab = this.exportTab.bind(this);
  }

  async exportFor(addonId) {
    if (addonId == browser.runtime.id)
      return this.exportTab(this.tab, kPERMISSIONS_ALL);

    const permissions = new Set(getGrantedPermissionsForAddon(addonId));
    if (configs.incognitoAllowedExternalAddons.includes(addonId))
      permissions.add(kPERMISSION_INCOGNITO);
    const cacheKey = Array.from(permissions).sort().join(',');
    return this.exportTab(this.tab, permissions, cacheKey);
  }

  async exportTab(sourceTab, permissions, commonCacheKey = '') {
    if (!sourceTab ||
        (sourceTab.incognito &&
         !permissions.has(kPERMISSION_INCOGNITO)))
      return null;
    const cacheKey = `${sourceTab.id}:${commonCacheKey}`;
    // The promise is cached here instead of the result,
    // to avoid cache miss caused by concurrent call.
    if (!(cacheKey in this.cache.tabs)) {
      this.cache.tabs[cacheKey] = this._exportTab(sourceTab, permissions, commonCacheKey);
    }
    return await this.cache.tabs[cacheKey];
  }

  async _exportTab(sourceTab, permissions, commonCacheKey = '') {
    const [effectiveFavIconUrl, children] = await Promise.all([
      (sourceTab.id in this.cache.effectiveFavIconUrls) ?
        this.cache.effectiveFavIconUrls[sourceTab.id] :
        TabFavIconHelper.getLastEffectiveFavIconURL(sourceTab).catch(ApiTabs.handleMissingTabError),
      doProgressively(
        sourceTab.$TST.children,
        child => this.exportTab(child, permissions, commonCacheKey),
        this.interval
      )
    ]);

    if (!(sourceTab.id in this.cache.effectiveFavIconUrls))
      this.cache.effectiveFavIconUrls[sourceTab.id] = effectiveFavIconUrl;

    const tabStates = sourceTab.$TST.states;
    const exportedTab = {
      states:         Constants.kTAB_SAFE_STATES_ARRAY.filter(state => tabStates.has(state)),
      indent:         parseInt(sourceTab.$TST.getAttribute(Constants.kLEVEL) || 0),
      ancestorTabIds: sourceTab.$TST.ancestorIds,
      children,
      bundledTabId:   sourceTab.$TST.bundledTabId
    };

    let allowedProperties = [
      // basic tabs.Tab properties
      'active',
      'attention',
      'audible',
      'autoDiscardable',
      'discarded',
      'height',
      'hidden',
      'highlighted',
      'id',
      'incognito',
      'index',
      'isArticle',
      'isInReaderMode',
      'lastAccessed',
      'mutedInfo',
      'openerTabId',
      'pinned',
      'selected',
      'sessionId',
      'sharingState',
      'status',
      'successorId',
      'width',
      'windowId'
    ];
    if (permissions.has(kPERMISSION_TABS) ||
        (permissions.has(kPERMISSION_ACTIVE_TAB) &&
         (sourceTab.active ||
          (sourceTab == this.tab && this.isContextTab)))) {
      allowedProperties = allowedProperties.concat([
        // specially allowed with "tabs" or "activeTab" permission
        'favIconUrl',
        'title',
        'url'
      ]);
      exportedTab.effectiveFavIconUrl = effectiveFavIconUrl;
    }
    if (permissions.has(kPERMISSION_COOKIES)) {
      allowedProperties.push('cookieStoreId');
      exportedTab.cookieStoreName = sourceTab.$TST.cookieStoreName;
    }

    allowedProperties = new Set(allowedProperties);
    for (const key of allowedProperties) {
      if (key in sourceTab)
        exportedTab[key] = sourceTab[key];
    }

    return exportedTab;
  }
}

export function getAddon(id) {
  return mAddons.get(id);
}

export function getGrantedPermissionsForAddon(id) {
  const addon = getAddon(id);
  return addon && addon.grantedPermissions || new Set();
}

function registerAddon(id, addon) {
  log('addon is registered: ', id, addon);

  // inherit properties from last effective value
  const oldAddon = getAddon(id);
  if (oldAddon) {
    if (!('listeningTypes' in addon) && 'listeningTypes' in oldAddon)
      addon.listeningTypes = oldAddon.listeningTypes;
    if (!('style' in addon) && 'style' in oldAddon)
      addon.style = oldAddon.style;
  }

  if (!addon.listeningTypes) {
    // for backward compatibility, send all message types available on TST 2.4.16 by default.
    addon.listeningTypes = [
      kNOTIFY_READY,
      kNOTIFY_SHUTDOWN,
      kNOTIFY_TAB_CLICKED,
      kNOTIFY_TAB_MOUSEDOWN,
      kNOTIFY_TAB_MOUSEUP,
      kNOTIFY_NEW_TAB_BUTTON_CLICKED,
      kNOTIFY_NEW_TAB_BUTTON_MOUSEDOWN,
      kNOTIFY_NEW_TAB_BUTTON_MOUSEUP,
      kNOTIFY_TABBAR_CLICKED,
      kNOTIFY_TABBAR_MOUSEDOWN,
      kNOTIFY_TABBAR_MOUSEUP
    ];
  }

  let requestedPermissions = addon.permissions || [];
  if (!Array.isArray(requestedPermissions))
    requestedPermissions = [requestedPermissions];
  addon.requestedPermissions = new Set(requestedPermissions);
  const grantedPermissions = configs.grantedExternalAddonPermissions[id] || [];
  addon.grantedPermissions = new Set(grantedPermissions);

  if (mIsBackend &&
      !addon.bypassPermissionCheck &&
      addon.requestedPermissions.size > 0 &&
      addon.grantedPermissions.size != addon.requestedPermissions.size)
    notifyPermissionRequest(addon, addon.requestedPermissions);

  addon.id = id;
  addon.lastRegistered = Date.now();
  mAddons.set(id, addon);

  onRegistered.dispatch(addon);
}

const mPermissionNotificationForAddon = new Map();

async function notifyPermissionRequest(addon, requestedPermissions) {
  log('notifyPermissionRequest ', addon, requestedPermissions);

  if (mPermissionNotificationForAddon.has(addon.id))
    return;

  mPermissionNotificationForAddon.set(addon.id, -1);
  const id = await browser.notifications.create({
    type:    'basic',
    iconUrl: Constants.kNOTIFICATION_DEFAULT_ICON,
    title:   browser.i18n.getMessage('api_requestedPermissions_title'),
    message: browser.i18n.getMessage(`api_requestedPermissions_message${isLinux() ? '_linux' : ''}`, [
      addon.name || addon.title || addon.id,
      Array.from(requestedPermissions, permission => {
        if (permission == kPERMISSION_INCOGNITO)
          return null;
        try {
          return browser.i18n.getMessage(`api_requestedPermissions_type_${permission}`) || permission;
        }
        catch(_error) {
          return permission;
        }
      }).filter(permission => !!permission).join('\n')
    ])
  });
  mPermissionNotificationForAddon.set(addon.id, id);
}

function setPermissions(addon, permisssions) {
  addon.grantedPermissions = permisssions;
  const cachedPermissions = JSON.parse(JSON.stringify(configs.grantedExternalAddonPermissions));
  cachedPermissions[addon.id] = Array.from(addon.grantedPermissions);
  configs.grantedExternalAddonPermissions = cachedPermissions;
  notifyPermissionChanged(addon);
}

function notifyPermissionChanged(addon) {
  const permissions = Array.from(addon.grantedPermissions);
  browser.runtime.sendMessage({
    type: kCOMMAND_BROADCAST_API_PERMISSION_CHANGED,
    id:   addon.id,
    permissions
  });
  if (addon.id == browser.runtime.id)
    return;
  browser.runtime.sendMessage(addon.id, {
    type:                 kNOTIFY_PERMISSIONS_CHANGED,
    grantedPermissions:   permissions.filter(permission => permission.startsWith('!')),
    privateWindowAllowed: configs.incognitoAllowedExternalAddons.includes(addon.id)
  }).catch(ApiTabs.createErrorHandler());
}

function unregisterAddon(id) {
  const addon = getAddon(id);
  log('addon is unregistered: ', id, addon);
  onUnregistered.dispatch(addon);
  mAddons.delete(id);
  delete mScrollLockedBy[id];
  delete mGroupingBlockedBy[id];
}

export function getAddons() {
  return mAddons.entries();
}

const mConnections = new Map();

function onCommonCommand(message, sender) {
  if (!message ||
      typeof message.type != 'string')
    return;

  const addon = getAddon(sender.id);

  switch (message.type) {
    case kSCROLL_LOCK:
      mScrollLockedBy[sender.id] = true;
      if (!addon)
        registerAddon(sender.id, sender);
      return Promise.resolve(true);

    case kSCROLL_UNLOCK:
      delete mScrollLockedBy[sender.id];
      return Promise.resolve(true);

    case kBLOCK_GROUPING:
      mGroupingBlockedBy[sender.id] = true;
      if (!addon)
        registerAddon(sender.id, sender);
      return Promise.resolve(true);

    case kUNBLOCK_GROUPING:
      delete mGroupingBlockedBy[sender.id];
      return Promise.resolve(true);

    case kSET_EXTRA_TAB_CONTENTS:
      if (!addon)
        registerAddon(sender.id, sender);
      break;

    case kSET_EXTRA_NEW_TAB_BUTTON_CONTENTS:
      if (!addon)
        registerAddon(sender.id, sender);
      break;
  }
}


// =======================================================================
// for backend
// =======================================================================

export async function initAsBackend() {
  // We must listen API messages from other addons here beacause:
  //  * Before notification messages are sent to other addons.
  //  * After configs are loaded and TST's background page is almost completely initialized.
  //    (to prevent troubles like breakage of `configs.cachedExternalAddons`, see also:
  //     https://github.com/piroor/treestyletab/issues/2300#issuecomment-498947370 )
  browser.runtime.onMessageExternal.addListener(onBackendCommand);

  const manifest = browser.runtime.getManifest();
  registerAddon(browser.runtime.id, {
    id:         browser.runtime.id,
    internalId: browser.runtime.getURL('').replace(/^moz-extension:\/\/([^\/]+)\/.*$/, '$1'),
    icons:      manifest.icons,
    listeningTypes: [],
    bypassPermissionCheck: true
  });
  browser.runtime.onConnectExternal.addListener(port => {
    if (!configs.APIEnabled)
      return;
    const sender = port.sender;
    mConnections.set(sender.id, port);
    port.onMessage.addListener(message => {
      onMessageExternal.dispatch(message, sender);
      SidebarConnection.sendMessage({
        type: 'external',
        message,
        sender
      });
    });
    port.onDisconnect.addListener(_message => {
      mConnections.delete(sender.id);
      onBackendCommand({
        type: kUNREGISTER_SELF,
        sender
      }).catch(ApiTabs.createErrorSuppressor());
    });
  });
  const respondedAddons = [];
  const notifiedAddons = {};
  const notifyAddons = configs.knownExternalAddons.concat(configs.cachedExternalAddons);
  log('initAsBackend: notifyAddons = ', notifyAddons);
  await Promise.all(notifyAddons.map(async id => {
    if (id in notifiedAddons)
      return;
    notifiedAddons[id] = true;
    try {
      id = await new Promise((resolve, reject) => {
        let responded = false;
        browser.runtime.sendMessage(id, {
          type: kNOTIFY_READY
        }).then(() => {
          responded = true;
          resolve(id);
        }).catch(ApiTabs.createErrorHandler(reject));
        setTimeout(() => {
          if (!responded)
            reject(new Error(`TSTAPI.initAsBackend: addon ${id} does not respond.`));
        }, 3000);
      });
      if (id)
        respondedAddons.push(id);
    }
    catch(e) {
      console.log(`TSTAPI.initAsBackend: failed to send "ready" message to "${id}":`, e);
    }
  }));
  log('initAsBackend: respondedAddons = ', respondedAddons);
  configs.cachedExternalAddons = respondedAddons;

  onInitialized.dispatch();
}

if (mIsBackend) {
  browser.notifications.onClicked.addListener(notificationId => {
    for (const [addonId, id] of mPermissionNotificationForAddon.entries()) {
      if (id != notificationId)
        continue;
      mPermissionNotificationForAddon.delete(addonId);
      browser.tabs.create({
        url: `moz-extension://${location.host}/options/options.html#externalAddonPermissionsGroup`
      });
      break;
    }
  });

  browser.notifications.onClosed.addListener((notificationId, _byUser) => {
    for (const [addonId, id] of mPermissionNotificationForAddon.entries()) {
      if (id != notificationId)
        continue;
      mPermissionNotificationForAddon.delete(addonId);
      break;
    }
  });

  SidebarConnection.onConnected.addListener((windowId, openCount) => {
    sendMessage({
      type:   kNOTIFY_SIDEBAR_SHOW,
      window: windowId,
      windowId,
      openCount
    });
  });

  SidebarConnection.onDisconnected.addListener((windowId, openCount) => {
    sendMessage({
      type:   kNOTIFY_SIDEBAR_HIDE,
      window: windowId,
      windowId,
      openCount
    });
  });

  /*
  // This mechanism doesn't work actually.
  // See also: https://github.com/piroor/treestyletab/issues/2128#issuecomment-454650407

  const mConnectionsForAddons = new Map();

  browser.runtime.onConnectExternal.addListener(port => {
    const sender = port.sender;
    log('Connected: ', sender.id);

    const connections = mConnectionsForAddons.get(sender.id) || new Set();
    connections.add(port);

    const addon = getAddon(sender.id);
    if (!addon) { // treat as register-self
      const message = {
        id:             sender.id,
        internalId:     sender.url.replace(/^moz-extension:\/\/([^\/]+)\/.*$/, '$1'),
        newlyInstalled: !configs.cachedExternalAddons.includes(sender.id)
      };
      registerAddon(sender.id, message);
      browser.runtime.sendMessage({
        type: kCOMMAND_BROADCAST_API_REGISTERED,
        sender,
        message
      }).catch(ApiTabs.createErrorSuppressor());
      if (message.newlyInstalled)
        configs.cachedExternalAddons = configs.cachedExternalAddons.concat([sender.id]);
    }

    const onMessage = message => {
      onBackendCommand(message, sender);
    };
    port.onMessage.addListener(onMessage);

    const onDisconnected = _message => {
      log('Disconnected: ', sender.id);
      port.onMessage.removeListener(onMessage);
      port.onDisconnect.removeListener(onDisconnected);

      connections.delete(port);
      if (connections.size > 0)
        return;

      setTimeout(() => {
        // if it is not re-registered while 10sec, it may be uninstalled.
        if (getAddon(sender.id))
          return;
        configs.cachedExternalAddons = configs.cachedExternalAddons.filter(id => id != sender.id);
      }, 10 * 1000);
      browser.runtime.sendMessage({
        type: kCOMMAND_BROADCAST_API_UNREGISTERED,
        sender
      }).catch(ApiTabs.createErrorSuppressor());
      unregisterAddon(sender.id);
      mConnectionsForAddons.delete(sender.id);
    }
    port.onDisconnect.addListener(onDisconnected);
  });
  */

  browser.runtime.onMessage.addListener((message, _sender) => {
    if (!message ||
        typeof message.type != 'string')
      return;

    switch (message.type) {
      case kCOMMAND_REQUEST_INITIALIZE:
        return Promise.resolve({
          addons:         exportAddons(),
          scrollLocked:   mScrollLockedBy,
          groupingLocked: mGroupingBlockedBy
        });

      case kCOMMAND_REQUEST_CONTROL_STATE:
        return Promise.resolve({
          scrollLocked:   mScrollLockedBy,
          groupingLocked: mGroupingBlockedBy
        });

      case kCOMMAND_GET_ADDONS: {
        const addons = [];
        for (const [id, addon] of mAddons.entries()) {
          addons.push({
            id,
            label:              addon.name || addon.title || addon.id,
            permissions:        Array.from(addon.requestedPermissions),
            permissionsGranted: Array.from(addon.requestedPermissions).join(',') == Array.from(addon.grantedPermissions).join(',')
          });
        }
        return Promise.resolve(addons);
      }; break;

      case kCOMMAND_SET_API_PERMISSION:
        setPermissions(getAddon(message.id), new Set(message.permissions));
        break;

      case kCOMMAND_NOTIFY_PERMISSION_CHANGED:
        notifyPermissionChanged(getAddon(message.id));
        break;

      case kCOMMAND_UNREGISTER_ADDON:
        unregisterAddon(message.id);
        break;
    }
  });
}

const mPromisedOnBeforeUnload = new Promise((resolve, _reject) => {
  // If this promise doesn't do anything then there seems to be a timeout so it only works if TST is disabled within about 10 seconds after this promise is used as a response to a message. After that it will not throw an error for the waiting extension.
  // If we use the following then the returned promise will be rejected when TST is disabled even for longer times:
  window.addEventListener('beforeunload', () => resolve());
});

const mWaitingShutdownMessages = new Map();

function onBackendCommand(message, sender) {
  if (!message ||
      typeof message != 'object' ||
      typeof message.type != 'string')
    return;

  const results = onMessageExternal.dispatch(message, sender);
  const firstPromise = results.find(result => result instanceof Promise);
  if (firstPromise)
    return firstPromise;

  switch (message.type) {
    case kPING:
      return Promise.resolve(true);

    case kREGISTER_SELF:
      return (async () => {
        message.internalId = sender.url.replace(/^moz-extension:\/\/([^\/]+)\/.*$/, '$1');
        message.id = sender.id;
        message.subPanel = message.subPanel || message.subpanel || null;
        if (message.subPanel) {
          const url = typeof message.subPanel.url === 'string' && new URL(message.subPanel.url, new URL('/', sender.url));
          if (!url || url.hostname !== message.internalId) {
            console.error(`"subPanel.url" must refer to a page packed in the registering extension.`);
            message.subPanel.url = 'about:blank?error=invalid-origin'
          } else
            message.subPanel.url = url.href;
        }
        message.newlyInstalled = !configs.cachedExternalAddons.includes(sender.id);
        registerAddon(sender.id, message);
        browser.runtime.sendMessage({
          type:    kCOMMAND_BROADCAST_API_REGISTERED,
          sender:  sender,
          message: message
        }).catch(ApiTabs.createErrorSuppressor());
        if (message.newlyInstalled)
          configs.cachedExternalAddons = configs.cachedExternalAddons.concat([sender.id]);
        if (message.listeningTypes &&
            message.listeningTypes.includes(kWAIT_FOR_SHUTDOWN) &&
            !mWaitingShutdownMessages.has(sender.id)) {
          const onShutdown = () => {
            const storedShutdown = mWaitingShutdownMessages.get(sender.id);
            // eslint-disable-next-line no-use-before-define
            if (storedShutdown && storedShutdown !== promisedShutdown)
              return; // it is obsolete

            const addon          = getAddon(sender.id);
            const lastRegistered = addon && addon.lastRegistered;
            setTimeout(() => {
              // if it is re-registered immediately, it was updated or reloaded.
              const addon = getAddon(sender.id);
              if (addon &&
                  addon.lastRegistered != lastRegistered)
                return;
              // otherwise it is uninstalled.
              browser.runtime.sendMessage({
                type: kCOMMAND_BROADCAST_API_UNREGISTERED,
                sender
              }).catch(ApiTabs.createErrorSuppressor());
              unregisterAddon(sender.id);
              configs.cachedExternalAddons = configs.cachedExternalAddons.filter(id => id != sender.id);
            }, 350);
          };
          const promisedShutdown = (async () => {
            try {
              const shouldUninit = await browser.runtime.sendMessage(sender.id, {
                type: kWAIT_FOR_SHUTDOWN
              });
              if (!shouldUninit)
                return;
            }
            catch (_error) {
              // Extension was disabled.
            }
            finally {
              mWaitingShutdownMessages.delete(sender.id);
            }
            onShutdown();
          })();
          mWaitingShutdownMessages.set(sender.id, promisedShutdown);
          promisedShutdown.catch(onShutdown);
        }
        return {
          grantedPermissions:   Array.from(getGrantedPermissionsForAddon(sender.id)).filter(permission => permission.startsWith('!')),
          privateWindowAllowed: configs.incognitoAllowedExternalAddons.includes(sender.id)
        };
      })();

    case kUNREGISTER_SELF:
      return (async () => {
        browser.runtime.sendMessage({
          type: kCOMMAND_BROADCAST_API_UNREGISTERED,
          sender
        }).catch(ApiTabs.createErrorSuppressor());
        unregisterAddon(sender.id);
        configs.cachedExternalAddons = configs.cachedExternalAddons.filter(id => id != sender.id);
        return true;
      })();

    case kWAIT_FOR_SHUTDOWN:
      return mPromisedOnBeforeUnload;

    default:
      return onCommonCommand(message, sender);
  }
}

function exportAddons() {
  const exported = {};
  for (const [id, addon] of getAddons()) {
    exported[id] = addon;
  }
  return exported;
}

export function isGroupingBlocked() {
  return Object.keys(mGroupingBlockedBy).length > 0;
}


// =======================================================================
// for frontend
// =======================================================================

export async function initAsFrontend() {
  log('initAsFrontend: start');
  let response;
  while (true) {
    response = await browser.runtime.sendMessage({ type: kCOMMAND_REQUEST_INITIALIZE });
    if (response)
      break;
    await wait(10);
  }
  browser.runtime.onMessageExternal.addListener((message, sender) => {
    if (!configs.APIEnabled)
      return;
    if (message &&
        typeof message == 'object' &&
        typeof message.type == 'string') {
      const results = onMessageExternal.dispatch(message, sender);
      log('onMessageExternal: ', message, ' => ', results, 'sender: ', sender);
      const firstPromise = results.find(result => result instanceof Promise);
      if (firstPromise)
        return firstPromise;
    }
    if (configs.incognitoAllowedExternalAddons.includes(sender.id) ||
        !document.documentElement.classList.contains('incognito'))
      return onCommonCommand(message, sender);
  });
  log('initAsFrontend: response = ', response);
  importAddons(response.addons);
  for (const [, addon] of getAddons()) {
    onRegistered.dispatch(addon);
  }
  mScrollLockedBy    = response.scrollLocked;
  mGroupingBlockedBy = response.groupingLocked;

  onInitialized.dispatch();
  log('initAsFrontend: finish');
}

if (mIsFrontend) {
  browser.runtime.onMessage.addListener((message, _sender) => {
    if (!message ||
        typeof message != 'object' ||
        typeof message.type != 'string')
      return;

    switch (message.type) {
      case kCOMMAND_BROADCAST_API_REGISTERED:
        registerAddon(message.sender.id, message.message);
        break;

      case kCOMMAND_BROADCAST_API_UNREGISTERED:
        unregisterAddon(message.sender.id);
        break;

      case kCOMMAND_BROADCAST_API_PERMISSION_CHANGED: {
        const addon = getAddon(message.id);
        addon.grantedPermissions = new Set(message.permissions);
      }; break;
    }
  });
}

function importAddons(addons) {
  if (!addons)
    console.log(new Error('null import'));
  for (const id of Object.keys(mAddons)) {
    unregisterAddon(id);
  }
  for (const [id, addon] of Object.entries(addons)) {
    registerAddon(id, addon);
  }
}

export function isScrollLocked() {
  return Object.keys(mScrollLockedBy).length > 0;
}

export async function notifyScrolled(params = {}) {
  const lockers = Object.keys(mScrollLockedBy);
  const tab     = params.tab;
  const window  = TabsStore.getCurrentWindowId();
  const cache   = {};
  const allTreeItems = Tab.getTabs(window).map(tab => new TreeItem(tab, { cache }));
  const results = await sendMessage({
    type: kNOTIFY_SCROLLED,
    tab:  tab && allTreeItems.find(treeItem => treeItem.tab.id == tab.id),
    tabs: allTreeItems,
    overflow: params.overflow,
    window,
    windowId: window,

    deltaX:       params.event.deltaX,
    deltaY:       params.event.deltaY,
    deltaZ:       params.event.deltaZ,
    deltaMode:    params.event.deltaMode,
    scrollTop:    params.scrollContainer.scrollTop,
    scrollTopMax: params.scrollContainer.scrollTopMax,

    altKey:   params.event.altKey,
    ctrlKey:  params.event.ctrlKey,
    metaKey:  params.event.metaKey,
    shiftKey: params.event.shiftKey,

    clientX:  params.event.clientX,
    clientY:  params.event.clientY
  }, {
    targets: lockers,
    tabProperties: ['tab', 'tabs']
  });
  for (const result of results) {
    if (!result || result.error || result.result === undefined)
      delete mScrollLockedBy[result.id];
  }
}


// =======================================================================
// Common utilities to send notification messages to other addons
// =======================================================================

export async function tryOperationAllowed(type, message = {}, options = {}) {
  if (!hasListenerForMessageType(type, options)) {
    //log(`=> ${type}: no listener, always allowed`);
    return true;
  }
  const results = await sendMessage({ ...message, type }, options).catch(_error => {});
  if (results.flat().some(result => result && result.result)) {
    log(`=> ${type}: canceled by some helper addon`);
    return false;
  }
  log(`=> ${type}: allowed by all helper addons`);
  return true;
}

export function hasListenerForMessageType(type, options = {}) {
  return getListenersForMessageType(type, options).length > 0;
}

export function getListenersForMessageType(type, { targets, except } = {}) {
  targets = targets instanceof Set ? targets : new Set(Array.isArray(targets) ? targets : targets ? [targets] : []);
  except  = except instanceof Set ? except : new Set(Array.isArray(except) ? except : except ? [except] : []);
  const finalTargets = new Set();
  for (const [id, addon] of getAddons()) {
    if (addon.listeningTypes.includes(type) &&
        (targets.size == 0 || targets.has(id)) &&
        !except.has(id))
      finalTargets.add(id);
  }
  //log('getListenersForMessageType ', { type, targets, except, finalTargets, all: mAddons });
  return Array.from(finalTargets, getAddon);
}

export async function sendMessage(message, options = {}) {
  if (!configs.APIEnabled)
    return [];

  const listenerAddons = getListenersForMessageType(message.type, options);
  const tabProperties = options.tabProperties || [];
  log(`sendMessage: sending message for ${message.type}: `, {
    message,
    listenerAddons,
    tabProperties
  });

  const promisedResults = spawnMessages(new Set(listenerAddons.map(addon => addon.id)), {
    message,
    tabProperties
  });
  return Promise.all(promisedResults).then(results => {
    log(`sendMessage: got responses for ${message.type}: `, results);
    return results;
  }).catch(ApiTabs.createErrorHandler());
}

function* spawnMessages(targets, params) {
  const message = params.message;
  const tabProperties = params.tabProperties || [];

  const incognitoParams = { windowId: message.windowId || message.window };
  for (const key of tabProperties) {
    if (!message[key])
      continue;
    if (Array.isArray(message[key]))
      incognitoParams.tab = message[key][0].tab;
    else
      incognitoParams.tab = message[key].tab;
    break;
  }

  const send = async (id) => {
    if (!isSafeAtIncognito(id, incognitoParams))
      return {
        id,
        result: undefined
      };

    const allowedMessage = await sanitizeMessage(message, { id, tabProperties });

    try {
      const result = await browser.runtime.sendMessage(id, allowedMessage);
      return {
        id,
        result
      };
    }
    catch(e) {
      console.log(`Error on sending message to ${id}`, allowedMessage, e);
      if (e &&
          e.message == 'Could not establish connection. Receiving end does not exist.') {
        browser.runtime.sendMessage(id, { type: kNOTIFY_READY }).catch(_error => {
          console.log(`Unregistering missing helper addon ${id}...`);
          unregisterAddon(id);
          if (mIsFrontend)
            browser.runtime.sendMessage({ type: kCOMMAND_UNREGISTER_ADDON, id });
        });
      }
      return {
        id,
        error: e
      };
    }
  };

  for (const id of targets) {
    yield send(id);
  }
}

export function isSafeAtIncognito(addonId, params) {
  if (addonId == browser.runtime.id)
    return true;
  const tab = params.tab;
  const window = params.windowId && TabsStore.windows.get(params.windowId);
  const hasIncognitoInfo = (window && window.incognito) || (tab && tab.incognito);
  return !hasIncognitoInfo || configs.incognitoAllowedExternalAddons.includes(addonId);
}

async function sanitizeMessage(message, params) {
  const addon = getAddon(params.id);
  if (!message ||
      !params.tabProperties ||
      params.tabProperties.length == 0 ||
      addon.bypassPermissionCheck)
    return message;

  const sanitizedProperties = {};
  const tasks = [];
  if (params.tabProperties) {
    for (const name of params.tabProperties) {
      const treeItem = message[name];
      if (!treeItem)
        continue;
      if (Array.isArray(treeItem))
        tasks.push((async treeItems => {
          const tabs = await Promise.all(treeItems.map(treeItem => treeItem.exportFor(addon.id)))
          sanitizedProperties[name] = tabs.filter(tab => !!tab);
        })(treeItem));
      else
        tasks.push((async () => {
          sanitizedProperties[name] = await treeItem.exportFor(addon.id);
        })());
    }
  }
  await Promise.all(tasks);
  return { ...message, ...sanitizedProperties };
}


// =======================================================================
// Common utilities for request-response type API call
// =======================================================================

export async function getTargetTabs(message, sender) {
  await Tab.waitUntilTrackedAll(message.window || message.windowId);
  if (Array.isArray(message.tabs))
    return getTabsFromWrongIds(message.tabs, sender);
  if (Array.isArray(message.tabIds))
    return getTabsFromWrongIds(message.tabIds, sender);
  if (message.window || message.windowId) {
    if (message.tab == '*' ||
        message.tabId == '*' ||
        message.tabs == '*' ||
        message.tabIds == '*')
      return Tab.getAllTabs(message.window || message.windowId, { iterator: true });
    else
      return Tab.getRootTabs(message.window || message.windowId, { iterator: true });
  }
  if (message.tab == '*' ||
      message.tabId == '*' ||
      message.tabs == '*' ||
      message.tabIds == '*') {
    const window = await browser.windows.getLastFocused({
      windowTypes: ['normal']
    }).catch(ApiTabs.createErrorHandler());
    return Tab.getAllTabs(window.id, { iterator: true });
  }
  if (message.tab || message.tabId)
    return getTabsFromWrongIds([message.tab || message.tabId], sender);
  return [];
}

async function getTabsFromWrongIds(ids, sender) {
  const window = await browser.windows.getLastFocused({
    populate: true
  }).catch(ApiTabs.createErrorHandler());
  const activeWindow = TabsStore.windows.get(window.id) || window;
  const tabs = await Promise.all(ids.map(id => getTabFromWrongId({ id, activeWindow, sender }).catch(error => {
    console.error(error);
    return null;
  })));
  log('getTabsFromWrongIds ', ids, ' => ', tabs, 'sender: ', sender);

  return tabs.flat().filter(tab => !!tab);
}

async function getTabFromWrongId({ id, activeWindow, sender }) {
  if (id && typeof id == 'object' && typeof id.id == 'number') // tabs.Tab
    id = id.id;
  let query   = String(id).toLowerCase();
  let baseTab = Tab.getActiveTab(activeWindow.id);
  const nonActiveTabMatched = query.match(/^([^-]+)-of-(.+)$/i);
  if (nonActiveTabMatched) {
    query = nonActiveTabMatched[1];
    id    = nonActiveTabMatched[2];
    if (/^\d+$/.test(id))
      id = parseInt(id);
    baseTab = Tab.get(id) || Tab.getByUniqueId(id);
  }
  switch (query) {
    case 'active':
    case 'current':
      return baseTab;

    case 'parent':
      return baseTab.$TST.parent;

    case 'root':
      return baseTab.$TST.rootTab;

    case 'next':
      return baseTab.$TST.nextTab;
    case 'nextcyclic':
      return baseTab.$TST.nextTab || Tab.getFirstTab(activeWindow.id);

    case 'previous':
    case 'prev':
      return baseTab.$TST.previousTab;
    case 'previouscyclic':
    case 'prevcyclic':
      return baseTab.$TST.previousTab || Tab.getLastTab(activeWindow.id);

    case 'nextsibling':
      return baseTab.$TST.nextSiblingTab;
    case 'nextsiblingcyclic': {
      const nextSibling = baseTab.$TST.nextSiblingTab;
      if (nextSibling)
        return nextSibling;
      const parent = baseTab.$TST.parent;
      if (parent)
        return parent.$TST.firstChild;
      return Tab.getFirstTab(activeWindow.id);
    }

    case 'previoussibling':
    case 'prevsibling':
      return baseTab.$TST.previousSiblingTab;
    case 'previoussiblingcyclic':
    case 'prevsiblingcyclic': {
      const previousSiblingTab = baseTab.$TST.previousSiblingTab;
      if (previousSiblingTab)
        return previousSiblingTab;
      const parent = baseTab.$TST.parent;
      if (parent)
        return parent.$TST.lastChild;
      return Tab.getLastRootTab(activeWindow.id);
    }

    case 'nextvisible':
      return baseTab.$TST.nearestVisibleFollowingTab;
    case 'nextvisiblecyclic':
      return baseTab.$TST.nearestVisibleFollowingTab || Tab.getFirstVisibleTab(activeWindow.id);

    case 'previousvisible':
    case 'prevvisible':
      return baseTab.$TST.nearestVisiblePrecedingTab;
    case 'previousvisiblecyclic':
    case 'prevvisiblecyclic':
      return baseTab.$TST.nearestVisiblePrecedingTab || Tab.getLastVisibleTab(activeWindow.id);

    case 'lastdescendant':
      return baseTab.$TST.lastDescendant;

    case 'sendertab':
      return sender.tab && Tab.get(sender.tab.id) || null;
    case 'highlighted':
    case 'multiselected':
      return Tab.getHighlightedTabs(activeWindow.id);
    default:
      return Tab.get(id) || Tab.getByUniqueId(id);
  }
}

export async function doProgressively(tabs, task, interval) {
  interval = Math.max(0, interval);
  let lastStartAt = Date.now();
  const results = [];
  for (const tab of tabs) {
    results.push(task(tab));
    if (interval && (Date.now() - lastStartAt >= interval)) {
      await wait(50);
      lastStartAt = Date.now();
    }
  }
  return Promise.all(results);
}

export function formatResult(results, originalMessage) {
  if (Array.isArray(originalMessage.tabs) ||
      originalMessage.tab == '*' ||
      originalMessage.tabs == '*')
    return results;
  if (originalMessage.tab)
    return results[0];
  return results;
}

export async function formatTabResult(treeItems, originalMessage, senderId) {
  if (Array.isArray(originalMessage.tabs) ||
      originalMessage.tab == '*' ||
      originalMessage.tabs == '*')
    return Promise.all(treeItems.map(treeItem => treeItem.exportFor(senderId)))
      .then(tabs => tabs.filter(tab => !!tab));
  return treeItems.length > 0 ? treeItems[0].exportFor(senderId) : null ;
}

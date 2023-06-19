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
 * Portions created by the Initial Developer are Copyright (C) 2010-2022
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): YUKI "Piro" Hiroshi <piro.outsider.reflex@gmail.com>
 *                 Infocatcher <https://github.com/Infocatcher>
 *                 Tetsuharu OHZEKI <https://github.com/saneyuki>
 *
 * ***** END LICENSE BLOCK ******/
'use strict';


import RichConfirm from '/extlib/RichConfirm.js';

import {
  log as internalLogger,
  wait,
  mapAndFilter,
  configs,
  shouldApplyAnimation,
  sha1sum,
  isMacOS,
  isLinux,
} from '/common/common.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as BackgroundConnection from './background-connection.js';
import * as Constants from '/common/constants.js';
import * as EventUtils from './event-utils.js';
import * as Scroll from './scroll.js';
import * as SidebarTabs from './sidebar-tabs.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TreeBehavior from '/common/tree-behavior.js';
import * as TSTAPI from '/common/tst-api.js';

import Tab from '/common/Tab.js';

function log(...args) {
  internalLogger('sidebar/drag-and-drop', ...args);
}


const kTREE_DROP_TYPE   = 'application/x-treestyletab-tree';
const kTYPE_X_MOZ_URL   = 'text/x-moz-url';
const kTYPE_URI_LIST    = 'text/uri-list';
const kTYPE_MOZ_TEXT_INTERNAL = 'text/x-moz-text-internal';
const kBOOKMARK_FOLDER  = 'x-moz-place:';
const kTYPE_ADDON_DRAG_DATA = `application/x-treestyletab-drag-data;provider=${browser.runtime.id}&id=`;

const kDROP_BEFORE  = 'before';
const kDROP_ON_SELF = 'self';
const kDROP_AFTER   = 'after';
const kDROP_IMPOSSIBLE = 'impossible';

const kDROP_POSITION = 'data-drop-position';

const kTABBAR_STATE_TAB_DRAGGING  = 'tab-dragging';
const kTABBAR_STATE_LINK_DRAGGING = 'link-dragging';

let mLongHoverExpandedTabs = [];
let mLongHoverTimer;
let mLongHoverTimerNext;

let mDraggingOnSelfWindow = false;
let mDraggingOnDraggedTabs = false;

let mCapturingForDragging = false;
let mReadyToCaptureMouseEvents = false;
let mLastDragEnteredTarget = null;
let mLastDropPosition      = null;
let mLastDragEventCoordinates = null;
let mDragTargetIsClosebox  = false;
let mCurrentDragData       = null;

let mDragBehaviorNotification;
let mInstanceId;

export function init() {
  document.addEventListener('dragstart', onDragStart);
  document.addEventListener('dragover', onDragOver);
  document.addEventListener('dragenter', onDragEnter);
  document.addEventListener('dragleave', onDragLeave);
  document.addEventListener('dragend', onDragEnd);
  document.addEventListener('drop', onDrop);

  browser.runtime.onMessage.addListener(onMessage);

  mDragBehaviorNotification = document.getElementById('tab-drag-notification');

  browser.runtime.sendMessage({ type: Constants.kCOMMAND_GET_INSTANCE_ID }).then(id => mInstanceId = id);
}


export function isCapturingForDragging() {
  return mCapturingForDragging;
}

export function endMultiDrag(tab, coordinates) {
  const treeItem = tab && new TSTAPI.TreeItem(tab);
  if (mCapturingForDragging) {
    window.removeEventListener('mouseover', onTSTAPIDragEnter, { capture: true });
    window.removeEventListener('mouseout',  onTSTAPIDragExit, { capture: true });
    document.releaseCapture();

    TSTAPI.sendMessage({
      type:    TSTAPI.kNOTIFY_TAB_DRAGEND,
      tab:     treeItem,
      window:  tab && tab.windowId,
      windowId: tab && tab.windowId,
      clientX: coordinates.clientX,
      clientY: coordinates.clientY
    }, { tabProperties: ['tab'] }).catch(_error => {});

    mLastDragEnteredTarget = null;
  }
  else if (mReadyToCaptureMouseEvents) {
    TSTAPI.sendMessage({
      type:    TSTAPI.kNOTIFY_TAB_DRAGCANCEL,
      tab:     treeItem,
      window:  tab && tab.windowId,
      windowId: tab && tab.windowId,
      clientX: coordinates.clientX,
      clientY: coordinates.clientY
    }, { tabProperties: ['tab'] }).catch(_error => {});
  }
  mCapturingForDragging = false;
  mReadyToCaptureMouseEvents = false;
}

function setDragData(dragData) {
  return mCurrentDragData = dragData;
}


/* helpers */

function getDragDataFromOneTab(tab, options = {}) {
  if (!tab)
    return {
      tab:      null,
      tabs:     [],
      windowId: null,
      instanceId: mInstanceId,
      structure:  []
    };
  const tabs = getDraggedTabsFromOneTab(tab, options);
  return {
    tab,
    tabs,
    structure:  TreeBehavior.getTreeStructureFromTabs(tabs),
    windowId:   tab.windowId,
    instanceId: mInstanceId
  };
}

function getDraggedTabsFromOneTab(tab, { asTree } = {}) {
  if (tab.$TST.multiselected)
    return Tab.getSelectedTabs(tab.windowId);
  if (asTree)
    return [tab].concat(tab.$TST.descendants);
  return [tab];
}

function sanitizeDragData(dragData) {
  return {
    tab:      dragData.tab && dragData.tab.$TST.sanitized,
    tabs:     dragData.tabs.map(tab => tab && tab.$TST.sanitized),
    structure:  dragData.structure,
    windowId:   dragData.windowId,
    instanceId: dragData.instanceId,
    behavior:   dragData.behavior,
    individualOnOutside: dragData.individualOnOutside
  };
}

function getDropAction(event) {
  const dragOverTab = EventUtils.getTabFromEvent(event);
  const targetTab   = dragOverTab || EventUtils.getTabFromTabbarEvent(event);
  const info = {
    dragOverTab,
    targetTab,
    dropPosition:  null,
    action:        null,
    parent:        null,
    insertBefore:  null,
    insertAfter:   null,
    defineGetter(name, getter) {
      delete this[name];
      Object.defineProperty(this, name, {
        get() {
          delete this[name];
          return this[name] = getter.call(this);
        },
        configurable: true,
        enumerable:   true
      });
    }
  };
  info.defineGetter('dragData', () => {
    const dragData = event.dataTransfer.getData(kTREE_DROP_TYPE);
    return (dragData && JSON.parse(dragData)) || mCurrentDragData;
  });
  info.defineGetter('draggedTab', () => {
    const dragData = info.dragData;
    if (dragData && dragData.instanceId != mInstanceId)
      return null;
    const tab      = dragData && dragData.tab;
    return tab && Tab.get(tab.id) || tab;
  });
  info.defineGetter('draggedTabs', () => {
    const dragData = info.dragData;
    if (dragData && dragData.instanceId != mInstanceId)
      return [];
    const tabIds = dragData && dragData.tabs;
    return !tabIds ? [] : mapAndFilter(tabIds, id => Tab.get(id) || undefined);
  });
  info.defineGetter('draggedTabIds', () => {
    return info.draggedTabs.map(tab => tab.id);
  });
  info.defineGetter('firstTargetTab', () => {
    return Tab.getFirstNormalTab(TabsStore.getCurrentWindowId()) || Tab.getFirstTab(TabsStore.getCurrentWindowId());
  });
  info.defineGetter('lastTargetTab', () => {
    return Tab.getLastTab(TabsStore.getCurrentWindowId());
  });
  info.defineGetter('canDrop', () => {
    if (info.dropPosition == kDROP_IMPOSSIBLE) {
      log('canDrop:undroppable: dropPosition == kDROP_IMPOSSIBLE');
      return false;
    }

    const draggedTab = info.dragData && info.dragData.tab;
    const isPrivateBrowsingTabDragged = draggedTab && draggedTab.incognito;
    const isPrivateBrowsingDropTarget = (info.dragOverTab || Tab.getFirstTab(TabsStore.getCurrentWindowId())).incognito;
    if (draggedTab &&
        isPrivateBrowsingTabDragged != isPrivateBrowsingDropTarget) {
      log('canDrop:undroppable: mispatched incognito status');
      return false;
    }
    else if (info.draggedTab) {
      if (info.action & Constants.kACTION_ATTACH) {
        if (info.draggedTab.windowId != TabsStore.getCurrentWindowId()) {
          return true;
        }
        if (info.parent &&
            info.parent.id == info.draggedTab.id) {
          log('canDrop:undroppable: drop on child');
          return false;
        }
        if (info.dragOverTab) {
          if (info.draggedTabIds.includes(info.dragOverTab.id)) {
            log('canDrop:undroppable: on self');
            return false;
          }
          const ancestors = info.dragOverTab.$TST.ancestors;
          /* too many function call in this way, so I use alternative way for better performance.
          return !info.draggedTabIds.includes(info.dragOverTab.id) &&
                   Tab.collectRootTabs(info.draggedTabs).every(rootTab =>
                     !ancestors.includes(rootTab)
                   );
          */
          for (const tab of info.draggedTabs.slice(0).reverse()) {
            const parent = tab.$TST.parent;
            if (!parent && ancestors.includes(parent)) {
              log('canDrop:undroppable: on descendant');
              return false;
            }
          }
          return true;
        }
      }
    }

    if (info.dragOverTab &&
        (info.dragOverTab.hidden ||
         (info.dragOverTab.$TST.collapsed &&
          info.dropPosition != kDROP_AFTER))) {
      log('canDrop:undroppable: on hidden tab');
      return false;
    }

    return true;
  });
  info.defineGetter('EventUtils.isCopyAction', () => EventUtils.isCopyAction(event));
  info.defineGetter('dropEffect', () => getDropEffectFromDropAction(info));

  if (!targetTab) {
    //log('dragging on non-tab element');
    const action = Constants.kACTION_MOVE | Constants.kACTION_DETACH;
    if (event.clientY < info.firstTargetTab.$TST.element.getBoundingClientRect().top) {
      //log('dragging above the first tab');
      info.targetTab    = info.insertBefore = info.firstTargetTab;
      info.dropPosition = kDROP_BEFORE;
      info.action       = action;
      if (info.draggedTab &&
          !info.draggedTab.pinned &&
          info.targetTab.pinned) {
        log('undroppable: above the first tab');
        info.dropPosition = kDROP_IMPOSSIBLE;
      }
    }
    else if (event.clientY > info.lastTargetTab.$TST.element.getBoundingClientRect().bottom) {
      //log('dragging below the last tab');
      info.targetTab    = info.insertAfter = info.lastTargetTab;
      info.dropPosition = kDROP_AFTER;
      info.action       = action;
      if (info.draggedTab &&
          info.draggedTab.pinned &&
          !info.targetTab.pinned) {
        log('undroppable: below the last tab');
        info.dropPosition = kDROP_IMPOSSIBLE;
      }
    }
    return info;
  }

  /**
   * Basically, tabs should have three areas for dropping of items:
   * [start][center][end], but, pinned tabs couldn't have its tree.
   * So, if a tab is dragged and the target tab is pinned, then, we
   * have to ignore the [center] area.
   */
  const onFaviconizedTab    = targetTab.pinned && configs.faviconizePinnedTabs;
  const dropAreasCount      = (info.draggedTab && onFaviconizedTab) ? 2 : 3 ;
  const targetTabRect       = targetTab.$TST.element.getBoundingClientRect();
  const targetTabCoordinate = onFaviconizedTab ? targetTabRect.left : targetTabRect.top ;
  const targetTabSize       = onFaviconizedTab ? targetTabRect.width : targetTabRect.height ;
  let beforeOrAfterDropAreaSize;
  if (dropAreasCount == 2) {
    beforeOrAfterDropAreaSize = Math.round(targetTabSize / dropAreasCount);
  }
  else { // enlarge the area to dop something on the tab itself
    beforeOrAfterDropAreaSize = Math.round(targetTabSize / 4);
  }
  const eventCoordinate = onFaviconizedTab ? event.clientX : event.clientY;
  //log('coordinates: ', {
  //  event: eventCoordinate,
  //  targetTab: targetTabCoordinate,
  //  area: beforeOrAfterDropAreaSize
  //});
  if (eventCoordinate < targetTabCoordinate + beforeOrAfterDropAreaSize) {
    info.dropPosition = kDROP_BEFORE;
    info.insertBefore = info.firstTargetTab;
  }
  else if (dropAreasCount == 2 ||
           eventCoordinate > targetTabCoordinate + targetTabSize - beforeOrAfterDropAreaSize) {
    info.dropPosition = kDROP_AFTER;
    info.insertAfter  = info.lastTargetTab;
  }
  else {
    info.dropPosition = kDROP_ON_SELF;
  }

  switch (info.dropPosition) {
    case kDROP_ON_SELF: {
      log('drop position = on ', info.targetTab.id);
      const insertAt = configs.insertDroppedTabsAt == Constants.kINSERT_INHERIT ? configs.insertNewChildAt : configs.insertDroppedTabsAt;
      info.action       = Constants.kACTION_ATTACH;
      info.parent       = targetTab;
      info.insertBefore = insertAt == Constants.kINSERT_TOP ?
        (targetTab && targetTab.$TST.firstChild || targetTab.$TST.unsafeNextTab /* instead of nearestVisibleFollowingTab, to avoid placing the tab after hidden tabs (too far from the target) */) :
        (targetTab.$TST.nextSiblingTab || targetTab.$TST.unsafeNearestFollowingForeignerTab /* instead of nearestFollowingForeignerTab, to avoid placing the tab after hidden tabs (too far from the target) */);
      info.insertAfter  = insertAt == Constants.kINSERT_TOP ?
        targetTab :
        (targetTab.$TST.lastDescendant || targetTab);
      if (info.draggedTab &&
          info.draggedTab.pinned != targetTab.pinned)
        info.dropPosition = kDROP_IMPOSSIBLE;
      if (info.draggedTab &&
          info.insertBefore == info.draggedTab) // failsafe
        info.insertBefore = insertAt == Constants.kINSERT_TOP ?
          info.draggedTab.$TST.unsafeNextTab :
          (info.draggedTab.$TST.nextSiblingTab ||
           info.draggedTab.$TST.unsafeNearestFollowingForeignerTab);
      if (configs.debug)
        log(' calculated info: ', info);
    }; break;

    case kDROP_BEFORE: {
      log('drop position = before ', info.targetTab.id);
      const referenceTabs = TreeBehavior.calculateReferenceTabsFromInsertionPosition(info.draggedTab, {
        context:      Constants.kINSERTION_CONTEXT_MOVED,
        insertBefore: targetTab
      });
      if (referenceTabs.parent)
        info.parent = referenceTabs.parent;
      if (referenceTabs.insertBefore)
        info.insertBefore = referenceTabs.insertBefore;
      if (referenceTabs.insertAfter)
        info.insertAfter = referenceTabs.insertAfter;
      info.action = Constants.kACTION_MOVE | (info.parent ? Constants.kACTION_ATTACH : Constants.kACTION_DETACH );
      //if (info.insertBefore)
      //  log('insertBefore = ', dumpTab(info.insertBefore));
      if (info.draggedTab &&
          ((info.draggedTab.pinned &&
            targetTab.$TST.followsUnpinnedTab) ||
           (!info.draggedTab.pinned &&
            targetTab.pinned)))
        info.dropPosition = kDROP_IMPOSSIBLE;
      if (configs.debug)
        log(' calculated info: ', info);
    }; break;

    case kDROP_AFTER: {
      log('drop position = after ', info.targetTab.id);
      const referenceTabs = TreeBehavior.calculateReferenceTabsFromInsertionPosition(info.draggedTab, {
        insertAfter: targetTab
      });
      if (referenceTabs.parent)
        info.parent = referenceTabs.parent;
      if (referenceTabs.insertBefore)
        info.insertBefore = referenceTabs.insertBefore;
      if (referenceTabs.insertAfter)
        info.insertAfter = referenceTabs.insertAfter;
      info.action = Constants.kACTION_MOVE | (info.parent ? Constants.kACTION_ATTACH : Constants.kACTION_DETACH );
      if (info.insertBefore) {
        /* strategy
             +-----------------------------------------------------
             |[TARGET   ]
             |     <= attach dragged tab to the parent of the target as its next sibling
             |  [DRAGGED]
             +-----------------------------------------------------
        */
        if (info.draggedTab &&
            info.draggedTab.$TST &&
            info.draggedTab.$TST.nearestVisibleFollowingTab &&
            info.draggedTab.$TST.nearestVisibleFollowingTab.id == info.insertBefore.id) {
          log('special case: promote tab');
          info.action      = Constants.kACTION_MOVE | Constants.kACTION_ATTACH;
          info.parent      = targetTab.$TST.parent;
          let insertBefore = targetTab.$TST.nextSiblingTab;
          let ancestor     = info.parent;
          while (ancestor && !insertBefore) {
            insertBefore = ancestor.$TST.nextSiblingTab;
            ancestor     = ancestor.$TST.parent;
          }
          info.insertBefore = insertBefore;
          info.insertAfter  = targetTab.$TST.lastDescendant;
        }
      }
      if (info.draggedTab &&
          ((info.draggedTab.pinned &&
            !targetTab.pinned) ||
           (!info.draggedTab.pinned &&
            targetTab.$TST.precedesPinnedTab)))
        info.dropPosition = kDROP_IMPOSSIBLE;
      if (configs.debug)
        log(' calculated info: ', info);
    }; break;
  }

  return info;
}
function getDropEffectFromDropAction(actionInfo) {
  if (!actionInfo.canDrop)
    return 'none';
  if (actionInfo.dragData &&
      actionInfo.dragData.instanceId != mInstanceId)
    return 'copy';
  if (!actionInfo.draggedTab)
    return 'link';
  if (actionInfo.isCopyAction)
    return 'copy';
  return 'move';
}

export function clearDropPosition() {
  for (const target of document.querySelectorAll(`[${kDROP_POSITION}]`)) {
    target.removeAttribute(kDROP_POSITION)
  }
  configs.lastDragOverSidebarOwnerWindowId = null;
}

export function clearDraggingTabsState() {
  for (const tab of Tab.getDraggingTabs(TabsStore.getCurrentWindowId(), { iterator: true })) {
    tab.$TST.removeState(Constants.kTAB_STATE_DRAGGING);
    TabsStore.removeDraggingTab(tab);
  }
}

export function clearDraggingState() {
  const window = TabsStore.windows.get(TabsStore.getCurrentWindowId());
  window.classList.remove(kTABBAR_STATE_TAB_DRAGGING);
  document.documentElement.classList.remove(kTABBAR_STATE_TAB_DRAGGING);
  document.documentElement.classList.remove(kTABBAR_STATE_LINK_DRAGGING);
}

function isDraggingAllActiveTabs(tab) {
  const draggingTabsCount = TabsStore.draggingTabsInWindow.get(tab.windowId).size;
  const allTabsCount      = TabsStore.windows.get(tab.windowId).tabs.size;
  return draggingTabsCount == allTabsCount;
}

function collapseAutoExpandedTabsWhileDragging() {
  if (mLongHoverExpandedTabs.length > 0 &&
      configs.autoExpandOnLongHoverRestoreIniitalState) {
    for (const tab of mLongHoverExpandedTabs) {
      BackgroundConnection.sendMessage({
        type:      Constants.kCOMMAND_SET_SUBTREE_COLLAPSED_STATE,
        tabId:     tab.id,
        collapsed: false,
        justNow:   true,
        stack:     configs.debug && new Error().stack
      });
    }
  }
  mLongHoverExpandedTabs = [];
}

async function handleDroppedNonTabItems(event, dropActionInfo) {
  event.stopPropagation();

  const uris = await retrieveURIsFromDragEvent(event);
  // uris.forEach(uRI => {
  //   if (uRI.indexOf(Constants.kURI_BOOKMARK_FOLDER) != 0)
  //     securityCheck(uRI, event);
  // });
  log('handleDroppedNonTabItems: ', uris);

  const dragOverTab = dropActionInfo.dragOverTab;
  if (dragOverTab &&
      dropActionInfo.dropPosition == kDROP_ON_SELF &&
      !dragOverTab.pinned) {
    const behavior = await getDroppedLinksOnTabBehavior();
    if (behavior <= Constants.kDROPLINK_ASK)
      return;
    if (behavior & Constants.kDROPLINK_LOAD) {
      BackgroundConnection.sendMessage({
        type:  Constants.kCOMMAND_ACTIVATE_TAB,
        tabId: dropActionInfo.dragOverTab.id,
        byMouseOperation: true
      });
      BackgroundConnection.sendMessage({
        type:  Constants.kCOMMAND_LOAD_URI,
        uri:   uris.shift(),
        tabId: dropActionInfo.dragOverTab.id
      });
    }
  }
  BackgroundConnection.sendMessage({
    type:           Constants.kCOMMAND_NEW_TABS,
    uris,
    windowId:       TabsStore.getCurrentWindowId(),
    parentId:       dropActionInfo.parent && dropActionInfo.parent.id,
    insertBeforeId: dropActionInfo.insertBefore && dropActionInfo.insertBefore.id,
    insertAfterId:  dropActionInfo.insertAfter && dropActionInfo.insertAfter.id
  });
}

const ACCEPTABLE_DRAG_DATA_TYPES = [
  kTYPE_URI_LIST,
  kTYPE_X_MOZ_URL,
  kTYPE_MOZ_TEXT_INTERNAL,
  'text/plain'
];

async function retrieveURIsFromDragEvent(event) {
  log('retrieveURIsFromDragEvent');
  const dt = event.dataTransfer;
  let urls = [];
  for (const type of ACCEPTABLE_DRAG_DATA_TYPES) {
    const urlData = dt.getData(type);
    if (urlData)
      urls = urls.concat(retrieveURIsFromData(urlData, type));
    if (urls.length)
      break;
  }
  for (const type of dt.types) {
    if (!/^application\/x-treestyletab-drag-data;(.+)$/.test(type))
      continue;
    const params     = RegExp.$1;
    const providerId = /provider=([^;&]+)/.test(params) && RegExp.$1;
    const dataId     = /id=([^;&]+)/.test(params) && RegExp.$1;
    try {
      const dragData = await browser.runtime.sendMessage(providerId, {
        type: 'get-drag-data',
        id:   dataId
      });
      if (!dragData || typeof dragData != 'object')
        break;
      for (const type of ACCEPTABLE_DRAG_DATA_TYPES) {
        const urlData = dragData[type];
        if (urlData)
          urls = urls.concat(retrieveURIsFromData(urlData, type));
        if (urls.length)
          break;
      }
    }
    catch(_error) {
    }
  }
  log(' => retrieved: ', urls);
  urls = urls.filter(uri =>
    uri &&
      uri.length &&
      uri.indexOf(kBOOKMARK_FOLDER) == 0 ||
      !/^\s*(javascript|data):/.test(uri)
  );
  log('  => filtered: ', urls);

  urls = urls.map(fixupURIFromText);
  log('  => fixed: ', urls);

  return urls;
}

function retrieveURIsFromData(data, type) {
  log('retrieveURIsFromData: ', type, data);
  switch (type) {
    case kTYPE_URI_LIST:
      return data
        .replace(/\r/g, '\n')
        .replace(/\n\n+/g, '\n')
        .split('\n')
        .filter(line => {
          return line.charAt(0) != '#';
        });

    case kTYPE_X_MOZ_URL:
      return data
        .trim()
        .replace(/\r/g, '\n')
        .replace(/\n\n+/g, '\n')
        .split('\n')
        .filter((_line, index) => {
          return index % 2 == 0;
        });

    case kTYPE_MOZ_TEXT_INTERNAL:
      return data
        .replace(/\r/g, '\n')
        .replace(/\n\n+/g, '\n')
        .trim()
        .split('\n');

    case 'text/plain':
      return data
        .replace(/\r/g, '\n')
        .replace(/\n\n+/g, '\n')
        .trim()
        .split('\n')
        .map(line => {
          return /^\w+:\/\/.+/.test(line) ? line : `ext+treestyletab:search:${line}`;
        });
  }
  return [];
}

function fixupURIFromText(maybeURI) {
  if (/^(ext\+)?\w+:/.test(maybeURI))
    return maybeURI;

  if (/^([^\.\s]+\.)+[^\.\s]{2}/.test(maybeURI))
    return `http://${maybeURI}`;

  return maybeURI;
}

async function getDroppedLinksOnTabBehavior() {
  let behavior = configs.dropLinksOnTabBehavior;
  if (behavior != Constants.kDROPLINK_ASK)
    return behavior;

  const confirm = new RichConfirm({
    message: browser.i18n.getMessage('dropLinksOnTabBehavior_message'),
    buttons: [
      browser.i18n.getMessage('dropLinksOnTabBehavior_load'),
      browser.i18n.getMessage('dropLinksOnTabBehavior_newtab')
    ],
    checkMessage: browser.i18n.getMessage('dropLinksOnTabBehavior_save')
  });
  const result = await confirm.show();
  switch (result.buttonIndex) {
    case 0:
      behavior = Constants.kDROPLINK_LOAD;
      break;
    case 1:
      behavior = Constants.kDROPLINK_NEWTAB;
      break;
    default:
      return result.buttonIndex;
  }
  if (result.checked)
    configs.dropLinksOnTabBehavior = behavior;
  return behavior;
}


/* DOM event listeners */

let mFinishCanceledDragOperation;
let mCurrentDragDataForExternalsId = null;
let mCurrentDragDataForExternals = null;
let mLastBrowserInfo = null;

function onDragStart(event, options = {}) {
  log('onDragStart: start ', event, options);
  clearDraggingTabsState(); // clear previous state anyway
  if (configs.enableWorkaroundForBug1548949)
    configs.workaroundForBug1548949DroppedTabs = '';

  let draggedTab = options.tab || EventUtils.getTabFromEvent(event);
  let behavior = 'behavior' in options ? options.behavior :
    event.shiftKey ? configs.tabDragBehaviorShift :
      configs.tabDragBehavior;

  if (draggedTab && draggedTab.$TST.subtreeCollapsed)
    behavior |= Constants.kDRAG_BEHAVIOR_ENTIRE_TREE;

  mCurrentDragDataForExternalsId = `${parseInt(Math.random() * 65000)}-${Date.now()}`;
  mCurrentDragDataForExternals = {};

  browser.runtime.getBrowserInfo().then(info => {
    mLastBrowserInfo = info;
  });

  const originalTarget = EventUtils.getElementOriginalTarget(event);
  const extraTabContentsDragData = originalTarget && JSON.parse(originalTarget.dataset && originalTarget.dataset.dragData || 'null');
  log('onDragStart: extraTabContentsDragData = ', extraTabContentsDragData);
  let dataOverridden = false;
  if (extraTabContentsDragData) {
    const dataSet = detectOverrideDragDataSet(extraTabContentsDragData, event);
    log('onDragStart: detected override data set = ', dataSet);
    /*
      expected drag data format:
        Tab:
          { type: 'tab',
            data: { asTree:      (boolean),
                    allowDetach: (boolean, will detach the tab to new window),
                    allowLink:   (boolean, will create link/bookmark from the tab) }}
        other arbitrary types:
          { type:          'text/plain',
            data:          'something text',
            effectAllowed: 'copy' }
          { type:          'text/x-moz-url',
            data:          'http://example.com/\nExample Link',
            effectAllowed: 'copyMove' }
          ...
    */
    let tabIsGiven = false;
    for (const data of dataSet) {
      if (!data)
        continue;
      switch (data.type) {
        case 'tab':
          if (data.data.id) {
            const tab = Tab.get(data.data.id);
            if (tab) {
              tabIsGiven = true;
              draggedTab = tab;
              behavior   = data.data.allowMove === false ? Constants.kDRAG_BEHAVIOR_NONE : Constants.kDRAG_BEHAVIOR_MOVE;
              if (data.data.allowDetach)
                behavior |= Constants.kDRAG_BEHAVIOR_TEAR_OFF;
              if (data.data.allowLink)
                behavior |= Constants.kDRAG_BEHAVIOR_ALLOW_BOOKMARK;
              if (data.data.asTree)
                behavior |= Constants.kDRAG_BEHAVIOR_ENTIRE_TREE;
            }
          }
          break;
        default: {
          const dt = event.dataTransfer;
          dt.effectAllowed = data.effectAllowed || 'copy';
          const type       = String(data.type);
          const stringData = String(data.data);
          dt.setData(type, stringData);
          //*** We need to sanitize drag data from helper addons, because
          //they can have sensitive data...
          //mCurrentDragDataForExternals[type] = stringData;
          dataOverridden = true;
        }; break;
      }
    }
    if (!tabIsGiven && dataOverridden)
      return;
  }

  const allowBookmark = !!(behavior & Constants.kDRAG_BEHAVIOR_ALLOW_BOOKMARK);
  const asTree = !!(behavior & Constants.kDRAG_BEHAVIOR_ENTIRE_TREE);
  const dragData = getDragDataFromOneTab(draggedTab, { asTree });
  dragData.individualOnOutside = dragData.tab && !dragData.tab.$TST.multiselected && !asTree
  dragData.behavior = behavior;
  if (!dragData.tab) {
    log('onDragStart: canceled / no dragged tab from drag data');
    return;
  }
  log('dragData: ', dragData);

  if (!(behavior & Constants.kDRAG_BEHAVIOR_MOVE) &&
      !(behavior & Constants.kDRAG_BEHAVIOR_TEAR_OFF) &&
      !allowBookmark) {
    log('ignore drag action because it can do nothing');
    event.stopPropagation();
    event.preventDefault();
    return;
  }

  const tab       = dragData.tab;
  const mousedown = EventUtils.getLastMousedown(event.button);

  if (mousedown &&
      mousedown.detail.lastInnerScreenY != window.mozInnerScreenY) {
    log('ignore accidental drag from updated visual gap');
    event.stopPropagation();
    event.preventDefault();
    return;
  }

  if (mousedown && mousedown.expired) {
    log('onDragStart: canceled / expired');
    event.stopPropagation();
    event.preventDefault();
    mLastDragEnteredTarget = tab.$TST.element;
    const startOnClosebox = mDragTargetIsClosebox = mousedown.detail.closebox;
    if (startOnClosebox)
      mLastDragEnteredTarget = tab.$TST.element.closeBoxElement;
    const windowId = TabsStore.getCurrentWindowId();
    TSTAPI.sendMessage({
      type:   TSTAPI.kNOTIFY_TAB_DRAGSTART,
      tab:    new TSTAPI.TreeItem(tab),
      window: windowId,
      windowId,
      startOnClosebox
    }, { tabProperties: ['tab'] }).catch(_error => {});
    window.addEventListener('mouseover', onTSTAPIDragEnter, { capture: true });
    window.addEventListener('mouseout',  onTSTAPIDragExit, { capture: true });
    document.body.setCapture(false);
    mCapturingForDragging = true;
    return;
  }

  // dragging on clickable element will be expected to cancel the operation
  if (EventUtils.isEventFiredOnClosebox(options.tab && options.tab.$TST.element || event) ||
      EventUtils.isEventFiredOnClickable(options.tab && options.tab.$TST.element || event)) {
    log('onDragStart: canceled / on undraggable element');
    event.stopPropagation();
    event.preventDefault();
    return;
  }

  EventUtils.cancelHandleMousedown();

  mDraggingOnSelfWindow = true;
  mDraggingOnDraggedTabs = true;
  mLastDropPosition = null;

  const dt = event.dataTransfer;
  dt.effectAllowed = 'copyMove';

  const sanitizedDragData = sanitizeDragData(dragData);
  dt.setData(kTREE_DROP_TYPE, JSON.stringify(sanitizedDragData));

  // Because addon cannot read drag data across private browsing mode,
  // we need to share detailed information of dragged tabs in different way!
  mCurrentDragData = sanitizedDragData;
  browser.runtime.sendMessage({
    type:     Constants.kCOMMAND_BROADCAST_CURRENT_DRAG_DATA,
    windowId: TabsStore.getCurrentWindowId(),
    dragData: sanitizedDragData
  }).catch(ApiTabs.createErrorSuppressor());

  if (!dataOverridden) {
    const mozUrl  = [];
    const urlList = [];
    for (const draggedTab of dragData.tabs) {
      draggedTab.$TST.addState(Constants.kTAB_STATE_DRAGGING);
      TabsStore.addDraggingTab(draggedTab);
      if (!dragData.individualOnOutside ||
          mozUrl.length == 0) {
        mozUrl.push(`${draggedTab.url}\n${draggedTab.title}`);
        urlList.push(`#${draggedTab.title}\n${draggedTab.url}`);
      }
    }
    mCurrentDragDataForExternals[kTYPE_X_MOZ_URL] = mozUrl.join('\n');
    mCurrentDragDataForExternals[kTYPE_URI_LIST] = urlList.join('\n');
    if (allowBookmark) {
      log('set kTYPE_X_MOZ_URL');
      dt.setData(kTYPE_X_MOZ_URL, mCurrentDragDataForExternals[kTYPE_X_MOZ_URL]);
      log('set kTYPE_URI_LIST');
      dt.setData(kTYPE_URI_LIST, mCurrentDragDataForExternals[kTYPE_URI_LIST]);
    }
  }
  {
    const dragDataType    = `${kTYPE_ADDON_DRAG_DATA}${mCurrentDragDataForExternalsId}`;
    const dragDataContent = JSON.stringify(mCurrentDragDataForExternals);
    try {
      dt.setData(dragDataType, dragDataContent);
    }
    catch(error) {
      console.error(error);
      console.log(`Failed to set drag data with the type ${dragDataType}:`, dragDataContent);
    }
  }

  {
    // We set negative offsets to get more visibility about drop targets.
    // See also: https://github.com/piroor/treestyletab/issues/2826
    const offset = -16;
    dt.setDragImage(tab.$TST.element, offset, offset);
  }

  TabsStore.windows.get(TabsStore.getCurrentWindowId()).classList.add(kTABBAR_STATE_TAB_DRAGGING);
  document.documentElement.classList.add(kTABBAR_STATE_TAB_DRAGGING);

  // The drag operation can be canceled by something, then
  // "dragend" event is not dispatched and TST wrongly keeps
  // its "dragging" state. So we clear the dragging state with
  // a delay. (This timer will be cleared immediately by dragover
  // event, if the dragging operation is not canceled.)
  // See also: https://github.com/piroor/treestyletab/issues/1778#issuecomment-404569842
  mFinishCanceledDragOperation = setTimeout(finishDrag, 500, 'onDragStart');

  if (!('behavior' in options) &&
      configs.showTabDragBehaviorNotification) {
    const invertedBehavior = event.shiftKey ? configs.tabDragBehavior : configs.tabDragBehaviorShift;
    const count            = dragData.tabs.length;
    const currentResult    = getTabDragBehaviorNotificationMessageType(behavior, count);
    const invertedResult   = getTabDragBehaviorNotificationMessageType(invertedBehavior, count);
    if (currentResult || invertedResult) {
      const invertSuffix = event.shiftKey ? 'without_shift' : 'with_shift';
      mDragBehaviorNotification.firstChild.textContent = [
        currentResult && browser.i18n.getMessage(`tabDragBehaviorNotification_message_base`, [
          browser.i18n.getMessage(`tabDragBehaviorNotification_message_${currentResult}`)]),
        invertedResult && browser.i18n.getMessage(`tabDragBehaviorNotification_message_inverted_base_${invertSuffix}`, [
          browser.i18n.getMessage(`tabDragBehaviorNotification_message_${invertedResult}`)]),
      ].join('\n');
      mDragBehaviorNotification.firstChild.style.animationDuration = !shouldApplyAnimation() ? 0 : browser.i18n.getMessage(`tabDragBehaviorNotification_message_duration_${currentResult && invertedResult ? 'both' : 'single'}`);
      mDragBehaviorNotification.classList.remove('hiding');
      mDragBehaviorNotification.classList.add('shown');
    }
  }

  TSTAPI.sendMessage({
    type:     TSTAPI.kNOTIFY_NATIVE_TAB_DRAGSTART,
    tab:      new TSTAPI.TreeItem(tab),
    windowId: TabsStore.getCurrentWindowId()
  }, { tabProperties: ['tab'] }).catch(_error => {});

  updateLastDragEventCoordinates(event);
  // Don't store raw URLs to save privacy!
  sha1sum(dragData.tabs.map(tab => tab.url).join('\n')).then(digest => {
    configs.lastDraggedTabs = {
      tabIds:     dragData.tabs.map(tab => tab.id),
      urlsDigest: digest
    };
  });

  log('onDragStart: started');
}
onDragStart = EventUtils.wrapWithErrorHandler(onDragStart);

/* acceptable input:
  {
    "default":    { type: ..., data: ... },
    "Ctrl":       { type: ..., data: ... },
    "MacCtrl":    { type: ..., data: ... },
    "Ctrl+Shift": { type: ..., data: ... },
    "Alt-Shift":  { type: ..., data: ... },
    ...
  }
*/
function detectOverrideDragDataSet(dataSet, event) {
  if (Array.isArray(dataSet))
    return dataSet.map(oneDataSet => detectOverrideDragDataSet(oneDataSet, event)).flat();

  if ('type' in dataSet)
    return [dataSet];

  const keys = [];
  if (event.altKey)
    keys.push('alt');
  if (event.ctrlKey) {
    if (isMacOS())
      keys.push('macctrl');
    else
      keys.push('ctrl');
  }
  if (event.metaKey) {
    if (isMacOS())
      keys.push('command');
    else
      keys.push('meta');
  }
  if (event.shiftKey)
    keys.push('shift');
  const findKey = keys.sort().join('+') || 'default';

  for (const key of Object.keys(dataSet)) {
    const normalizedKey = key.split(/[-\+]/).filter(part => !!part).sort().join('+').toLowerCase();
    if (normalizedKey != findKey)
      continue;
    if (Array.isArray(dataSet[key]))
      return dataSet[key];
    else
      return [dataSet[key]];
  }
  return [];
}

function getTabDragBehaviorNotificationMessageType(behavior, count) {
  if (behavior & Constants.kDRAG_BEHAVIOR_ENTIRE_TREE && count > 1) {
    if (behavior & Constants.kDRAG_BEHAVIOR_ALLOW_BOOKMARK)
      return 'tree_bookmark';
    else if (behavior & Constants.kDRAG_BEHAVIOR_TEAR_OFF)
      return 'tree_tearoff';
    else
      return '';
  }
  else {
    if (behavior & Constants.kDRAG_BEHAVIOR_ALLOW_BOOKMARK)
      return 'tab_bookmark';
    else if (behavior & Constants.kDRAG_BEHAVIOR_TEAR_OFF)
      return 'tab_tearoff';
    else
      return '';
  }
}

let mLastDragOverTimestamp = null;
let mDelayedClearDropPosition = null;

function onDragOver(event) {
  if (mFinishCanceledDragOperation) {
    clearTimeout(mFinishCanceledDragOperation);
    mFinishCanceledDragOperation = null;
  }

  if (!isLinux()) {
    if (mDelayedClearDropPosition)
      clearTimeout(mDelayedClearDropPosition);
    mDelayedClearDropPosition = setTimeout(() => {
      mDelayedClearDropPosition = null;
      clearDropPosition();
    }, 250);
  }

  event.preventDefault(); // this is required to override default dragover actions!
  Scroll.autoScrollOnMouseEvent(event);

  updateLastDragEventCoordinates(event);

  // reduce too much handling of too frequent dragover events...
  const now = Date.now();
  if (now - (mLastDragOverTimestamp || 0) < configs.minimumIntervalToProcessDragoverEvent)
    return;
  mLastDragOverTimestamp = now;

  const info = getDropAction(event);
  const dt   = event.dataTransfer;

  if (isEventFiredOnTabDropBlocker(event) ||
      !info.canDrop) {
    log('onDragOver: not droppable');
    dt.dropEffect = 'none';
    if (mLastDropPosition)
      clearDropPosition();
    mLastDropPosition = null;
    return;
  }

  let dropPositionTargetTab = info.targetTab;
  if (dropPositionTargetTab && dropPositionTargetTab.$TST.collapsed)
    dropPositionTargetTab = info.targetTab.$TST.nearestVisiblePrecedingTab || info.targetTab;
  if (!dropPositionTargetTab) {
    log('onDragOver: no drop target tab');
    dt.dropEffect = 'none';
    mLastDropPosition = null;
    return;
  }

  if (!info.draggedTab ||
      dropPositionTargetTab.id != info.draggedTab.id) {
    const dropPosition = `${dropPositionTargetTab.id}:${info.dropPosition}`;
    if (dropPosition == mLastDropPosition) {
      log('onDragOver: no move');
      return;
    }
    clearDropPosition();
    dropPositionTargetTab.$TST.element.setAttribute(kDROP_POSITION, info.dropPosition);
    mLastDropPosition = dropPosition;
    log('onDragOver: set drop position to ', dropPosition);
  }
  else {
    mLastDropPosition = null;
  }
}
onDragOver = EventUtils.wrapWithErrorHandler(onDragOver);

function isEventFiredOnTabDropBlocker(event) {
  let node = event.target;
  if (node.nodeType != Node.ELEMENT_NODE)
    node = node.parentNode;
  return node && !!node.closest('.tab-drop-blocker');
}

function onDragEnter(event) {
  configs.lastDragOverSidebarOwnerWindowId = TabsStore.getCurrentWindowId();

  mDraggingOnSelfWindow = true;

  const info = getDropAction(event);
  try {
    const enteredTab = EventUtils.getTabFromEvent(event);
    const leftTab    = SidebarTabs.getTabFromDOMNode(event.relatedTarget);
    if (leftTab != enteredTab) {
      mDraggingOnDraggedTabs = (
        info.dragData &&
        info.dragData.tabs.some(tab => tab.id == enteredTab.id)
      );
    }
    TabsStore.windows.get(TabsStore.getCurrentWindowId()).classList.add(kTABBAR_STATE_TAB_DRAGGING);
    document.documentElement.classList.add(kTABBAR_STATE_TAB_DRAGGING);
  }
  catch(_e) {
  }

  const dt   = event.dataTransfer;
  dt.dropEffect = info.dropEffect;
  if (info.dropEffect == 'link')
    document.documentElement.classList.add(kTABBAR_STATE_LINK_DRAGGING);

  updateLastDragEventCoordinates(event);

  if (!configs.autoExpandOnLongHover ||
      !info.canDrop ||
      !info.dragOverTab)
    return;

  reserveToProcessLongHover.cancel();

  if (info.draggedTab &&
      info.dragOverTab.id == info.draggedTab.id)
    return;

  reserveToProcessLongHover({
    dragOverTabId: info.targetTab && info.targetTab.id,
    draggedTabId:  info.draggedTab && info.draggedTab.id,
    dropEffect:    info.dropEffect
  });
}
onDragEnter = EventUtils.wrapWithErrorHandler(onDragEnter);

function reserveToProcessLongHover(params = {}) {
  mLongHoverTimerNext = setTimeout(() => {
    mLongHoverTimerNext = null;
    mLongHoverTimer = setTimeout(async () => {
      log('reservedProcessLongHover: ', params);

      const dragOverTab = Tab.get(params.dragOverTabId);
      if (!dragOverTab ||
          dragOverTab.$TST.element.getAttribute(kDROP_POSITION) != 'self')
        return;

      // auto-switch for staying on tabs
      if (!dragOverTab.active &&
          params.dropEffect == 'link') {
        BackgroundConnection.sendMessage({
          type:  Constants.kCOMMAND_ACTIVATE_TAB,
          tabId: dragOverTab.id,
          byMouseOperation: true
        });
      }

      if (!dragOverTab ||
          !dragOverTab.$TST.isAutoExpandable)
        return;

      // auto-expand for staying on a parent
      if (configs.autoExpandIntelligently) {
        BackgroundConnection.sendMessage({
          type:  Constants.kCOMMAND_SET_SUBTREE_COLLAPSED_STATE_INTELLIGENTLY_FOR,
          tabId: dragOverTab.id
        });
      }
      else {
        if (!mLongHoverExpandedTabs.includes(params.dragOverTabId))
          mLongHoverExpandedTabs.push(params.dragOverTabId);
        BackgroundConnection.sendMessage({
          type:      Constants.kCOMMAND_SET_SUBTREE_COLLAPSED_STATE,
          tabId:     dragOverTab.id,
          collapsed: false,
          stack:     configs.debug && new Error().stack
        });
      }
    }, configs.autoExpandOnLongHoverDelay);
  }, 0);
}
reserveToProcessLongHover.cancel = function() {
  clearTimeout(mLongHoverTimer);
  clearTimeout(mLongHoverTimerNext);
};

function onDragLeave(event) {
  if (configs.lastDragOverSidebarOwnerWindowId == TabsStore.getCurrentWindowId())
    configs.lastDragOverSidebarOwnerWindowId = null;

  let leftFromTabBar = false;
  try {
    const info       = getDropAction(event);
    const leftTab    = EventUtils.getTabFromEvent(event);
    const enteredTab = SidebarTabs.getTabFromDOMNode(event.relatedTarget);
    if (leftTab != enteredTab) {
      if (info.dragData &&
          info.dragData.tabs.some(tab => tab.id == leftTab.id) &&
          (!enteredTab ||
           !info.dragData.tabs.every(tab => tab.id == enteredTab.id))) {
        onDragLeave.delayedLeftFromDraggedTabs = setTimeout(() => {
          delete onDragLeave.delayedLeftFromDraggedTabs;
          mDraggingOnDraggedTabs = false;
        }, 10);
      }
      else {
        leftFromTabBar = !enteredTab || enteredTab.windowId != TabsStore.getCurrentWindowId();
        if (onDragLeave.delayedLeftFromDraggedTabs) {
          clearTimeout(onDragLeave.delayedLeftFromDraggedTabs);
          delete onDragLeave.delayedLeftFromDraggedTabs;
        }
      }
    }
  }
  catch(_e) {
    leftFromTabBar = true;
  }

  if (leftFromTabBar) {
    onDragLeave.delayedLeftFromTabBar = setTimeout(() => {
      delete onDragLeave.delayedLeftFromTabBar;
      mDraggingOnSelfWindow = false;
      mDraggingOnDraggedTabs = false;
      clearDropPosition();
      clearDraggingState();
      mLastDropPosition = null;
    }, 10);
  }
  else if (onDragLeave.delayedLeftFromTabBar) {
    clearTimeout(onDragLeave.delayedLeftFromTabBar);
    delete onDragLeave.delayedLeftFromTabBar;
  }

  updateLastDragEventCoordinates(event);
  clearTimeout(mLongHoverTimer);
  mLongHoverTimer = null;
}
onDragLeave = EventUtils.wrapWithErrorHandler(onDragLeave);

function onDrop(event) {
  setTimeout(() => collapseAutoExpandedTabsWhileDragging(), 0);
  if (mLastDropPosition) {
    clearDropPosition();
    mLastDropPosition = null;
  }

  const dropActionInfo = getDropAction(event);
  log('onDrop ', dropActionInfo, event.dataTransfer);

  if (!dropActionInfo.canDrop) {
    log('undroppable');
    return;
  }

  const dt = event.dataTransfer;
  if (dt.dropEffect != 'link' &&
      dt.dropEffect != 'move' &&
      dropActionInfo.dragData &&
      !dropActionInfo.dragData.tab) {
    log('invalid drop');
    return;
  }

  // We need to cancel the drop event explicitly to prevent Firefox tries to load the dropped URL to the tab itself.
  // This is required to use "ext+treestyletab:tabbar" in a regular tab.
  // See also: https://github.com/piroor/treestyletab/issues/3056
  event.preventDefault();

  if (dropActionInfo.dragData &&
      dropActionInfo.dragData.tab) {
    log('there are dragged tabs');
    if (configs.enableWorkaroundForBug1548949) {
      configs.workaroundForBug1548949DroppedTabs = dropActionInfo.dragData.tabs.map(tab => `${mInstanceId}/${tab.id}`).join('\n');
      log('workaround for bug 1548949: setting last dropped tabs: ', configs.workaroundForBug1548949DroppedTabs);
    }
    const fromOtherProfile = dropActionInfo.dragData.instanceId != mInstanceId;
    BackgroundConnection.sendMessage({
      type:                Constants.kCOMMAND_PERFORM_TABS_DRAG_DROP,
      windowId:            dropActionInfo.dragData.windowId,
      tabs:                dropActionInfo.dragData.tabs,
      structure:           dropActionInfo.dragData.structure,
      action:              dropActionInfo.action,
      allowedActions:      dropActionInfo.dragData.behavior,
      attachToId:          dropActionInfo.parent && dropActionInfo.parent.id,
      insertBeforeId:      dropActionInfo.insertBefore && dropActionInfo.insertBefore.id,
      insertAfterId:       dropActionInfo.insertAfter && dropActionInfo.insertAfter.id,
      destinationWindowId: TabsStore.getCurrentWindowId(),
      duplicate:           !fromOtherProfile && dt.dropEffect == 'copy',
      import:              fromOtherProfile
    });
    return;
  }

  if (dt.types.includes(kTYPE_MOZ_TEXT_INTERNAL) &&
      configs.guessDraggedNativeTabs) {
    log('there are dragged native tabs');
    const url = dt.getData(kTYPE_MOZ_TEXT_INTERNAL);
    browser.tabs.query({ url }).then(tabs => {
      if (!tabs.length) {
        log('=> from other profile');
        handleDroppedNonTabItems(event, dropActionInfo);
        return;
      }
      log('=> possible dragged tabs: ', tabs);
      tabs = tabs.sort((a, b) => b.lastAccessed - a.lastAccessed);
      if (configs.enableWorkaroundForBug1548949) {
        configs.workaroundForBug1548949DroppedTabs = tabs.map(tab => `${mInstanceId}/${tab.id}`).join('\n');
        log('workaround for bug 1548949: setting last dropped tabs: ', configs.workaroundForBug1548949DroppedTabs);
      }
      const recentTab = tabs[0];
      const allowedActions = event.shiftKey ?
        configs.tabDragBehaviorShift :
        configs.tabDragBehavior;
      BackgroundConnection.sendMessage({
        type:                Constants.kCOMMAND_PERFORM_TABS_DRAG_DROP,
        windowId:            recentTab.windowId,
        tabs:                [recentTab],
        structure:           [-1],
        action:              dropActionInfo.action,
        allowedActions,
        attachToId:          dropActionInfo.parent && dropActionInfo.parent.id,
        insertBeforeId:      dropActionInfo.insertBefore && dropActionInfo.insertBefore.id,
        insertAfterId:       dropActionInfo.insertAfter && dropActionInfo.insertAfter.id,
        destinationWindowId: TabsStore.getCurrentWindowId(),
        duplicate:           dt.dropEffect == 'copy',
        import:              false
      });
    });
    return;
  }

  log('link or bookmark item is dropped');
  handleDroppedNonTabItems(event, dropActionInfo);
}
onDrop = EventUtils.wrapWithErrorHandler(onDrop);

async function onDragEnd(event) {
  log('onDragEnd, ', { event, mDraggingOnSelfWindow, mDraggingOnDraggedTabs, dropEffect: event.dataTransfer.dropEffect });
  if (!mLastDragEventCoordinates) {
    console.error(new Error('dragend is handled after finishDrag'));
    return;
  }
  const lastDragEventCoordinatesX = mLastDragEventCoordinates.x;
  const lastDragEventCoordinatesY = mLastDragEventCoordinates.y;
  const lastDragEventCoordinatesTimestamp = mLastDragEventCoordinates.timestamp;
  const droppedOnSidebarArea = !!configs.lastDragOverSidebarOwnerWindowId;

  let dragData = event.dataTransfer.getData(kTREE_DROP_TYPE);
  dragData = (dragData && JSON.parse(dragData)) || mCurrentDragData;
  if (dragData) {
    dragData.tab  = dragData.tab && Tab.get(dragData.tab.id) || dragData.tab;
    dragData.tabs = dragData.tabs && dragData.tabs.map(tab => tab && Tab.get(tab.id) || tab);
  }

  TSTAPI.sendMessage({
    type:     TSTAPI.kNOTIFY_NATIVE_TAB_DRAGEND,
    windowId: TabsStore.getCurrentWindowId()
  }).catch(_error => {});

  // Don't clear flags immediately, because they are referred by following operations in this function.
  setTimeout(finishDrag, 0, 'onDragEnd');

  if (!dragData ||
      !(dragData.behavior & Constants.kDRAG_BEHAVIOR_TEAR_OFF))
    return;

  let handledBySomeone = event.dataTransfer.dropEffect != 'none';

  if (event.dataTransfer.getData(kTYPE_URI_LIST)) {
    log('do nothing by TST for dropping just for bookmarking or linking');
    return;
  }
  else if (configs.enableWorkaroundForBug1548949) {
    // Due to the bug 1548949, "dropEffect" can become "move" even if no one
    // actually handles the drop. Basically kTREE_DROP_TYPE is not processible
    // by anyone except TST, so, we can treat the dropend as "dropped outside
    // the sidebar" when all dragged tabs are exact same to last tabs dropped
    // to a sidebar on this Firefox instance.
    // The only one exception is the case: tabs have been dropped to a TST
    // sidebar on any other Firefox instance. In this case tabs dropped to the
    // foreign Firefox will become duplicated: imported to the foreign Firefox
    // and teared off from the source window. This is clearly undesider
    // behavior from misdetection, but I decide to ignore it because it looks
    // quite rare case.
    await wait(250); // wait until "workaroundForBug1548949DroppedTabs" is synchronized
    const draggedTabs = dragData.tabs.map(tab => `${mInstanceId}/${tab.id}`).join('\n');
    const lastDroppedTabs = configs.workaroundForBug1548949DroppedTabs;
    handledBySomeone = draggedTabs == lastDroppedTabs;
    log('workaround for bug 1548949: detect dragged tabs are handled by me or not.',
        { handledBySomeone, draggedTabs, lastDroppedTabs });
    configs.workaroundForBug1548949DroppedTabs = null;
  }

  if (event.dataTransfer.mozUserCancelled ||
      handledBySomeone) {
    log('dragged items are processed by someone: ', event.dataTransfer.dropEffect);
    return;
  }

  if (droppedOnSidebarArea) {
    log('dropped on the tab bar (from event): detaching is canceled');
    return;
  }

  // Workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1767165
  // The bug affects only on Firefox 99 and 100. This hack should be removed after Firefox 101 is released.
  const fixDragEndCoordinates = (configs.enableWorkaroundForBug1767165_fixDragEndCoordinates && mLastBrowserInfo) ?
    ((major) => major >= 99 && major <= 100)(parseInt(mLastBrowserInfo.version.split('.')[0])) :
    false;
  const subframeXOffset = fixDragEndCoordinates ? (window.mozInnerScreenX - window.screenX) : 0;
  const subframeYOffset = fixDragEndCoordinates ? (window.mozInnerScreenY - window.screenY) : 0;

  if (configs.ignoreTabDropNearSidebarArea) {
    const windowX = window.mozInnerScreenX;
    const windowY = window.mozInnerScreenY;
    const windowW = window.innerWidth;
    const windowH = window.innerHeight;
    const offset  = dragData.tab.$TST.element && dragData.tab.$TST.element.getBoundingClientRect().height / 2;
    // Workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1561522
    const fixedEventScreenX = (event.screenX - subframeXOffset) / window.devicePixelRatio;
    const fixedEventScreenY = (event.screenY - subframeYOffset) / window.devicePixelRatio;
    const now = Date.now();
    log('dragend at: ', {
      windowX,
      windowY,
      windowW,
      windowH,
      eventScreenX: event.screenX,
      eventScreenY: event.screenY,
      eventClientX: event.clientX,
      eventClientY: event.clientY,
      fixedEventScreenX,
      fixedEventScreenY,
      devicePixelRatio: window.devicePixelRatio,
      lastDragEventCoordinatesX,
      lastDragEventCoordinatesY,
      offset,
      subframeXOffset,
      subframeYOffset,
    });
    if ((event.screenX >= windowX - offset &&
         event.screenY >= windowY - offset &&
         event.screenX <= windowX + windowW + offset &&
         event.screenY <= windowY + windowH + offset) ||
        // Workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1561522
        (fixedEventScreenX >= windowX - offset &&
         fixedEventScreenY >= windowY - offset &&
         fixedEventScreenX <= windowX + windowW + offset &&
         fixedEventScreenY <= windowY + windowH + offset)) {
      log('dropped near the tab bar (from coordinates): detaching is canceled');
      return;
    }
    // Workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1561879
    // On macOS sometimes drag gesture is canceled immediately with (0,0) coordinates.
    // (This happens on Windows also.)
    const delayFromLast = now - lastDragEventCoordinatesTimestamp;
    const rawOffsetX    = Math.abs(event.screenX - subframeXOffset - lastDragEventCoordinatesX);
    const rawOffsetY    = Math.abs(event.screenY - subframeYOffset - lastDragEventCoordinatesY);
    const fixedOffsetX  = Math.abs(fixedEventScreenX - lastDragEventCoordinatesX);
    const fixedOffsetY  = Math.abs(fixedEventScreenY - lastDragEventCoordinatesY);
    log('check: ', {
      now,
      lastDragEventCoordinatesTimestamp,
      delayFromLast,
      maxDelay: configs.maximumDelayForBug1561879,
      offset,
      rawOffsetX,
      rawOffsetY,
      fixedOffsetX,
      fixedOffsetY,
      subframeXOffset,
      subframeYOffset,
    });
    if (event.screenX == 0 &&
        event.screenY == 0 &&
        // We need to accept intentional drag and drop at left edge of the screen.
        // For safety, cancel only when the coordinates become (0,0) accidently from the bug.
        delayFromLast < configs.maximumDelayForBug1561879 &&
        (rawOffsetX > offset || fixedOffsetX > offset) &&
        (rawOffsetY > offset || fixedOffsetY > offset)) {
      log('dropped at unknown position: detaching is canceled');
      return;
    }
  }

  log('trying to detach tab from window');
  event.stopPropagation();
  event.preventDefault();

  if (isDraggingAllActiveTabs(dragData.tab)) {
    log('all tabs are dragged, so it is nonsence to tear off them from the window');
    return;
  }

  const detachTabs = dragData.individualOnOutside ? [dragData.tab] : dragData.tabs;
  BackgroundConnection.sendMessage({
    type:      Constants.kCOMMAND_NEW_WINDOW_FROM_TABS,
    tabIds:    detachTabs.map(tab => tab.id),
    duplicate: EventUtils.isAccelKeyPressed(event),
    left:      event.screenX - subframeXOffset,
    top:       event.screenY - subframeYOffset,
  });
}
onDragEnd = EventUtils.wrapWithErrorHandler(onDragEnd);

function finishDrag(trigger) {
  log(`finishDrag from ${trigger || 'unknown'}`);

  mDragBehaviorNotification.classList.add('hiding');
  mDragBehaviorNotification.classList.remove('shown');
  setTimeout(() => {
    mDragBehaviorNotification.classList.remove('hiding');
  }, configs.collapseDuration);

  mDraggingOnSelfWindow = false;

  wait(100).then(() => {
    mCurrentDragData = null;
    mCurrentDragDataForExternalsId = null;
    mCurrentDragDataForExternals = null;
    browser.runtime.sendMessage({
      type:     Constants.kCOMMAND_BROADCAST_CURRENT_DRAG_DATA,
      windowId: TabsStore.getCurrentWindowId(),
      dragData: null
    }).catch(ApiTabs.createErrorSuppressor());
  });

  onFinishDrag();
}

function onFinishDrag() {
  clearDraggingTabsState();
  clearDropPosition();
  mLastDropPosition = null;
  updateLastDragEventCoordinates();
  mLastDragOverTimestamp = null;
  clearDraggingState();
  collapseAutoExpandedTabsWhileDragging();
  mDraggingOnSelfWindow = false;
  mDraggingOnDraggedTabs = false;
}

function updateLastDragEventCoordinates(event = null) {
  mLastDragEventCoordinates = !event ? null : {
    x: event.screenX,
    y: event.screenY,
    timestamp: Date.now(),
  };
}


/* drag on tabs API */

function onTSTAPIDragEnter(event) {
  Scroll.autoScrollOnMouseEvent(event);
  const tab = EventUtils.getTabFromEvent(event);
  if (!tab)
    return;
  let target = tab.$TST.element;
  if (mDragTargetIsClosebox && EventUtils.isEventFiredOnClosebox(event))
    target = tab.$TST.element.closeBoxElement;
  cancelDelayedTSTAPIDragExitOn(target);
  if (tab &&
      (!mDragTargetIsClosebox ||
       EventUtils.isEventFiredOnClosebox(event))) {
    if (target != mLastDragEnteredTarget) {
      TSTAPI.sendMessage({
        type:     TSTAPI.kNOTIFY_TAB_DRAGENTER,
        tab:      new TSTAPI.TreeItem(tab),
        window:   tab.windowId,
        windowId: tab.windowId
      }, { tabProperties: ['tab'] }).catch(_error => {});
    }
  }
  mLastDragEnteredTarget = target;
}

function onTSTAPIDragExit(event) {
  if (mDragTargetIsClosebox &&
      !EventUtils.isEventFiredOnClosebox(event))
    return;
  const tab = EventUtils.getTabFromEvent(event);
  if (!tab)
    return;
  let target = tab.$TST.element;
  if (mDragTargetIsClosebox && EventUtils.isEventFiredOnClosebox(event))
    target = tab.$TST.element.closeBoxElement;
  cancelDelayedTSTAPIDragExitOn(target);
  target.onTSTAPIDragExitTimeout = setTimeout(() => {
    delete target.onTSTAPIDragExitTimeout;
    TSTAPI.sendMessage({
      type:     TSTAPI.kNOTIFY_TAB_DRAGEXIT,
      tab:      new TSTAPI.TreeItem(tab),
      window:   tab.windowId,
      windowId: tab.windowId
    }, { tabProperties: ['tab'] }).catch(_error => {});
  }, 10);
}

function cancelDelayedTSTAPIDragExitOn(target) {
  if (target && target.onTSTAPIDragExitTimeout) {
    clearTimeout(target.onTSTAPIDragExitTimeout);
    delete target.onTSTAPIDragExitTimeout;
  }
}


function onMessage(message, _sender, _respond) {
  if (!message ||
      typeof message.type != 'string')
    return;

  switch (message.type) {
    case Constants.kCOMMAND_BROADCAST_CURRENT_DRAG_DATA:
      setDragData(message.dragData || null);
      if (!message.dragData)
        onFinishDrag();
      break;
  }
}


TSTAPI.onMessageExternal.addListener((message, _sender) => {
  switch (message.type) {
    case TSTAPI.kGET_DRAG_DATA:
      if (mCurrentDragDataForExternals &&
          message.id == mCurrentDragDataForExternalsId)
        return Promise.resolve(mCurrentDragDataForExternals);
      break;
  }
});

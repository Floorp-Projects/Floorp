/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import EventListenerManager from '/extlib/EventListenerManager.js';

import {
  log as internalLogger,
  dumpTab,
  wait,
  countMatched,
  configs,
  notify,
  getWindowParamsFromSource,
} from '/common/common.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as Bookmark from '/common/bookmark.js';
import * as Constants from '/common/constants.js';
import * as ContextualIdentities from '/common/contextual-identities.js';
import * as SidebarConnection from '/common/sidebar-connection.js';
import * as Sync from '/common/sync.js';
import * as TabsInternalOperation from '/common/tabs-internal-operation.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TreeBehavior from '/common/tree-behavior.js';

import Tab from '/common/Tab.js';

import * as TabsGroup from './tabs-group.js';
import * as TabsMove from './tabs-move.js';
import * as TabsOpen from './tabs-open.js';
import * as Tree from './tree.js';

function log(...args) {
  internalLogger('background/commands', ...args);
}

export const onTabsClosing = new EventListenerManager();
export const onMoveUp      = new EventListenerManager();
export const onMoveDown    = new EventListenerManager();

function uniqTabsAndDescendantsSet(tabs) {
  if (!Array.isArray(tabs))
    tabs = [tabs];
  return Array.from(new Set(tabs.map(tab => [tab].concat(tab.$TST.descendants)).flat())).sort(Tab.compare);
}

export function reloadTree(tabs) {
  for (const tab of uniqTabsAndDescendantsSet(tabs)) {
    browser.tabs.reload(tab.id)
      .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
  }
}

export function reloadDescendants(rootTabs) {
  const rootTabsSet = new Set(rootTabs);
  for (const tab of uniqTabsAndDescendantsSet(rootTabs)) {
    if (rootTabsSet.has(tab))
      continue;
    browser.tabs.reload(tab.id)
      .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
  }
}

export async function closeTree(tabs) {
  tabs = uniqTabsAndDescendantsSet(tabs);
  const windowId = tabs[0].windowId;
  const canceled = (await onTabsClosing.dispatch(tabs.map(tab => tab.id), { windowId })) === false;
  if (canceled)
    return;
  tabs.reverse(); // close bottom to top!
  TabsInternalOperation.removeTabs(tabs);
}

export async function closeDescendants(rootTabs) {
  const rootTabsSet = new Set(rootTabs);
  const tabs     = uniqTabsAndDescendantsSet(rootTabs).filter(tab => !rootTabsSet.has(tab));
  const windowId = rootTabs[0].windowId;
  const canceled = (await onTabsClosing.dispatch(tabs.map(tab => tab.id), { windowId })) === false;
  if (canceled)
    return;
  tabs.reverse(); // close bottom to top!
  TabsInternalOperation.removeTabs(tabs);
}

export async function closeOthers(exceptionRoots) {
  const exceptionTabs = uniqTabsAndDescendantsSet(exceptionRoots);
  const windowId      = exceptionTabs[0].windowId;
  const tabs          = Tab.getNormalTabs(windowId, { iterator: true, reversed: true }); // except pinned or hidden tabs, close bottom to top!
  const closeTabs     = [];
  for (const tab of tabs) {
    if (!exceptionTabs.includes(tab))
      closeTabs.push(tab);
  }
  const canceled = (await onTabsClosing.dispatch(closeTabs.map(tab => tab.id), { windowId })) === false;
  if (canceled)
    return;
  TabsInternalOperation.removeTabs(closeTabs);
}

export function collapseTree(rootTabs, options = {}) {
  const tabs = (
    options.recursively ?
      uniqTabsAndDescendantsSet(rootTabs) :
      Array.isArray(rootTabs) && rootTabs || [rootTabs]
  ).filter(tab => tab.$TST.hasChild && !tab.$TST.subtreeCollapsed);
  for (const tab of tabs) {
    Tree.collapseExpandSubtree(tab, {
      collapsed: true,
      broadcast: true
    });
  }
}

export function collapseAll(windowId) {
  for (const tab of Tab.getNormalTabs(windowId, { iterator: true })) {
    collapseTree(tab);
  }
}

export function expandTree(rootTabs, options = {}) {
  const tabs = (
    options.recursively ?
      uniqTabsAndDescendantsSet(rootTabs) :
      Array.isArray(rootTabs) && rootTabs || [rootTabs]
  ).filter(tab => tab.$TST.hasChild && tab.$TST.subtreeCollapsed);
  for (const tab of tabs) {
    Tree.collapseExpandSubtree(tab, {
      collapsed: false,
      broadcast: true
    });
  }
}

export function expandAll(windowId) {
  for (const tab of Tab.getNormalTabs(windowId, { iterator: true })) {
    expandTree(tab);
  }
}

export async function bookmarkTree(rootTabs, { parentId, index, showDialog } = {}) {
  const tabs = uniqTabsAndDescendantsSet(rootTabs);
  if (tabs.length > 1 &&
      tabs[0].$TST.isGroupTab)
    tabs.shift();

  const options = { parentId, index, showDialog };
  const topLevelTabs = rootTabs.filter(tab => tab.$TST.ancestorIds.length == 0);
  if (topLevelTabs.length == 1 &&
      topLevelTabs[0].$TST.isGroupTab)
    options.title = topLevelTabs[0].title;

  const tab = tabs[0];
  if (configs.showDialogInSidebar &&
      SidebarConnection.isOpen(tab.windowId)) {
    return SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_BOOKMARK_TABS_WITH_DIALOG,
      windowId: tab.windowId,
      tabIds:   tabs.map(tab => tab.id),
      options
    });
  }
  else {
    return Bookmark.bookmarkTabs(tabs, {
      ...options,
      showDialog: true
    });
  }
}


export async function openNewTabAs(options = {}) {
  let activeTabs;
  if (!options.baseTab) {
    activeTabs = await browser.tabs.query({
      active:        true,
      currentWindow: true,
    }).catch(ApiTabs.createErrorHandler());
    if (activeTabs.length == 0)
      activeTabs = await browser.tabs.query({
        currentWindow: true,
      }).catch(ApiTabs.createErrorHandler());
  }
  const activeTab = options.baseTab || Tab.get(activeTabs[0].id);

  let parent, insertBefore, insertAfter;
  let isOrphan = false;
  switch (options.as) {
    case Constants.kNEWTAB_DO_NOTHING:
    default:
      break;

    case Constants.kNEWTAB_OPEN_AS_ORPHAN:
      isOrphan    = true;
      insertAfter = Tab.getLastTab(activeTab.windowId);
      break;

    case Constants.kNEWTAB_OPEN_AS_CHILD: {
      parent = activeTab;
      const refTabs = Tree.getReferenceTabsForNewChild(null, parent);
      insertBefore = refTabs.insertBefore;
      insertAfter  = refTabs.insertAfter;
      log('detected reference tabs: ',
          { parent, insertBefore, insertAfter });
    }; break;

    case Constants.kNEWTAB_OPEN_AS_SIBLING:
      parent      = activeTab.$TST.parent;
      insertAfter = parent && parent.$TST.lastDescendant;
      break;

    case Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING_WITH_INHERITED_CONTAINER:
      options.cookieStoreId = activeTab.cookieStoreId;
    case Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING: {
      parent       = activeTab.$TST.parent;
      const refTabs = Tree.getReferenceTabsForNewNextSibling(activeTab, options);
      insertBefore = refTabs.insertBefore;
      insertAfter  = refTabs.insertAfter;
    }; break;
  }

  if (!options.cookieStoreId) {
    switch (configs.inheritContextualIdentityToChildTabMode) {
      case Constants.kCONTEXTUAL_IDENTITY_FROM_PARENT:
        if (parent)
          options.cookieStoreId = parent.cookieStoreId;
        break;

      case Constants.kCONTEXTUAL_IDENTITY_FROM_LAST_ACTIVE:
        options.cookieStoreId = activeTab.cookieStoreId;
        break;

      default:
        break;
    }
  }

  TabsOpen.openNewTab({
    parent, insertBefore, insertAfter,
    isOrphan,
    windowId:      activeTab.windowId,
    inBackground:  !!options.inBackground,
    cookieStoreId: options.cookieStoreId
  });
}

export async function indent(tab, options = {}) {
  const newParent = tab.$TST.previousSiblingTab;
  if (!newParent ||
      newParent == tab.$TST.parent)
    return false;

  if (!options.followChildren)
    Tree.detachAllChildren(tab, {
      broadcast: true,
      behavior:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD
    });
  const insertAfter = newParent.$TST.lastDescendant || newParent;
  await Tree.attachTabTo(tab, newParent, {
    broadcast:   true,
    forceExpand: true,
    insertAfter
  });
  return true;
}

export async function outdent(tab, options = {}) {
  const parent = tab.$TST.parent;
  if (!parent)
    return false;

  const newParent = parent.$TST.parent;
  if (!options.followChildren)
    Tree.detachAllChildren(tab, {
      broadcast: true,
      behavior:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD
    });
  if (newParent) {
    const insertAfter = parent.$TST.lastDescendant || parent;
    await Tree.attachTabTo(tab, newParent, {
      broadcast:   true,
      forceExpand: true,
      insertAfter
    });
  }
  else {
    await Tree.detachTab(tab, {
      broadcast: true,
    });
    const insertAfter = parent.$TST.lastDescendant || parent;
    await Tree.moveTabSubtreeAfter(tab, insertAfter, {
      broadcast: true,
    });
  }
  return true;
}

// drag and drop helper
async function performTabsDragDrop(params = {}) {
  const windowId = params.windowId || TabsStore.getCurrentWindowId();
  const destinationWindowId = params.destinationWindowId || windowId;

  log('performTabsDragDrop ', () => ({
    tabs:                params.tabs.map(dumpTab),
    attachTo:            dumpTab(params.attachTo),
    insertBefore:        dumpTab(params.insertBefore),
    insertAfter:         dumpTab(params.insertAfter),
    windowId:            params.windowId,
    destinationWindowId: params.destinationWindowId,
    action:              params.action,
    allowedActions:      params.allowedActions
  }));

  if (!(params.allowedActions & Constants.kDRAG_BEHAVIOR_MOVE) &&
      !params.duplicate) {
    log('not allowed action');
    return;
  }

  const movedTabs = await moveTabsWithStructure(params.tabs, {
    ...params,
    windowId, destinationWindowId,
    broadcast: true
  });
  if (movedTabs.length == 0)
    return;
  if (windowId != destinationWindowId) {
    // Firefox always focuses to the dropped (mvoed) tab if it is dragged from another window.
    // TST respects Firefox's the behavior.
    browser.tabs.update(movedTabs[0].id, { active: true })
      .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
  }
}

async function performTabsDragDropWithMessage(message) {
  const draggedTabIds = message.import ? [] : message.tabs.map(tab => tab.id);
  await Tab.waitUntilTracked(draggedTabIds.concat([
    message.attachToId,
    message.insertBeforeId,
    message.insertAfterId
  ]));
  log('perform tabs dragdrop requested: ', message);
  return performTabsDragDrop({
    ...message,
    tabs:         message.import ? message.tabs : draggedTabIds.map(id => Tab.get(id)),
    attachTo:     message.attachToId && Tab.get(message.attachToId),
    insertBefore: message.insertBeforeId && Tab.get(message.insertBeforeId),
    insertAfter:  message.insertAfterId && Tab.get(message.insertAfterId)
  });
}

// useful utility for general purpose
export async function moveTabsWithStructure(tabs, params = {}) {
  log('moveTabsWithStructure ', () => tabs.map(dumpTab));

  let movedTabs = tabs.filter(tab => !!tab);
  if (!movedTabs.length)
    return [];

  let movedRoots = params.import ? [] : Tab.collectRootTabs(movedTabs);

  const movedWholeTree = [].concat(movedRoots);
  for (const movedRoot of movedRoots) {
    const descendants = movedRoot.$TST.descendants;
    for (const descendant of descendants) {
      if (!movedWholeTree.includes(descendant))
        movedWholeTree.push(descendant);
    }
  }
  log('=> movedTabs: ', () => movedTabs.map(dumpTab).join(' / '));

  while (params.insertBefore &&
         movedWholeTree.includes(params.insertBefore)) {
    params.insertBefore = params.insertBefore && params.insertBefore.$TST.nextTab;
  }
  while (params.insertAfter &&
         movedWholeTree.includes(params.insertAfter)) {
    params.insertAfter = params.insertAfter && params.insertAfter.$TST.previousTab;
  }

  const windowId = params.windowId || tabs[0].windowId;
  const destinationWindowId = params.destinationWindowId ||
    params.insertBefore && params.insertBefore.windowId || 
      params.insertAfter && params.insertAfter.windowId ||
        windowId;

  // Basically tabs should not be moved between regular window and private browsing window,
  // so there are some codes to prevent shch operations. This is for failsafe.
  if (movedTabs[0].incognito != Tab.getFirstTab(destinationWindowId).incognito)
    return [];

  if (!params.import && movedWholeTree.length != movedTabs.length) {
    log('=> partially moved');
    if (!params.duplicate)
      await Tree.detachTabsFromTree(movedTabs, {
        broadcast: params.broadcast
      });
  }

  if (params.import) {
    const window = TabsStore.windows.get(destinationWindowId);
    const initialIndex = params.insertBefore ? params.insertBefore.index :
      params.insertAfter ? params.insertAfter.index+1 :
        window.tabs.size;
    window.toBeOpenedOrphanTabs += tabs.length;
    movedTabs = [];
    let index = 0;
    for (const tab of tabs) {
      let importedTab;
      const createParams = {
        url:      tab.url,
        windowId: destinationWindowId,
        index:    initialIndex + index,
        active:   index == 0
      };
      try {
        importedTab = await browser.tabs.create(createParams);
      }
      catch(error) {
        console.log(error);
      }
      if (!importedTab)
        importedTab = await browser.tabs.create({
          ...createParams,
          url: `about:blank?${tab.url}`
        });
      movedTabs.push(importedTab);
      index++;
    }
    await wait(100); // wait for all imported tabs are tracked
    movedTabs = movedTabs.map(tab => Tab.get(tab.id));
    await Tree.applyTreeStructureToTabs(movedTabs, params.structure, {
      broadcast: true
    });
    movedRoots = Tab.collectRootTabs(movedTabs);
  }
  else if (params.duplicate ||
      windowId != destinationWindowId) {
    movedTabs = await Tree.moveTabs(movedTabs, {
      destinationWindowId,
      duplicate:    params.duplicate,
      insertBefore: params.insertBefore,
      insertAfter:  params.insertAfter,
      broadcast:    params.broadcast
    });
    movedRoots = Tab.collectRootTabs(movedTabs);
  }

  log('try attach/detach');
  let shouldExpand = false;
  if (!params.attachTo) {
    log('=> detach');
    detachTabsWithStructure(movedRoots, {
      broadcast: params.broadcast
    });
    shouldExpand = true;
  }
  else {
    log('=> attach');
    await attachTabsWithStructure(movedRoots, params.attachTo, {
      insertBefore: params.insertBefore,
      insertAfter:  params.insertAfter,
      draggedTabs:  movedTabs,
      broadcast:    params.broadcast
    });
    shouldExpand = !params.attachTo.$TST.subtreeCollapsed;
  }

  log('=> moving tabs ', () => movedTabs.map(dumpTab));
  if (params.insertBefore)
    await TabsMove.moveTabsBefore(
      movedTabs,
      params.insertBefore,
      { broadcast: params.broadcast }
    );
  else if (params.insertAfter)
    await TabsMove.moveTabsAfter(
      movedTabs,
      params.insertAfter,
      { broadcast: params.broadcast }
    );
  else
    log('=> already placed at expected position');

  /*
  const treeStructure = getTreeStructureFromTabs(movedTabs);

  const newTabs;
  const replacedGroupTabs = Tab.doAndGetNewTabs(() => {
    newTabs = moveTabsInternal(movedTabs, {
      duplicate:    params.duplicate,
      insertBefore: params.insertBefore,
      insertAfter:  params.insertAfter
    });
  }, windowId);
  log('=> opened group tabs: ', replacedGroupTabs);
  params.draggedTab.ownerDocument.defaultView.setTimeout(() => {
    if (!TabsStore.ensureLivingTab(tab)) // it was removed while waiting
      return;
    log('closing needless group tabs');
    replacedGroupTabs.reverse().forEach(function(tab) {
      log(' check: ', tab.label+'('+tab.index+') '+getLoadingURI(tab));
      if (tab.$TST.isGroupTab &&
        !tab.$TST.hasChild)
        removeTab(tab);
    }, this);
  }, 0);
  */

  if (shouldExpand) {
    log('=> expand dropped tabs');
    // Collapsed tabs may be moved to the root level,
    // then we need to expand them.
    await Promise.all(movedRoots.map(tab => {
      if (!tab.$TST.collapsed)
        return;
      return Tree.collapseExpandTabAndSubtree(tab, {
        collapsed: false,
        broadcast: params.broadcast
      });
    }));
  }

  log('=> finished');

  return movedTabs;
}

async function attachTabsWithStructure(tabs, parent, options = {}) {
  log('attachTabsWithStructure: start ', () => [tabs.map(dumpTab), dumpTab(parent)]);
  if (parent &&
      !options.insertBefore &&
      !options.insertAfter) {
    const refTabs = Tree.getReferenceTabsForNewChild(
      tabs[0],
      parent,
      { ignoreTabs: tabs }
    );
    options.insertBefore = refTabs.insertBefore;
    options.insertAfter  = refTabs.insertAfter;
  }

  if (options.insertBefore)
    await TabsMove.moveTabsBefore(
      options.draggedTabs || tabs,
      options.insertBefore,
      { broadcast: options.broadcast }
    );
  else if (options.insertAfter)
    await TabsMove.moveTabsAfter(
      options.draggedTabs || tabs,
      options.insertAfter,
      { broadcast: options.broadcast }
    );

  const memberOptions = {
    ...options,
    insertBefore: null,
    insertAfter:  null,
    dontMove:     true,
    forceExpand:  options.draggedTabs.some(tab => tab.active)
  };
  return Promise.all(tabs.map(async tab => {
    if (parent)
      await Tree.attachTabTo(tab, parent, memberOptions);
    else
      await Tree.detachTab(tab, memberOptions);
    // The tree can remain being collapsed by other addons like TST Lock Tree Collapsed.
    const collapsed = parent && parent.$TST.subtreeCollapsed;
    return Tree.collapseExpandTabAndSubtree(tab, {
      ...memberOptions,
      collapsed,
    });
  }));
}

function detachTabsWithStructure(tabs, options = {}) {
  log('detachTabsWithStructure: start ', () => tabs.map(dumpTab));
  for (const tab of tabs) {
    Tree.detachTab(tab, options);
    Tree.collapseExpandTabAndSubtree(tab, {
      ...options,
      collapsed: false
    });
  }
}

export async function moveUp(tab, options = {}) {
  const previousTab = tab.$TST.nearestVisiblePrecedingTab;
  if (!previousTab)
    return false;
  const moved = await moveBefore(tab, {
    ...options,
    referenceTabId: previousTab.id
  });
  if (moved && !options.followChildren)
    await onMoveUp.dispatch(tab);
  return moved;
}

export async function moveDown(tab, options = {}) {
  const nextTab = options.followChildren ? tab.$TST.nearestFollowingForeignerTab : tab.$TST.nearestVisibleFollowingTab;
  if (!nextTab)
    return false;
  const moved = await moveAfter(tab, {
    ...options,
    referenceTabId: nextTab.id
  });
  if (moved && !options.followChildren)
    await onMoveDown.dispatch(tab);
  return moved;
}

export async function moveBefore(tab, options = {}) {
  const insertBefore = Tab.get(options.referenceTabId || options.referenceTab) || null;
  if (!insertBefore)
    return false;

  if (!options.followChildren) {
    Tree.detachAllChildren(tab, {
      broadcast: true,
      behavior:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD
    });
    await TabsMove.moveTabBefore(
      tab,
      insertBefore,
      { broadcast: true }
    );
  }
  else {
    const referenceTabs = TreeBehavior.calculateReferenceTabsFromInsertionPosition(tab, {
      context: Constants.kINSERTION_CONTEXT_MOVED,
      insertBefore
    });
    if (!referenceTabs.insertBefore &&
        !referenceTabs.insertAfter)
      return false;
    await moveTabsWithStructure([tab].concat(tab.$TST.descendants), {
      attachTo:     referenceTabs.parent,
      insertBefore: referenceTabs.insertBefore,
      insertAfter:  referenceTabs.insertAfter,
      broadcast:    true
    });
  }
  return true;
}

export async function moveAfter(tab, options = {}) {
  const insertAfter = Tab.get(options.referenceTabId || options.referenceTab) || null;
  if (!insertAfter)
    return false;

  if (!options.followChildren) {
    Tree.detachAllChildren(tab, {
      broadcast: true,
      behavior:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD
    });
    await TabsMove.moveTabAfter(
      tab,
      insertAfter,
      { broadcast: true }
    );
  }
  else {
    const referenceTabs = TreeBehavior.calculateReferenceTabsFromInsertionPosition(tab, {
      context: Constants.kINSERTION_CONTEXT_MOVED,
      insertAfter
    });
    if (!referenceTabs.insertBefore && !referenceTabs.insertAfter)
      return false;
    await moveTabsWithStructure([tab].concat(tab.$TST.descendants), {
      attachTo:     referenceTabs.parent,
      insertBefore: referenceTabs.insertBefore,
      insertAfter:  referenceTabs.insertAfter,
      broadcast:    true
    });
  }
  return true;
}


/* commands to simulate Firefox's native tab cocntext menu */

export async function duplicateTab(sourceTab, options = {}) {
  /*
    Due to difference between Firefox's "duplicate tab" implementation,
    TST sometimes fails to detect duplicated tabs based on its
    session information. Thus we need to duplicate as an internally
    duplicated tab. For more details, see also:
    https://github.com/piroor/treestyletab/issues/1437#issuecomment-334952194
  */
  const isMultiselected = options.multiselected === false ? false : sourceTab.$TST.multiselected;
  const sourceTabs = isMultiselected ? Tab.getSelectedTabs(sourceTab.windowId) : [sourceTab];
  log('source tabs: ', sourceTabs);
  const duplicatedTabs = await Tree.moveTabs(sourceTabs, {
    duplicate:           true,
    destinationWindowId: options.destinationWindowId || sourceTabs[0].windowId,
    insertAfter:         sourceTabs[sourceTabs.length-1]
  });
  await Tree.behaveAutoAttachedTabs(duplicatedTabs, {
    baseTabs:  sourceTabs,
    behavior:  typeof options.behavior == 'number' ? options.behavior : configs.autoAttachOnDuplicated,
    broadcast: true
  });
  return duplicatedTabs;
}

export async function moveTabToStart(tab, options = {}) {
  const isMultiselected = options.multiselected === false ? false : tab.$TST.multiselected;
  return moveTabsToStart(isMultiselected ? Tab.getSelectedTabs(tab.windowId) : [tab].concat(tab.$TST.descendants));
}

export async function moveTabsToStart(movedTabs) {
  if (movedTabs.length === 0)
    return;
  const tab       = movedTabs[0];
  const allTabs   = tab.pinned ? Tab.getPinnedTabs(tab.windowId) : Tab.getUnpinnedTabs(tab.windowId);
  const movedTabsSet = new Set(movedTabs);
  let firstOtherTab;
  for (const tab of allTabs) {
    if (movedTabsSet.has(tab))
      continue;
    firstOtherTab = tab;
    break;
  }
  if (firstOtherTab)
    await moveTabsWithStructure(movedTabs, {
      insertBefore: firstOtherTab,
      broadcast:    true
    });
}

export async function moveTabToEnd(tab, options = {}) {
  const isMultiselected = options.multiselected === false ? false : tab.$TST.multiselected;
  return moveTabsToEnd(isMultiselected ? Tab.getSelectedTabs(tab.windowId) : [tab].concat(tab.$TST.descendants));
}

export async function moveTabsToEnd(movedTabs) {
  if (movedTabs.length === 0)
    return;
  const tab       = movedTabs[0];
  const allTabs   = tab.pinned ? Tab.getPinnedTabs(tab.windowId) : Tab.getUnpinnedTabs(tab.windowId);
  const movedTabsSet = new Set(movedTabs);
  let lastOtherTabs;
  for (let i = allTabs.length - 1; i > -1; i--) {
    const tab = allTabs[i];
    if (movedTabsSet.has(tab))
      continue;
    lastOtherTabs = tab;
    break;
  }
  if (lastOtherTabs)
    await moveTabsWithStructure(movedTabs, {
      insertAfter: lastOtherTabs,
      broadcast:   true
    });
}

export async function openTabInWindow(tab, options = {}) {
  if (options.multiselected !== false && tab.$TST.multiselected) {
    return openTabsInWindow(Tab.getSelectedTabs(tab.windowId));
  }
  else {
    const sourceWindow = await browser.windows.get(tab.windowId);
    const windowParams = {
      //active: true,  // not supported in Firefox...
      tabId:     tab.id,
      ...getWindowParamsFromSource(sourceWindow, options),
    };
    const window = await browser.windows.create(windowParams).catch(ApiTabs.createErrorHandler());
    return window.id;
  }
}

export async function openTabsInWindow(tabs) {
  const movedTabs = await Tree.openNewWindowFromTabs(tabs);
  return movedTabs.length > 0 ? movedTabs[0].windowId : null;
}


export async function restoreTabs(count) {
  const toBeRestoredTabSessions = (await browser.sessions.getRecentlyClosed({
    maxResults: browser.sessions.MAX_SESSION_RESULTS
  }).catch(ApiTabs.createErrorHandler())).filter(session => session.tab).slice(0, count);
  log('restoreTabs: toBeRestoredTabSessions = ', toBeRestoredTabSessions);
  const promisedRestoredTabs = [];
  for (const session of toBeRestoredTabSessions.reverse()) {
    log('restoreTabs: Tabrestoring session = ', session);
    promisedRestoredTabs.push(Tab.doAndGetNewTabs(async () => {
      browser.sessions.restore(session.tab.sessionId).catch(ApiTabs.createErrorSuppressor());
      await Tab.waitUntilTrackedAll();
    }));
  }
  const restoredTabs = Array.from(new Set((await Promise.all(promisedRestoredTabs)).flat()));
  log('restoreTabs: restoredTabs = ', restoredTabs);
  await Promise.all(restoredTabs.map(tab => tab && Tab.get(tab.id).$TST.opened));

  if (restoredTabs.length > 0) {
    // Parallelly restored tabs can have ghost "active" state, so we need to clear them
    const activeTab = Tab.getActiveTab(restoredTabs[0].windowId);
    if (restoredTabs.some(tab => tab.id == activeTab.id))
      await TabsInternalOperation.setTabActive(activeTab);
  }

  return Tab.sort(restoredTabs);
}


export async function bookmarkTab(tab, options = {}) {
  if (options.multiselected !== false && tab.$TST.multiselected)
    return bookmarkTabs(Tab.getSelectedTabs(tab.windowId));

  if (configs.showDialogInSidebar &&
      SidebarConnection.isOpen(tab.windowId)) {
    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_BOOKMARK_TAB_WITH_DIALOG,
      windowId: tab.windowId,
      tabId:    tab.id
    });
  }
  else {
    Bookmark.bookmarkTab(tab, {
      showDialog: true
    });
  }
}

export async function bookmarkTabs(tabs) {
  if (tabs.length == 0)
    return;
  if (configs.showDialogInSidebar &&
      SidebarConnection.isOpen(tabs[0].windowId)) {
    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_BOOKMARK_TABS_WITH_DIALOG,
      windowId: tabs[0].windowId,
      tabIds:   tabs.map(tab => tab.id)
    });
  }
  else {
    Bookmark.bookmarkTabs(tabs, {
      showDialog: true
    });
  }
}

export async function reopenInContainer(sourceTabOrTabs, cookieStoreId, options = {}) {
  let sourceTabs;
  if (Array.isArray(sourceTabOrTabs)) {
    sourceTabs = sourceTabOrTabs;
  }
  else {
    const isMultiselected = options.multiselected === false ? false : sourceTabOrTabs.$TST.multiselected;
    sourceTabs = isMultiselected ? Tab.getSelectedTabs(sourceTabOrTabs.windowId) : [sourceTabOrTabs];
  }
  if (sourceTabs.length === 0)
    return [];
  const tabs = await TabsOpen.openURIsInTabs(sourceTabs.map(tab => tab.url), {
    isOrphan: true,
    windowId: sourceTabs[0].windowId,
    cookieStoreId
  });
  await Tree.behaveAutoAttachedTabs(tabs, {
    baseTabs:  sourceTabs,
    behavior:  configs.autoAttachOnDuplicated,
    broadcast: true
  });
  return tabs;
}


SidebarConnection.onMessage.addListener(async (windowId, message) => {
  switch (message.type) {
    case Constants.kCOMMAND_NEW_TAB_AS: {
      const baseTab = Tab.get(message.baseTabId);
      if (baseTab)
        openNewTabAs({
          baseTab,
          as:            message.as,
          cookieStoreId: message.cookieStoreId,
          inBackground:  message.inBackground
        });
    }; break;

    case Constants.kCOMMAND_PERFORM_TABS_DRAG_DROP:
      performTabsDragDropWithMessage(message);
      break;

    case Constants.kCOMMAND_TOGGLE_MUTED: {
      await Tab.waitUntilTracked(message.tabId);
      const root = Tab.get(message.tabId);
      log('toggle muted state: ', message, root);
      if (!root)
        break;

      const multiselected = root.$TST.multiselected;
      const tabs = multiselected ?
        Tab.getSelectedTabs(root.windowId, { iterator: true }) :
        [root] ;
      const toBeMuted = (!multiselected && root.$TST.subtreeCollapsed) ?
        !root.$TST.maybeMuted :
        !root.$TST.muted ;

      log('  toBeMuted: ', toBeMuted);
      if (!multiselected &&
          root.$TST.subtreeCollapsed) {
        const tabsInTree = [root, ...root.$TST.descendants];
        let toBeUpdatedTabs = tabsInTree.filter(tab =>
          // The "audible" possibly become "false" when the tab is
          // really audible but muted.
          // However, we can think more simply and robustly.
          //  - We need to mute "audible" tabs.
          //  - We need to unmute "muted" tabs.
          // So, tabs which any of "audible" or "muted" is "true"
          // have enough reason to be updated.
          (tab.audible || tab.mutedInfo.muted) &&
            // And we really need to update only tabs not been the
            // expected state.
            (tab.$TST.muted != toBeMuted)
        );
        // but if there is no target tab, we should update all of the tab and descendants.
        if (toBeUpdatedTabs.length == 0)
          toBeUpdatedTabs = tabsInTree.filter(tab =>
            tab.$TST.muted != toBeMuted
          );
        log('  toBeUpdatedTabs: ', toBeUpdatedTabs);
        for (const tab of toBeUpdatedTabs) {
          browser.tabs.update(tab.id, {
            muted: toBeMuted
          }).catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
        }
      }
      else {
        log('  tabs: ', tabs);
        for (const tab of tabs) {
          browser.tabs.update(tab.id, {
            muted: toBeMuted
          }).catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
        }
      }
    }; break;
  }
});

// for automated tests
browser.runtime.onMessage.addListener((message, _sender) => {
  switch (message.type) {
    case Constants.kCOMMAND_PERFORM_TABS_DRAG_DROP:
      performTabsDragDropWithMessage(message);
      break;
  }
});


const DESCENDANT_MATCHER = /^(>+) /;

async function collectBookmarkItems(root, recursively) {
  let items = await browser.bookmarks.getChildren(root.id);
  if (recursively) {
    let expandedItems = [];
    for (const item of items) {
      switch (item.type) {
        case 'bookmark':
          expandedItems.push(item);
          break;
        case 'folder':
          expandedItems = expandedItems.concat(await collectBookmarkItems(item, recursively));
          break;
      }
    }
    items = expandedItems;
  }
  else {
    items = items.filter(item => item.type == 'bookmark');
  }
  if (countMatched(items, item => !DESCENDANT_MATCHER.test(item.title)) > 1) {
    for (const item of items) {
      item.title = DESCENDANT_MATCHER.test(item.title) ?
        item.title.replace(DESCENDANT_MATCHER, '>$1 ') :
        `> ${item.title}`;
    }
    items.unshift({
      title: '',
      url:   TabsGroup.makeGroupTabURI({
        title:               root.title,
        temporaryAggressive: true
      }),
      group: true,
      discarded: false
    });
  }
  return items;
}

export async function openBookmarksWithStructure(items, { activeIndex = 0, discarded } = {}) {
  if (typeof discarded == 'undefined')
    discarded = configs.openAllBookmarksWithStructureDiscarded;

  const lastItemIndicesWithLevel = new Map();
  let lastMaxLevel = 0;
  const structure = items.reduce((result, item, index) => {
    const { cookieStoreId, url } = ContextualIdentities.getIdFromBookmark(item);
    if (cookieStoreId) {
      item.cookieStoreId = cookieStoreId;
      if (url)
        item.url = url;
    }

    let level = 0;
    if (lastItemIndicesWithLevel.size > 0 &&
        item.title.match(DESCENDANT_MATCHER)) {
      level = RegExp.$1.length;
      if (level - lastMaxLevel > 1) {
        level = lastMaxLevel + 1;
      }
      else {
        while (lastMaxLevel > level) {
          lastItemIndicesWithLevel.delete(lastMaxLevel--);
        }
      }
      lastItemIndicesWithLevel.set(level, index);
      lastMaxLevel = level;
      result.push(lastItemIndicesWithLevel.get(level - 1) - lastItemIndicesWithLevel.get(0));
      item.title = item.title.replace(DESCENDANT_MATCHER, '')
    }
    else {
      result.push(-1);
      lastItemIndicesWithLevel.clear();
      lastItemIndicesWithLevel.set(0, index);
    }
    return result;
  }, []);

  const windowId = TabsStore.getCurrentWindowId() || (await browser.windows.getCurrent()).id;
  const tabs = await TabsOpen.openURIsInTabs(
    // we need to isolate it - unexpected parameter like "index" will break the behavior.
    items.map(bookmark => ({
      url:           bookmark.url,
      title:         bookmark.title,
      cookieStoreId: bookmark.cookieStoreId,
    })),
    {
      windowId,
      isOrphan:     true,
      inBackground: true,
      fixPositions: true,
      discarded,
    }
  );

  if (tabs.length > activeIndex)
    TabsInternalOperation.activateTab(tabs[activeIndex]);
  if (tabs.length == structure.length)
    await Tree.applyTreeStructureToTabs(tabs, structure);

  // tabs can be opened at middle of an existing tree due to browser.tabs.insertAfterCurrent=true
  const referenceTabs = TreeBehavior.calculateReferenceTabsFromInsertionPosition(tabs, {
    context:      Constants.kINSERTION_CONTEXT_CREATED,
    insertAfter:  tabs[0].$TST.previousTab,
    insertBefore: tabs[tabs.length - 1].$TST.nextTab
  });
  if (referenceTabs.parent)
    await Tree.attachTabTo(tabs[0], referenceTabs.parent, {
      insertAfter:  referenceTabs.insertAfter,
      insertBefore: referenceTabs.insertBefore
    });
}

export async function openAllBookmarksWithStructure(id, { discarded, recursively } = {}) {
  if (typeof discarded == 'undefined')
    discarded = configs.openAllBookmarksWithStructureDiscarded;

  let item = await browser.bookmarks.get(id);
  if (Array.isArray(item))
    item = item[0];
  if (!item)
    return;

  if (item.type != 'folder') {
    item = await browser.bookmarks.get(item.parentId);
    if (Array.isArray(item))
      item = item[0];
  }

  const items = await collectBookmarkItems(item, recursively);
  const activeIndex = items.findIndex(item => !item.group);

  openBookmarksWithStructure(items, { activeIndex, discarded });
}


export function sendTabsToDevice(tabs, { to, recursively } = {}) {
  if (recursively)
    tabs = Tab.collectRootTabs(tabs).map(tab => [tab, ...tab.$TST.descendants]).flat();
  tabs = tabs.filter(Sync.isSendableTab);
  Sync.sendMessage(to, getTabsDataToSend(tabs));

  const multiple = tabs.length > 1 ? '_multiple' : '';
  notify({
    title: browser.i18n.getMessage(
      `sentTabs_notification_title${multiple}`,
      [Sync.getDeviceName(to)]
    ),
    message: browser.i18n.getMessage(
      `sentTabs_notification_message${multiple}`,
      [Sync.getDeviceName(to)]
    ),
    timeout: configs.syncSentTabsNotificationTimeout
  });
}

export async function sendTabsToAllDevices(tabs, { recursively } = {}) {
  if (recursively)
    tabs = Tab.collectRootTabs(tabs).map(tab => [tab, ...tab.$TST.descendants]).flat();
  tabs = tabs.filter(Sync.isSendableTab);
  const data = getTabsDataToSend(tabs);
  const devices = await Sync.getOtherDevices();
  for (const device of devices) {
    Sync.sendMessage(device.id, data);
  }

  const multiple = tabs.length > 1 ? '_multiple' : '';
  notify({
    title:   browser.i18n.getMessage(`sentTabsToAllDevices_notification_title${multiple}`),
    message: browser.i18n.getMessage(`sentTabsToAllDevices_notification_message${multiple}`),
    timeout: configs.syncSentTabsNotificationTimeout
  });
}

function getTabsDataToSend(tabs) {
  return {
    type:       Constants.kSYNC_DATA_TYPE_TABS,
    tabs:       tabs.map(tab => ({ url: tab.url, cookieStoreId: tab.cookieStoreId })),
    structure : TreeBehavior.getTreeStructureFromTabs(tabs).map(item => item.parent)
  };
}

export function manageSyncDevices(windowId) {
  browser.tabs.create({
    windowId,
    url: `${Constants.kSHORTHAND_URIS.options}#syncTabsToDeviceOptions`
  });
}

Sync.onMessage.addListener(async message => {
  const data = message.data;
  if (data.type != Constants.kSYNC_DATA_TYPE_TABS ||
      !Array.isArray(data.tabs))
    return;

  const multiple = data.tabs.length > 1 ? '_multiple' : '';
  notify({
    title: browser.i18n.getMessage(
      `receiveTabs_notification_title${multiple}`,
      [Sync.getDeviceName(message.from)]
    ),
    message: browser.i18n.getMessage(
      `receiveTabs_notification_message${multiple}`,
      data.tabs.length > 1 ?
        [data.tabs[0].url, data.tabs.length, data.tabs.length - 1] :
        [data.tabs[0].url]
    ),
    timeout: configs.syncReceivedTabsNotificationTimeout
  });

  const windowId = TabsStore.getCurrentWindowId() || (await browser.windows.getCurrent()).id;
  const window = TabsStore.windows.get(windowId);
  const initialIndex = window.tabs.size;
  window.toBeOpenedOrphanTabs += data.tabs.length;
  let index = 0;
  const tabs = [];
  for (const tab of data.tabs) {
    const createParams = {
      windowId,
      url:    tab.url,
      index:  initialIndex + index,
      active: index == 0
    };
    if (tab.cookieStoreId &&
        tab.cookieStoreId != 'firefox-default' &&
        ContextualIdentities.get(tab.cookieStoreId))
      createParams.cookieStoreId = tab.cookieStoreId;
    let openedTab;
    try {
      openedTab = await browser.tabs.create(createParams);
    }
    catch(error) {
      console.log(error);
    }
    if (!openedTab)
      openedTab = await browser.tabs.create({
        ...createParams,
        url: `about:blank?${tab.url}`
      });
    tabs.push(openedTab);
    index++;
  }

  if (!Array.isArray(data.structure))
    return;

  await wait(100); // wait for all tabs are tracked
  await Tree.applyTreeStructureToTabs(tabs.map(tab => Tab.get(tab.id)), data.structure, {
    broadcast: true
  });
});

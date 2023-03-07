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

import EventListenerManager from '/extlib/EventListenerManager.js';

import {
  log as internalLogger,
  wait,
  dumpTab,
  mapAndFilter,
  configs,
  shouldApplyAnimation,
  getWindowParamsFromSource,
} from '/common/common.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as Constants from '/common/constants.js';
import * as MetricsData from '/common/metrics-data.js';
import * as SidebarConnection from '/common/sidebar-connection.js';
import * as TabsInternalOperation from '/common/tabs-internal-operation.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TreeBehavior from '/common/tree-behavior.js';
import * as TSTAPI from '/common/tst-api.js';
import * as UserOperationBlocker from '/common/user-operation-blocker.js';

import Tab from '/common/Tab.js';
import Window from '/common/Window.js';

import * as TabsMove from './tabs-move.js';

function log(...args) {
  internalLogger('background/tree', ...args);
}
function logCollapseExpand(...args) {
  internalLogger('sidebar/collapse-expand', ...args);
}


export const onAttached     = new EventListenerManager();
export const onDetached     = new EventListenerManager();
export const onSubtreeCollapsedStateChanging = new EventListenerManager();
export const onSubtreeCollapsedStateChanged  = new EventListenerManager();


const mUnattachableTabIds = new Set();

export function markTabIdAsUnattachable(id) {
  mUnattachableTabIds.add(id);
}

export function clearUnattachableTabId(id) {
  mUnattachableTabIds.delete(id);
}

function isTabIdUnattachable(id) {
  return mUnattachableTabIds.has(id);
}


// return moved (or not)
export async function attachTabTo(child, parent, options = {}) {
  parent = TabsStore.ensureLivingTab(parent);
  child = TabsStore.ensureLivingTab(child);
  if (!parent || !child) {
    log('missing information: ', { parent, child });
    return false;
  }

  log('attachTabTo: ', {
    child:            child.id,
    parent:           parent.id,
    children:         parent.$TST.getAttribute(Constants.kCHILDREN),
    insertAt:         options.insertAt,
    insertBefore:     options.insertBefore && options.insertBefore.id,
    insertAfter:      options.insertAfter && options.insertAfter.id,
    lastRelatedTab:   options.lastRelatedTab && options.lastRelatedTab.id,
    dontMove:         options.dontMove,
    dontUpdateIndent: options.dontUpdateIndent,
    forceExpand:      options.forceExpand,
    dontExpand:       options.dontExpand,
    delayedMove:      options.delayedMove,
    dontSyncParentToOpenerTab: options.dontSyncParentToOpenerTab,
    broadcast:        options.broadcast,
    broadcasted:      options.broadcasted,
    stack:            `${configs.debug && new Error().stack}\n${options.stack || ''}`
  });

  if (isTabIdUnattachable(child.id)) {
    log('=> do not attach an unattachable tab to another (maybe already removed)');
    return false;
  }
  if (isTabIdUnattachable(parent.id)) {
    log('=> do not attach to an unattachable tab (maybe already removed)');
    return false;
  }

  if (parent.pinned || child.pinned) {
    log('=> pinned tabs cannot be attached');
    return false;
  }
  if (parent.windowId != child.windowId) {
    log('=> could not attach tab to a parent in different window');
    return false;
  }
  const ancestors = [parent].concat(parent.$TST.ancestors);
  if (ancestors.includes(child)) {
    log('=> canceled for recursive request');
    return false;
  }

  if (options.dontMove) {
    log('=> do not move');
    options.insertBefore = child.$TST.nextTab;
    if (!options.insertBefore)
      options.insertAfter = child.$TST.previousTab;
  }

  if (!options.insertBefore && !options.insertAfter) {
    const refTabs = getReferenceTabsForNewChild(child, parent, options);
    options.insertBefore = refTabs.insertBefore;
    options.insertAfter  = refTabs.insertAfter;
    log('=> calculate reference tabs ', refTabs);
  }
  options.insertAfter = options.insertAfter || parent;
  log(`reference tabs for ${child.id}: `, {
    insertBefore: options.insertBefore,
    insertAfter:  options.insertAfter
  });

  if (!options.synchronously)
    await Tab.waitUntilTrackedAll(child.windowId);

  parent = TabsStore.ensureLivingTab(parent);
  child = TabsStore.ensureLivingTab(child);
  if (!parent || !child) {
    log('attachTabTo: parent or child is closed before attaching.');
    return false;
  }
  if (isTabIdUnattachable(child.id) || isTabIdUnattachable(parent.id)) {
    log('attachTabTo: parent or child is marked as unattachable (maybe already removed)');
    return false;
  }

  const newIndex = Tab.calculateNewTabIndex({
    insertBefore: options.insertBefore,
    insertAfter:  options.insertAfter,
    ignoreTabs:   [child]
  });
  const moved = newIndex != child.index;
  log(`newIndex for ${child.id}: `, newIndex);

  const newlyAttached = (
    !parent.$TST.childIds.includes(child.id) ||
    child.$TST.parentId != parent.id
  );
  if (!newlyAttached)
    log('=> already attached');

  if (newlyAttached) {
    detachTab(child, {
      ...options,
      // Don't broadcast this detach operation, because this "attachTabTo" can be
      // broadcasted. If we broadcast this detach operation, the tab is detached
      // twice in the sidebar!
      broadcast: false
    });

    log('attachTabTo: setting child information to ', parent.id);
    // we need to set its children via the "children" setter, to invalidate cached information.
    parent.$TST.children = parent.$TST.childIds.concat([child.id]);

    // We don't need to update its parent information, because the parent's
    // "children" setter updates the child itself automatically.

    const parentLevel = parseInt(parent.$TST.getAttribute(Constants.kLEVEL) || 0);
    if (!options.dontUpdateIndent)
      updateTabsIndent(child, parentLevel + 1, { justNow: options.synchronously });

    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_CHILDREN_CHANGED,
      windowId: parent.windowId,
      tabId:    parent.id,
      childIds: parent.$TST.childIds,
      addedChildIds:   [child.id],
      removedChildIds: [],
      newlyAttached
    });
    if (TSTAPI.hasListenerForMessageType(TSTAPI.kNOTIFY_TREE_ATTACHED)) {
      const cache = {};
      TSTAPI.sendMessage({
        type:   TSTAPI.kNOTIFY_TREE_ATTACHED,
        tab:    new TSTAPI.TreeItem(child, { cache }),
        parent: new TSTAPI.TreeItem(parent, { cache })
      }, { tabProperties: ['tab', 'parent'] }).catch(_error => {});
    }
  }

  if (child.openerTabId != parent.id &&
      !options.dontSyncParentToOpenerTab &&
      configs.syncParentTabAndOpenerTab) {
    log(`openerTabId of ${child.id} is changed by TST!: ${child.openerTabId} (original) => ${parent.id} (changed by TST)`, new Error().stack);
    child.openerTabId = parent.id;
    child.$TST.updatingOpenerTabIds.push(parent.id);
    child.$TST.updatedOpenerTabId = child.openerTabId; // workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1409262
    browser.tabs.update(child.id, { openerTabId: parent.id })
      .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
    wait(200).then(() => {
      const index = child.$TST.updatingOpenerTabIds.findIndex(id => id == parent.id);
      child.$TST.updatingOpenerTabIds.splice(index, 1);
    });
  }

  if (newlyAttached)
    await collapseExpandForAttachedTab(child, parent, options);

  if (!options.dontMove) {
    let nextTab = options.insertBefore;
    let prevTab = options.insertAfter;
    if (!nextTab && !prevTab) {
      nextTab = Tab.getTabAt(child.windowId, newIndex);
      if (!nextTab)
        prevTab = Tab.getTabAt(child.windowId, newIndex - 1);
    }
    log('move newly attached child: ', dumpTab(child), {
      next: dumpTab(nextTab),
      prev: dumpTab(prevTab)
    });
    if (!nextTab ||
        // We should not use a descendant of the "child" tab as the reference tab
        // when we are going to attach the "child" and its descendants to the new
        // parent.
        // See also: https://github.com/piroor/treestyletab/issues/2892#issuecomment-862424942
        nextTab.$TST.parent == child) {
      await moveTabSubtreeAfter(child, prevTab, {
        ...options,
        broadcast: true
      });
    }
    else {
      await moveTabSubtreeBefore(child, nextTab, {
        ...options,
        broadcast: true
      });
    }
  }

  child.$TST.opened.then(() => {
    if (!TabsStore.ensureLivingTab(child) || // not removed while waiting
        child.$TST.parent != parent) // not detached while waiting
      return;

    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_TAB_ATTACHED_COMPLETELY,
      windowId: child.windowId,
      childId:  child.id,
      parentId: parent.id,
      newlyAttached
    });
  });

  onAttached.dispatch(child, {
    ...options,
    parent,
    insertBefore: options.insertBefore,
    insertAfter:  options.insertAfter,
    newIndex, newlyAttached
  });

  return !options.dontMove && moved;
}

async function collapseExpandForAttachedTab(tab, parent, options = {}) {
  // Because the tab is possibly closing for "reopen" operation,
  // we need to apply "forceExpand" immediately. Otherwise, when
  // the tab is closed with "subtree collapsed" state, descendant
  // tabs are also closed even if "forceExpand" is "true".
  log('newly attached tab ', tab.id);
  if (parent.$TST.subtreeCollapsed &&
      !options.forceExpand) {
    log('  the tree is collapsed, but keep collapsed by forceExpand option');
    collapseExpandTabAndSubtree(tab, {
      collapsed: true,
      justNow:   true,
      broadcast: true
    });
  }

  const isNewTreeCreatedManually = !options.justNow && parent.$TST.childIds.length == 1;
  let parentTreeCollasped = parent.$TST.subtreeCollapsed;
  let parentCollasped     = parent.$TST.collapsed;

  const cache = {};
  const allowed = (options.forceExpand || !!options.dontExpand) && await TSTAPI.tryOperationAllowed(
    TSTAPI.kNOTIFY_TRY_EXPAND_TREE_FROM_ATTACHED_CHILD,
    { tab: new TSTAPI.TreeItem(parent, { cache }) },
    { tabProperties: ['tab'] }
  );
  if (!TabsStore.ensureLivingTab(tab))
    return;

  if (options.forceExpand && allowed) {
    log(`  expand tab ${tab.id} by forceExpand option`);
    if (parentTreeCollasped)
      collapseExpandSubtree(parent, {
        ...options,
        collapsed: false,
        broadcast: true
      });
    else
      collapseExpandTabAndSubtree(tab, {
        ...options,
        collapsed: false,
        broadcast: true
      });
    parentTreeCollasped = false;
  }
  if (!options.dontExpand) {
    if (allowed) {
      if (configs.autoCollapseExpandSubtreeOnAttach &&
          (isNewTreeCreatedManually ||
           parent.$TST.isAutoExpandable)) {
        log('  collapse others by collapseExpandTreesIntelligentlyFor');
        await collapseExpandTreesIntelligentlyFor(parent, {
          broadcast: true
        });
      }
      if (configs.autoCollapseExpandSubtreeOnSelect ||
          isNewTreeCreatedManually ||
          parent.$TST.isAutoExpandable ||
          options.forceExpand) {
        log('  expand ancestor tabs');
        parentTreeCollasped = false;
        parentCollasped     = false;
        await Promise.all([parent].concat(parent.$TST.ancestors).map(async ancestor => {
          if (!ancestor.$TST.subtreeCollapsed)
            return;
          const allowed = await TSTAPI.tryOperationAllowed(
            TSTAPI.kNOTIFY_TRY_EXPAND_TREE_FROM_ATTACHED_CHILD,
            { tab: new TSTAPI.TreeItem(ancestor, { cache }) },
            { tabProperties: ['tab'] }
          );
          if (!allowed) {
            parentTreeCollasped = true;
            parentCollasped     = true;
            return;
          }
          if (!TabsStore.ensureLivingTab(tab))
            return;
          collapseExpandSubtree(ancestor, {
            ...options,
            collapsed:    false,
            broadcast:    true
          });
          parentTreeCollasped = false;
        }));
        if (!TabsStore.ensureLivingTab(tab))
          return;
      }
    }
  }
  else if (parent.$TST.isAutoExpandable ||
           parent.$TST.collapsed) {
    log('  collapse auto expanded tree');
    collapseExpandTabAndSubtree(tab, {
      ...options,
      collapsed:    true,
      broadcast:    true
    });
  }
  if (parentTreeCollasped || parentCollasped) {
    log('  collapse tab because the parent is collapsed');
    collapseExpandTabAndSubtree(tab, {
      ...options,
      collapsed: true,
      broadcast: true
    });
  }
}

export function getReferenceTabsForNewChild(child, parent, { insertAt, ignoreTabs, lastRelatedTab, children, descendants } = {}) {
  log('getReferenceTabsForNewChild ', { child, parent, insertAt, ignoreTabs, lastRelatedTab, children, descendants });
  if (typeof insertAt !== 'number')
    insertAt = configs.insertNewChildAt;
  log('  insertAt = ', insertAt);
  if (parent && !descendants)
    descendants = parent.$TST.descendants;
  if (ignoreTabs)
    descendants = descendants.filter(tab => !ignoreTabs.includes(tab));
  log('  descendants = ', descendants);
  let insertBefore, insertAfter;
  if (descendants.length > 0) {
    const firstChild     = descendants[0];
    const lastDescendant = descendants[descendants.length - 1];
    switch (insertAt) {
      case Constants.kINSERT_END:
      default:
        insertAfter = lastDescendant;
        log(`  insert ${child && child.id} after lastDescendant ${insertAfter && insertAfter.id} (insertAt=kINSERT_END)`);
        break;
      case Constants.kINSERT_TOP:
        insertBefore = firstChild;
        log(`  insert ${child && child.id} before firstChild ${insertBefore && insertBefore.id} (insertAt=kINSERT_TOP)`);
        break;
      case Constants.kINSERT_NEAREST: {
        const allTabs = Tab.getOtherTabs((child || parent).windowId, ignoreTabs);
        const index = child ? allTabs.indexOf(child) : -1;
        log('  insertAt=kINSERT_NEAREST ', { allTabs, index });
        if (index < allTabs.indexOf(firstChild)) {
          insertBefore = firstChild;
          insertAfter  = parent;
          log(`  insert ${child && child.id} between parent ${insertAfter && insertAfter.id} and firstChild ${insertBefore && insertBefore.id} (insertAt=kINSERT_NEAREST)`);
        }
        else if (index > allTabs.indexOf(lastDescendant)) {
          insertAfter  = lastDescendant;
          log(`  insert ${child && child.id} after lastDescendant ${insertAfter && insertAfter.id} (insertAt=kINSERT_NEAREST)`);
        }
        else { // inside the tree
          if (parent && !children)
            children = parent.$TST.children;
          if (ignoreTabs)
            children = children.filter(tab => !ignoreTabs.includes(tab));
          for (const child of children) {
            if (index > allTabs.indexOf(child))
              continue;
            insertBefore = child;
            log(`  insert ${child && child.id} before nearest following child ${insertBefore && insertBefore.id} (insertAt=kINSERT_NEAREST)`);
            break;
          }
          if (!insertBefore) {
            insertAfter = lastDescendant;
            log(`  insert ${child && child.id} after lastDescendant ${insertAfter && insertAfter.id} (insertAt=kINSERT_NEAREST)`);
          }
        }
      }; break;
      case Constants.kINSERT_NEXT_TO_LAST_RELATED_TAB: {
        // Simulates Firefox's default behavior with `browser.tabs.insertRelatedAfterCurrent`=`true`.
        // The result will become same to kINSERT_NO_CONTROL case,
        // but this is necessary for environments with disabled the preference.
        if ((lastRelatedTab === undefined) && parent)
          lastRelatedTab = child && parent.$TST.lastRelatedTabId == child.id ? parent.$TST.previousLastRelatedTab : parent.$TST.lastRelatedTab; // it could be updated already...
        if (lastRelatedTab) {
          insertAfter  = lastRelatedTab.$TST.lastDescendant || lastRelatedTab;
          log(`  insert ${child && child.id} after lastRelatedTab ${lastRelatedTab.id} (insertAt=kINSERT_NEXT_TO_LAST_RELATED_TAB)`);
        }
        else {
          insertBefore = firstChild;
          log(`  insert ${child && child.id} before firstChild (insertAt=kINSERT_NEXT_TO_LAST_RELATED_TAB)`);
        }
      }; break;
      case Constants.kINSERT_NO_CONTROL:
        break;
    }
  }
  else {
    insertAfter = parent;
    log(`  insert ${child && child.id} after parent`);
  }
  if (insertBefore == child) {
    // Return unsafe tab, to avoid placing the child after hidden tabs
    // (too far from the place it should be.)
    insertBefore = insertBefore && insertBefore.$TST.unsafeNextTab;
    log(`  => insert ${child && child.id} before next tab ${insertBefore && insertBefore.id} of the child tab itelf`);
  }
  if (insertAfter == child) {
    insertAfter = insertAfter && insertAfter.$TST.previousTab;
    log(`  => insert ${child && child.id} after previous tab ${insertAfter && insertAfter.id} of the child tab itelf`);
  }
  // disallow to place tab in invalid position
  if (insertBefore) {
    if (parent && insertBefore.index <= parent.index) {
      insertBefore = null;
      log(`  => do not put ${child && child.id} before a tab preceding to the parent`);
    }
    //TODO: we need to reject more cases...
  }
  if (insertAfter) {
    const allTabsInTree = [...descendants];
    if (parent)
      allTabsInTree.unshift(parent);
    const lastMember    = allTabsInTree[allTabsInTree.length - 1];
    if (insertAfter.index >= lastMember.index) {
      insertAfter = lastMember;
      log(`  => do not put ${child && child.id} after the last tab ${insertAfter && insertAfter.id} in the tree`);
    }
    //TODO: we need to reject more cases...
  }
  return { insertBefore, insertAfter };
}

export function getReferenceTabsForNewNextSibling(base, options = {}) {
  log('getReferenceTabsForNewNextSibling ', base);
  let insertBefore = base.$TST.nextSiblingTab;
  if (insertBefore &&
      insertBefore.pinned &&
      !options.pinned) {
    insertBefore = Tab.getFirstNormalTab(base.windowId);
  }
  let insertAfter  = base.$TST.lastDescendant || base;
  if (insertAfter &&
      !insertAfter.pinned &&
      options.pinned) {
    insertAfter = Tab.getLastPinnedTab(base.windowId);
  }
  return { insertBefore, insertAfter };
}

export function detachTab(child, options = {}) {
  log('detachTab: ', child.id, options,
      { stack: `${configs.debug && new Error().stack}\n${options.stack || ''}` });
  // the "parent" option is used for removing child.
  const parent = TabsStore.ensureLivingTab(options.parent) || child.$TST.parent;

  if (parent) {
    // we need to set children and parent via setters, to invalidate cached information.
    parent.$TST.children = parent.$TST.childIds.filter(id => id != child.id);
    log('detachTab: children information is updated ', parent.id, parent.$TST.childIds);
    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_CHILDREN_CHANGED,
      windowId: parent.windowId,
      tabId:    parent.id,
      childIds: parent.$TST.childIds,
      addedChildIds:   [],
      removedChildIds: [child.id],
      detached: true
    });
    if (TSTAPI.hasListenerForMessageType(TSTAPI.kNOTIFY_TREE_DETACHED)) {
      const cache = {};
      TSTAPI.sendMessage({
        type:      TSTAPI.kNOTIFY_TREE_DETACHED,
        tab:       new TSTAPI.TreeItem(child, { cache }),
        oldParent: new TSTAPI.TreeItem(parent, { cache })
      }, { tabProperties: ['tab', 'oldParent'] }).catch(_error => {});
    }
    // We don't need to clear its parent information, because the old parent's
    // "children" setter removes the parent ifself from the detached child
    // automatically.
  }
  else {
    log(` => parent(${child.$TST.parentId}) is already removed, or orphan tab`);
    // This can happen when the parent tab was detached via the native tab bar
    // or Firefox's built-in command to detach tab from window.
  }

  if (!options.toBeRemoved && !options.toBeDetached)
    updateTabsIndent(child);

  if (child.openerTabId &&
      !options.dontSyncParentToOpenerTab &&
      configs.syncParentTabAndOpenerTab) {
    log(`openerTabId of ${child.id} is cleared by TST!: ${child.openerTabId} (original)`, configs.debug && new Error().stack);
    child.openerTabId = child.id;
    child.$TST.updatedOpenerTabId = child.openerTabId; // workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1409262
    browser.tabs.update(child.id, { openerTabId: child.id }) // set self id instead of null, because it requires any valid tab id...
      .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
  }

  onDetached.dispatch(child, {
    oldParentTab: parent,
    toBeRemoved:  !!options.toBeRemoved,
    toBeDetached: !!options.toBeDetached
  });
}

export async function detachTabsFromTree(tabs, options = {}) {
  if (!Array.isArray(tabs))
    tabs = [tabs];
  tabs = Array.from(tabs).reverse();
  const promisedAttach = [];
  for (const tab of tabs) {
    const children = tab.$TST.children;
    const parent   = tab.$TST.parent;
    for (const child of children) {
      if (!tabs.includes(child)) {
        if (parent) {
          promisedAttach.push(attachTabTo(child, parent, {
            ...options,
            dontMove: true
          }));
        }
        else {
          detachTab(child, options);
          if (child.$TST.collapsed)
            await collapseExpandTabAndSubtree(child, {
              ...options,
              collapsed: false
            });
        }
      }
    }
  }
  if (promisedAttach.length > 0)
    await Promise.all(promisedAttach);
}

export async function detachAllChildren(
  tab = null,
  { windowId, children, descendants, parent, nearestFollowingRootTab, newParent, behavior, dontExpand, dontSyncParentToOpenerTab,
    ...options } = {}
) {
  if (tab) {
    windowId    = tab.$TST.windowId;
    parent      = tab.$TST.parent;
    children    = tab.$TST.children;
    descendants = tab.$TST.descendants;
  }
  log('detachAllChildren: ',
      tab && tab.id,
      { children, parent, nearestFollowingRootTab, newParent, behavior, dontExpand, dontSyncParentToOpenerTab },
      options);
  // the "children" option is used for removing tab.
  children = children ? children.map(TabsStore.ensureLivingTab) : tab.$TST.children;

  if (behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD &&
      newParent &&
      !children.includes(newParent))
    children.unshift(newParent);

  if (!children.length)
    return;
  log(' => children to be detached: ', () => children.map(dumpTab));

  if (behavior === undefined)
    behavior = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_SIMPLY_DETACH_ALL_CHILDREN;
  if (behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE)
    behavior = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD;

  options.dontUpdateInsertionPositionInfo = true;

  // the "parent" option is used for removing tab.
  parent = TabsStore.ensureLivingTab(parent) || (tab && tab.$TST.parent);
  if (tab &&
      tab.$TST.isGroupTab &&
      Tab.getRemovingTabs(tab.windowId).length == children.length) {
    behavior = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN;
    options.dontUpdateIndent = false;
  }

  let previousTab = null;
  let nextTab = null;
  if (behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN &&
      !configs.moveTabsToBottomWhenDetachedFromClosedParent) {
    nextTab = nearestFollowingRootTab !== undefined ?
      nearestFollowingRootTab :
      tab && tab.$TST.nearestFollowingRootTab;
    previousTab = nextTab ?
      nextTab.$TST.previousTab :
      Tab.getLastTab(windowId || tab.windowId);
    const descendantsSet = new Set(descendants || tab.$TST.descendants);
    while (previousTab && (!tab || descendantsSet.has(previousTab))) {
      previousTab = previousTab.$TST.previousTab;
    }
  }

  if (behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB) {
    // open new group tab and replace the detaching tab with it.
    behavior = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN;
  }

  if (!dontExpand &&
      ((tab && !tab.$TST.collapsed) ||
       behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN)) {
    if (tab) {
      await collapseExpandSubtree(tab, {
        ...options,
        collapsed: false
      });
    }
    else {
      for (const child of children) {
        await collapseExpandTabAndSubtree(child, {
          ...options,
          collapsed:   false,
          forceExpand: behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
        });
      }
    }
  }

  let count = 0;
  for (const child of children) {
    if (!child)
      continue;
    const promises = [];
    if (behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN) {
      promises.push(detachTab(child, { ...options, dontSyncParentToOpenerTab }));

      // reference tabs can be closed while waiting...
      if (nextTab && nextTab.$TST.removing)
        nextTab = null;
      if (previousTab && previousTab.$TST.removing)
        previousTab = null;

      if (nextTab) {
        promises.push(moveTabSubtreeBefore(child, nextTab, options));
      }
      else {
        promises.push(moveTabSubtreeAfter(child, previousTab, options));
        previousTab = child.$TST.lastDescendant || child;
      }
    }
    else if (behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD) {
      promises.push(detachTab(child, { ...options, dontSyncParentToOpenerTab }));
      if (count == 0) {
        if (parent) {
          promises.push(attachTabTo(child, parent, {
            ...options,
            dontSyncParentToOpenerTab,
            dontExpand: true,
            dontMove:   true
          }));
        }
        promises.push(collapseExpandSubtree(child, {
          ...options,
          collapsed: false
        }));
        //deleteTabValue(child, Constants.kTAB_STATE_SUBTREE_COLLAPSED);
      }
      else {
        promises.push(attachTabTo(child, children[0], {
          ...options,
          dontSyncParentToOpenerTab,
          dontExpand: true,
          dontMove:   true
        }));
      }
    }
    else if (behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN &&
             parent) {
      promises.push(attachTabTo(child, parent, {
        ...options,
        dontSyncParentToOpenerTab,
        dontExpand: true,
        dontMove:   true
      }));
    }
    else { // behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_SIMPLY_DETACH_ALL_CHILDREN
      promises.push(detachTab(child, { ...options, dontSyncParentToOpenerTab }));
    }
    count++;
    await Promise.all(promises);
  }
}

// returns moved (or not)
export async function behaveAutoAttachedTab(
  tab,
  { baseTab, behavior, broadcast, dontMove } = {}
) {
  if (!configs.autoAttach)
    return false;

  baseTab = baseTab || Tab.getActiveTab(TabsStore.getCurrentWindowId() || tab.windowId);
  log('behaveAutoAttachedTab ', tab.id, baseTab.id, { baseTab, behavior });

  if (baseTab &&
      baseTab.$TST.ancestors.includes(tab)) {
    log(' => ignore possibly restored ancestor tab to avoid cyclic references');
    return false;
  }

  if (baseTab.pinned) {
    if (!tab.pinned)
      return false;
    behavior = Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING;
    log(' => override behavior for pinned tabs');
  }
  switch (behavior) {
    default:
      return false;

    case Constants.kNEWTAB_OPEN_AS_ORPHAN:
      log(' => kNEWTAB_OPEN_AS_ORPHAN');
      detachTab(tab, {
        broadcast
      });
      if (tab.$TST.nextTab)
        return TabsMove.moveTabAfter(tab, Tab.getLastTab(tab.windowId), {
          delayedMove: true
        });
      return false;

    case Constants.kNEWTAB_OPEN_AS_CHILD:
      log(' => kNEWTAB_OPEN_AS_CHILD');
      return attachTabTo(tab, baseTab, {
        dontMove:    dontMove || configs.insertNewChildAt == Constants.kINSERT_NO_CONTROL,
        forceExpand: true,
        delayedMove: true,
        broadcast
      });

    case Constants.kNEWTAB_OPEN_AS_SIBLING: {
      log(' => kNEWTAB_OPEN_AS_SIBLING');
      const parent = baseTab.$TST.parent;
      if (parent) {
        await attachTabTo(tab, parent, {
          delayedMove: true,
          broadcast
        });
        return true;
      }
      else {
        detachTab(tab, {
          broadcast
        });
        return TabsMove.moveTabAfter(tab, Tab.getLastTab(tab.windowId), {
          delayedMove: true
        });
      }
    };

    case Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING:
    case Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING_WITH_INHERITED_CONTAINER: {
      log(' => kNEWTAB_OPEN_AS_NEXT_SIBLING(_WITH_INHERITED_CONTAINER)');
      let nextSibling = baseTab.$TST.nextSiblingTab;
      if (nextSibling == tab)
        nextSibling = null;
      const parent = baseTab.$TST.parent;
      if (parent) {
        return attachTabTo(tab, parent, {
          insertBefore: nextSibling,
          insertAfter:  baseTab.$TST.lastDescendant || baseTab,
          delayedMove:  true,
          broadcast
        });
      }
      else {
        detachTab(tab, {
          broadcast
        });
        if (nextSibling)
          return TabsMove.moveTabBefore(tab, nextSibling, {
            delayedMove: true,
            broadcast
          });
        else
          return TabsMove.moveTabAfter(tab, baseTab.$TST.lastDescendant, {
            delayedMove: true,
            broadcast
          });
      }
    };
  }
}

export async function behaveAutoAttachedTabs(tabs, options = {}) {
  switch (options.behavior) {
    default:
      return false;

    case Constants.kNEWTAB_OPEN_AS_ORPHAN:
      if (options.baseTabs && !options.baseTab)
        options.baseTab = options.baseTabs[options.baseTabs.length - 1];
      for (const tab of tabs) {
        await behaveAutoAttachedTab(tab, options);
      }
      return false;

    case Constants.kNEWTAB_OPEN_AS_CHILD: {
      if (options.baseTabs && !options.baseTab)
        options.baseTab = options.baseTabs[0];
      let moved = false;
      for (const tab of tabs) {
        moved = (await behaveAutoAttachedTab(tab, options)) || moved;
      }
      return moved;
    };

    case Constants.kNEWTAB_OPEN_AS_SIBLING:
    case Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING: {
      if (options.baseTabs && !options.baseTab)
        options.baseTab = options.baseTabs[options.baseTabs.length - 1];
      let moved = false;
      for (const tab of tabs.reverse()) {
        moved = (await behaveAutoAttachedTab(tab, options)) || moved;
      }
      return moved;
    };
  }
}

function updateTabsIndent(tabs, level = undefined, options = {}) {
  if (!tabs)
    return;

  if (!Array.isArray(tabs))
    tabs = [tabs];

  if (!tabs.length)
    return;

  if (level === undefined)
    level = tabs[0].$TST.ancestors.length;

  for (let i = 0, maxi = tabs.length; i < maxi; i++) {
    const item = tabs[i];
    if (!item || item.pinned)
      continue;

    updateTabIndent(item, level, options);
  }
}

// this is called multiple times on a session restoration, so this should be throttled for better performance
function updateTabIndent(tab, level = undefined, options = {}) {
  let timer = updateTabIndent.delayed.get(tab.id);
  if (timer)
    clearTimeout(timer);
  if (options.justNow || !shouldApplyAnimation()) {
    return updateTabIndentNow(tab, level, options);
  }
  timer = setTimeout(() => {
    updateTabIndent.delayed.delete(tab.id);
    updateTabIndentNow(tab, level);
  }, 100);
  updateTabIndent.delayed.set(tab.id, timer);
}
updateTabIndent.delayed = new Map();

function updateTabIndentNow(tab, level = undefined, options = {}) {
  if (!TabsStore.ensureLivingTab(tab))
    return;
  tab.$TST.setAttribute(Constants.kLEVEL, level);
  updateTabsIndent(tab.$TST.children, level + 1, options);
  SidebarConnection.sendMessage({
    type:     Constants.kCOMMAND_NOTIFY_TAB_LEVEL_CHANGED,
    windowId: tab.windowId,
    tabId:    tab.id,
    level
  });
}


// collapse/expand tabs

export async function collapseExpandSubtree(tab, params = {}) {
  params.collapsed = !!params.collapsed;
  if (!tab || !TabsStore.ensureLivingTab(tab))
    return;
  if (!TabsStore.ensureLivingTab(tab)) // it was removed while waiting
    return;
  params.stack = `${configs.debug && new Error().stack}\n${params.stack || ''}`;
  logCollapseExpand('collapseExpandSubtree: ', dumpTab(tab), tab.$TST.subtreeCollapsed, params);
  await collapseExpandSubtreeInternal(tab, params);
  onSubtreeCollapsedStateChanged.dispatch(tab, { collapsed: !!params.collapsed });
  if (TSTAPI.hasListenerForMessageType(TSTAPI.kNOTIFY_TREE_COLLAPSED_STATE_CHANGED)) {
    TSTAPI.sendMessage({
      type:      TSTAPI.kNOTIFY_TREE_COLLAPSED_STATE_CHANGED,
      tab:       new TSTAPI.TreeItem(tab),
      collapsed: !!params.collapsed
    }, { tabProperties: ['tab'] }).catch(_error => {});
  }
}
async function collapseExpandSubtreeInternal(tab, params = {}) {
  if (!params.force &&
      tab.$TST.subtreeCollapsed == params.collapsed)
    return;

  if (params.collapsed) {
    tab.$TST.addState(Constants.kTAB_STATE_SUBTREE_COLLAPSED);
    tab.$TST.removeState(Constants.kTAB_STATE_SUBTREE_EXPANDED_MANUALLY);
  }
  else {
    tab.$TST.removeState(Constants.kTAB_STATE_SUBTREE_COLLAPSED);
  }
  //setTabValue(tab, Constants.kTAB_STATE_SUBTREE_COLLAPSED, params.collapsed);

  const childTabs = tab.$TST.children;
  const lastExpandedTabIndex = childTabs.length - 1;
  for (let i = 0, maxi = childTabs.length; i < maxi; i++) {
    const childTab = childTabs[i];
    if (i == lastExpandedTabIndex &&
        !params.collapsed) {
      await collapseExpandTabAndSubtree(childTab, {
        collapsed: params.collapsed,
        justNow:   params.justNow,
        anchor:    tab,
        last:      true,
        broadcast: false
      });
    }
    else {
      await collapseExpandTabAndSubtree(childTab, {
        collapsed: params.collapsed,
        justNow:   params.justNow,
        broadcast: false
      });
    }
  }

  onSubtreeCollapsedStateChanging.dispatch(tab, { collapsed: params.collapsed });
  SidebarConnection.sendMessage({
    type:      Constants.kCOMMAND_NOTIFY_SUBTREE_COLLAPSED_STATE_CHANGED,
    windowId:  tab.windowId,
    tabId:     tab.id,
    collapsed: !!params.collapsed,
    justNow:   params.justNow,
    anchorId:  tab.id,
    last:      true
  });
}

export function manualCollapseExpandSubtree(tab, params = {}) {
  params.manualOperation = true;
  collapseExpandSubtree(tab, params);
  if (!params.collapsed) {
    tab.$TST.addState(Constants.kTAB_STATE_SUBTREE_EXPANDED_MANUALLY);
    //setTabValue(tab, Constants.kTAB_STATE_SUBTREE_EXPANDED_MANUALLY, true);
  }
}

export async function collapseExpandTabAndSubtree(tab, params = {}) {
  if (!tab)
    return;

  // allow to expand root collapsed tab
  if (!tab.$TST.collapsed &&
      !tab.$TST.parent)
    return;

  collapseExpandTab(tab, params);

  if (params.collapsed &&
      tab.active &&
      configs.unfocusableCollapsedTab) {
    logCollapseExpand('current tree is going to be collapsed');
    const allowed = await TSTAPI.tryOperationAllowed(
      TSTAPI.kNOTIFY_TRY_MOVE_FOCUS_FROM_COLLAPSING_TREE,
      { tab: new TSTAPI.TreeItem(tab) },
      { tabProperties: ['tab'] }
    );
    if (allowed) {
      let newSelection = tab.$TST.nearestVisibleAncestorOrSelf;
      if (configs.avoidDiscardedTabToBeActivatedIfPossible && newSelection.discarded)
        newSelection = newSelection.$TST.nearestLoadedTabInTree ||
                         newSelection.$TST.nearestLoadedTab ||
                         newSelection;
      logCollapseExpand('=> switch to ', newSelection.id);
      TabsInternalOperation.activateTab(newSelection, { silently: true });
    }
  }

  if (!tab.$TST.subtreeCollapsed) {
    const children = tab.$TST.children;
    await Promise.all(children.map((child, index) => {
      const last = params.last &&
                     (index == children.length - 1);
      return collapseExpandTabAndSubtree(child, {
        ...params,
        collapsed: params.collapsed,
        justNow:   params.justNow,
        anchor:    last && params.anchor,
        last:      last,
        broadcast: params.broadcast
      });
    }));
  }
}

export async function collapseExpandTab(tab, params = {}) {
  if (tab.pinned && params.collapsed) {
    log('CAUTION: a pinned tab is going to be collapsed, but canceled.',
        dumpTab(tab), { stack: configs.debug && new Error().stack });
    params.collapsed = false;
  }

  // When an asynchronous "expand" operation is processed after a
  // synchronous "collapse" operation, it can produce an expanded
  // child tab under "subtree-collapsed" parent. So this is a failsafe.
  if (!params.forceExpand &&
      !params.collapsed &&
      tab.$TST.ancestors.some(ancestor => ancestor.$TST.subtreeCollapsed)) {
    log('collapseExpandTab: canceled to avoid expansion under collapsed tree ',
        tab.$TST.ancestors.find(ancestor => ancestor.$TST.subtreeCollapsed));
    return;
  }

  const stack = `${configs.debug && new Error().stack}\n${params.stack || ''}`;
  logCollapseExpand(`collapseExpandTab ${tab.id} `, params, { stack })
  const last = params.last &&
                 (!tab.$TST.hasChild || tab.$TST.subtreeCollapsed);
  const byAncestor = tab.$TST.ancestors.some(ancestor => ancestor.$TST.subtreeCollapsed) == params.collapsed;
  const collapseExpandInfo = {
    ...params,
    anchor: last && params.anchor,
    last
  };

  if (params.collapsed) {
    tab.$TST.addState(Constants.kTAB_STATE_COLLAPSED);
    TabsStore.removeVisibleTab(tab);
    TabsStore.removeExpandedTab(tab);
  }
  else {
    tab.$TST.removeState(Constants.kTAB_STATE_COLLAPSED);
    TabsStore.addVisibleTab(tab);
    TabsStore.addExpandedTab(tab);
  }

  Tab.onCollapsedStateChanged.dispatch(tab, collapseExpandInfo);

  // the message is called multiple times on a session restoration, so it should be throttled for better performance
  let timer = collapseExpandTab.delayedNotify.get(tab.id);
  if (timer)
    clearTimeout(timer);
  timer = setTimeout(() => {
    collapseExpandTab.delayedNotify.delete(tab.id);
    if (!TabsStore.ensureLivingTab(tab))
      return;
    SidebarConnection.sendMessage({
      type:      Constants.kCOMMAND_NOTIFY_TAB_COLLAPSED_STATE_CHANGED,
      windowId:  tab.windowId,
      tabId:     tab.id,
      anchorId:  collapseExpandInfo.anchor && collapseExpandInfo.anchor.id,
      justNow:   params.justNow,
      collapsed: params.collapsed,
      last,
      stack,
      byAncestor
    });
  }, shouldApplyAnimation() ? 100 : 0);
  collapseExpandTab.delayedNotify.set(tab.id, timer);
}
collapseExpandTab.delayedNotify = new Map();

export async function collapseExpandTreesIntelligentlyFor(tab, options = {}) {
  if (!tab)
    return;

  logCollapseExpand('collapseExpandTreesIntelligentlyFor ', tab);
  const window = TabsStore.windows.get(tab.windowId);
  if (window.doingIntelligentlyCollapseExpandCount > 0) {
    logCollapseExpand('=> done by others');
    return;
  }
  window.doingIntelligentlyCollapseExpandCount++;

  const expandedAncestors = [tab.id]
    .concat(tab.$TST.ancestors.map(ancestor => ancestor.id))
    .concat(tab.$TST.descendants.map(descendant => descendant.id));
  const collapseTabs = Tab.getSubtreeCollapsedTabs(tab.windowId, {
    '!id': expandedAncestors
  });
  logCollapseExpand(`${collapseTabs.length} tabs can be collapsed, ancestors: `, expandedAncestors);
  const allowedToCollapse = new Set();
  await Promise.all(collapseTabs.map(async tab => {
    const allowed = await TSTAPI.tryOperationAllowed(
      TSTAPI.kNOTIFY_TRY_COLLAPSE_TREE_FROM_OTHER_EXPANSION,
      { tab: new TSTAPI.TreeItem(tab) },
      { tabProperties: ['tab'] }
    );
    if (allowed)
      allowedToCollapse.add(tab);
  }));
  for (const collapseTab of collapseTabs) {
    if (!allowedToCollapse.has(collapseTab))
      continue;
    let dontCollapse = false;
    const parentTab = collapseTab.$TST.parent;
    if (parentTab) {
      dontCollapse = true;
      if (!parentTab.$TST.subtreeCollapsed) {
        for (const ancestor of collapseTab.$TST.ancestors) {
          if (!expandedAncestors.includes(ancestor.id))
            continue;
          dontCollapse = false;
          break;
        }
      }
    }
    logCollapseExpand(`${collapseTab.id}: dontCollapse = ${dontCollapse}`);

    const manuallyExpanded = collapseTab.$TST.states.has(Constants.kTAB_STATE_SUBTREE_EXPANDED_MANUALLY);
    if (!dontCollapse && !manuallyExpanded)
      collapseExpandSubtree(collapseTab, {
        ...options,
        collapsed: true
      });
  }

  collapseExpandSubtree(tab, {
    ...options,
    collapsed: false
  });
  window.doingIntelligentlyCollapseExpandCount--;
}

export async function fixupSubtreeCollapsedState(tab, options = {}) {
  let fixed = false;
  if (!tab.$TST.hasChild)
    return fixed;
  const firstChild = tab.$TST.firstChild;
  const childrenCollapsed = firstChild.$TST.collapsed;
  const collapsedStateMismatched = tab.$TST.subtreeCollapsed != childrenCollapsed;
  const nextIsFirstChild = tab.$TST.nextTab == firstChild;
  log('fixupSubtreeCollapsedState ', {
    tab: tab.id,
    childrenCollapsed,
    collapsedStateMismatched,
    nextIsFirstChild
  });
  if (collapsedStateMismatched) {
    log(' => set collapsed state');
    await collapseExpandSubtree(tab, {
      ...options,
      collapsed: childrenCollapsed
    });
    fixed = true;
  }
  if (!nextIsFirstChild) {
    log(' => move child tabs');
    await followDescendantsToMovedRoot(tab, options);
    fixed = true;
  }
  return fixed;
}


// operate tabs based on tree information

export async function moveTabSubtreeBefore(tab, nextTab, options = {}) {
  if (!tab)
    return;
  if (nextTab && nextTab.$TST.isAllPlacedBeforeSelf([tab].concat(tab.$TST.descendants))) {
    log('moveTabSubtreeBefore:no need to move');
    return;
  }

  log('moveTabSubtreeBefore: ', tab.id, nextTab && nextTab.id);
  const window = TabsStore.windows.get(tab.windowId);
  window.subTreeMovingCount++;
  try {
    await TabsMove.moveTabInternallyBefore(tab, nextTab, options);
    if (!TabsStore.ensureLivingTab(tab)) // it is removed while waiting
      throw new Error('the tab was removed before moving of descendants');
    await followDescendantsToMovedRoot(tab, options);
  }
  catch(e) {
    log(`failed to move subtree: ${String(e)}`);
  }
  await wait(0);
  window.subTreeMovingCount--;
}

export async function moveTabSubtreeAfter(tab, previousTab, options = {}) {
  if (!tab)
    return;

  log('moveTabSubtreeAfter: ', tab.id, previousTab && previousTab.id);
  if (previousTab && previousTab.$TST.isAllPlacedAfterSelf([tab].concat(tab.$TST.descendants))) {
    log(' => no need to move');
    return;
  }

  const window = TabsStore.windows.get(tab.windowId);
  window.subTreeMovingCount++;
  try {
    await TabsMove.moveTabInternallyAfter(tab, previousTab, options);
    if (!TabsStore.ensureLivingTab(tab)) // it is removed while waiting
      throw new Error('the tab was removed before moving of descendants');
    await followDescendantsToMovedRoot(tab, options);
  }
  catch(e) {
    log(`failed to move subtree: ${String(e)}`);
  }
  await wait(0);
  window.subTreeMovingCount--;
}

async function followDescendantsToMovedRoot(tab, options = {}) {
  if (!tab.$TST.hasChild)
    return;

  log('followDescendantsToMovedRoot: ', tab);
  const window = TabsStore.windows.get(tab.windowId);
  window.subTreeChildrenMovingCount++;
  window.subTreeMovingCount++;
  await TabsMove.moveTabsAfter(tab.$TST.descendants, tab, options);
  window.subTreeChildrenMovingCount--;
  window.subTreeMovingCount--;
}

// before https://bugzilla.mozilla.org/show_bug.cgi?id=1394376 is fixed (Firefox 67 or older)
let mSlowDuplication = false;
browser.runtime.getBrowserInfo().then(browserInfo => {
  if (parseInt(browserInfo.version.split('.')[0]) < 68)
    mSlowDuplication = true;
});

export async function moveTabs(tabs, options = {}) {
  tabs = tabs.filter(TabsStore.ensureLivingTab);
  if (tabs.length == 0)
    return [];

  log('moveTabs: ', () => ({ tabs: tabs.map(dumpTab), options }));

  const windowId = parseInt(tabs[0].windowId || TabsStore.getCurrentWindowId());

  let newWindow = options.destinationPromisedNewWindow;

  let destinationWindowId = options.destinationWindowId;
  if (!destinationWindowId && !newWindow)
    destinationWindowId = TabsStore.getCurrentWindowId();

  const isAcrossWindows = windowId != destinationWindowId || !!newWindow;

  options.insertAfter = options.insertAfter || Tab.getLastTab(destinationWindowId);

  let movedTabs = tabs;
  const structure = TreeBehavior.getTreeStructureFromTabs(tabs);
  log('original tree structure: ', structure);

  if (!options.duplicate)
    await detachTabsFromTree(tabs, options);

  if (isAcrossWindows || options.duplicate) {
    if (mSlowDuplication)
      UserOperationBlocker.blockIn(windowId, { throbber: true });
    try {
      let window;
      const prepareWindow = () => {
        window = Window.init(destinationWindowId);
        if (isAcrossWindows) {
          window.toBeOpenedTabsWithPositions += tabs.length;
          window.toBeOpenedOrphanTabs += tabs.length;
          for (const tab of tabs) {
            window.toBeAttachedTabs.add(tab.id);
          }
        }
      };
      if (newWindow) {
        newWindow = newWindow.then(window => {
          log('moveTabs: destination window is ready, ', window);
          destinationWindowId = window.id;
          prepareWindow();
          return window;
        });
      }
      else {
        prepareWindow();
      }

      let movedTabIds = tabs.map(tab => tab.id);
      await Promise.all([
        newWindow,
        (async () => {
          const sourceWindow = TabsStore.windows.get(tabs[0].windowId);
          if (options.duplicate) {
            sourceWindow.toBeOpenedTabsWithPositions += tabs.length;
            sourceWindow.toBeOpenedOrphanTabs += tabs.length;
            sourceWindow.duplicatingTabsCount += tabs.length;
          }
          if (isAcrossWindows) {
            for (const tab of tabs) {
              sourceWindow.toBeDetachedTabs.add(tab.id);
            }
          }

          log('preparing tabs');
          if (options.duplicate) {
            const startTime = Date.now();
            // This promise will be resolved with very large delay.
            // (See also https://bugzilla.mozilla.org/show_bug.cgi?id=1394376 )
            const promisedDuplicatedTabs = Promise.all(movedTabIds.map(async (id, _index) => {
              try {
                return await browser.tabs.duplicate(id).catch(ApiTabs.createErrorHandler());
              }
              catch(e) {
                ApiTabs.handleMissingTabError(e);
                return null;
              }
            })).then(tabs => {
              log(`ids from API responses are resolved in ${Date.now() - startTime}msec: `, () => tabs.map(dumpTab));
              return tabs;
            });
            movedTabs = await promisedDuplicatedTabs;
            if (mSlowDuplication)
              UserOperationBlocker.setProgress(50, windowId);
            movedTabs = movedTabs.map(tab => Tab.get(tab.id));
            movedTabIds = movedTabs.map(tab => tab.id);
          }
          else {
            for (const tab of movedTabs) {
              if (tab.$TST.parent &&
                  !movedTabs.includes(tab.$TST.parent))
                detachTab(tab, {
                  broadcast:    true,
                  toBeDetached: true
                });
            }
          }
        })()
      ]);
      log('moveTabs: all windows and tabs are ready, ', movedTabIds, destinationWindowId);
      let toIndex = (tabs.some(tab => tab.pinned) ? Tab.getPinnedTabs(destinationWindowId) : Tab.getAllTabs(destinationWindowId)).length;
      log('toIndex = ', toIndex);
      if (options.insertBefore &&
          options.insertBefore.windowId == destinationWindowId) {
        try {
          toIndex = Tab.get(options.insertBefore.id).index;
        }
        catch(e) {
          ApiTabs.handleMissingTabError(e);
          log('options.insertBefore is unavailable');
        }
      }
      else if (options.insertAfter &&
               options.insertAfter.windowId == destinationWindowId) {
        try {
          toIndex = Tab.get(options.insertAfter.id).index + 1;
        }
        catch(e) {
          ApiTabs.handleMissingTabError(e);
          log('options.insertAfter is unavailable');
        }
      }
      if (!isAcrossWindows &&
          movedTabs[0].index < toIndex)
        toIndex--;
      log(' => ', toIndex);
      if (isAcrossWindows) {
        movedTabs = await ApiTabs.safeMoveAcrossWindows(movedTabIds, {
          windowId: destinationWindowId,
          index:    toIndex
        });
        movedTabs   = movedTabs.map(tab => Tab.get(tab.id));
        movedTabIds = movedTabs.map(tab => tab.id);
        for (const tab of movedTabs) {
          tab.windowId = destinationWindowId;
        }
        log('moved across windows: ', movedTabIds);
      }

      log('applying tree structure', structure);
      // wait until tabs.onCreated are processed (for safety)
      let newTabs;
      const startTime = Date.now();
      const maxDelay = configs.maximumAcceptableDelayForTabDuplication;
      while (Date.now() - startTime < maxDelay) {
        newTabs = mapAndFilter(movedTabs,
                               tab => Tab.get(tab.id) || undefined);
        if (mSlowDuplication)
          UserOperationBlocker.setProgress(Math.round(newTabs.length / tabs.length * 50) + 50, windowId);
        if (newTabs.length < tabs.length) {
          log('retrying: ', movedTabIds, newTabs.length, tabs.length);
          await wait(100);
          continue;
        }
        await Promise.all(newTabs.map(tab => tab.$TST.opened));
        await applyTreeStructureToTabs(newTabs, structure, {
          broadcast: true
        });
        if (options.duplicate) {
          for (const tab of newTabs) {
            tab.$TST.removeState(Constants.kTAB_STATE_DUPLICATING, { broadcast: true });
            TabsStore.removeDuplicatingTab(tab);
          }
        }
        break;
      }

      if (!newTabs) {
        log('failed to move tabs (timeout)');
        newTabs = [];
      }
      movedTabs = newTabs;
    }
    catch(e) {
      if (configs.debug)
        console.log('failed to move/duplicate tabs ', e, new Error().stack);
      throw e;
    }
    finally {
      if (mSlowDuplication)
        UserOperationBlocker.unblockIn(windowId, { throbber: true });
    }
  }


  movedTabs = mapAndFilter(movedTabs, tab => Tab.get(tab.id) || undefined);
  if (options.insertBefore) {
    await TabsMove.moveTabsBefore(
      movedTabs,
      options.insertBefore,
      options
    );
  }
  else if (options.insertAfter) {
    await TabsMove.moveTabsAfter(
      movedTabs,
      options.insertAfter,
      options
    );
  }
  else {
    log('no move: just duplicate or import');
  }
  // Tabs can be removed while waiting, so we need to
  // refresh the array of tabs.
  movedTabs = mapAndFilter(movedTabs, tab => Tab.get(tab.id) || undefined);

  if (isAcrossWindows) {
    for (const tab of movedTabs) {
      if (tab.$TST.parent ||
          parseInt(tab.$TST.getAttribute(Constants.kLEVEL) || 0) == 0)
        continue;
      updateTabIndent(tab, 0);
    }
  }

  return movedTabs;
}

export async function openNewWindowFromTabs(tabs, options = {}) {
  if (tabs.length == 0)
    return [];

  log('openNewWindowFromTabs: ', tabs, options);
  const sourceWindow = await browser.windows.get(tabs[0].windowId);
  const windowParams = {
    //active: true,  // not supported in Firefox...
    url:       'about:blank',
    ...getWindowParamsFromSource(sourceWindow, options),
  };
  let newWindow;
  const promsiedNewWindow = browser.windows.create(windowParams)
    .then(createdWindow => {
      newWindow = createdWindow;
      log('openNewWindowFromTabs: new window is ready, ', newWindow, windowParams);
      UserOperationBlocker.blockIn(newWindow.id);
      return newWindow;
    })
    .catch(ApiTabs.createErrorHandler());
  tabs = tabs.filter(TabsStore.ensureLivingTab);
  const movedTabs = await moveTabs(tabs, {
    ...options,
    destinationPromisedNewWindow: promsiedNewWindow
  });

  log('closing needless tabs');
  browser.windows.get(newWindow.id, { populate: true })
    .then(window => {
      const movedTabIds = new Set(movedTabs.map(tab => tab.id));
      log('moved tabs: ', movedTabIds);
      const removeTabs = mapAndFilter(window.tabs, tab =>
        !movedTabIds.has(tab.id) && Tab.get(tab.id) || undefined
      );
      log('removing tabs: ', removeTabs);
      TabsInternalOperation.removeTabs(removeTabs);
      UserOperationBlocker.unblockIn(newWindow.id);
    })
    .catch(ApiTabs.createErrorSuppressor());

  return movedTabs;
}


/* "treeStructure" is an array of integers, meaning:
  [A]     => TreeBehavior.STRUCTURE_NO_PARENT (parent is not in this tree)
    [B]   => 0 (parent is 1st item in this tree)
    [C]   => 0 (parent is 1st item in this tree)
      [D] => 2 (parent is 2nd in this tree)
  [E]     => TreeBehavior.STRUCTURE_NO_PARENT (parent is not in this tree, and this creates another tree)
    [F]   => 0 (parent is 1st item in this another tree)
  See also getTreeStructureFromTabs() in tree-behavior.js
*/
export async function applyTreeStructureToTabs(tabs, treeStructure, options = {}) {
  if (!tabs || !treeStructure)
    return;

  MetricsData.add('applyTreeStructureToTabs: start');

  log('applyTreeStructureToTabs: ', () => ({ tabs: tabs.map(dumpTab), treeStructure, options }));
  tabs = tabs.slice(0, treeStructure.length);
  treeStructure = treeStructure.slice(0, tabs.length);

  let expandStates = tabs.map(tab => !!tab);
  expandStates = expandStates.slice(0, tabs.length);
  while (expandStates.length < tabs.length)
    expandStates.push(TreeBehavior.STRUCTURE_NO_PARENT);

  MetricsData.add('applyTreeStructureToTabs: preparation');

  let parent = null;
  let tabsInTree = [];
  const promises   = [];
  for (let i = 0, maxi = tabs.length; i < maxi; i++) {
    const tab = tabs[i];
    /*
    if (tab.$TST.collapsed)
      collapseExpandTabAndSubtree(tab, {
        ...options,
        collapsed: false,
        justNow: true
      });
    */
    const structureInfo = treeStructure[i];
    let parentIndexInTree = TreeBehavior.STRUCTURE_NO_PARENT;
    if (typeof structureInfo == 'number') { // legacy format
      parentIndexInTree = structureInfo;
    }
    else {
      parentIndexInTree = structureInfo.parent;
      expandStates[i]   = !structureInfo.collapsed;
    }
    log(`  applyTreeStructureToTabs: parent for ${tab.id} => ${parentIndexInTree}`);
    if (parentIndexInTree == TreeBehavior.STRUCTURE_NO_PARENT ||
        parentIndexInTree == TreeBehavior.STRUCTURE_KEEP_PARENT) {
      // there is no parent, so this is a new parent!
      parent = null;
      tabsInTree = [tab];
    }
    else {
      tabsInTree.push(tab);
      parent = parentIndexInTree < tabsInTree.length ? tabsInTree[parentIndexInTree] : null;
    }
    log('   => parent = ', parent);
    if (parentIndexInTree != TreeBehavior.STRUCTURE_KEEP_PARENT)
      detachTab(tab, { justNow: true });
    if (parent && tab != parent) {
      parent.$TST.removeState(Constants.kTAB_STATE_SUBTREE_COLLAPSED); // prevent focus changing by "current tab attached to collapsed tree"
      promises.push(attachTabTo(tab, parent, {
        ...options,
        dontExpand: true,
        dontMove:   true,
        justNow:    true
      }));
    }
  }
  if (promises.length > 0)
    await Promise.all(promises);
  MetricsData.add('applyTreeStructureToTabs: attach/detach');

  log('expandStates: ', expandStates);
  for (let i = tabs.length - 1; i > -1; i--) {
    const tab = tabs[i];
    const expanded = expandStates[i];
    collapseExpandSubtree(tab, {
      ...options,
      collapsed: expanded === undefined ? !tab.$TST.hasChild : !expanded ,
      justNow:   true,
      force:     true
    });
  }
  MetricsData.add('applyTreeStructureToTabs: collapse/expand');
}



//===================================================================
// Fixup tree structure for unexpectedly inserted tabs
//===================================================================

class TabActionForNewPosition {
  constructor(action, { tab, parent, insertBefore, insertAfter, isTabCreating, mustToApply } = {}) {
    this.action        = action || null;
    this.tab           = tab;
    this.parent        = parent;
    this.insertBefore  = insertBefore;
    this.insertAfter   = insertAfter;
    this.isTabCreating = isTabCreating;
    this.mustToApply   = mustToApply;
  }

  async applyIfNeeded() {
    if (!this.mustToApply)
      return;
    return this.apply();
  }

  async apply() {
    log('TabActionForNewPosition: applying ', this);
    switch (this.action) {
      case 'invalid':
        throw new Error('invalid action: this must not happen!');

      case 'attach': {
        const attached = attachTabTo(this.tab, Tab.get(this.parent), {
          insertBefore: Tab.get(this.insertBefore),
          insertAfter:  Tab.get(this.insertAfter),
          forceExpand:  this.isTabCreating,
          broadcast:    true,
          synchronously: this.isTabCreating,
        });
        if (!this.isTabCreating)
          await attached;
        followDescendantsToMovedRoot(this.tab);
      }; break;

      case 'detach':
        detachTab(this.tab, { broadcast: true });
        followDescendantsToMovedRoot(this.tab);
        if (!this.insertBefore && !this.insertAfter)
          break;

      case 'move':
        if (this.insertBefore) {
          moveTabSubtreeBefore(
            this.tab,
            Tab.get(this.insertBefore),
            { broadcast: true }
          );
          return;
        }
        else if (this.insertAfter) {
          moveTabSubtreeAfter(
            this.tab,
            Tab.get(this.insertAfter),
            { broadcast: true }
          );
          return;
        }

      default:
        followDescendantsToMovedRoot(this.tab);
        break;
    }
  }
}

export function detectTabActionFromNewPosition(tab, moveInfo = {}) {
  const isTabCreating = moveInfo && !!moveInfo.isTabCreating;

  if (tab.pinned)
    return new TabActionForNewPosition(tab.$TST.parentId ? 'detach' : 'move', {
      tab,
      isTabCreating,
    });

  log('detectTabActionFromNewPosition: ', dumpTab(tab), moveInfo);
  const tree   = moveInfo.treeForActionDetection || snapshotForActionDetection(tab);
  const target = tree.target;
  log('  calculate new position: ', tab, tree);

  const toIndex   = moveInfo.toIndex;
  const fromIndex = moveInfo.fromIndex;
  if (toIndex == fromIndex) { // no move?
    log('=> no move');
    return new TabActionForNewPosition();
  }

  const prevTab = tree.tabsById[target.previous];
  const nextTab = tree.tabsById[target.next];
  log('prevTab: ', dumpTab(prevTab));
  log('nextTab: ', dumpTab(nextTab));

  const prevParent = prevTab && tree.tabsById[prevTab.parent];
  const nextParent = nextTab && tree.tabsById[nextTab.parent];

  const prevLevel  = prevTab ? prevTab.level : -1 ;
  const nextLevel  = nextTab ? nextTab.level : -1 ;
  log('prevLevel: '+prevLevel);
  log('nextLevel: '+nextLevel);

  const oldParent = tree.tabsById[target.parent];
  let newParent = null;
  let mustToApply = false;

  if (!oldParent) {
    if (!nextTab) {
      log('=> A root level tab, placed at the end of tabs. We should keep it in the root level.');
      return new TabActionForNewPosition();
    }
    if (!nextParent) {
      log(' => A root level tab, placed before another root level tab. We should keep it in the root level.');
      return new TabActionForNewPosition();
    }
  }

  if (target.mayBeReplacedWithContainer) {
    log('=> replaced by Firefox Multi-Acount Containers or Temporary Containers');
    newParent = prevLevel < nextLevel ? prevTab : prevParent;
    mustToApply = true;
  }
  else if (oldParent &&
           prevTab &&
           oldParent == prevTab) {
    log('=> no need to fix case');
    newParent = oldParent;
  }
  else if (!prevTab) {
    log('=> moved to topmost position');
    newParent = null;
    mustToApply = !!oldParent;
  }
  else if (!nextTab) {
    log('=> moved to last position');
    let ancestor = oldParent;
    while (ancestor) {
      if (ancestor == prevParent) {
        log(' => moving in related tree: keep it attached in existing tree');
        newParent = prevParent;
        break;
      }
      ancestor = tree.tabsById[ancestor.parent];
    }
    if (!newParent) {
      log(' => moving from other tree: keep it orphaned');
    }
    mustToApply = !!oldParent && newParent != oldParent;
  }
  else if (prevParent == nextParent) {
    log('=> moved into existing tree');
    newParent = prevParent;
    mustToApply = !oldParent || newParent != oldParent;
  }
  else if (prevLevel > nextLevel  &&
           nextTab.parent != tab.id) {
    log('=> moved to end of existing tree');
    if (!target.active &&
        target.children.length == 0 &&
        (Date.now() - target.trackedAt) < 500) {
      log('=> maybe newly opened tab');
      newParent = prevParent;
    }
    else {
      log('=> maybe drag and drop (or opened with active state and position)');
      const realDelta = Math.abs(toIndex - fromIndex);
      newParent = realDelta < 2 ? prevParent : (oldParent || nextParent) ;
    }
    while (newParent && newParent.collapsed) {
      log('=> the tree is collapsed, up to parent tree')
      newParent = tree.tabsById[newParent.parent];
    }
    mustToApply = !!oldParent && newParent != oldParent;
  }
  else if (prevLevel < nextLevel &&
           nextTab.parent == prevTab.id) {
    log('=> moved to first child position of existing tree');
    newParent = prevTab || oldParent || nextParent;
    mustToApply = !!oldParent && newParent != oldParent;
  }

  log('calculated parent: ', {
    old: oldParent && oldParent.id,
    new: newParent && newParent.id
  });

  if (newParent) {
    let ancestor = newParent;
    while (ancestor) {
      if (ancestor == target) {
        if (moveInfo.toIndex - moveInfo.fromIndex == 1) {
          log('=> maybe move-down by keyboard shortcut or something.');
          let nearestForeigner = tab.$TST.nearestFollowingForeignerTab;
          if (nearestForeigner &&
              nearestForeigner == tab)
            nearestForeigner = nearestForeigner.$TST.nextTab;
          log('nearest foreigner tab: ', nearestForeigner && nearestForeigner.id);
          if (nearestForeigner) {
            if (nearestForeigner.$TST.hasChild)
              return new TabActionForNewPosition('attach', {
                tab,
                isTabCreating,
                parent:      nearestForeigner.id,
                insertAfter: nearestForeigner.id,
                mustToApply,
              });
            return new TabActionForNewPosition(tab.$TST.parent ? 'detach' : 'move', {
              tab,
              isTabCreating,
              insertAfter: nearestForeigner.id,
              mustToApply,
            });
          }
        }
        log('=> invalid move: a parent is moved inside its own tree!');
        return new TabActionForNewPosition('invalid');
      }
      ancestor = tree.tabsById[ancestor.parent];
    }
  }

  if (newParent != oldParent) {
    if (newParent) {
      return new TabActionForNewPosition('attach', {
        tab,
        isTabCreating,
        parent:       newParent.id,
        insertBefore: nextTab && nextTab.id,
        insertAfter:  prevTab && prevTab.id,
        mustToApply,
      });
    }
    else {
      return new TabActionForNewPosition('detach', {
        tab,
        isTabCreating,
        mustToApply,
      });
    }
  }
  return new TabActionForNewPosition('move', {
    tab,
    isTabCreating,
    mustToApply,
  });
}


//===================================================================
// Take snapshot
//===================================================================

export function snapshotForActionDetection(targetTab) {
  const prevTab = targetTab.$TST.nearestCompletelyOpenedNormalPrecedingTab;
  const nextTab = targetTab.$TST.nearestCompletelyOpenedNormalFollowingTab;
  const tabs = Array.from(new Set([
    ...(prevTab && prevTab.$TST.ancestors || []),
    prevTab,
    targetTab,
    nextTab,
    targetTab.$TST.parent
  ]))
    .filter(TabsStore.ensureLivingTab)
    .sort((a, b) => a.index - b.index);
  return snapshotTree(targetTab, tabs);
}

function snapshotTree(targetTab, tabs) {
  const allTabs = tabs || Tab.getTabs(targetTab.windowId);

  const snapshotById = {};
  function snapshotChild(tab) {
    if (!TabsStore.ensureLivingTab(tab) || tab.pinned)
      return null;
    return snapshotById[tab.id] = {
      id:            tab.id,
      url:           tab.url,
      cookieStoreId: tab.cookieStoreId,
      active:        tab.active,
      children:      tab.$TST.children.map(child => child.id),
      collapsed:     tab.$TST.subtreeCollapsed,
      pinned:        tab.pinned,
      level:         tab.$TST.ancestorIds.length, // parseInt(tab.$TST.getAttribute(Constants.kLEVEL) || 0), // we need to use the number of real ancestors instead of a cached "level", because it will be updated with delay
      trackedAt:     tab.$TST.trackedAt,
      mayBeReplacedWithContainer: tab.$TST.mayBeReplacedWithContainer
    };
  }
  const snapshotArray = allTabs.map(tab => snapshotChild(tab));
  for (const tab of allTabs) {
    const item = snapshotById[tab.id];
    if (!item)
      continue;
    const parent = tab.$TST.parent;
    item.parent = parent && parent.id;
    const next = tab.$TST.nearestCompletelyOpenedNormalFollowingTab;
    item.next = next && next.id;
    const previous = tab.$TST.nearestCompletelyOpenedNormalPrecedingTab;
    item.previous = previous && previous.id;
  }
  const activeTab = Tab.getActiveTab(targetTab.windowId);
  return {
    target:   snapshotById[targetTab.id],
    active:   activeTab && snapshotById[activeTab.id],
    tabs:     snapshotArray,
    tabsById: snapshotById
  };
}


SidebarConnection.onMessage.addListener(async (windowId, message) => {
  switch (message.type) {
    case Constants.kCOMMAND_SET_SUBTREE_COLLAPSED_STATE: {
      await Tab.waitUntilTracked(message.tabId);
      const tab = Tab.get(message.tabId);
      if (!tab)
        return;
      const params = {
        collapsed: message.collapsed,
        justNow:   message.justNow,
        broadcast: true,
        stack:     message.stack
      };
      if (message.manualOperation)
        manualCollapseExpandSubtree(tab, params);
      else
        collapseExpandSubtree(tab, params);
    }; break;

    case Constants.kCOMMAND_SET_SUBTREE_COLLAPSED_STATE_INTELLIGENTLY_FOR: {
      await Tab.waitUntilTracked(message.tabId);
      const tab = Tab.get(message.tabId);
      if (tab)
        await collapseExpandTreesIntelligentlyFor(tab);
    }; break;

    case Constants.kCOMMAND_NEW_WINDOW_FROM_TABS: {
      log('new window requested: ', message);
      await Tab.waitUntilTracked(message.tabIds);
      const tabs = message.tabIds.map(id => TabsStore.tabs.get(id));
      if (!message.duplicate)
        await detachTabsFromTree(tabs);
      openNewWindowFromTabs(tabs, message);
    }; break;
  }
});

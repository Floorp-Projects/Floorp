/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  configs
} from './common.js';
import * as Constants from './constants.js';
import * as SidebarConnection from './sidebar-connection.js';

function log(...args) {
  internalLogger('common/tree-behavior', ...args);
}

export function getParentTabOperationBehavior(tab, { context, byInternalOperation, preventEntireTreeBehavior, parent, windowId } = {}) {
  const sidebarVisible = SidebarConnection.isInitialized() ? ((windowId || tab) && SidebarConnection.isOpen(windowId || tab.windowId)) : true;
  log('getParentTabOperationBehavior ', tab, { byInternalOperation, preventEntireTreeBehavior, parent, sidebarVisible /*, stack: configs.debug && new Error().stack */ });

  // strategy: https://github.com/piroor/treestyletab/issues/2860#issuecomment-820622273
  let behavior;
  switch (configs.parentTabOperationBehaviorMode) {
    case Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CONSISTENT:
      log(' => kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CONSISTENT');
      if (context == Constants.kPARENT_TAB_OPERATION_CONTEXT_MOVE) {
        behavior = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE;
      }
      else {
        behavior = tab.$TST.subtreeCollapsed ?
          Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE :
          configs.closeParentBehavior_insideSidebar_expanded;
      }
      break;
    default:
    case Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_PARALLEL:
      log(' => kPARENT_TAB_OPERATION_BEHAVIOR_MODE_PARALLEL');
      if (context == Constants.kPARENT_TAB_OPERATION_CONTEXT_MOVE) {
        behavior = byInternalOperation ?
          Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE :
          configs.moveParentBehavior_outsideSidebar_expanded;
      }
      else {
        behavior = byInternalOperation ?
          (tab.$TST.subtreeCollapsed ?
            Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE :
            configs.closeParentBehavior_insideSidebar_expanded) :
          configs.closeParentBehavior_outsideSidebar_expanded;
      }
      break;
    case Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CUSTOM: // kPARENT_TAB_BEHAVIOR_ONLY_ON_SIDEBAR
      log(' => kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CUSTOM');
      if (context == Constants.kPARENT_TAB_OPERATION_CONTEXT_MOVE) {
        behavior = byInternalOperation ?
          Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE :
          sidebarVisible ?
            (tab.$TST.subtreeCollapsed ?
              configs.moveParentBehavior_outsideSidebar_collapsed :
              configs.moveParentBehavior_outsideSidebar_expanded) :
            (tab.$TST.subtreeCollapsed ?
              configs.moveParentBehavior_noSidebar_collapsed :
              configs.moveParentBehavior_noSidebar_expanded);
      }
      else {
        behavior = byInternalOperation ?
          (tab.$TST.subtreeCollapsed ?
            Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE :
            configs.closeParentBehavior_insideSidebar_expanded) :
          sidebarVisible ?
            (tab.$TST.subtreeCollapsed ?
              configs.closeParentBehavior_outsideSidebar_collapsed :
              configs.closeParentBehavior_outsideSidebar_expanded) :
            (tab.$TST.subtreeCollapsed ?
              configs.closeParentBehavior_noSidebar_collapsed :
              configs.closeParentBehavior_noSidebar_expanded);
      }
      break;
  }
  const parentTab = parent || tab.$TST.parent;

  log(' => behavior: ', behavior);

  if (behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_INTELLIGENTLY) {
    behavior = parentTab ? Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN : Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD;
    log(' => intelligent behavior: ', behavior);
  }

  if (behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE &&
      preventEntireTreeBehavior) {
    behavior = parentTab ? Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN : Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD;
    log(' => preventEntireTreeBehavior behavior: ', behavior);
  }

  // Promote all children to upper level, if this is the last child of the parent.
  // This is similar to "taking by representation".
  if (behavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD &&
      parentTab &&
      parentTab.$TST.childIds.length == 1 &&
      configs.promoteAllChildrenWhenClosedParentIsLastChild) {
    behavior = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN;
    log(' => blast child ehavior: ', behavior);
  }

  return behavior;
}

export function getClosingTabsFromParent(tab, removeInfo = {}) {
  log('getClosingTabsFromParent: ', tab, removeInfo);
  const closeParentBehavior = getParentTabOperationBehavior(tab, {
    ...removeInfo,
    context: Constants.kPARENT_TAB_OPERATION_CONTEXT_CLOSE,
  });
  log('getClosingTabsFromParent: closeParentBehavior ', closeParentBehavior);
  if (closeParentBehavior != Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE)
    return [tab];
  return [tab].concat(tab.$TST.descendants);
}

export function calculateReferenceTabsFromInsertionPosition(
  tab,
  { context, insertBefore, insertAfter } = {}
) {
  const firstTab = (Array.isArray(tab) ? tab[0] : tab) || tab;
  const lastTab  = (Array.isArray(tab) ? tab[tab.length - 1] : tab) || tab;
  log('calculateReferenceTabsFromInsertionPosition ', {
    firstTab:     firstTab && firstTab.id,
    lastTab:      lastTab && lastTab.id,
    insertBefore: insertBefore && insertBefore.id,
    insertAfter : insertAfter && insertAfter.id
  });
  if (insertBefore) {
    /* strategy for moved case
         +------------------ CASE 1 ---------------------------
         |     <= detach from parent, and move
         |[TARGET  ]
         +------------------ CASE 2 ---------------------------
         |  [      ]
         |     <= attach to the parent of the target, and move
         |[TARGET  ]
         +------------------ CASE 3 ---------------------------
         |[        ]
         |     <= attach to the parent of the target, and move
         |[TARGET  ]
         +------------------ CASE 4 ---------------------------
         |[        ]
         |     <= attach to the parent of the target (previous tab), and move
         |  [TARGET]
         +-----------------------------------------------------
    */
    /* strategy for shown case
         +------------------ CASE 5 ---------------------------
         |     <= detach from parent, and move
         |[TARGET  ]
         +------------------ CASE 6 ---------------------------
         |  [      ]
         |     <= if the inserted tab has a parent and it is not the parent of the target, attach to the parent of the target. Otherwise keep inserted as a root.
         |[TARGET  ]
         +------------------ CASE 7 ---------------------------
         |[        ]
         |     <= attach to the parent of the target, and move
         |[TARGET  ]
         +------------------ CASE 8 ---------------------------
         |[        ]
         |     <= attach to the parent of the target (previous tab), and move
         |  [TARGET]
         +-----------------------------------------------------
    */
    let prevTab = insertBefore &&
      (configs.fixupTreeOnTabVisibilityChanged ?
        insertBefore.$TST.nearestVisiblePrecedingTab :
        insertBefore.$TST.unsafeNearestExpandedPrecedingTab);
    if (prevTab == lastTab) // failsafe
      prevTab = !firstTab ? null :
        configs.fixupTreeOnTabVisibilityChanged ?
          firstTab.$TST.nearestVisiblePrecedingTab :
          firstTab.$TST.unsafeNearestExpandedPrecedingTab;
    if (!prevTab) {
      log('calculateReferenceTabsFromInsertionPosition: from insertBefore, CASE 1/5');
      // allow to move pinned tab to beside of another pinned tab
      if (!firstTab ||
          firstTab.pinned == (insertBefore && insertBefore.pinned)) {
        return {
          insertBefore
        };
      }
      else {
        return {};
      }
    }
    else {
      const prevLevel   = Number(prevTab.$TST.getAttribute(Constants.kLEVEL) || 0);
      const targetLevel = Number(insertBefore.$TST.getAttribute(Constants.kLEVEL) || 0);
      let parent = null;
      if (!firstTab || !firstTab.pinned) {
        if (prevLevel < targetLevel) {
          if (context == Constants.kINSERTION_CONTEXT_MOVED) {
            log('calculateReferenceTabsFromInsertionPosition: from insertBefore, CASE 4, prevTab = ', prevTab);
            parent = prevTab;
          }
          else {
            log('calculateReferenceTabsFromInsertionPosition: from insertBefore, CASE 8, prevTab = ', prevTab);
            parent = (firstTab && firstTab.$TST.parent != prevTab) ? prevTab : null;
          }
        }
        else {
          const possibleParent = insertBefore && insertBefore.$TST.parent;
          if (context == Constants.kINSERTION_CONTEXT_MOVED || prevLevel == targetLevel) {
            log('calculateReferenceTabsFromInsertionPosition: from insertBefore, CASE 2/3/7');
            parent = possibleParent;
          }
          else {
            log('calculateReferenceTabsFromInsertionPosition: from insertBefore, CASE 6');
            parent = firstTab && (firstTab.$TST.parent != possibleParent && possibleParent || firstTab.$TST.parent);
          }
        }
      }
      const result = {
        parent,
        insertAfter: prevTab,
        insertBefore
      };
      log(' => ', result);
      return result;
    }
  }
  if (insertAfter) {
    /* strategy for moved case
         +------------------ CASE 1 ---------------------------
         |[TARGET  ]
         |     <= if the target has a parent, attach to it and and move
         +------------------ CASE 2 ---------------------------
         |  [TARGET]
         |     <= attach to the parent of the target, and move
         |[        ]
         +------------------ CASE 3 ---------------------------
         |[TARGET  ]
         |     <= attach to the parent of the target, and move
         |[        ]
         +------------------ CASE 4 ---------------------------
         |[TARGET  ]
         |     <= attach to the target, and move
         |  [      ]
         +-----------------------------------------------------
    */
    /* strategy for shown case
         +------------------ CASE 5 ---------------------------
         |[TARGET  ]
         |     <= if the inserted tab has a parent, detach. Otherwise keep inserted as a root.
         +------------------ CASE 6 ---------------------------
         |  [TARGET]
         |     <= if the inserted tab has a parent and it is not the parent of the next tab, attach to the parent of the target. Otherwise attach to the parent of the next tab.
         |[        ]
         +------------------ CASE 7 ---------------------------
         |[TARGET  ]
         |     <= attach to the parent of the target, and move
         |[        ]
         +------------------ CASE 8 ---------------------------
         |[TARGET  ]
         |     <= attach to the target, and move
         |  [      ]
         +-----------------------------------------------------
    */
    // We need to refer unsafeNearestExpandedFollowingTab instead of a visible tab, to avoid
    // placing the tab after hidden tabs (it is too far from the target).
    let unsafeNextTab = insertAfter &&
      insertAfter.$TST.unsafeNearestExpandedFollowingTab;
    if (firstTab && unsafeNextTab == firstTab) // failsafe
      unsafeNextTab = lastTab && lastTab.$TST.unsafeNearestExpandedFollowingTab;
    let nextTab = insertAfter &&
      (configs.fixupTreeOnTabVisibilityChanged ?
        insertAfter.$TST.nearestVisibleFollowingTab :
        unsafeNextTab);
    if (firstTab && nextTab == firstTab) // failsafe
      nextTab = configs.fixupTreeOnTabVisibilityChanged ?
        (lastTab && lastTab.$TST.nearestVisibleFollowingTab) :
        unsafeNextTab;
    if (!nextTab) {
      let result;
      if (context == Constants.kINSERTION_CONTEXT_MOVED) {
        log('calculateReferenceTabsFromInsertionPosition: from insertAfter, CASE 1');
        result = {
          parent:       insertAfter && insertAfter.$TST.parent,
          insertBefore: unsafeNextTab,
          insertAfter
        };
      }
      else {
        log('calculateReferenceTabsFromInsertionPosition: from insertAfter, CASE 5');
        result = {
          parent:       firstTab && firstTab.$TST.parent && insertAfter && insertAfter.$TST.parent,
          insertBefore: unsafeNextTab,
          insertAfter
        };
      }
      log(' => ', result);
      return result;
    }
    else {
      const targetLevel = Number(insertAfter.$TST.getAttribute(Constants.kLEVEL) || 0);
      const nextLevel   = Number(nextTab.$TST.getAttribute(Constants.kLEVEL) || 0);
      let parent = null;
      if (!firstTab || !firstTab.pinned) {
        if (targetLevel < nextLevel) {
          log('calculateReferenceTabsFromInsertionPosition: from insertAfter, CASE 4/8');
          parent = insertAfter;
        }
        else  {
          const possibleParent = insertAfter && insertAfter.$TST.parent;
          if (context == Constants.kINSERTION_CONTEXT_MOVED || targetLevel == nextLevel) {
            log('calculateReferenceTabsFromInsertionPosition: from insertAfter, CASE 2/3/7');
            parent = possibleParent;
          }
          else {
            log('calculateReferenceTabsFromInsertionPosition: from insertAfter, CASE 6');
            parent = firstTab && (firstTab.$TST.parent != possibleParent && possibleParent || firstTab.$TST.parent);
          }
        }
      }
      const result = {
        parent,
        insertBefore: unsafeNextTab || nextTab,
        insertAfter
      };
      log(' => ', result);
      return result;
    }
  }
  throw new Error('calculateReferenceTabsFromInsertionPosition requires one of insertBefore or insertAfter parameter!');
}


export const STRUCTURE_NO_PARENT = -1;
export const STRUCTURE_KEEP_PARENT = -2;

export function getTreeStructureFromTabs(tabs, { full, keepParentOfRootTabs } = {}) {
  if (!tabs || !tabs.length)
    return [];

  /* this returns...
    [A]     => STRUCTURE_NO_PARENT (parent is not in this tree)
      [B]   => 0 (parent is 1st item in this tree)
      [C]   => 0 (parent is 1st item in this tree)
        [D] => 2 (parent is 2nd in this tree)
    [E]     => STRUCTURE_NO_PARENT (parent is not in this tree, and this creates another tree)
      [F]   => 0 (parent is 1st item in this another tree)
  */
  const tabIds = tabs.map(tab => tab.id);
  return cleanUpTreeStructureArray(
    tabs.map((tab, index) => {
      const parentId = tab.$TST.parentId;
      const indexInGivenTabs = parent ? tabIds.indexOf(parentId) : STRUCTURE_NO_PARENT ;
      return indexInGivenTabs >= index ? STRUCTURE_NO_PARENT : indexInGivenTabs ;
    }),
    STRUCTURE_NO_PARENT
  ).map((parentIndex, index) => {
    if (parentIndex == STRUCTURE_NO_PARENT &&
        keepParentOfRootTabs)
      parentIndex = STRUCTURE_KEEP_PARENT;
    const tab = tabs[index];
    const item = {
      id:        tab.$TST.uniqueId.id,
      parent:    parentIndex,
      collapsed: tab.$TST.subtreeCollapsed
    };
    if (full) {
      item.title  = tab.title;
      item.url    = tab.url;
      item.pinned = tab.pinned;
      item.originalId = tab.id;
    }
    return item;
  });
}
function cleanUpTreeStructureArray(treeStructure, defaultParent) {
  let offset = 0;
  treeStructure = treeStructure
    .map((position, index) => {
      return (position == index) ? STRUCTURE_NO_PARENT : position ;
    })
    .map((position, index) => {
      if (position == STRUCTURE_NO_PARENT) {
        offset = index;
        return position;
      }
      return position - offset;
    });

  /* The final step, this validates all of values.
     Smaller than STRUCTURE_NO_PARENT is invalid, so it becomes to STRUCTURE_NO_PARENT. */
  treeStructure = treeStructure.map(index => {
    return index < STRUCTURE_NO_PARENT ? defaultParent : index ;
  });
  return treeStructure;
}

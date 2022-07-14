/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  dumpTab,
  configs
} from '/common/common.js';

import * as Constants from '/common/constants.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TreeBehavior from '/common/tree-behavior.js';
import * as TSTAPI from '/common/tst-api.js';

import Tab from '/common/Tab.js';

import * as Commands from './commands.js';
import * as TabsGroup from './tabs-group.js';
import * as Tree from './tree.js';
import * as TreeStructure from './tree-structure.js';

function log(...args) {
  internalLogger('background/handle-moved-tabs', ...args);
}
function logApiTabs(...args) {
  internalLogger('common/api-tabs', ...args);
}


Tab.onCreated.addListener((tab, info = {}) => {
  if (!info.mayBeReplacedWithContainer &&
      (info.duplicated ||
       info.restored ||
       info.skipFixupTree ||
       // do nothing for already attached tabs
       (tab.openerTabId &&
        tab.$TST.parent == Tab.get(tab.openerTabId))))
    return;
  // if the tab is opened inside existing tree by someone, we must fixup the tree.
  if (!(info.positionedBySelf ||
        info.movedBySelfWhileCreation) &&
      (tab.$TST.nearestCompletelyOpenedNormalFollowingTab ||
       tab.$TST.nearestCompletelyOpenedNormalPrecedingTab ||
       (info.treeForActionDetection &&
        info.treeForActionDetection.target &&
        (info.treeForActionDetection.target.next ||
         info.treeForActionDetection.target.previous))))
    tryFixupTreeForInsertedTab(tab, {
      toIndex:   tab.index,
      fromIndex: Tab.getLastTab(tab.windowId).index,
      treeForActionDetection: info.treeForActionDetection,
      isTabCreating: true
    });
});

Tab.onMoving.addListener((tab, moveInfo) => {
  // avoid TabMove produced by browser.tabs.insertRelatedAfterCurrent=true or something.
  const window           = TabsStore.windows.get(tab.windowId);
  const isNewlyOpenedTab = window.openingTabs.has(tab.id);
  const positionControlled = configs.insertNewChildAt != Constants.kINSERT_NO_CONTROL;
  if (isNewlyOpenedTab &&
      !moveInfo.byInternalOperation &&
      !moveInfo.alreadyMoved &&
      !moveInfo.isSubstantiallyMoved &&
      positionControlled) {
    const opener = tab.$TST.openerTab;
    // if there is no valid opener, it can be a restored initial tab in a restored window
    // and can be just moved as a part of window restoration process.
    if (opener) {
      log('onTabMove for new child tab: move back '+moveInfo.toIndex+' => '+moveInfo.fromIndex);
      moveBack(tab, moveInfo);
      return false;
    }
  }
  return true;
});

async function tryFixupTreeForInsertedTab(tab, moveInfo = {}) {
  const parentTabOperationBehavior = TreeBehavior.getParentTabOperationBehavior(tab, {
    context: Constants.kPARENT_TAB_OPERATION_CONTEXT_MOVE,
    ...moveInfo,
  });
  log('tryFixupTreeForInsertedTab ', { parentTabOperationBehavior, moveInfo });
  if (!moveInfo.isTabCreating &&
      parentTabOperationBehavior != Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE) {
    const replacedGroupTab = (parentTabOperationBehavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB) ?
      await TabsGroup.tryReplaceTabWithGroup(tab, { insertBefore: tab.$TST.firstChild }) :
      null;
    if (!replacedGroupTab && tab.$TST.hasChild)
      await Tree.detachAllChildren(tab, {
        behavior:  parentTabOperationBehavior,
        nearestFollowingRootTab: tab.$TST.firstChild.$TST.nearestFollowingRootTab,
        broadcast: true
      });
    await Tree.detachTab(tab, {
      broadcast: true
    });
  }

  log('The tab can be placed inside existing tab unexpectedly, so now we are trying to fixup tree.');
  const action = detectTabActionFromNewPosition(tab, moveInfo);
  if (!action) {
    log('no action');
    return;
  }

  // notify event to helper addons with action and allow or deny
  const cache = {};
  const allowed = await TSTAPI.tryOperationAllowed(
    TSTAPI.kNOTIFY_TRY_FIXUP_TREE_ON_TAB_MOVED,
    {
      tab:          new TSTAPI.TreeItem(tab, { cache }),
      fromIndex:    moveInfo.fromIndex,
      toIndex:      moveInfo.toIndex,
      action:       action.action,
      parent:       action.parent && new TSTAPI.TreeItem(Tab.get(action.parent), { cache }),
      insertBefore: action.insertBefore && new TSTAPI.TreeItem(Tab.get(action.insertBefore), { cache }),
      insertAfter:  action.insertAfter && new TSTAPI.TreeItem(Tab.get(action.insertAfter), { cache })
    },
    { tabProperties: ['tab', 'parent', 'insertBefore', 'insertAfter'] }
  );
  if (!allowed) {
    log('no action - canceled by a helper addon');
    return;
  }

  log('action: ', action);
  switch (action.action) {
    case 'moveBack':
      moveBack(tab, moveInfo);
      return;

    case 'attach': {
      const attached = Tree.attachTabTo(tab, Tab.get(action.parent), {
        insertBefore: Tab.get(action.insertBefore),
        insertAfter:  Tab.get(action.insertAfter),
        forceExpand:  moveInfo.isTabCreating,
        broadcast:    true,
        synchronously: moveInfo.isTabCreating
      });
      if (!moveInfo.isTabCreating)
        await attached;
      Tree.followDescendantsToMovedRoot(tab);
    }; break;

    case 'detach':
      Tree.detachTab(tab, { broadcast: true });
      Tree.followDescendantsToMovedRoot(tab);
      if (!action.insertBefore && !action.insertAfter)
        break;

    case 'move':
      if (action.insertBefore) {
        Tree.moveTabSubtreeBefore(
          tab,
          Tab.get(action.insertBefore),
          { broadcast: true }
        );
        return;
      }
      else if (action.insertAfter) {
        Tree.moveTabSubtreeAfter(
          tab,
          Tab.get(action.insertAfter),
          { broadcast: true }
        );
        return;
      }

    default:
      Tree.followDescendantsToMovedRoot(tab);
      break;
  }
}

function reserveToEnsureRootTabVisible(tab) {
  reserveToEnsureRootTabVisible.tabIds.add(tab.id);
  if (reserveToEnsureRootTabVisible.reserved)
    clearTimeout(reserveToEnsureRootTabVisible.reserved);
  reserveToEnsureRootTabVisible.reserved = setTimeout(() => {
    delete reserveToEnsureRootTabVisible.reserved;
    const tabs = Array.from(reserveToEnsureRootTabVisible.tabIds, Tab.get);
    reserveToEnsureRootTabVisible.tabIds.clear();
    for (const tab of tabs) {
      if (tab.$TST.parent ||
          !tab.$TST.collapsed)
        continue;
      Tree.collapseExpandTabAndSubtree(tab, {
        collapsed: false,
        broadcast: true
      });
    }
  }, 150);
}
reserveToEnsureRootTabVisible.tabIds = new Set();

Tab.onMoved.addListener((tab, moveInfo = {}) => {
  if (!moveInfo.byInternalOperation &&
      !moveInfo.isSubstantiallyMoved &&
      !tab.$TST.duplicating) {
    log('process moved tab');
    tryFixupTreeForInsertedTab(tab, moveInfo);
  }
  else {
    log('internal move');
  }
  reserveToEnsureRootTabVisible(tab);
});

Commands.onMoveUp.addListener(async tab => {
  await tryFixupTreeForInsertedTab(tab, {
    toIndex:   tab.index,
    fromIndex: tab.index + 1,
  });
});

Commands.onMoveDown.addListener(async tab => {
  await tryFixupTreeForInsertedTab(tab, {
    toIndex:   tab.index,
    fromIndex: tab.index - 1,
  });
});

TreeStructure.onTabAttachedFromRestoredInfo.addListener((tab, moveInfo) => { tryFixupTreeForInsertedTab(tab, moveInfo); });

function moveBack(tab, moveInfo) {
  log('Move back tab from unexpected move: ', dumpTab(tab), moveInfo);
  const id     = tab.id;
  const window = TabsStore.windows.get(tab.windowId);
  window.internalMovingTabs.add(id);
  logApiTabs(`handle-moved-tabs:moveBack: browser.tabs.move() `, tab.id, {
    windowId: moveInfo.windowId,
    index:    moveInfo.fromIndex
  });
  // Because we need to use the raw "fromIndex" directly,
  // we cannot use TabsMove.moveTabInternallyBefore/After here.
  return browser.tabs.move(tab.id, {
    windowId: moveInfo.windowId,
    index:    moveInfo.fromIndex
  }).catch(ApiTabs.createErrorHandler(e => {
    if (window.internalMovingTabs.has(id))
      window.internalMovingTabs.delete(id);
    ApiTabs.handleMissingTabError(e);
  }));
}

function detectTabActionFromNewPosition(tab, moveInfo = {}) {
  if (tab.pinned)
    return tab.$TST.parentId ? { action: 'detach' } : { action: 'move' };

  log('detectTabActionFromNewPosition: ', dumpTab(tab), moveInfo);
  const tree   = moveInfo.treeForActionDetection || Tree.snapshotForActionDetection(tab);
  const target = tree.target;
  log('  calculate new position: ', tab, tree);

  const toIndex   = moveInfo.toIndex;
  const fromIndex = moveInfo.fromIndex;
  if (toIndex == fromIndex) { // no move?
    log('=> no move');
    return { action: null };
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

  if (!oldParent) {
    if (!nextTab) {
      log('=> A root level tab, placed at the end of tabs. We should keep it in the root level.');
      return { action: null };
    }
    if (!nextParent) {
      log(' => A root level tab, placed before another root level tab. We should keep it in the root level.');
      return { action: null };
    }
  }

  if (target.mayBeReplacedWithContainer) {
    log('=> replaced by Firefox Multi-Acount Containers or Temporary Containers');
    newParent = prevLevel < nextLevel ? prevTab : prevParent;
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
  }
  else if (prevParent == nextParent) {
    log('=> moved into existing tree');
    newParent = prevParent;
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
  }
  else if (prevLevel < nextLevel &&
           nextTab.parent == prevTab.id) {
    log('=> moved to first child position of existing tree');
    newParent = prevTab || oldParent || nextParent;
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
              return {
                action:      'attach',
                parent:      nearestForeigner.id,
                insertAfter: nearestForeigner.id
              };
            return {
              action:      tab.$TST.parent ? 'detach' : 'move',
              insertAfter: nearestForeigner.id
            };
          }
        }
        log('=> invalid move: a parent is moved inside its own tree, thus move back!');
        return { action: 'moveBack' };
      }
      ancestor = tree.tabsById[ancestor.parent];
    }
  }

  if (newParent != oldParent) {
    if (newParent) {
      return {
        action:       'attach',
        parent:       newParent.id,
        insertBefore: nextTab && nextTab.id,
        insertAfter:  prevTab && prevTab.id
      };
    }
    else {
      return { action: 'detach' };
    }
  }
  return { action: 'move' };
}

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
  const action = Tree.detectTabActionFromNewPosition(tab, moveInfo);
  if (!action.action) {
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
    case 'invalid':
      moveBack(tab, moveInfo);
      return;

    default:
      log('tryFixupTreeForInsertedTab: apply action for unattached tab: ', tab, action);
      await action.apply();
      return;
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

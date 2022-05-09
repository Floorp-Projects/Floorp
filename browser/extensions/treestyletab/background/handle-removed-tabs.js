/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  dumpTab,
  wait,
  mapAndFilterUniq,
  countMatched,
  configs
} from '/common/common.js';

import * as Constants from '/common/constants.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TabsInternalOperation from '/common/tabs-internal-operation.js';
import * as TreeBehavior from '/common/tree-behavior.js';

import Tab from '/common/Tab.js';

import * as Background from './background.js';
import * as TabsGroup from './tabs-group.js';
import * as Tree from './tree.js';
import * as Commands from './commands.js';

function log(...args) {
  internalLogger('background/handle-removed-tabs', ...args);
}


Tab.onRemoving.addListener(async (tab, removeInfo = {}) => {
  log('Tabs.onRemoving ', dumpTab(tab), removeInfo);
  if (removeInfo.isWindowClosing)
    return;

  let closeParentBehavior;
  let newParent;
  const successor = tab.$TST.possibleSuccessorWithDifferentContainer;
  if (successor) {
    log('override closeParentBehaior with kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD for different container successor ', successor);
    closeParentBehavior = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD;
    // When a new tab is created with a different container and this tab
    // is removed immediately before the new tab is completely handled,
    // TST fails to detect the new tab as the successor of this tab. Thus,
    // we treat the new tab as the successor - the first child of this
    // (actually not attached to this tab yet).
    if (successor && successor != tab.$TST.firstChild)
      newParent = successor;
  }
  else {
    closeParentBehavior = TreeBehavior.getParentTabOperationBehavior(tab, {
      context: Constants.kPARENT_TAB_OPERATION_CONTEXT_CLOSE,
      ...removeInfo,
    });
    log('detected closeParentBehaior: ', closeParentBehavior);
  }

  if (closeParentBehavior != Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE &&
      tab.$TST.subtreeCollapsed)
    Tree.collapseExpandSubtree(tab, {
      collapsed: false,
      justNow:   true,
      broadcast: false // because the tab is going to be closed, broadcasted Tree.collapseExpandSubtree can be ignored.
    });

  const postProcessParams = {
    windowId:     tab.windowId,
    removedTab:   tab.$TST.export(true),
    structure:    TreeBehavior.getTreeStructureFromTabs([tab, ...tab.$TST.descendants], {
      full:                 true,
      keepParentOfRootTabs: true
    }),
    insertBefore: tab, // not firstChild, because the "tab" is disappeared from tree.
    parent:       tab.$TST.parent,
    newParent,
    children:     tab.$TST.children,
    descendants:  tab.$TST.descendants,
    nearestFollowingRootTab: tab.$TST.nearestFollowingRootTab,
    closeParentBehavior
  };

  if (tab.$TST.subtreeCollapsed) {
    tryGrantCloseTab(tab, closeParentBehavior).then(async granted => {
      if (!granted)
        return;
      log('Tabs.onRemoving: granted to close ', dumpTab(tab));
      handleRemovingPostProcess(postProcessParams)
    });
    // First we always need to detach children from the closing parent.
    // They will be processed again after confirmation.
    Tree.detachAllChildren(tab, {
      newParent,
      behavior:         Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
      dontExpand:       true,
      dontUpdateIndent: true,
      broadcast:        true
    });
  }
  else {
    await handleRemovingPostProcess(postProcessParams)
  }

  const window = TabsStore.windows.get(tab.windowId);
  if (!window.internalClosingTabs.has(tab.$TST.parentId))
    Tree.detachTab(tab, {
      dontUpdateIndent: true,
      dontSyncParentToOpenerTab: true,
      broadcast:        true
    });
});
async function handleRemovingPostProcess({ closeParentBehavior, windowId, parent, newParent, insertBefore, nearestFollowingRootTab, children, descendants, removedTab, structure } = {}) {
  log('handleRemovingPostProcess ', { closeParentBehavior, windowId, parent, newParent, insertBefore, nearestFollowingRootTab, children, descendants, removedTab, structure });
  if (closeParentBehavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE)
    await closeChildTabs(descendants, {
      triggerTab:        removedTab,
      originalStructure: structure
    });

  const replacedGroupTab = (closeParentBehavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB) ?
    await TabsGroup.tryReplaceTabWithGroup(null, { windowId, parent, children, insertBefore, newParent }) :
    null;

  if (!replacedGroupTab) {
    await Tree.detachAllChildren(null, {
      windowId,
      parent,
      newParent,
      children,
      descendants,
      nearestFollowingRootTab,
      behavior:  closeParentBehavior,
      broadcast: true
    });
  }
}

async function tryGrantCloseTab(tab, closeParentBehavior) {
  log('tryGrantClose: ', { alreadyGranted: configs.grantedRemovingTabIds, closing: dumpTab(tab) });
  const alreadyGranted = configs.grantedRemovingTabIds.includes(tab.id);
  configs.grantedRemovingTabIds = configs.grantedRemovingTabIds.filter(id => id != tab.id);
  if (!tab || alreadyGranted) {
    log(' => no need to confirm');
    return true;
  }

  const self = tryGrantCloseTab;

  self.closingTabIds.push(tab.id);
  if (closeParentBehavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE) {
    self.closingDescendantTabIds = self.closingDescendantTabIds
      .concat(TreeBehavior.getClosingTabsFromParent(tab).map(tab => tab.id));
    self.closingDescendantTabIds = Array.from(new Set(self.closingDescendantTabIds));
  }

  if (self.promisedGrantedToCloseTabs) {
    log(' => have promisedGrantedToCloseTabs');
    return self.promisedGrantedToCloseTabs;
  }

  self.closingTabWasActive = self.closingTabWasActive || tab.active;

  let shouldRestoreCount;
  self.promisedGrantedToCloseTabs = wait(250).then(async () => {
    log(' => confirmation with delay');
    const closingTabIds = new Set(self.closingTabIds);
    let allClosingTabs = new Set();
    allClosingTabs.add(tab);
    self.closingTabIds = Array.from(closingTabIds);
    self.closingDescendantTabIds = mapAndFilterUniq(self.closingDescendantTabIds, id => {
      if (closingTabIds.has(id))
        return undefined;
      const tab = Tab.get(id);
      if (tab) // ignore already closed tabs
        allClosingTabs.add(tab);
      return id;
    });
    allClosingTabs = Array.from(allClosingTabs);
    shouldRestoreCount = self.closingTabIds.length;
    const restorableClosingTabsCount = countMatched(
      allClosingTabs,
      tab => tab.url != 'about:blank' &&
             tab.url != configs.guessNewOrphanTabAsOpenedByNewTabCommandUrl
    );
    log(' => restorableClosingTabsCount: ', restorableClosingTabsCount);
    if (restorableClosingTabsCount > 0) {
      log('tryGrantClose: show confirmation for ', allClosingTabs);
      return Background.confirmToCloseTabs(allClosingTabs.slice(1).map(tab => tab.$TST.sanitized), {
        windowId:   tab.windowId,
        messageKey: 'warnOnCloseTabs_fromOutside_message',
        titleKey:   'warnOnCloseTabs_fromOutside_title',
        minConfirmCount: 0
      });
    }
    return true;
  })
    .then(async (granted) => {
      log(' => granted: ', granted);
      // remove the closed tab itself because it is already closed!
      configs.grantedRemovingTabIds = configs.grantedRemovingTabIds.filter(id => id != tab.id);
      if (granted)
        return true;
      log(`tryGrantClose: not granted, restore ${shouldRestoreCount} tabs`);
      // this is required to wait until the closing tab is stored to the "recently closed" list
      wait(0).then(async () => {
        const restoredTabs = await Commands.restoreTabs(shouldRestoreCount);
        log('tryGrantClose: restored ', restoredTabs);
      });
      return false;
    });

  const granted = await self.promisedGrantedToCloseTabs;
  self.closingTabIds              = [];
  self.closingDescendantTabIds    = [];
  self.closingTabWasActive        = false;
  self.promisedGrantedToCloseTabs = null;
  return granted;
}
tryGrantCloseTab.closingTabIds              = [];
tryGrantCloseTab.closingDescendantTabIds    = [];
tryGrantCloseTab.closingTabWasActive        = false;
tryGrantCloseTab.promisedGrantedToCloseTabs = null;

async function closeChildTabs(tabs, { triggerTab, originalStructure } = {}) {
  //if (!fireTabSubtreeClosingEvent(parent, tabs))
  //  return;

  //markAsClosedSet([parent].concat(tabs));
  // close bottom to top!
  await TabsInternalOperation.removeTabs(tabs.slice(0).reverse(), { triggerTab, originalStructure });
  //fireTabSubtreeClosedEvent(parent, tabs);
}

Tab.onRemoved.addListener((tab, info) => {
  log('Tabs.onRemoved: removed ', dumpTab(tab));
  configs.grantedRemovingTabIds = configs.grantedRemovingTabIds.filter(id => id != tab.id);

  if (info.isWindowClosing)
    return;

  // The removing tab may be attached to another tab or
  // other tabs may be attached to the removing tab.
  // We need to detach such relations always on this timing.
  if (info.oldChildren.length > 0) {
    Tree.detachAllChildren(tab, {
      children:  info.oldChildren,
      parent:    info.oldParent,
      behavior:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
      broadcast: true
    });
  }
  if (info.oldParent) {
    Tree.detachTab(tab, {
      dontSyncParentToOpenerTab: true,
      parent:    info.oldParent,
      broadcast: true
    });
  }
});

browser.windows.onRemoved.addListener(windowId  => {
  const window = TabsStore.windows.get(windowId);
  if (!window)
    return;
  configs.grantedRemovingTabIds = configs.grantedRemovingTabIds.filter(id => !window.tabs.has(id));
});


Tab.onDetached.addListener((tab, info = {}) => {
  log('Tabs.onDetached ', dumpTab(tab));
  let closeParentBehavior = TreeBehavior.getParentTabOperationBehavior(tab, {
    context: Constants.kPARENT_TAB_OPERATION_CONTEXT_MOVE,
    ...info
  });
  if (closeParentBehavior == Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE)
    closeParentBehavior = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD;

  const dontSyncParentToOpenerTab = info.trigger == 'tabs.onDetached';
  Tree.detachAllChildren(tab, {
    dontSyncParentToOpenerTab,
    behavior:  closeParentBehavior,
    broadcast: true
  });
  //reserveCloseRelatedTabs(toBeClosedTabs);
  Tree.detachTab(tab, {
    dontSyncParentToOpenerTab,
    dontUpdateIndent: true,
    broadcast:        true
  });
  //restoreTabAttributes(tab, backupAttributes);
});

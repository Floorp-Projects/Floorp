/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  configs
} from '/common/common.js';
import * as Constants from '/common/constants.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TreeBehavior from '/common/tree-behavior.js';

import Tab from '/common/Tab.js';

import * as Background from './background.js';
import * as BackgroundCache from './background-cache.js';
import * as Tree from './tree.js';
import * as TreeStructure from './tree-structure.js';

function log(...args) {
  internalLogger('background/handle-tree-changes', ...args);
}

let mInitialized = false;

function reserveDetachHiddenTab(tab) {
  reserveDetachHiddenTab.tabs.add(tab);
  if (reserveDetachHiddenTab.reserved)
    clearTimeout(reserveDetachHiddenTab.reserved);
  reserveDetachHiddenTab.reserved = setTimeout(async () => {
    delete reserveDetachHiddenTab.reserved;
    const tabs = new Set(Tab.sort(Array.from(reserveDetachHiddenTab.tabs)));
    reserveDetachHiddenTab.tabs.clear();
    log('try to detach hidden tabs: ', tabs);
    for (const tab of tabs) {
      if (!TabsStore.ensureLivingTab(tab))
        continue;
      for (const descendant of tab.$TST.descendants) {
        if (descendant.hidden)
          continue;
        const nearestVisibleAncestor = descendant.$TST.ancestors.find(ancestor => !ancestor.hidden && !tabs.has(ancestor));
        if (nearestVisibleAncestor &&
            nearestVisibleAncestor == descendant.$TST.parent)
          continue;
        for (const ancestor of descendant.$TST.ancestors) {
          if (!ancestor.hidden &&
              !ancestor.$TST.collapsed)
            break;
          if (!ancestor.$TST.subtreeCollapsed)
            continue;
          await Tree.collapseExpandSubtree(ancestor, {
            collapsed: false,
            broadcast: true
          });
        }
        if (nearestVisibleAncestor) {
          log(` => reattach descendant ${descendant.id} to ${nearestVisibleAncestor.id}`);
          await Tree.attachTabTo(descendant, nearestVisibleAncestor, {
            dontMove:  true,
            broadcast: true
          });
        }
        else {
          log(` => detach descendant ${descendant.id}`);
          await Tree.detachTab(descendant, {
            broadcast: true
          });
        }
      }
      if (tab.$TST.hasParent &&
          !tab.$TST.parent.hidden) {
        log(` => detach hidden tab ${tab.id}`);
        await Tree.detachTab(tab, {
          broadcast: true
        });
      }
    }
  }, 100);
}
reserveDetachHiddenTab.tabs = new Set();

Tab.onHidden.addListener(tab => {
  if (configs.fixupTreeOnTabVisibilityChanged)
    reserveDetachHiddenTab(tab);
});

function reserveAttachShownTab(tab) {
  tab.$TST.addState(Constants.kTAB_STATE_SHOWING);
  reserveAttachShownTab.tabs.add(tab);
  if (reserveAttachShownTab.reserved)
    clearTimeout(reserveAttachShownTab.reserved);
  reserveAttachShownTab.reserved = setTimeout(async () => {
    delete reserveAttachShownTab.reserved;
    const tabs = new Set(Tab.sort(Array.from(reserveAttachShownTab.tabs)));
    reserveAttachShownTab.tabs.clear();
    log('try to attach shown tabs: ', tabs);
    for (const tab of tabs) {
      if (!TabsStore.ensureLivingTab(tab) ||
          tab.$TST.hasParent) {
        tab.$TST.removeState(Constants.kTAB_STATE_SHOWING);
        continue;
      }
      const referenceTabs = TreeBehavior.calculateReferenceTabsFromInsertionPosition(tab, {
        context:      Constants.kINSERTION_CONTEXT_SHOWN,
        insertAfter:  tab.$TST.nearestVisiblePrecedingTab,
        // Instead of nearestFollowingForeignerTab, to avoid placing the tab
        // after hidden tabs (too far from the target)
        insertBefore: tab.$TST.unsafeNearestFollowingForeignerTab
      });
      if (referenceTabs.parent) {
        log(` => attach shown tab ${tab.id} to ${referenceTabs.parent.id}`);
        await Tree.attachTabTo(tab, referenceTabs.parent, {
          insertBefore: referenceTabs.insertBefore,
          insertAfter:  referenceTabs.insertAfter,
          broadcast:    true
        });
      }
      tab.$TST.removeState(Constants.kTAB_STATE_SHOWING);
    }
  }, 100);
}
reserveAttachShownTab.tabs = new Set();

Tab.onShown.addListener(tab => {
  if (configs.fixupTreeOnTabVisibilityChanged)
    reserveAttachShownTab(tab);
});

Background.onReady.addListener(() => {
  mInitialized = true;
});

Tree.onSubtreeCollapsedStateChanged.addListener((tab, _info) => {
  if (mInitialized)
    TreeStructure.reserveToSaveTreeStructure(tab.windowId);
  BackgroundCache.markWindowCacheDirtyFromTab(tab, Constants.kWINDOW_STATE_CACHED_SIDEBAR_COLLAPSED_DIRTY);
});

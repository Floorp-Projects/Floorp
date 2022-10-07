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
import * as TabsInternalOperation from '/common/tabs-internal-operation.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TreeBehavior from '/common/tree-behavior.js';

import Tab from '/common/Tab.js';

import * as TabsMove from './tabs-move.js';
import * as TabsOpen from './tabs-open.js';
import * as Tree from './tree.js';

function log(...args) {
  internalLogger('background/handle-new-tabs', ...args);
}


// this should return false if the tab is / may be moved while processing
Tab.onCreating.addListener((tab, info = {}) => {
  if (info.duplicatedInternally)
    return true;

  log('Tabs.onCreating ', dumpTab(tab), tab.openerTabId, info);

  const possibleOpenerTab = info.activeTab || Tab.getActiveTab(tab.windowId);
  const opener = tab.$TST.openerTab;
  if (opener) {
    tab.$TST.setAttribute(Constants.kPERSISTENT_ORIGINAL_OPENER_TAB_ID, opener.$TST.uniqueId.id);
    if (!info.bypassTabControl)
      TabsStore.addToBeGroupedTab(tab);
  }
  else {
    let dontMove = false;
    if (!info.maybeOrphan &&
        !info.bypassTabControl &&
        possibleOpenerTab &&
        !info.restored) {
      let autoAttachBehavior = configs.autoAttachOnNewTabCommand;
      if (tab.$TST.nextTab &&
          possibleOpenerTab == tab.$TST.previousTab) {
        // New tab opened with browser.tabs.insertAfterCurrent=true may have
        // next tab. In this case the tab is expected to be placed next to the
        // active tab always, so we should change the behavior specially.
        // See also:
        //   https://github.com/piroor/treestyletab/issues/2054
        //   https://github.com/piroor/treestyletab/issues/2194#issuecomment-505272940
        dontMove = true;
        switch (autoAttachBehavior) {
          case Constants.kNEWTAB_OPEN_AS_ORPHAN:
          case Constants.kNEWTAB_OPEN_AS_SIBLING:
          case Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING:
            if (possibleOpenerTab.$TST.hasChild)
              autoAttachBehavior = Constants.kNEWTAB_OPEN_AS_CHILD;
            else
              autoAttachBehavior = Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING;
            break;

          case Constants.kNEWTAB_OPEN_AS_CHILD:
          default:
            break;
        }
      }
      if (tab.$TST.isNewTabCommandTab) {
        if (!info.positionedBySelf) {
          log('behave as a tab opened by new tab command');
          return handleNewTabFromActiveTab(tab, {
            activeTab:                 possibleOpenerTab,
            autoAttachBehavior,
            dontMove,
            inheritContextualIdentityMode: configs.inheritContextualIdentityToChildTabMode
          }).then(moved => !moved);
        }
        return false;
      }
      else if (possibleOpenerTab != tab) {
        tab.$TST.possibleOpenerTab = possibleOpenerTab.id;
      }
      tab.$TST.isNewTab = !info.fromExternal;
    }
    if (info.fromExternal &&
        !info.bypassTabControl) {
      log('behave as a tab opened from external application');
      // we may need to reopen the tab with loaded URL
      if (configs.inheritContextualIdentityToTabsFromExternalMode != Constants.kCONTEXTUAL_IDENTITY_DEFAULT)
        tab.$TST.fromExternal = true;
      return Tree.behaveAutoAttachedTab(tab, {
        baseTab:   possibleOpenerTab,
        behavior:  configs.autoAttachOnOpenedFromExternal,
        dontMove,
        broadcast: true
      }).then(moved => !moved);
    }
    log('behave as a tab opened with any URL');
    if (!info.restored &&
        !info.positionedBySelf &&
        !info.bypassTabControl &&
        configs.autoAttachOnAnyOtherTrigger != Constants.kNEWTAB_DO_NOTHING) {
      if (configs.inheritContextualIdentityToTabsFromAnyOtherTriggerMode != Constants.kCONTEXTUAL_IDENTITY_DEFAULT)
        tab.$TST.anyOtherTrigger = true;
      log('controlled as a new tab from other unknown trigger');
      return Tree.behaveAutoAttachedTab(tab, {
        baseTab:   possibleOpenerTab,
        behavior:  configs.autoAttachOnAnyOtherTrigger,
        dontMove,
        broadcast: true
      }).then(moved => !moved);
    }
    tab.$TST.positionedBySelf = info.positionedBySelf;
    return true;
  }

  log(`opener: ${dumpTab(opener)}, positionedBySelf = ${info.positionedBySelf}`);
  if (!info.bypassTabControl &&
      opener &&
      opener.pinned &&
      opener.windowId == tab.windowId) {
    return handleTabsFromPinnedOpener(tab, opener).then(moved => !moved);
  }
  else if (!info.maybeOrphan || info.bypassTabControl) {
    if (info.fromExternal &&
        !info.bypassTabControl &&
        configs.inheritContextualIdentityToTabsFromExternalMode != Constants.kCONTEXTUAL_IDENTITY_DEFAULT)
      tab.$TST.fromExternal = true;
    const behavior = info.fromExternal && !info.bypassTabControl ?
      configs.autoAttachOnOpenedFromExternal :
      info.duplicated ?
        configs.autoAttachOnDuplicated :
        configs.autoAttachOnOpenedWithOwner;
    return Tree.behaveAutoAttachedTab(tab, {
      baseTab:   opener,
      behavior,
      dontMove:  info.positionedBySelf || info.mayBeReplacedWithContainer,
      broadcast: true
    }).then(moved => !moved);
  }
  return true;
});

async function handleNewTabFromActiveTab(tab, params = {}) {
  const activeTab = params.activeTab;
  log('handleNewTabFromActiveTab: activeTab = ', dumpTab(activeTab), params);
  if (activeTab &&
      activeTab.$TST.ancestors.includes(tab)) {
    log(' => ignore restored ancestor tab');
    return false;
  }
  const moved = await Tree.behaveAutoAttachedTab(tab, {
    baseTab:   activeTab,
    behavior:  params.autoAttachBehavior,
    broadcast: true,
    dontMove:  params.dontMove || false
  });
  if (tab.cookieStoreId && tab.cookieStoreId != 'firefox-default') {
    log('handleNewTabFromActiveTab: do not reopen tab opened with non-default contextual identity ', tab.cookieStoreId);
    return moved;
  }

  const parent = tab.$TST.parent;
  let cookieStoreId = null;
  switch (params.inheritContextualIdentityMode) {
    case Constants.kCONTEXTUAL_IDENTITY_FROM_PARENT:
      if (parent)
        cookieStoreId = parent.cookieStoreId;
      break;

    case Constants.kCONTEXTUAL_IDENTITY_FROM_LAST_ACTIVE:
      cookieStoreId = activeTab.cookieStoreId
      break;

    default:
      return moved;
  }
  if ((tab.cookieStoreId || 'firefox-default') == (cookieStoreId || 'firefox-default')) {
    log('handleNewTabFromActiveTab: no need to reopen with inherited contextual identity ', cookieStoreId);
    return moved;
  }

  log('handleNewTabFromActiveTab: reopen with inherited contextual identity ', cookieStoreId);
  // We need to prevent grouping of this original tab and the reopened tab
  // by the "multiple tab opened in XXX msec" feature.
  const window = TabsStore.windows.get(tab.windowId);
  window.openedNewTabs.delete(tab.id);
  await TabsOpen.openURIInTab(params.url || null, {
    windowId: activeTab.windowId,
    parent,
    insertBefore: tab,
    cookieStoreId
  });
  TabsInternalOperation.removeTab(tab);
  return moved;
}

async function handleTabsFromPinnedOpener(tab, opener) {
  const parent = Tab.getGroupTabForOpener(opener);
  if (parent) {
    log('handleTabsFromPinnedOpener: attach to corresponding group tab');
    tab.$TST.setAttribute(Constants.kPERSISTENT_ALREADY_GROUPED_FOR_PINNED_OPENER, true);
    tab.$TST.alreadyMovedAsOpenedFromPinnedOpener = true;
    // it could be updated already...
    const lastRelatedTab = opener.$TST.lastRelatedTabId == tab.id ? opener.$TST.previousLastRelatedTab : opener.$TST.lastRelatedTab;
    return Tree.attachTabTo(tab, parent, {
      lastRelatedTab,
      forceExpand:    true, // this is required to avoid the group tab itself is active from active tab in collapsed tree
      broadcast:      true
    });
  }

  if (configs.autoGroupNewTabsFromPinned &&
      tab.$TST.needToBeGroupedSiblings.length > 0) {
    log('handleTabsFromPinnedOpener: controlled by auto-grouping');
    return false;
  }

  switch (configs.insertNewTabFromPinnedTabAt) {
    case Constants.kINSERT_NEXT_TO_LAST_RELATED_TAB: {
      const lastRelatedTab = opener.$TST.lastRelatedTab;
      if (lastRelatedTab) {
        log(`handleTabsFromPinnedOpener: place after last related tab ${dumpTab(lastRelatedTab)}`);
        tab.$TST.alreadyMovedAsOpenedFromPinnedOpener = true;
        return TabsMove.moveTabAfter(tab, lastRelatedTab.$TST.lastDescendant || lastRelatedTab, {
          delayedMove: true,
          broadcast:   true
        });
      }
    };
    case Constants.kINSERT_TOP: {
      const lastPinnedTab = Tab.getLastPinnedTab(tab.windowId);
      if (lastPinnedTab) {
        log(`handleTabsFromPinnedOpener: opened from pinned opener: place after last pinned tab ${dumpTab(lastPinnedTab)}`);
        tab.$TST.alreadyMovedAsOpenedFromPinnedOpener = true;
        return TabsMove.moveTabAfter(tab, lastPinnedTab, {
          delayedMove: true,
          broadcast:   true
        });
      }
      const firstNormalTab = Tab.getFirstNormalTab(tab.windowId);
      if (firstNormalTab) {
        log(`handleTabsFromPinnedOpener: opened from pinned opener: place before first pinned tab ${dumpTab(firstNormalTab)}`);
        tab.$TST.alreadyMovedAsOpenedFromPinnedOpener = true;
        return TabsMove.moveTabBefore(tab, firstNormalTab, {
          delayedMove: true,
          broadcast:   true
        });
      }
    }; break;

    case Constants.kINSERT_END: {
      const lastTab = Tab.getLastTab(tab.windowId);
      log('handleTabsFromPinnedOpener: opened from pinned opener: place after the last tab ', lastTab);
      tab.$TST.alreadyMovedAsOpenedFromPinnedOpener = true;
      return TabsMove.moveTabAfter(tab, lastTab, {
        delayedMove: true,
        broadcast:   true
      });
    };
  }

  return Promise.resolve(false);
}

Tab.onCreated.addListener((tab, info = {}) => {
  if (!info.duplicated ||
      info.bypassTabControl)
    return;
  const original = info.originalTab;
  log('duplicated ', dumpTab(tab), dumpTab(original));
  if (info.duplicatedInternally) {
    log('duplicated by internal operation');
    tab.$TST.addState(Constants.kTAB_STATE_DUPLICATING, { broadcast: true });
    TabsStore.addDuplicatingTab(tab);
  }
  else {
    // On old versions of Firefox, duplicated tabs had no openerTabId so they were
    // not handled by Tab.onCreating listener. Today they are already handled before
    // here, so this is just a failsafe (or for old versions of Firefox).
    // See also: https://github.com/piroor/treestyletab/issues/2830#issuecomment-831414189
    Tree.behaveAutoAttachedTab(tab, {
      baseTab:   original,
      behavior:  configs.autoAttachOnDuplicated,
      dontMove:  info.positionedBySelf || info.movedBySelfWhileCreation || info.mayBeReplacedWithContainer,
      broadcast: true
    });
  }
});

Tab.onUpdated.addListener((tab, changeInfo) => {
  if ('openerTabId' in changeInfo &&
      configs.syncParentTabAndOpenerTab &&
      !tab.$TST.updatingOpenerTabIds.includes(changeInfo.openerTabId) /* accept only changes from outside of TST */) {
    Tab.waitUntilTrackedAll(tab.windowId).then(() => {
      const parent = tab.$TST.openerTab;
      if (!parent ||
          parent.windowId != tab.windowId ||
          parent == tab.$TST.parent)
        return;
      Tree.attachTabTo(tab, parent, {
        insertAt:    Constants.kINSERT_NEAREST,
        forceExpand: tab.active,
        broadcast:   true
      });
    });
  }

  if (tab.$TST.openedCompletely &&
      tab.windowId == tab.$windowIdOnCreated && // Don't treat tab as "opened from active tab" if it is moved across windows while loading
      (changeInfo.url || changeInfo.status == 'complete') &&
      (tab.$TST.isNewTab ||
       tab.$TST.fromExternal ||
       tab.$TST.anyOtherTrigger)) {
    log('loaded tab ', dumpTab(tab), { isNewTab: tab.$TST.isNewTab, fromExternal: tab.$TST.fromExternal, anyOtherTrigger: tab.$TST.anyOtherTrigger });
    delete tab.$TST.isNewTab;
    const possibleOpenerTab = Tab.get(tab.$TST.possibleOpenerTab);
    delete tab.$TST.possibleOpenerTab;
    log('possibleOpenerTab ', dumpTab(possibleOpenerTab));

    if (tab.$TST.fromExternal) {
      delete tab.$TST.fromExternal;
      log('behave as a tab opened from external application (delayed)');
      handleNewTabFromActiveTab(tab, {
        url:                           tab.url,
        activeTab:                     possibleOpenerTab,
        autoAttachBehavior:            configs.autoAttachOnOpenedFromExternal,
        inheritContextualIdentityMode: configs.inheritContextualIdentityToTabsFromExternalMode
      });
      return;
    }

    if (tab.$TST.anyOtherTrigger) {
      delete tab.$TST.anyOtherTrigger;
      log('behave as a tab opened from any other trigger (delayed)');
      handleNewTabFromActiveTab(tab, {
        url:                           tab.url,
        activeTab:                     possibleOpenerTab,
        autoAttachBehavior:            configs.autoAttachOnAnyOtherTrigger,
        inheritContextualIdentityMode: configs.inheritContextualIdentityToTabsFromAnyOtherTriggerMode
      });
      return;
    }

    const window = TabsStore.windows.get(tab.windowId);
    log('window.openedNewTabs ', window.openedNewTabs);
    if (tab.$TST.parent ||
        !possibleOpenerTab ||
        window.openedNewTabs.has(tab.id) ||
        tab.$TST.openedWithOthers ||
        tab.$TST.positionedBySelf) {
      log(' => no need to control');
      return;
    }

    if (tab.$TST.isNewTabCommandTab) {
      log('behave as a tab opened by new tab command (delayed)');
      handleNewTabFromActiveTab(tab, {
        activeTab:                     possibleOpenerTab,
        autoAttachBehavior:            configs.autoAttachOnNewTabCommand,
        inheritContextualIdentityMode: configs.inheritContextualIdentityToChildTabMode
      });
      return;
    }

    const siteMatcher  = /^\w+:\/\/([^\/]+)(?:$|\/.*$)/;
    const openerTabSite = possibleOpenerTab.url.match(siteMatcher);
    const newTabSite    = tab.url.match(siteMatcher);
    if (openerTabSite && newTabSite && openerTabSite[1] == newTabSite[1]) {
      log('behave as a tab opened from same site (delayed)');
      handleNewTabFromActiveTab(tab, {
        url:                           tab.url,
        activeTab:                     possibleOpenerTab,
        autoAttachBehavior:            configs.autoAttachSameSiteOrphan,
        inheritContextualIdentityMode: configs.inheritContextualIdentityToSameSiteOrphanMode
      });
      return;
    }

    log('checking special openers (delayed)', { opener: possibleOpenerTab.url, child: tab.url });
    for (const rule of Constants.kAGGRESSIVE_OPENER_TAB_DETECTION_RULES_WITH_URL) {
      if (rule.opener.test(possibleOpenerTab.url) &&
          rule.child.test(tab.url)) {
        log('behave as a tab opened from special opener (delayed)', { rule });
        handleNewTabFromActiveTab(tab, {
          url:                tab.url,
          activeTab:          possibleOpenerTab,
          autoAttachBehavior: Constants.kNEWTAB_OPEN_AS_CHILD
        });
        return;
      }
    }
  }
});


Tab.onAttached.addListener(async (tab, attachInfo = {}) => {
  if (!attachInfo.windowId)
    return;

  const parentTabOperationBehavior = TreeBehavior.getParentTabOperationBehavior(tab, {
    context:  Constants.kPARENT_TAB_OPERATION_CONTEXT_MOVE,
    ...attachInfo,
  });
  if (parentTabOperationBehavior != Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE)
    return;

  log('Tabs.onAttached ', dumpTab(tab), attachInfo);

  log('descendants of attached tab: ', () => attachInfo.descendants.map(dumpTab));
  const movedTabs = await Tree.moveTabs(attachInfo.descendants, {
    destinationWindowId: tab.windowId,
    insertAfter:         tab
  });
  log('moved descendants: ', () => movedTabs.map(dumpTab));
  if (attachInfo.descendants.length == movedTabs.length) {
    await Tree.applyTreeStructureToTabs(
      [tab, ...movedTabs],
      attachInfo.structure
    );
  }
  else {
    for (const movedTab of movedTabs) {
      Tree.attachTabTo(movedTab, tab, {
        broadcast: true,
        dontMove:  true
      });
    }
  }
});

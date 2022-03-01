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
  configs,
  isMacOS,
} from '/common/common.js';

import * as ApiTabs from '/common/api-tabs.js';
import * as Constants from '/common/constants.js';
import * as Permissions from '/common/permissions.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TabsInternalOperation from '/common/tabs-internal-operation.js';
import * as TSTAPI from '/common/tst-api.js';

import Tab from '/common/Tab.js';
import Window from '/common/Window.js';

import * as Background from './background.js';
import * as Tree from './tree.js';

function log(...args) {
  internalLogger('background/handle-tab-focus', ...args);
}


let mTabSwitchedByShortcut       = false;
let mMaybeTabSwitchingByShortcut = false;

Window.onInitialized.addListener(window => {
  browser.tabs.query({
    windowId: window.id,
    active:   true
  })
    .then(activeTabs => {
      if (!window.lastActiveTab)
        window.lastActiveTab = activeTabs[0].id;
    });
});

Tab.onActivating.addListener(async (tab, info = {}) => { // return false if the activation should be canceled
  log('Tabs.onActivating ', { tab: dumpTab(tab), info });
  if (tab.$TST.shouldReloadOnSelect) {
    browser.tabs.reload(tab.id)
      .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
    delete tab.$TST.shouldReloadOnSelect;
  }
  const window = TabsStore.windows.get(tab.windowId);
  log('  lastActiveTab: ', window.lastActiveTab);
  const lastActiveTab = Tab.get(window.lastActiveTab);
  cancelDelayedExpand(lastActiveTab);
  let shouldSkipCollapsed = (
    !info.byInternalOperation &&
    mMaybeTabSwitchingByShortcut &&
    configs.skipCollapsedTabsForTabSwitchingShortcuts
  );
  mTabSwitchedByShortcut = mMaybeTabSwitchingByShortcut;
  const focusDirection = !lastActiveTab ?
    0 :
    (!lastActiveTab.$TST.nearestVisiblePrecedingTab &&
     !tab.$TST.nearestVisibleFollowingTab) ?
      -1 :
      (!lastActiveTab.$TST.nearestVisibleFollowingTab &&
       !tab.$TST.nearestVisiblePrecedingTab) ?
        1 :
        (lastActiveTab.index > tab.index) ?
          -1 :
          1;
  const cache = {};
  if (tab.$TST.collapsed) {
    if (!tab.$TST.parent) {
      // This is invalid case, generally never should happen,
      // but actually happen on some environment:
      // https://github.com/piroor/treestyletab/issues/1717
      // So, always expand orphan collapsed tab as a failsafe.
      Tree.collapseExpandTab(tab, {
        collapsed: false,
        broadcast: true
      });
      await handleNewActiveTab(tab, info);
    }
    else if (!shouldSkipCollapsed) {
      log('=> reaction for focus given from outside of TST');
      let allowed = false;
      if (configs.unfocusableCollapsedTab) {
        log('  => apply unfocusableCollapsedTab');
        allowed = await TSTAPI.tryOperationAllowed(
          TSTAPI.kNOTIFY_TRY_EXPAND_TREE_FROM_FOCUSED_COLLAPSED_TAB,
          { tab: new TSTAPI.TreeItem(tab, { cache }),
            focusDirection },
          { tabProperties: ['tab'] }
        );
        if (allowed) {
          const toBeExpandedAncestors = [tab].concat(tab.$TST.ancestors) ;
          for (const ancestor of toBeExpandedAncestors) {
            Tree.collapseExpandSubtree(ancestor, {
              collapsed: false,
              broadcast: true
            });
          }
        }
        else {
          shouldSkipCollapsed = true;
          log('  => canceled by someone.');
        }
      }
      info.allowed = allowed;
      if (!shouldSkipCollapsed)
        await handleNewActiveTab(tab, info);
    }
    if (shouldSkipCollapsed) {
      log('=> reaction for focusing collapsed descendant while Ctrl-Tab/Ctrl-Shift-Tab');
      let successor = tab.$TST.nearestVisibleAncestorOrSelf;
      if (!successor) // this seems invalid case...
        return false;
      log('successor = ', successor.id);
      if (shouldSkipCollapsed &&
          (window.lastActiveTab == successor.id ||
           successor.$TST.descendants.some(tab => tab.id == window.lastActiveTab)) &&
          focusDirection > 0) {
        log('=> redirect successor (focus moved from the successor itself or its descendants)');
        successor = successor.$TST.nearestVisibleFollowingTab;
        if (successor &&
            successor.discarded &&
            configs.avoidDiscardedTabToBeActivatedIfPossible)
          successor = successor.$TST.nearestLoadedTabInTree ||
                        successor.$TST.nearestLoadedTab ||
                        successor;
        if (!successor)
          successor = Tab.getFirstVisibleTab(tab.windowId);
        log('=> ', successor.id);
      }
      else if (!mTabSwitchedByShortcut && // intentional focus to a discarded tabs by Ctrl-Tab/Ctrl-Shift-Tab is always allowed!
               successor.discarded &&
               configs.avoidDiscardedTabToBeActivatedIfPossible) {
        log('=> redirect successor (successor is discarded)');
        successor = successor.$TST.nearestLoadedTabInTree ||
                      successor.$TST.nearestLoadedTab ||
                      successor;
        log('=> ', successor.id);
      }
      const allowed = await TSTAPI.tryOperationAllowed(
        TSTAPI.kNOTIFY_TRY_REDIRECT_FOCUS_FROM_COLLAPSED_TAB,
        { tab: new TSTAPI.TreeItem(tab, { cache }),
          focusDirection },
        { tabProperties: ['tab'] }
      );
      if (allowed) {
        window.lastActiveTab = successor.id;
        if (mMaybeTabSwitchingByShortcut)
          setupDelayedExpand(successor);
        TabsInternalOperation.activateTab(successor, { silently: true });
        log('Tabs.onActivating: discarded? ', dumpTab(tab), tab && tab.discarded);
        if (tab.discarded)
          tab.$TST.discardURLAfterCompletelyLoaded = tab.url;
        return false;
      }
      else {
        log('  => canceled by someone.');
      }
    }
  }
  else if (info.byActiveTabRemove &&
           (!configs.autoCollapseExpandSubtreeOnSelect ||
            configs.autoCollapseExpandSubtreeOnSelectExceptActiveTabRemove)) {
    log('=> reaction for removing current tab');
    window.lastActiveTab = tab.id;
    return true;
  }
  else if (tab.$TST.hasChild &&
           tab.$TST.subtreeCollapsed &&
           !shouldSkipCollapsed) {
    log('=> reaction for newly active parent tab');
    await handleNewActiveTab(tab, info);
  }
  delete tab.$TST.discardOnCompletelyLoaded;
  window.lastActiveTab = tab.id;

  if (mMaybeTabSwitchingByShortcut)
    setupDelayedExpand(tab);
  else
    tryHighlightBundledTab(tab, {
      ...info,
      shouldSkipCollapsed
    });

  return true;
});

async function handleNewActiveTab(tab, info = {}) {
  log('handleNewActiveTab: ', dumpTab(tab), info);
  const shouldCollapseExpandNow = configs.autoCollapseExpandSubtreeOnSelect;
  const canCollapseTree         = shouldCollapseExpandNow;
  const canExpandTree           = shouldCollapseExpandNow && !info.silently;
  if (canExpandTree &&
      info.allowed !== false) {
    const allowed = await TSTAPI.tryOperationAllowed(
      tab.active ?
        TSTAPI.kNOTIFY_TRY_EXPAND_TREE_FROM_FOCUSED_PARENT :
        TSTAPI.kNOTIFY_TRY_EXPAND_TREE_FROM_FOCUSED_BUNDLED_PARENT,
      { tab: new TSTAPI.TreeItem(tab) },
      { tabProperties: ['tab'] }
    );
    if (!allowed)
      return;
    if (canCollapseTree &&
        configs.autoExpandIntelligently)
      Tree.collapseExpandTreesIntelligentlyFor(tab, {
        broadcast: true
      });
    else
      Tree.collapseExpandSubtree(tab, {
        collapsed: false,
        broadcast: true
      });
  }
}

async function tryHighlightBundledTab(tab, info) {
  const bundledTab = tab.$TST.bundledTab;
  const oldBundledTabs = TabsStore.bundledActiveTabsInWindow.get(tab.windowId);
  for (const tab of oldBundledTabs.values()) {
    if (tab == bundledTab)
      continue;
    tab.$TST.removeState(Constants.kTAB_STATE_BUNDLED_ACTIVE, { broadcast: true });
  }

  if (!bundledTab)
    return;

  bundledTab.$TST.addState(Constants.kTAB_STATE_BUNDLED_ACTIVE, { broadcast: true });

  await wait(100);
  if (!tab.active || // ignore tab already inactivated while waiting
      tab.$TST.hasOtherHighlighted || // ignore manual highlighting
      bundledTab.pinned)
    return;

  if (bundledTab.$TST.hasChild &&
      bundledTab.$TST.subtreeCollapsed &&
      !info.shouldSkipCollapsed)
    await handleNewActiveTab(bundledTab, info);
}

Tab.onUpdated.addListener((tab, changeInfo = {}) => {
  if ('url' in changeInfo) {
    if (tab.$TST.discardURLAfterCompletelyLoaded &&
        tab.$TST.discardURLAfterCompletelyLoaded != changeInfo.url)
      delete tab.$TST.discardURLAfterCompletelyLoaded;
  }
});

Tab.onStateChanged.addListener(tab => {
  if (tab.status != 'complete')
    return;

  if (typeof browser.tabs.discard == 'function') {
    if (tab.url == tab.$TST.discardURLAfterCompletelyLoaded &&
        configs.autoDiscardTabForUnexpectedFocus) {
      log('Try to discard accidentally restored tab (on restored) ', dumpTab(tab));
      wait(configs.autoDiscardTabForUnexpectedFocusDelay).then(() => {
        if (!TabsStore.ensureLivingTab(tab) ||
            tab.active)
          return;
        if (tab.status == 'complete')
          browser.tabs.discard(tab.id)
            .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
        else
          tab.$TST.discardOnCompletelyLoaded = true;
      });
    }
    else if (tab.$TST.discardOnCompletelyLoaded && !tab.active) {
      log('Discard accidentally restored tab (on complete) ', dumpTab(tab));
      browser.tabs.discard(tab.id)
        .catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
    }
  }
  delete tab.$TST.discardURLAfterCompletelyLoaded;
  delete tab.$TST.discardOnCompletelyLoaded;
});

async function setupDelayedExpand(tab) {
  if (!tab)
    return;
  cancelDelayedExpand(tab);
  TabsStore.removeToBeExpandedTab(tab);
  const [ctrlTabHandlingEnabled, allowedToExpandViaAPI] = await Promise.all([
    Permissions.isGranted(Permissions.ALL_URLS),
    TSTAPI.tryOperationAllowed(
      TSTAPI.kNOTIFY_TRY_EXPAND_TREE_FROM_LONG_PRESS_CTRL_KEY,
      { tab: new TSTAPI.TreeItem(tab) },
      { tabProperties: ['tab'] }
    ),
  ]);
  if (!configs.autoExpandOnTabSwitchingShortcuts ||
      !tab.$TST.hasChild ||
      !tab.$TST.subtreeCollapsed ||
      !ctrlTabHandlingEnabled ||
      !allowedToExpandViaAPI)
    return;
  TabsStore.addToBeExpandedTab(tab);
  tab.$TST.delayedExpand = setTimeout(() => {
    if (!tab.$TST.delayedExpand) { // already canceled
      log('delayed expand is already canceled ', tab.id);
      return;
    }
    log('delayed expand by long-press of ctrl key on ', tab.id);
    TabsStore.removeToBeExpandedTab(tab);
    Tree.collapseExpandTreesIntelligentlyFor(tab, {
      broadcast: true
    });
  }, configs.autoExpandOnTabSwitchingShortcutsDelay);
}

function cancelDelayedExpand(tab) {
  if (!tab ||
      !tab.$TST.delayedExpand)
    return;
  clearTimeout(tab.$TST.delayedExpand);
  delete tab.$TST.delayedExpand;
  TabsStore.removeToBeExpandedTab(tab);
}

function cancelAllDelayedExpand(windowId) {
  for (const tab of TabsStore.toBeExpandedTabsInWindow.get(windowId)) {
    cancelDelayedExpand(tab);
  }
}

Tab.onCollapsedStateChanged.addListener((tab, info = {}) => {
  if (info.collapsed)
    tab.$TST.addState(Constants.kTAB_STATE_COLLAPSED_DONE);
  else
    tab.$TST.removeState(Constants.kTAB_STATE_COLLAPSED_DONE);
});


Background.onInit.addListener(() => {
  browser.windows.onFocusChanged.addListener(() => {
    mMaybeTabSwitchingByShortcut = false;
  });
});

Background.onBuilt.addListener(() => {
  browser.runtime.onMessage.addListener(onMessage);
});


function onMessage(message, sender) {
  if (!message ||
      typeof message.type != 'string')
    return;

  //log('onMessage: ', message, sender);
  switch (message.type) {
    case Constants.kNOTIFY_TAB_MOUSEDOWN:
      mMaybeTabSwitchingByShortcut =
        mTabSwitchedByShortcut = false;
      break;

    case Constants.kCOMMAND_NOTIFY_MAY_START_TAB_SWITCH: {
      if (message.modifier != (configs.accelKey || (isMacOS() ? 'meta' : 'control')))
        return;
      log('kCOMMAND_NOTIFY_MAY_START_TAB_SWITCH ', message.modifier);
      mMaybeTabSwitchingByShortcut = true;
      if (sender.tab && sender.tab.active) {
        const window = TabsStore.windows.get(sender.tab.windowId);
        window.lastActiveTab = sender.tab.id;
      }
    }; break;
    case Constants.kCOMMAND_NOTIFY_MAY_END_TAB_SWITCH:
      if (message.modifier != (configs.accelKey || (isMacOS() ? 'meta' : 'control')))
        return;
      log('kCOMMAND_NOTIFY_MAY_END_TAB_SWITCH ', message.modifier);
      return (async () => {
        if (mTabSwitchedByShortcut &&
            configs.skipCollapsedTabsForTabSwitchingShortcuts &&
            sender.tab) {
          await Tab.waitUntilTracked(sender.tab.id);
          let tab = Tab.get(sender.tab.id);
          if (!tab) {
            const tabs = await browser.tabs.query({ currentWindow: true, active: true }).catch(ApiTabs.createErrorHandler());
            await Tab.waitUntilTracked(tabs[0].id);
            tab = Tab.get(tabs[0].id);
          }
          cancelAllDelayedExpand(tab.windowId);
          if (configs.autoCollapseExpandSubtreeOnSelect &&
              tab &&
              TabsStore.windows.get(tab.windowId).lastActiveTab == tab.id &&
              (await TSTAPI.tryOperationAllowed(
                TSTAPI.kNOTIFY_TRY_EXPAND_TREE_FROM_END_TAB_SWITCH,
                { tab: new TSTAPI.TreeItem(tab) },
                { tabProperties: ['tab'] }
              ))) {
            Tree.collapseExpandSubtree(tab, {
              collapsed: false,
              broadcast: true
            });
          }
        }
        mMaybeTabSwitchingByShortcut =
          mTabSwitchedByShortcut = false;
      })();
  }
}

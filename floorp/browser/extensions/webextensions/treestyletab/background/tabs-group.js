/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  configs,
  wait,
  countMatched,
  dumpTab
} from '/common/common.js';
import * as Constants from '/common/constants.js';

import Tab from '/common/Tab.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TabsInternalOperation from '/common/tabs-internal-operation.js';

import * as TabsOpen from './tabs-open.js';
import * as TabsMove from './tabs-move.js';
import * as Tree from './tree.js';

function log(...args) {
  internalLogger('background/tabs-group', ...args);
}

export function makeGroupTabURI({ title, temporary, temporaryAggressive, openerTabId } = {}) {
  const url = new URL(Constants.kGROUP_TAB_URI);

  if (title)
    url.searchParams.set('title', title);

  if (temporaryAggressive)
    url.searchParams.set('temporaryAggressive', 'true');
  else if (temporary)
    url.searchParams.set('temporary', 'true');

  if (openerTabId)
    url.searchParams.set('openerTabId', openerTabId);

  return url.href;
}

export function temporaryStateParams(state) {
  switch (state) {
    case Constants.kGROUP_TAB_TEMPORARY_STATE_PASSIVE:
      return { temporary: true };
    case Constants.kGROUP_TAB_TEMPORARY_STATE_AGGRESSIVE:
      return { temporaryAggressive: true };
    default:
      break;
  }
  return {};
}

export async function groupTabs(tabs, { broadcast, ...groupTabOptions } = {}) {
  const rootTabs = Tab.collectRootTabs(tabs);
  if (rootTabs.length <= 0)
    return null;

  log('groupTabs: ', () => tabs.map(dumpTab));

  const uri = makeGroupTabURI({
    title:     browser.i18n.getMessage('groupTab_label', rootTabs[0].title),
    temporary: true,
    ...groupTabOptions
  });
  const groupTab = await TabsOpen.openURIInTab(uri, {
    windowId:     rootTabs[0].windowId,
    parent:       rootTabs[0].$TST.parent,
    insertBefore: rootTabs[0],
    inBackground: true
  });

  await Tree.detachTabsFromTree(tabs, {
    broadcast: !!broadcast
  });
  await TabsMove.moveTabsAfter(tabs.slice(1), tabs[0], {
    broadcast: !!broadcast
  });
  for (const tab of rootTabs) {
    await Tree.attachTabTo(tab, groupTab, {
      forceExpand: true, // this is required to avoid the group tab itself is active from active tab in collapsed tree
      dontMove:  true,
      broadcast: !!broadcast
    });
  }
  return groupTab;
}

export function reserveToCleanupNeedlessGroupTab(tabOrTabs) {
  const tabs = Array.isArray(tabOrTabs) ? tabOrTabs : [tabOrTabs] ;
  for (const tab of tabs) {
    if (!TabsStore.ensureLivingTab(tab))
      continue;
    if (tab.$TST.reservedCleanupNeedlessGroupTab)
      clearTimeout(tab.$TST.reservedCleanupNeedlessGroupTab);
    tab.$TST.reservedCleanupNeedlessGroupTab = setTimeout(() => {
      if (!tab.$TST)
        return;
      delete tab.$TST.reservedCleanupNeedlessGroupTab;
      cleanupNeedlssGroupTab(tab);
    }, 100);
  }
}

function cleanupNeedlssGroupTab(tabs) {
  if (!Array.isArray(tabs))
    tabs = [tabs];
  log('trying to clanup needless temporary group tabs from ', () => tabs.map(dumpTab));
  const tabsToBeRemoved = [];
  for (const tab of tabs) {
    if (tab.$TST.isTemporaryGroupTab) {
      if (tab.$TST.childIds.length > 1)
        break;
      const lastChild = tab.$TST.firstChild;
      if (lastChild &&
          !lastChild.$TST.isTemporaryGroupTab &&
          !lastChild.$TST.isTemporaryAggressiveGroupTab)
        break;
    }
    else if (tab.$TST.isTemporaryAggressiveGroupTab) {
      if (tab.$TST.childIds.length > 1)
        break;
    }
    else {
      break;
    }
    tabsToBeRemoved.push(tab);
  }
  log('=> to be removed: ', () => tabsToBeRemoved.map(dumpTab));
  TabsInternalOperation.removeTabs(tabsToBeRemoved, { keepDescendants: true });
}

export async function tryReplaceTabWithGroup(tab, { windowId, parent, children, insertBefore, newParent } = {}) {
  if (tab) {
    windowId     = tab.windowId;
    parent       = tab.$TST.parent;
    children     = tab.$TST.children;
    insertBefore = insertBefore || tab.$TST.unsafeNextTab;
  }

  if (children.length <= 1 ||
      countMatched(children,
                   tab => !tab.$TST.states.has(Constants.kTAB_STATE_TO_BE_REMOVED)) <= 1)
    return null;

  log('trying to replace the closing tab with a new group tab');

  const firstChild = children[0];
  const uri = makeGroupTabURI({
    title:     browser.i18n.getMessage('groupTab_label', firstChild.title),
    ...temporaryStateParams(configs.groupTabTemporaryStateForOrphanedTabs)
  });
  const window = TabsStore.windows.get(windowId);
  window.toBeOpenedTabsWithPositions++;
  const groupTab = await TabsOpen.openURIInTab(uri, {
    windowId,
    insertBefore,
    inBackground: true
  });
  log('group tab: ', dumpTab(groupTab));
  if (!groupTab) // the window is closed!
    return;
  if (newParent || parent)
    await Tree.attachTabTo(groupTab, newParent || parent, {
      dontMove:  true,
      broadcast: true
    });
  for (const child of children) {
    await Tree.attachTabTo(child, groupTab, {
      dontMove:  true,
      broadcast: true
    });
  }

  // This can be triggered on closing of multiple tabs,
  // so we should cleanup it on such cases for safety.
  // https://github.com/piroor/treestyletab/issues/2317
  wait(1000).then(() => reserveToCleanupNeedlessGroupTab(groupTab));

  return groupTab;
}

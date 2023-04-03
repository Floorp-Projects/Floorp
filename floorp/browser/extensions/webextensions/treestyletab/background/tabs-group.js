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
import * as ApiTabs from '/common/api-tabs.js';
import * as Constants from '/common/constants.js';
import * as SidebarConnection from '/common/sidebar-connection.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TabsInternalOperation from '/common/tabs-internal-operation.js';
import * as TreeBehavior from '/common/tree-behavior.js';

import Tab from '/common/Tab.js';

import * as TabsMove from './tabs-move.js';
import * as TabsOpen from './tabs-open.js';
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

function reserveToCleanupNeedlessGroupTab(tabOrTabs) {
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


// ====================================================================
// init/update group tabs
// ====================================================================

/*
  To prevent the tab is closed by Firefox, we need to inject scripts dynamically.
  See also: https://github.com/piroor/treestyletab/issues/1670#issuecomment-350964087
*/
async function tryInitGroupTab(tab) {
  if (!tab.$TST.isGroupTab &&
      !tab.$TST.hasGroupTabURL)
    return;
  log('tryInitGroupTab ', tab);
  const scriptOptions = {
    runAt:           'document_start',
    matchAboutBlank: true
  };
  try {
    const results = await browser.tabs.executeScript(tab.id, {
      ...scriptOptions,
      code:  '[window.prepared, document.documentElement.matches(".initialized")]',
    }).catch(error => {
      if (ApiTabs.isMissingHostPermissionError(error) &&
          tab.$TST.hasGroupTabURL) {
        log('  tryInitGroupTab: failed to run script for restored/discarded tab, reload the tab for safety ', tab.id);
        browser.tabs.reload(tab.id);
        return [[false, false, true]];
      }
      return ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError)(error);
    });
    const [prepared, initialized, reloaded] = results && results[0] || [];
    log('  tryInitGroupTab: groupt tab state ', tab.id, { prepared, initialized, reloaded });
    if (reloaded) {
      log('  => reloaded ', tab.id);
      return;
    }
    if (prepared && initialized) {
      log('  => already initialized ', tab.id);
      return;
    }
  }
  catch(error) {
    log('  tryInitGroupTab: error while checking initialized: ', tab.id, error);
  }
  try {
    const titleElementExists = await browser.tabs.executeScript(tab.id, {
      ...scriptOptions,
      code:  '!!document.querySelector("#title")',
    }).catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError, ApiTabs.handleMissingHostPermissionError));
    if (titleElementExists && !titleElementExists[0] && tab.status == 'complete') { // we need to load resources/group-tab.html at first.
      log('  => title element exists, load again ', tab.id);
      return browser.tabs.update(tab.id, { url: tab.url }).catch(ApiTabs.createErrorSuppressor());
    }
  }
  catch(error) {
    log('  tryInitGroupTab error while checking title element: ', tab.id, error);
  }
  Promise.all([
    browser.tabs.executeScript(tab.id, {
      ...scriptOptions,
      //file:  '/common/l10n.js'
      file:  '/extlib/l10n-classic.js' // ES module does not supported as a content script...
    }).catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError, ApiTabs.handleMissingHostPermissionError)),
    browser.tabs.executeScript(tab.id, {
      ...scriptOptions,
      file:  '/resources/group-tab.js'
    }).catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError, ApiTabs.handleMissingHostPermissionError))
  ]).then(() => {
    log('tryInitGroupTab completely initialized: ', tab.id);
  });

  if (tab.$TST.states.has(Constants.kTAB_STATE_UNREAD)) {
    tab.$TST.removeState(Constants.kTAB_STATE_UNREAD, { permanently: true });
    SidebarConnection.sendMessage({
      type:     Constants.kCOMMAND_NOTIFY_TAB_UPDATED,
      windowId: tab.windowId,
      tabId:    tab.id,
      removedStates: [Constants.kTAB_STATE_UNREAD]
    });
  }
}

function reserveToUpdateRelatedGroupTabs(tab, changedInfo) {
  const ancestorGroupTabs = [tab]
    .concat(tab.$TST.ancestors)
    .filter(tab => tab.$TST.isGroupTab);
  for (const tab of ancestorGroupTabs) {
    if (tab.$TST.reservedUpdateRelatedGroupTab)
      clearTimeout(tab.$TST.reservedUpdateRelatedGroupTab);
    tab.$TST.reservedUpdateRelatedGroupTabChangedInfo = tab.$TST.reservedUpdateRelatedGroupTabChangedInfo || new Set();
    for (const info of changedInfo) {
      tab.$TST.reservedUpdateRelatedGroupTabChangedInfo.add(info);
    }
    tab.$TST.reservedUpdateRelatedGroupTab = setTimeout(() => {
      if (!tab.$TST)
        return;
      delete tab.$TST.reservedUpdateRelatedGroupTab;
      updateRelatedGroupTab(tab, Array.from(tab.$TST.reservedUpdateRelatedGroupTabChangedInfo));
      delete tab.$TST.reservedUpdateRelatedGroupTabChangedInfo;
    }, 100);
  }
}

async function updateRelatedGroupTab(groupTab, changedInfo = []) {
  if (!TabsStore.ensureLivingTab(groupTab))
    return;

  await tryInitGroupTab(groupTab);
  if (changedInfo.includes('tree')) {
    try {
      await browser.tabs.executeScript(groupTab.id, {
        runAt:           'document_start',
        matchAboutBlank: true,
        code:            `window.updateTree && window.updateTree()`,
      }).catch(error => {
        if (ApiTabs.isMissingHostPermissionError(error))
          throw error;
        return ApiTabs.createErrorSuppressor(ApiTabs.handleMissingTabError, ApiTabs.handleUnloadedError)(error);
      });
    }
    catch(error) {
      if (ApiTabs.isMissingHostPermissionError(error)) {
        log('  updateRelatedGroupTab: failed to run script for restored/discarded tab, reload the tab for safety ', groupTab.id);
        browser.tabs.reload(groupTab.id);
        return;
      }
    }
  }

  const firstChild = groupTab.$TST.firstChild;
  if (!firstChild) // the tab can be closed while waiting...
    return;

  if (changedInfo.includes('title')) {
    let newTitle;
    if (Constants.kGROUP_TAB_DEFAULT_TITLE_MATCHER.test(groupTab.title)) {
      newTitle = browser.i18n.getMessage('groupTab_label', firstChild.title);
    }
    else if (Constants.kGROUP_TAB_FROM_PINNED_DEFAULT_TITLE_MATCHER.test(groupTab.title)) {
      const opener = groupTab.$TST.openerTab;
      if (opener) {
        if (opener &&
            opener.favIconUrl) {
          SidebarConnection.sendMessage({
            type:       Constants.kCOMMAND_NOTIFY_TAB_FAVICON_UPDATED,
            windowId:   groupTab.windowId,
            tabId:      groupTab.id,
            favIconUrl: opener.favIconUrl
          });
        }
        newTitle = browser.i18n.getMessage('groupTab_fromPinnedTab_label', opener.title);
      }
    }

    if (newTitle && groupTab.title != newTitle) {
      browser.tabs.executeScript(groupTab.id, {
        runAt:           'document_start',
        matchAboutBlank: true,
        code:            `window.setTitle && window.setTitle(${JSON.stringify(newTitle)})`,
      }).catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError, ApiTabs.handleMissingHostPermissionError));
    }
  }
}

Tab.onRemoved.addListener((tab, _closeInfo = {}) => {
  const ancestors = tab.$TST.ancestors;
  wait(0).then(() => {
    reserveToCleanupNeedlessGroupTab(ancestors);
  });
});

Tab.onUpdated.addListener((tab, changeInfo) => {
  if ('url' in changeInfo ||
      'previousUrl' in changeInfo ||
      'state' in changeInfo) {
    const status = changeInfo.status || tab && tab.status;
    const url = changeInfo.url ? changeInfo.url :
      status == 'complete' && tab ? tab.url : '';
    if (tab &&
        status == 'complete') {
      if (url.indexOf(Constants.kGROUP_TAB_URI) == 0) {
        tab.$TST.addState(Constants.kTAB_STATE_GROUP_TAB, { permanently: true });
      }
      else if (!Constants.kSHORTHAND_ABOUT_URI.test(url)) {
        tab.$TST.getPermanentStates().then(async (states) => {
          if (url.indexOf(Constants.kGROUP_TAB_URI) == 0)
            return;
          // Detect group tab from different session - which can have different UUID for the URL.
          const PREFIX_REMOVER = /^moz-extension:\/\/[^\/]+/;
          const pathPart = url.replace(PREFIX_REMOVER, '');
          if (states.includes(Constants.kTAB_STATE_GROUP_TAB) &&
              pathPart.split('?')[0] == Constants.kGROUP_TAB_URI.replace(PREFIX_REMOVER, '')) {
            const parameters = pathPart.replace(/^[^\?]+\?/, '');
            const oldUrl = tab.url;
            await wait(100); // for safety
            if (tab.url != oldUrl)
              return;
            browser.tabs.update(tab.id, {
              url: `${Constants.kGROUP_TAB_URI}?${parameters}`
            }).catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
            tab.$TST.addState(Constants.kTAB_STATE_GROUP_TAB);
          }
          else {
            tab.$TST.removeState(Constants.kTAB_STATE_GROUP_TAB, { permanently: true });
          }
        });
      }
    }
    // restored tab can be replaced with blank tab. we need to restore it manually.
    else if (changeInfo.url == 'about:blank' &&
             changeInfo.previousUrl &&
             changeInfo.previousUrl.indexOf(Constants.kGROUP_TAB_URI) == 0) {
      const oldUrl = tab.url;
      wait(100).then(() => { // redirect with delay to avoid infinite loop of recursive redirections.
        if (tab.url != oldUrl)
          return;
        browser.tabs.update(tab.id, {
          url: changeInfo.previousUrl
        }).catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError));
        tab.$TST.addState(Constants.kTAB_STATE_GROUP_TAB, { permanently: true });
      });
    }

    if (changeInfo.status ||
        changeInfo.url ||
        url.indexOf(Constants.kGROUP_TAB_URI) == 0)
      tryInitGroupTab(tab);
  }

  if ('title' in changeInfo) {
    const group = Tab.getGroupTabForOpener(tab);
    if (group)
      reserveToUpdateRelatedGroupTabs(group, ['title', 'tree']);
  }
});

Tab.onGroupTabDetected.addListener(tab => {
  tryInitGroupTab(tab);
});

Tab.onLabelUpdated.addListener(tab => {
  reserveToUpdateRelatedGroupTabs(tab, ['title', 'tree']);
});

Tab.onActivating.addListener((tab, _info = {}) => {
  tryInitGroupTab(tab);
});

Tab.onPinned.addListener(tab => {
  Tree.collapseExpandSubtree(tab, {
    collapsed: false,
    broadcast: true
  });
  const children = tab.$TST.children;
  Tree.detachAllChildren(tab, {
    behavior: TreeBehavior.getParentTabOperationBehavior(tab, {
      context: Constants.kPARENT_TAB_OPERATION_CONTEXT_CLOSE,
      preventEntireTreeBehavior: true,
    }),
    broadcast: true
  });
  Tree.detachTab(tab, {
    broadcast: true
  });

  if (configs.autoGroupNewTabsFromPinned)
    groupTabs(children, {
      title:       browser.i18n.getMessage('groupTab_fromPinnedTab_label', tab.title),
      temporary:   true,
      openerTabId: tab.$TST.uniqueId.id
    });
});

Tree.onAttached.addListener((tab, _info = {}) => {
  reserveToUpdateRelatedGroupTabs(tab, ['tree']);
});

Tree.onDetached.addListener((_tab, detachInfo) => {
  if (!detachInfo.oldParentTab)
    return;
  if (detachInfo.oldParentTab.$TST.isGroupTab)
    reserveToCleanupNeedlessGroupTab(detachInfo.oldParentTab);
  reserveToUpdateRelatedGroupTabs(detachInfo.oldParentTab, ['tree']);
});

/*
Tree.onSubtreeCollapsedStateChanging.addListener((tab, _info) => {
  reserveToUpdateRelatedGroupTabs(tab);
});
*/

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
  sanitizeForHTMLText
} from '/common/common.js';

import * as Constants from '/common/constants.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TSTAPI from '/common/tst-api.js';
import * as SidebarConnection from '/common/sidebar-connection.js';
import * as TreeBehavior from '/common/tree-behavior.js';
import * as Dialog from '/common/dialog.js';

import Tab from '/common/Tab.js';

import * as TabsGroup from './tabs-group.js';
import * as TabsOpen from './tabs-open.js';
import * as Tree from './tree.js';
import * as Background from './background.js';

function log(...args) {
  internalLogger('background/handle-group-tabs', ...args);
}

// ====================================================================
// init/update group tabs
// ====================================================================

/*
  To prevent the tab is closed by Firefox, we need to inject scripts dynamically.
  See also: https://github.com/piroor/treestyletab/issues/1670#issuecomment-350964087
*/
export async function tryInitGroupTab(tab) {
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

export function reserveToUpdateRelatedGroupTabs(tab, changedInfo) {
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
    TabsGroup.reserveToCleanupNeedlessGroupTab(ancestors);
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
    TabsGroup.groupTabs(children, {
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
    TabsGroup.reserveToCleanupNeedlessGroupTab(detachInfo.oldParentTab);
  reserveToUpdateRelatedGroupTabs(detachInfo.oldParentTab, ['tree']);
});

/*
Tree.onSubtreeCollapsedStateChanging.addListener((tab, _info) => { 
  reserveToUpdateRelatedGroupTabs(tab);
});
*/


// ====================================================================
// auto-grouping of tabs
// ====================================================================

Tab.onBeforeCreate.addListener(async (tab, info) => {
  const window  = TabsStore.windows.get(tab.windowId);
  if (!window)
    return;

  const openerId  = tab.openerTabId;
  const openerTab = openerId && (await browser.tabs.get(openerId).catch(ApiTabs.createErrorHandler(ApiTabs.handleMissingTabError)));
  if ((openerTab &&
       openerTab.pinned &&
       openerTab.windowId == tab.windowId) ||
      (!openerTab &&
       !info.maybeOrphan)) {
    if (window.preventAutoGroupNewTabsUntil > Date.now()) {
      window.preventAutoGroupNewTabsUntil += configs.autoGroupNewTabsTimeout;
    }
    else {
      window.openedNewTabs.set(tab.id, {
        id:       tab.id,
        openerId: openerTab && openerTab.id,
        openerIsPinned: openerTab && openerTab.pinned,
        maybeFromBookmark: tab.$TST.maybeFromBookmark
      });
    }
  }
  if (window.openedNewTabsTimeout)
    clearTimeout(window.openedNewTabsTimeout);
  window.openedNewTabsTimeout = setTimeout(
    onNewTabsTimeout,
    configs.autoGroupNewTabsTimeout,
    window
  );
});

const mToBeGroupedTabSets = [];

async function onNewTabsTimeout(window) {
  if (Tab.needToWaitTracked(window.id))
    await Tab.waitUntilTrackedAll(window.id);
  if (Tab.needToWaitMoved(window.id))
    await Tab.waitUntilMovedAll(window.id);

  let tabReferences = Array.from(window.openedNewTabs.values());
  log('onNewTabsTimeout for ', tabReferences);

  window.openedNewTabs.clear();

  const blocked = TSTAPI.isGroupingBlocked();
  log('  blocked?: ', blocked);
  tabReferences = tabReferences.filter(tabReference => {
    if (blocked || !tabReference.id)
      return false;
    const tab = Tab.get(tabReference.id);
    if (!tab)
      return false;
    const uniqueId = tab && tab.$TST && tab.$TST.uniqueId;
    return !uniqueId || (!uniqueId.duplicated && !uniqueId.restored);
  });
  if (tabReferences.length == 0) {
    log(' => no tab to be grouped.');
    return;
  }

  if (tabReferences.length > 1) {
    for (const tabReference of tabReferences) {
      Tab.get(tabReference.id).$TST.openedWithOthers = true;
    }
  }

  mToBeGroupedTabSets.push(tabReferences);
  tryGroupNewTabs();
}

async function tryGroupNewTabs() {
  if (tryGroupNewTabs.running)
    return;

  const tabReferences = mToBeGroupedTabSets.shift();
  if (!tabReferences)
    return;

  log('tryGroupNewTabs for ', tabReferences);
  tryGroupNewTabs.running = true;
  try {
    const fromPinned    = [];
    const fromOthers    = [];
    // extract only pure new tabs
    for (const tabReference of tabReferences) {
      const tab = Tab.get(tabReference.id);
      if (!tab)
        continue;
      if (tabReference.openerTabId)
        tab.openerTabId = parseInt(tabReference.openerTabId); // restore the opener information
      const uniqueId = tab.$TST.uniqueId;
      if (tab.$TST.isGroupTab || uniqueId.duplicated || uniqueId.restored)
        continue;
      if (tabReference.openerIsPinned) {
        // We should check the "autoGroupNewTabsFromPinned" config here,
        // because to-be-grouped tabs should be ignored by the handler for
        // "autoAttachSameSiteOrphan" behavior.
        if (tab.$TST.hasPinnedOpener &&
            configs.autoGroupNewTabsFromPinned)
          fromPinned.push(tab);
      }
      else {
        fromOthers.push(tab);
      }
    }
    log(' => ', { fromPinned, fromOthers });

    if (fromPinned.length > 0 && configs.autoGroupNewTabsFromPinned) {
      const newRootTabs = Tab.collectRootTabs(Tab.sort(fromPinned));
      if (newRootTabs.length > 0) {
        await tryGroupNewTabsFromPinnedOpener(newRootTabs);
      }
    }

    // We can assume that new tabs from a bookmark folder and from other
    // sources won't be mixed.
    const openedFromBookmarkFolder = fromOthers.length > 0 && await new Promise((resolve, _reject) => {
      const maybeFromBookmarks = [];
      let restCount = fromOthers.length;
      for (const tab of fromOthers) {
        tab.$TST.promisedPossibleOpenerBookmarks.then(bookmarks => {
          log(` bookmarks from tab ${tab.id}: `, bookmarks);
          restCount--;
          if (bookmarks.length > 0)
            maybeFromBookmarks.push(tab);
          if (areMostTabsFromSameBookmarkFolder(maybeFromBookmarks, tabReferences.length))
            resolve(true);
          else if (restCount == 0)
            resolve(false);
        });
      }
    });
    log(' => openedFromBookmarkFolder: ', openedFromBookmarkFolder);
    const newRootTabs = Tab.collectRootTabs(Tab.sort(fromOthers));
    if (newRootTabs.length > 1) {
      if (openedFromBookmarkFolder) {
        if (configs.autoGroupNewTabsFromBookmarks)
          await TabsGroup.groupTabs(newRootTabs, {
            ...TabsGroup.temporaryStateParams(configs.groupTabTemporaryStateForNewTabsFromBookmarks),
            broadcast: true
          });
      }
      else if (configs.autoGroupNewTabsFromOthers) {
        const granted = await confirmToAutoGroupNewTabsFromOthers(fromOthers);
        if (granted)
          await TabsGroup.groupTabs(newRootTabs, {
            ...TabsGroup.temporaryStateParams(configs.groupTabTemporaryStateForNewTabsFromOthers),
            broadcast: true
          });
      }
    }
  }
  catch(e) {
    log('Error on tryGroupNewTabs: ', String(e), e.stack);
  }
  finally {
    tryGroupNewTabs.running = false;
    if (mToBeGroupedTabSets.length > 0)
      tryGroupNewTabs();
  }
}

function areMostTabsFromSameBookmarkFolder(bookmarkedTabs, allNewTabsCount) {
  log('areMostTabsFromSameBookmarkFolder ', { bookmarkedTabs, allNewTabsCount });
  const parentIds = bookmarkedTabs.map(tab => tab.$TST.possibleOpenerBookmarks.map(bookmark => bookmark.parentId)).flat();
  const counts    = [];
  const countById = {};
  for (const id of parentIds) {
    if (!(id in countById))
      counts.push(countById[id] = { id, count: 0 });
    countById[id].count++;
  }
  log(' counts: ', counts);
  if (counts.length == 0)
    return false;

  const greatestCount = counts.sort((a, b) => b.count - a.count)[0];
  const minCount = allNewTabsCount * configs.tabsFromSameFolderMinThresholdPercentage / 100;
  log(' => ', { greatestCount, minCount });
  return greatestCount.count > minCount;
}

async function confirmToAutoGroupNewTabsFromOthers(tabs) {
  if (tabs.length <= 1 ||
      !configs.warnOnAutoGroupNewTabs)
    return true;

  const windowId = tabs[0].windowId;
  const win = await browser.windows.get(windowId);

  const listing = configs.warnOnAutoGroupNewTabsWithListing ?
    Background.tabsToHTMLList(tabs, { maxRows: configs.warnOnAutoGroupNewTabsWithListingMaxRows }) :
    '';
  const result = await Dialog.show(win, {
    content: `
      <div>${sanitizeForHTMLText(browser.i18n.getMessage('warnOnAutoGroupNewTabs_message', [tabs.length]))}</div>${listing}
    `.trim(),
    buttons: [
      browser.i18n.getMessage('warnOnAutoGroupNewTabs_close'),
      browser.i18n.getMessage('warnOnAutoGroupNewTabs_cancel')
    ],
    checkMessage: browser.i18n.getMessage('warnOnAutoGroupNewTabs_warnAgain'),
    checked: true,
    modal: true, // for popup
    type:  'common-dialog', // for popup
    url:   '/resources/blank.html',  // for popup, required on Firefox ESR68
    title: browser.i18n.getMessage('warnOnAutoGroupNewTabs_title'), // for popup
    onShownInPopup(container) {
      setTimeout(() => { // this need to be done on the next tick, to use the height of the box for     calculation of dialog size
        const style = container.querySelector('ul').style;
        style.height = '0px'; // this makes the box shrinkable
        style.maxHeight = 'none';
        style.minHeight = '0px';
      }, 0);
    }
  });

  switch (result.buttonIndex) {
    case 0:
      if (!result.checked)
        configs.warnOnAutoGroupNewTabs = false;
      return true;
    case 1:
      if (!result.checked) {
        configs.warnOnAutoGroupNewTabs = false;
        configs.autoGroupNewTabsFromOthers = false;
      }
    default:
      return false;
  }
}

async function tryGroupNewTabsFromPinnedOpener(rootTabs) {
  log(`tryGroupNewTabsFromPinnedOpener: ${rootTabs.length} root tabs are opened from pinned tabs`);

  // First, collect pinned opener tabs.
  let pinnedOpeners = [];
  const childrenOfPinnedTabs = {};
  for (const tab of rootTabs) {
    const opener = tab.$TST.openerTab;
    if (!pinnedOpeners.includes(opener))
      pinnedOpeners.push(opener);
  }
  log('pinnedOpeners ', () => pinnedOpeners.map(dumpTab));

  // Second, collect tabs opened from pinned openers including existing tabs
  // (which were left ungrouped in previous process).
  const openerOf = {};
  const allRootTabs = await Tab.getRootTabs(rootTabs[0].windowId)
  for (const tab of allRootTabs) {
    if (tab.$TST.getAttribute(Constants.kPERSISTENT_ALREADY_GROUPED_FOR_PINNED_OPENER))
      continue;
    if (rootTabs.includes(tab)) { // newly opened tab
      const opener = tab.$TST.openerTab;
      if (!opener)
        continue;
      openerOf[tab.id] = opener;
      const tabs = childrenOfPinnedTabs[opener.id] || [];
      childrenOfPinnedTabs[opener.id] = tabs.concat([tab]);
      continue;
    }
    const opener = Tab.getByUniqueId(tab.$TST.getAttribute(Constants.kPERSISTENT_ORIGINAL_OPENER_TAB_ID));
    if (!opener ||
        !opener.pinned ||
        opener.windowId != tab.windowId)
      continue;
    // existing and not yet grouped tab
    if (!pinnedOpeners.includes(opener))
      pinnedOpeners.push(opener);
    openerOf[tab.id] = opener;
    const tabs = childrenOfPinnedTabs[opener.id] || [];
    childrenOfPinnedTabs[opener.id] = tabs.concat([tab]);
  }

  // Ignore pinned openeres which has no child tab to be grouped.
  pinnedOpeners = pinnedOpeners.filter(opener => {
    return childrenOfPinnedTabs[opener.id].length > 1 || Tab.getGroupTabForOpener(opener);
  });
  log(' => ', () => pinnedOpeners.map(dumpTab));

  // Move newly opened tabs to expected position before grouping!
  // Note that we should refer "insertNewChildAt" instead of "insertNewTabFromPinnedTabAt"
  // because these children are going to be controlled in a sub tree.
  for (const tab of rootTabs.slice(0).sort((a, b) => a.id - b.id)/* process them in the order they were opened */) {
    const opener   = openerOf[tab.id];
    const siblings = tab.$TST.needToBeGroupedSiblings;
    if (!pinnedOpeners.includes(opener) ||
        Tab.getGroupTabForOpener(opener) ||
        siblings.length == 0 ||
        tab.$TST.alreadyMovedAsOpenedFromPinnedOpener)
      continue;
    let refTabs = {};
    try {
      refTabs = Tree.getReferenceTabsForNewChild(tab, null, {
        lastRelatedTab: opener.$TST.previousLastRelatedTab,
        parent:         siblings[0],
        children:       siblings,
        descendants:    siblings.map(sibling => [sibling, ...sibling.$TST.descendants]).flat()
      });
    }
    catch(_error) {
      // insertChildAt == "no control" case
    }
    if (refTabs.insertAfter) {
      await Tree.moveTabSubtreeAfter(
        tab,
        refTabs.insertAfter,
        { broadcast: true }
      );
      log(`newly opened child ${tab.id} has been moved after ${refTabs.insertAfter && refTabs.insertAfter.id}`);
    }
    else if (refTabs.insertBefore) {
      await Tree.moveTabSubtreeBefore(
        tab,
        refTabs.insertBefore,
        { broadcast: true }
      );
      log(`newly opened child ${tab.id} has been moved before ${refTabs.insertBefore && refTabs.insertBefore.id}`);
    }
    else {
      continue;
    }
    tab.$TST.alreadyMovedAsOpenedFromPinnedOpener = true;
  }

  // Finally, try to group opened tabs.
  const newGroupTabs = new Map();
  for (const opener of pinnedOpeners) {
    const children = childrenOfPinnedTabs[opener.id].sort((a, b) => a.index - b.index);
    let parent = Tab.getGroupTabForOpener(opener);
    if (parent) {
      for (const child of children) {
        TabsStore.removeToBeGroupedTab(child);
      }
      continue;
    }
    log(`trying to group children of ${dumpTab(opener)}: `, () => children.map(dumpTab));
    const uri = TabsGroup.makeGroupTabURI({
      title:       browser.i18n.getMessage('groupTab_fromPinnedTab_label', opener.title),
      openerTabId: opener.$TST.uniqueId.id,
      ...TabsGroup.temporaryStateParams(configs.groupTabTemporaryStateForChildrenOfPinned)
    });
    parent = await TabsOpen.openURIInTab(uri, {
      windowId:     opener.windowId,
      insertBefore: children[0],
      cookieStoreId: opener.cookieStoreId,
      inBackground: true
    });
    log('opened group tab: ', dumpTab(parent));
    newGroupTabs.set(opener, true);
    for (const child of children) {
      // Prevent the tab to be grouped again after it is ungrouped manually.
      child.$TST.setAttribute(Constants.kPERSISTENT_ALREADY_GROUPED_FOR_PINNED_OPENER, true);
      TabsStore.removeToBeGroupedTab(child);
      await Tree.attachTabTo(child, parent, {
        forceExpand: true, // this is required to avoid the group tab itself is active from active tab in collapsed tree
        dontMove:    true,
        broadcast:   true
      });
    }
  }
  return true;
}

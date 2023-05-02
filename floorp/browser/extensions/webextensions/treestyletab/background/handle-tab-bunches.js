/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  dumpTab,
  configs,
  sanitizeForHTMLText,
  compareAsNumber,
} from '/common/common.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as Constants from '/common/constants.js';
import * as Dialog from '/common/dialog.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TSTAPI from '/common/tst-api.js';

import Tab from '/common/Tab.js';

import * as TabsGroup from './tabs-group.js';
import * as TabsOpen from './tabs-open.js';
import * as Tree from './tree.js';

function log(...args) {
  internalLogger('background/handle-tab-bunches', ...args);
}

// ====================================================================
// Detection of a bunch of tabs opened at same time.
// Firefox's WebExtensions API doesn't provide ability to know which tabs
// are opened together by a single trigger. Thus TST tries to detect such
// "tab bunches" based on their opened timing.
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
    if (window.preventToDetectTabBunchesUntil > Date.now()) {
      window.preventToDetectTabBunchesUntil += configs.tabBunchesDetectionTimeout;
    }
    else {
      window.openedNewTabs.set(tab.id, {
        id:       tab.id,
        windowId: tab.windowId,
        indexOnCreated: tab.$indexOnCreated,
        openerId: openerTab && openerTab.id,
        openerIsPinned: openerTab && openerTab.pinned,
        maybeFromBookmark: tab.$TST.maybeFromBookmark,
        shouldNotGrouped: TSTAPI.isGroupingBlocked(),
      });
    }
  }
  if (window.delayedTabBunchesDetection)
    clearTimeout(window.delayedTabBunchesDetection);
  window.delayedTabBunchesDetection = setTimeout(
    tryDetectTabBunches,
    configs.tabBunchesDetectionTimeout,
    window
  );
});

const mPossibleTabBunches = [];

async function tryDetectTabBunches(window) {
  if (Tab.needToWaitTracked(window.id))
    await Tab.waitUntilTrackedAll(window.id);
  if (Tab.needToWaitMoved(window.id))
    await Tab.waitUntilMovedAll(window.id);

  let tabReferences = Array.from(window.openedNewTabs.values());
  log('tryDetectTabBunches for ', tabReferences);

  window.openedNewTabs.clear();

  tabReferences = tabReferences.filter(tabReference => {
    if (!tabReference.id)
      return false;
    const tab = Tab.get(tabReference.id);
    if (!tab)
      return false;
    const uniqueId = tab && tab.$TST && tab.$TST.uniqueId;
    return !uniqueId || (!uniqueId.duplicated && !uniqueId.restored);
  });
  if (tabReferences.length == 0) {
    log(' => there is no possible bunches of tabs.');
    return;
  }

  if (tabReferences.length > 1) {
    for (const tabReference of tabReferences) {
      Tab.get(tabReference.id).$TST.openedWithOthers = true;
    }
  }

  if (areTabsFromOtherDeviceWithInsertAfterCurrent(tabReferences) &&
      configs.fixupOrderOfTabsFromOtherDevice) {
    const ids   = tabReferences.map(tabReference => tabReference.id);
    const index = tabReferences.map(tabReference => Tab.get(tabReference.id).index).sort(compareAsNumber)[0];
    log(' => gather tabs from other device at ', ids, index);
    await browser.tabs.move(ids, { index });
  }

  if (configs.autoGroupNewTabsFromBookmarks ||
      configs.autoGroupNewTabsFromPinned ||
      configs.autoGroupNewTabsFromOthers) {
    mPossibleTabBunches.push(tabReferences);
    tryGroupTabBunches();
  }
}

async function tryGroupTabBunches() {
  if (tryGroupTabBunches.running)
    return;

  const tabReferences = mPossibleTabBunches.shift();
  if (!tabReferences)
    return;

  log('tryGroupTabBunches for ', tabReferences);
  tryGroupTabBunches.running = true;
  try {
    const fromPinned    = [];
    const fromOthers    = [];
    // extract only pure new tabs
    for (const tabReference of tabReferences) {
      if (tabReference.shouldNotGrouped)
        continue;
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
        await tryGroupTabBunchesFromPinnedOpener(newRootTabs);
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
    log('Error on tryGroupTabBunches: ', String(e), e.stack);
  }
  finally {
    tryGroupTabBunches.running = false;
    if (mPossibleTabBunches.length > 0)
      tryGroupTabBunches();
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
    Dialog.tabsToHTMLList(tabs, {
      maxRows:   configs.warnOnAutoGroupNewTabsWithListingMaxRows,
      maxHeight: Math.round(win.height * 0.8),
      maxWidth:  Math.round(win.width * 0.75)
    }) :
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

async function tryGroupTabBunchesFromPinnedOpener(rootTabs) {
  log(`tryGroupTabBunchesFromPinnedOpener: ${rootTabs.length} root tabs are opened from pinned tabs`);

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


// Detect tabs sent from other device with `browser.tabs.insertAfterCurrent`=true based on their index
// (Workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1596787 )
// See also: https://github.com/piroor/treestyletab/issues/2419
function areTabsFromOtherDeviceWithInsertAfterCurrent(tabReferences) {
  if (tabReferences.length == 0)
    return false;

  const activeTab = Tab.getActiveTab(tabReferences[0].windowId);
  if (!activeTab)
    return false;

  const activeIndex        = activeTab.index;
  const followingTabsCount = Tab.getTabs(activeTab.windowId).filter(tab => tab.index > activeIndex).length;

  const createdCount    = tabReferences.length;
  const expectedIndices = [activeIndex + 1];
  const actualIndices   = tabReferences.map(tabReference => tabReference.indexOnCreated);

  const overTabsCount = Math.max(0, createdCount - followingTabsCount);
  const shouldCountDown = Math.min(createdCount - 1, createdCount - Math.floor(overTabsCount / 2));
  const shouldCountUp = createdCount - shouldCountDown - 1;
  for (let i = 0; i < shouldCountUp; i++) {
    expectedIndices.push(activeIndex + 2 + i + (createdCount - overTabsCount));
  }
  for (let i = shouldCountDown - 1; i > -1; i--) {
    expectedIndices.push(activeIndex + 1 + i);
  }
  const received = actualIndices.join(',') == expectedIndices.join(',');
  log('areTabsFromOtherDeviceWithInsertAfterCurrent:', received, { overTabsCount, shouldCountUp, shouldCountDown, size: expectedIndices.length, actualIndices, expectedIndices });
  return received;
}

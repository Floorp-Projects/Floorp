/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  wait,
  configs
} from '/common/common.js';
import { is, isNot /*, ok, ng*/ } from '/tests/assert.js';
//import Tab from '/common/Tab.js';

import * as Constants from '/common/constants.js';
import * as Utils from './utils.js';

let win;

export async function setup() {
  win = await browser.windows.create();
  // wait until timeout the special timer which prevents grouping of new tabs
  await wait(configs.tabBunchesDetectionDelayOnNewWindow);
}

export async function teardown() {
  await browser.windows.remove(win.id);
  win = null;
}


export async function testInheritContainerFromAutoAttachedParent() {
  await Utils.setConfigs({
    inheritContextualIdentityToChildTabMode: Constants.kCONTEXTUAL_IDENTITY_FROM_PARENT,
    autoAttachOnNewTabCommand: Constants.kNEWTAB_OPEN_AS_CHILD
  });

  const parent = await browser.tabs.create({
    windowId: win.id,
    cookieStoreId: 'firefox-container-1'
  });
  let originalTab;
  const newTabs = await Utils.doAndGetNewTabs(async () => {
    originalTab = await browser.tabs.create({
      windowId:      win.id,
      cookieStoreId: 'firefox-default'
    });
    await wait(1000); // wait until new tab is reopened by TST
  }, { windowId: win.id });
  is({
    newTabsCount:    1,
    newTabParent:    parent.id,
    newTabContainer: 'firefox-container-1'
  }, {
    newTabsCount:    newTabs.length,
    newTabParent:    newTabs.length > 0 && newTabs[0].$TST.parentId,
    newTabContainer: newTabs.length > 0 && newTabs[0].cookieStoreId
  }, 'a new tab implicitly attached to the active tab must inherit the contianer of the old active tab, and there must not be any needless group tab to group the original tab and the reopened tab.');
  isNot(originalTab.id, newTabs[0].id,
        'the new tab must be reopened');
}

export async function testDoNotInheritContainerFromExplicitParent() {
  await Utils.setConfigs({
    inheritContextualIdentityToChildTabMode: Constants.kCONTEXTUAL_IDENTITY_FROM_PARENT
  });

  const parent = await browser.tabs.create({
    windowId: win.id,
    cookieStoreId: 'firefox-container-1'
  });
  const newTabs = await Utils.doAndGetNewTabs(async () => {
    await browser.tabs.create({
      windowId:      win.id,
      cookieStoreId: 'firefox-default',
      openerTabId:   parent.id
    });
    await wait(1000); // wait until new tab is reopened by TST
  }, { windowId: win.id });
  is({
    newTabsCount:    1,
    newTabParent:    parent.id,
    newTabContainer: 'firefox-default'
  }, {
    newTabsCount:    newTabs.length,
    newTabParent:    newTabs.length > 0 && newTabs[0].$TST.parentId,
    newTabContainer: newTabs.length > 0 && newTabs[0].cookieStoreId
  }, 'a new tab explicitly attached to the active tab must not inherit the contianer of the old active tab.');
}

export async function testTryToOpenUnpinnedTabBeforePinnedTab() {
  await Utils.setConfigs({
    autoAttachOnNewTabCommand: Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING
  });

  const tabs = await Utils.createTabs({
    A: { index: 0, pinned: true, active: true },
    B: { index: 1, pinned: true }
  }, { windowId: win.id });
  const newTabs = await Utils.doAndGetNewTabs(async () => {
    await browser.runtime.sendMessage({
      type: Constants.kCOMMAND_SIMULATE_SIDEBAR_MESSAGE,
      message: {
        type:      Constants.kCOMMAND_NEW_TAB_AS,
        baseTabId: tabs.A.id,
        as:        Constants.kNEWTAB_OPEN_AS_NEXT_SIBLING
      }
    });
    await wait(1000);
  }, { windowId: win.id });
  is(1, newTabs.length, 'a new tab must be opened');
  is(2, newTabs[0].index, 'a new tab must be placed after pinned tabs');
}

export async function testReopenedWithPositionByAnotherAddonImmediatelyWhileCreating() {
  await Utils.setConfigs({
    insertNewChildAt:            Constants.kINSERT_END,
    insertNewTabFromPinnedTabAt: Constants.kINSERT_END
  });
  await wait(1000); // this is required to avoid affections from other tests
  let tabs = await Utils.createTabs({
    A: { index: 1, active: true },
    B: { index: 2 }
  }, { windowId: win.id });
  await wait(1000);

  let onCreated;
  // Simulation of a feature of something another addon which reopens a new tab with
  // different container immediately, like Firefox Multi-Account Containers.
  const promisedTabReopenedByAnotherAddon = new Promise((resolve, reject) => {
    onCreated = async tab => {
      browser.tabs.onCreated.removeListener(onCreated);
      try {
        // wait until the tab is attached by TST
        const start = Date.now();
        while (true) {
          if (Date.now() - start > 1000)
            throw new Error('timeout');
          await wait(1);
          const tabs = await Utils.refreshTabs({ opened: tab });
          if (!tabs.opened || !tabs.opened.$TST.parent)
            continue;
          tab = tabs.opened;
          break;
        }
        const preparedTabs = await Utils.refreshTabs({ A: tabs.A, B: tabs.B, C: tab });
        {
          const { A, B, C } = preparedTabs;
          is([
            `${A.id}`,
            `${A.id} => ${C.id}`,
            `${B.id}`
          ], Utils.treeStructure([A, C, B]),
             'tabs must be initialized with specified structure');
        }
        // Firefox Multi-Account Containers reopens a new tab with different container
        // with a blocking listener for "browser.webRequest.onBeforeRequest".
        // TST doesn't have a permission to simulate that, so instead I wait until a
        // content script is executed in the new tab.
        const reopenedTab = await browser.tabs.create({
          // This must be an already loaded URL, othwrwise tabs.executeScript()
          // is called for about:blank page and this test fails with "missing
          // host permission" error.
          url:   `${location.origin}/resources/ui-color.css`,
          index: 2
        });
        await browser.tabs.executeScript(reopenedTab.id, {
          code: 'location.href'
        });
        browser.tabs.remove(tab.id);
        resolve(reopenedTab);
      }
      catch(e){
        reject(e);
      }
    };
  });

  browser.tabs.onCreated.addListener(onCreated);
  browser.tabs.create({
    url:         'about:blank?newchild',
    openerTabId: tabs.A.id,
    index:       2,
    active:      true
  });

  const reopenedTab = await promisedTabReopenedByAnotherAddon;

  // wait until TST's operation is finished
  await wait(1000);
  tabs = await Utils.refreshTabs({
    A: tabs.A,
    B: tabs.B,
    reopened: reopenedTab
  });
  is([
    `${tabs.A.id}`,
    `${tabs.A.id} => ${tabs.reopened.id}`,
    `${tabs.B.id}`,
  ], Utils.treeStructure([tabs.A, tabs.reopened, tabs.B]),
     'reopened tab must be attached from its position');
}

/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  wait,
} from '/common/common.js';
import { is /*, ok, ng*/ } from '/tests/assert.js';
import * as TSTAPI from '/common/tst-api.js';
//import Tab from '/common/Tab.js';

import * as Utils from './utils.js';

let win;

export async function setup() {
  win = await browser.windows.create();
}

export async function teardown() {
  await browser.windows.remove(win.id);
  win = null;
}


export async function testCloseTabsToTopTabs() {
  await Utils.setConfigs({
    warnOnCloseTabs: false
  });

  let tabs = await Utils.createTabs({
    A: { index: 1 },
    B: { index: 2 },
    C: { index: 3 },
    D: { index: 4 },
    P: { pinned: true }
  }, { windowId: win.id });

  tabs = await Utils.refreshTabs(tabs);
  await Utils.waitUntilAllTabChangesFinished(async () => {
    await browser.runtime.sendMessage({
      type: TSTAPI.kCONTEXT_MENU_CLICK,
      info: {
        menuItemId: 'context_closeTabsToTheStart'
      },
      tab: tabs.C.$TST.sanitized
    });
    await wait(500);
  });

  const afterTabs = await browser.tabs.query({ windowId: win.id });
  is([],
     afterTabs.map(tab => tab.id).filter(id => id == tabs.A.id || id == tabs.B.id),
     'tabs must be closed');
  is([tabs.P.id, tabs.C.id, tabs.D.id],
     afterTabs.map(tab => tab.id).filter(id => id == tabs.P.id || id == tabs.C.id || id == tabs.D.id),
     'specified tabs and pinned tabs must be open');
}

export async function testCloseTabsToBottomTabs() {
  await Utils.setConfigs({
    warnOnCloseTabs: false
  });

  let tabs = await Utils.createTabs({
    A: { index: 1 },
    B: { index: 2 },
    C: { index: 3 },
    D: { index: 4 },
    P1: { pinned: true },
    P2: { pinned: true }
  }, { windowId: win.id });

  tabs = await Utils.refreshTabs(tabs);
  await Utils.waitUntilAllTabChangesFinished(async () => {
    await browser.runtime.sendMessage({
      type: TSTAPI.kCONTEXT_MENU_CLICK,
      info: {
        menuItemId: 'context_closeTabsToTheEnd'
      },
      tab: tabs.B.$TST.sanitized
    });
    await wait(500);
  });

  const afterTabs1 = await browser.tabs.query({ windowId: win.id });
  is([],
     afterTabs1.map(tab => tab.id).filter(id => id == tabs.C.id || id == tabs.D.id),
     'tabs must be closed');
  is([tabs.P1.id, tabs.P2.id, tabs.A.id, tabs.B.id],
     afterTabs1.map(tab => tab.id).filter(id => id == tabs.P1.id || id == tabs.P2.id || id == tabs.A.id || id == tabs.B.id),
     'specified tab must be open');

  await Utils.waitUntilAllTabChangesFinished(async () => {
    await browser.runtime.sendMessage({
      type: TSTAPI.kCONTEXT_MENU_CLICK,
      info: {
        menuItemId: 'context_closeTabsToTheEnd'
      },
      tab: tabs.P1.$TST.sanitized
    });
    await wait(500);
  });

  const afterTabs2 = await browser.tabs.query({ windowId: win.id });
  is([],
     afterTabs2.map(tab => tab.id).filter(id => id == tabs.A.id || id == tabs.B.id),
     'tabs must be closed');
  is([tabs.P1.id, tabs.P2.id],
     afterTabs2.map(tab => tab.id).filter(id => id == tabs.P1.id || id == tabs.P2.id),
     'pinned tabs must be open');
}

export async function testCloseOtherTabs() {
  await Utils.setConfigs({
    warnOnCloseTabs: false
  });

  let tabs = await Utils.createTabs({
    A: { index: 1 },
    B: { index: 2 },
    C: { index: 3 },
    D: { index: 4 }
  }, { windowId: win.id });

  tabs = await Utils.refreshTabs(tabs);
  await Utils.waitUntilAllTabChangesFinished(async () => {
    await browser.runtime.sendMessage({
      type: TSTAPI.kCONTEXT_MENU_CLICK,
      info: {
        menuItemId: 'context_closeOtherTabs'
      },
      tab: tabs.B.$TST.sanitized
    });
    await wait(500);
  });

  const afterTabs = await browser.tabs.query({ windowId: win.id });
  is([],
     afterTabs.map(tab => tab.id).filter(id => id == tabs.A.id || id == tabs.C.id || id == tabs.D.id),
     'tabs must be closed');
  is([tabs.B.id],
     afterTabs.map(tab => tab.id).filter(id => id == tabs.B.id),
     'specified tab must be open');
}


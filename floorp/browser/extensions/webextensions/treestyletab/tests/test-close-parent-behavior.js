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
import { is, ok/*, ng*/ } from '/tests/assert.js';
//import Tab from '/common/Tab.js';

import * as Constants from '/common/constants.js';
import * as Utils from './utils.js';

let win;

export async function setup() {
  await Utils.setConfigs({
    warnOnCloseTabs:                              false,
    warnOnCloseTabsByClosebox:                    false,
    moveTabsToBottomWhenDetachedFromClosedParent: false,
    closeParentBehavior_outsideSidebar_collapsed: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_SIMPLY_DETACH_ALL_CHILDREN,
    closeParentBehavior_outsideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_SIMPLY_DETACH_ALL_CHILDREN,
    closeParentBehavior_noSidebar_collapsed:      Constants.kPARENT_TAB_OPERATION_BEHAVIOR_SIMPLY_DETACH_ALL_CHILDREN,
    closeParentBehavior_noSidebar_expanded:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_SIMPLY_DETACH_ALL_CHILDREN,
    moveParentBehavior_outsideSidebar_collapsed:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_SIMPLY_DETACH_ALL_CHILDREN,
    moveParentBehavior_outsideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_SIMPLY_DETACH_ALL_CHILDREN,
    moveParentBehavior_noSidebar_collapsed:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_SIMPLY_DETACH_ALL_CHILDREN,
    moveParentBehavior_noSidebar_expanded:        Constants.kPARENT_TAB_OPERATION_BEHAVIOR_SIMPLY_DETACH_ALL_CHILDREN,
    sidebarVirtuallyOpenedWindows:                [],
    sidebarVirtuallyClosedWindows:                [],

    autoAttachOnNewTabCommand:                              Constants.kNEWTAB_DO_NOTHING,
    autoAttachOnNewTabButtonMiddleClick:                    Constants.kNEWTAB_DO_NOTHING,
    autoAttachOnNewTabButtonAccelClick:                     Constants.kNEWTAB_DO_NOTHING,
    autoAttachOnDuplicated:                                 Constants.kNEWTAB_DO_NOTHING,
    autoAttachSameSiteOrphan:                               Constants.kNEWTAB_DO_NOTHING,
    autoAttachOnOpenedFromExternal:                         Constants.kNEWTAB_DO_NOTHING,
    autoAttachOnAnyOtherTrigger:                            Constants.kNEWTAB_DO_NOTHING,
    guessNewOrphanTabAsOpenedByNewTabCommand:               false,
    inheritContextualIdentityToChildTabMode:                Constants.kCONTEXTUAL_IDENTITY_DEFAULT,
    inheritContextualIdentityToSameSiteOrphanMode:          Constants.kCONTEXTUAL_IDENTITY_DEFAULT,
    inheritContextualIdentityToTabsFromExternalMode:        Constants.kCONTEXTUAL_IDENTITY_DEFAULT,
    inheritContextualIdentityToTabsFromAnyOtherTriggerMode: Constants.kCONTEXTUAL_IDENTITY_DEFAULT,
  });
  win = await browser.windows.create();
}

export async function teardown() {
  await browser.windows.remove(win.id);
  win = null;
  configs.sidebarVirtuallyOpenedWindows = [];
  configs.sidebarVirtuallyClosedWindows = [];
}

async function expandAll(windowId) {
  await browser.runtime.sendMessage({
    type:  'treestyletab:api:expand-tree',
    tabId: '*',
    windowId
  });
  await wait(250);
}

async function collapseAll(windowId) {
  await browser.runtime.sendMessage({
    type:  'treestyletab:api:collapse-tree',
    tabId: '*',
    windowId
  });
  await wait(250);
}

function openSidebar() {
  configs.sidebarVirtuallyOpenedWindows = [win.id];
  configs.sidebarVirtuallyClosedWindows = [];
}

function closeSidebar() {
  configs.sidebarVirtuallyOpenedWindows = [];
  configs.sidebarVirtuallyClosedWindows = [win.id];
}

async function cleanupTabs() {
  const tabs = await browser.tabs.query({ windowId: win.id });
  await browser.tabs.remove(tabs.slice(1).map(tab => tab.id));
}


async function assertFirstChildPromoted({ operator, collapsed } = {}) {
  await cleanupTabs();
  let tabs = await Utils.prepareTabsInWindow(
    { A: { index: 1 },
      B: { index: 2, openerTabId: 'A' },
      C: { index: 3, openerTabId: 'B' },
      D: { index: 4, openerTabId: 'A' },
      E: { index: 5 },
      F: { index: 6, openerTabId: 'E' },
      G: { index: 7, openerTabId: 'E' } },
    win.id,
    [ 'A',
      'A => B',
      'A => B => C',
      'A => D',
      'E',
      'E => F',
      'E => G' ]
  );
  if (collapsed)
    await collapseAll(win.id);
  else
    await expandAll(win.id);

  await Utils.waitUntilAllTabChangesFinished(() => operator([tabs.B, tabs.E]));

  delete tabs.B;
  delete tabs.E;
  tabs = await Utils.refreshTabs(tabs);
  {
    const { A, C, D, F, G } = tabs;
    is([
      `${A.id}`,
      `${A.id} => ${C.id}`,
      `${A.id} => ${D.id}`,
      `${F.id}`,
      `${F.id} => ${G.id}`,
    ], Utils.treeStructure([A, C, D, F, G]),
       'the first child must be promoted');
  }
}

async function assertAllChildrenPromoted({ operator, collapsed } = {}) {
  await cleanupTabs();
  let tabs = await Utils.prepareTabsInWindow(
    { A: { index: 1 },
      B: { index: 2, openerTabId: 'A' },
      C: { index: 3, openerTabId: 'B' },
      D: { index: 4, openerTabId: 'B' },
      E: { index: 5 },
      F: { index: 6, openerTabId: 'E' },
      G: { index: 7, openerTabId: 'E' } },
    win.id,
    [ 'A',
      'A => B',
      'A => B => C',
      'A => B => D',
      'E',
      'E => F',
      'E => G' ]
  );
  if (collapsed)
    await collapseAll(win.id);
  else
    await expandAll(win.id);

  await Utils.waitUntilAllTabChangesFinished(() => operator([tabs.B, tabs.E]));

  delete tabs.B;
  delete tabs.E;
  tabs = await Utils.refreshTabs(tabs);
  {
    const { A, C, D, F, G } = tabs;
    is([
      `${A.id}`,
      `${A.id} => ${C.id}`,
      `${A.id} => ${D.id}`,
      `${F.id}`,
      `${G.id}`,
    ], Utils.treeStructure([A, C, D, F, G]),
       'all children must be promoted');
  }
}

async function assertPromotedIntelligently({ operator, collapsed } = {}) {
  await cleanupTabs();
  let tabs = await Utils.prepareTabsInWindow(
    { A: { index: 1 },
      B: { index: 2, openerTabId: 'A' },
      C: { index: 3, openerTabId: 'B' },
      D: { index: 4, openerTabId: 'B' },
      E: { index: 5, openerTabId: 'A' },
      F: { index: 6 },
      G: { index: 7, openerTabId: 'F' },
      H: { index: 8, openerTabId: 'F' } },
    win.id,
    [ 'A',
      'A => B',
      'A => B => C',
      'A => B => D',
      'A => E',
      'F',
      'F => G',
      'F => H' ]
  );
  if (collapsed)
    await collapseAll(win.id);
  else
    await expandAll(win.id);

  await Utils.waitUntilAllTabChangesFinished(() => operator([tabs.B, tabs.F]));

  delete tabs.B;
  delete tabs.F;
  tabs = await Utils.refreshTabs(tabs);
  {
    const { A, C, D, E, G, H } = tabs;
    is([
      `${A.id}`,
      `${A.id} => ${C.id}`,
      `${A.id} => ${D.id}`,
      `${A.id} => ${E.id}`,
      `${G.id}`,
      `${G.id} => ${H.id}`,
    ], Utils.treeStructure([A, C, D, E, G, H]),
       'all children must be promoted if there parent, otherwise promote the first child');
  }
}

async function assertAllChildrenDetached({ operator, collapsed } = {}) {
  await cleanupTabs();
  let tabs = await Utils.prepareTabsInWindow(
    { A: { index: 1 },
      B: { index: 2, openerTabId: 'A' },
      C: { index: 3, openerTabId: 'B' },
      D: { index: 4, openerTabId: 'B' },
      E: { index: 5, openerTabId: 'A' },
      F: { index: 6 },
      G: { index: 7, openerTabId: 'F' },
      H: { index: 8, openerTabId: 'F' } },
    win.id,
    [ 'A',
      'A => B',
      'A => B => C',
      'A => B => D',
      'A => E',
      'F',
      'F => G',
      'F => H' ]
  );
  if (collapsed)
    await collapseAll(win.id);
  else
    await expandAll(win.id);

  await Utils.waitUntilAllTabChangesFinished(() => operator([tabs.B, tabs.F]));

  delete tabs.B;
  delete tabs.F;
  tabs = await Utils.refreshTabs(tabs);
  {
    const { A, C, D, E, G, H } = tabs;
    is([
      `${A.id}`,
      `${A.id} => ${E.id}`,
      `${C.id}`,
      `${D.id}`,
      `${G.id}`,
      `${H.id}`,
    ], Utils.treeStructure([A, E, C, D, G, H]),
       'all children must be detached');
  }
}

/*
async function assertAllChildrenSimplyDetached({ operator, collapsed } = {}) {
  await cleanupTabs();
  let tabs = await Utils.prepareTabsInWindow(
    { A: { index: 1 },
      B: { index: 2, openerTabId: 'A' },
      C: { index: 3, openerTabId: 'B' },
      D: { index: 4, openerTabId: 'B' },
      E: { index: 5, openerTabId: 'A' },
      F: { index: 6 },
      G: { index: 7, openerTabId: 'F' },
      H: { index: 8, openerTabId: 'F' } },
    win.id,
    [ 'A',
      'A => B',
      'A => B => C',
      'A => B => D',
      'A => E',
      'F',
      'F => G',
      'F => H' ]
  );
  if (collapsed)
    await collapseAll(win.id);
  else
    await expandAll(win.id);

  await Utils.waitUntilAllTabChangesFinished(() => operator([tabs.B, tabs.F]));

  delete tabs.B;
  delete tabs.F;
  tabs = await Utils.refreshTabs(tabs);
  {
    const { A, C, D, E, G, H } = tabs;
    is([
      `${A.id}`,
      `${C.id}`,
      `${D.id}`,
      `${A.id} => ${E.id}`,
      `${G.id}`,
      `${H.id}`,
    ], Utils.treeStructure([A, C, D, E, G, H]),
       'all children must be detached at their original position');
  }
}
*/

async function assertEntireTreeClosed({ operator, collapsed } = {}) {
  await cleanupTabs();
  const tabs = await Utils.prepareTabsInWindow(
    { A: { index: 1 },
      B: { index: 2, openerTabId: 'A' },
      C: { index: 3, openerTabId: 'B' },
      D: { index: 4, openerTabId: 'B' },
      E: { index: 5 },
      F: { index: 6, openerTabId: 'E' },
      G: { index: 7, openerTabId: 'E' } },
    win.id,
    [ 'A',
      'A => B',
      'A => B => C',
      'A => B => D',
      'E',
      'E => F',
      'E => G' ]
  );
  if (collapsed)
    await collapseAll(win.id);
  else
    await expandAll(win.id);

  await Utils.waitUntilAllTabChangesFinished(() => operator([tabs.B, tabs.E]));
  const afterTabs = await Promise.all(
    Array.from(Object.values(tabs))
      .map(tab => browser.tabs.get(tab.id).catch(_error => null))
  );
  is([tabs.A.id],
     afterTabs.filter(tab => !!tab).map(tab => tab.id),
     'all closed parents and their children must be removed, and only upper level tab must be left');
}

async function assertEntireTreeMoved({ operator, collapsed } = {}) {
  await cleanupTabs();
  const tabs = await Utils.prepareTabsInWindow(
    { A: { index: 1 },
      B: { index: 2, openerTabId: 'A' },
      C: { index: 3, openerTabId: 'B' },
      D: { index: 4, openerTabId: 'B' },
      E: { index: 5 },
      F: { index: 6, openerTabId: 'E' },
      G: { index: 7, openerTabId: 'E' } },
    win.id,
    [ 'A',
      'A => B',
      'A => B => C',
      'A => B => D',
      'E',
      'E => F',
      'E => G' ]
  );
  const [allTabs] = await Promise.all([
    browser.tabs.query({ windowId: win.id }),
    collapsed ?
      collapseAll(win.id) :
      expandAll(win.id),
  ]);

  const operatedTabs = [tabs.B, tabs.E];
  await Utils.waitUntilAllTabChangesFinished(() => operator(operatedTabs));
  const [oldFirstTab, afterOperatedTabs, afterOtherTabs] = await Promise.all([
    browser.tabs.get(allTabs[0].id),
    Promise.all([tabs.B, tabs.C, tabs.D, tabs.E, tabs.F, tabs.G]
      .map(tab => browser.tabs.get(tab.id).catch(_error => null))),
    Promise.all([tabs.A]
      .map(tab => browser.tabs.get(tab.id).catch(_error => null))),
  ]);
  ok(afterOperatedTabs.every(tab => tab && tab.index < oldFirstTab.index),
     'all operated tabs must be placed before the first tab: ' +
       afterOperatedTabs.map(tab => tab && tab.index).join(',') + ' < ' + oldFirstTab.index);
  ok(afterOtherTabs.every(tab => tab && tab.index > oldFirstTab.index),
     'all other must be placed after the first tab also: ' +
       afterOtherTabs.map(tab => tab && tab.index).join(',') + ' > ' + oldFirstTab.index);
}

async function assertClosedParentIsReplacedWithGroup({ operator, collapsed } = {}) {
  await cleanupTabs();
  let tabs = await Utils.prepareTabsInWindow(
    { A: { index: 1 },
      B: { index: 2, openerTabId: 'A' },
      C: { index: 3, openerTabId: 'B' },
      D: { index: 4, openerTabId: 'B' },
      E: { index: 5 },
      F: { index: 6, openerTabId: 'E' },
      G: { index: 7, openerTabId: 'E' } },
    win.id,
    [ 'A',
      'A => B',
      'A => B => C',
      'A => B => D',
      'E',
      'E => F',
      'E => G' ]
  );
  if (collapsed)
    await collapseAll(win.id);
  else
    await expandAll(win.id);

  const beforeTabIds = new Set((await browser.tabs.query({ windowId: win.id })).map(tab => tab.id));
  await Utils.waitUntilAllTabChangesFinished(() => operator([tabs.B, tabs.E]));
  const openedTabs = (await browser.tabs.query({ windowId: win.id })).filter(tab => !beforeTabIds.has(tab.id));
  is(2,
     openedTabs.length,
     'group tabs must be opened for closed parent tabs');

  delete tabs.B;
  delete tabs.E;
  tabs.opened1 = openedTabs[0];
  tabs.opened2 = openedTabs[1];

  tabs = await Utils.refreshTabs(tabs);
  {
    const { A, opened1, C, D, opened2, F, G} = tabs;
    is([
      `${A.id}`,
      `${A.id} => ${opened1.id}`,
      `${A.id} => ${opened1.id} => ${C.id}`,
      `${A.id} => ${opened1.id} => ${D.id}`,
      `${opened2.id}`,
      `${opened2.id} => ${F.id}`,
      `${opened2.id} => ${G.id}`,
    ], Utils.treeStructure([A, opened1, C, D, opened2, F, G]),
       'tree structure must be kept');
  }
}

async function closeTabsFromSidebar(tabs) {
  for (const tab of tabs) {
    await browser.runtime.sendMessage({
      type:   Constants.kCOMMAND_REMOVE_TABS_BY_MOUSE_OPERATION,
      tabIds: [tab.id],
    });
    await wait(250);
  }
}

async function closeTabsFromOutside(tabs) {
  await browser.tabs.remove(tabs.map(tab => tab.id));
}

/*
async function moveTabsFromSidebar(tabs) {
  const allTabs = await browser.tabs.query({ windowId: win.id });
  await browser.runtime.sendMessage({
    type:                Constants.kCOMMAND_PERFORM_TABS_DRAG_DROP,
    windowId:            win.id,
    tabs:                tabs.map(tab => tab && tab.$TST.sanitized),
    structure:           [],
    action:              'move',
    allowedActions:      Constants.kDRAG_BEHAVIOR_MOVE | Constants.kDRAG_BEHAVIOR_TEAR_OFF | Constants.kDRAG_BEHAVIOR_ENTIRE_TREE,
    insertBeforeId:      allTabs[0].id,
    destinationWindowId: win.id,
    duplicate:           false,
    import:              false,
  });
}
*/

async function moveTabsFromOutside(tabs) {
  const tabIds = tabs.map(tab => tab.id);
  for (const id of tabIds) {
    await browser.tabs.move(id, { index: 0 });
    await wait(300);
  }
  //await browser.tabs.remove(tabIds);
}


export async function testPermanentlyConsistentBehaviors() {
  openSidebar();

  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CONSISTENT;
  await Utils.waitUntilAllTabChangesFinished(() => assertEntireTreeClosed({ operator: closeTabsFromSidebar, collapsed: true }));
  //await Utils.waitUntilAllTabChangesFinished(() => assertEntireTreeMoved({ operator: moveTabsFromSidebar, collapsed: true }));
  //await Utils.waitUntilAllTabChangesFinished(() => assertEntireTreeMoved({ operator: moveTabsFromSidebar }));

  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_PARALLEL;
  await Utils.waitUntilAllTabChangesFinished(() => assertEntireTreeClosed({ operator: closeTabsFromSidebar, collapsed: true }));

  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CUSTOM;
  await Utils.waitUntilAllTabChangesFinished(() => assertEntireTreeClosed({ operator: closeTabsFromSidebar, collapsed: true }));
}


async function assertCloseBehaved({
  collapsed_insideSidebar, expanded_insideSidebar,
  collapsed_outsideSidebar, expanded_outsideSidebar,
  collapsed_noSidebar, expanded_noSidebar
}) {
  openSidebar();
  await Utils.waitUntilAllTabChangesFinished(() => collapsed_insideSidebar({ operator: closeTabsFromSidebar, collapsed: true }));
  await Utils.waitUntilAllTabChangesFinished(() => expanded_insideSidebar({ operator: closeTabsFromSidebar }));
  await Utils.waitUntilAllTabChangesFinished(() => collapsed_outsideSidebar({ operator: closeTabsFromOutside, collapsed: true }));
  await Utils.waitUntilAllTabChangesFinished(() => expanded_outsideSidebar({ operator: closeTabsFromOutside }));
  closeSidebar();
  await Utils.waitUntilAllTabChangesFinished(() => collapsed_noSidebar({ operator: closeTabsFromOutside, collapsed: true }));
  await Utils.waitUntilAllTabChangesFinished(() => expanded_noSidebar({ operator: closeTabsFromOutside }));
}

async function assertMoveBehaved({
  collapsed_outsideSidebar, expanded_outsideSidebar,
  collapsed_noSidebar, expanded_noSidebar
}) {
  openSidebar();
  await Utils.waitUntilAllTabChangesFinished(() => collapsed_outsideSidebar({ operator: moveTabsFromOutside, collapsed: true }));
  await Utils.waitUntilAllTabChangesFinished(() => expanded_outsideSidebar({ operator: moveTabsFromOutside }));
  closeSidebar();
  await Utils.waitUntilAllTabChangesFinished(() => collapsed_noSidebar({ operator: moveTabsFromOutside, collapsed: true }));
  await Utils.waitUntilAllTabChangesFinished(() => expanded_noSidebar({ operator: moveTabsFromOutside }));
}


export async function testConsistentMode_entireTree() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CONSISTENT;

  configs.closeParentBehavior_insideSidebar_expanded = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE;
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertEntireTreeClosed,
    collapsed_outsideSidebar: assertEntireTreeClosed,
    expanded_outsideSidebar:  assertEntireTreeClosed,
    collapsed_noSidebar:      assertEntireTreeClosed,
    expanded_noSidebar:       assertEntireTreeClosed,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertEntireTreeMoved,
    expanded_outsideSidebar:  assertEntireTreeMoved,
    collapsed_noSidebar:      assertEntireTreeMoved,
    expanded_noSidebar:       assertEntireTreeMoved,
  });
}

export async function testConsistentMode_firstChild() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CONSISTENT;

  configs.closeParentBehavior_insideSidebar_expanded = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD;
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertFirstChildPromoted,
    collapsed_outsideSidebar: assertEntireTreeClosed,
    expanded_outsideSidebar:  assertFirstChildPromoted,
    collapsed_noSidebar:      assertEntireTreeClosed,
    expanded_noSidebar:       assertFirstChildPromoted,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertEntireTreeMoved,
    expanded_outsideSidebar:  assertEntireTreeMoved,
    collapsed_noSidebar:      assertEntireTreeMoved,
    expanded_noSidebar:       assertEntireTreeMoved,
  });
}

export async function testConsistentMode_allChildren() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CONSISTENT;

  configs.closeParentBehavior_insideSidebar_expanded = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN;
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertAllChildrenPromoted,
    collapsed_outsideSidebar: assertEntireTreeClosed,
    expanded_outsideSidebar:  assertAllChildrenPromoted,
    collapsed_noSidebar:      assertEntireTreeClosed,
    expanded_noSidebar:       assertAllChildrenPromoted,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertEntireTreeMoved,
    expanded_outsideSidebar:  assertEntireTreeMoved,
    collapsed_noSidebar:      assertEntireTreeMoved,
    expanded_noSidebar:       assertEntireTreeMoved,
  });
}

export async function testConsistentMode_intelligently() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CONSISTENT;

  configs.closeParentBehavior_insideSidebar_expanded = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_INTELLIGENTLY;
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertPromotedIntelligently,
    collapsed_outsideSidebar: assertEntireTreeClosed,
    expanded_outsideSidebar:  assertPromotedIntelligently,
    collapsed_noSidebar:      assertEntireTreeClosed,
    expanded_noSidebar:       assertPromotedIntelligently,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertEntireTreeMoved,
    expanded_outsideSidebar:  assertEntireTreeMoved,
    collapsed_noSidebar:      assertEntireTreeMoved,
    expanded_noSidebar:       assertEntireTreeMoved,
  });
}

export async function testConsistentMode_detach() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CONSISTENT;

  configs.closeParentBehavior_insideSidebar_expanded = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN;
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertAllChildrenDetached,
    collapsed_outsideSidebar: assertEntireTreeClosed,
    expanded_outsideSidebar:  assertAllChildrenDetached,
    collapsed_noSidebar:      assertEntireTreeClosed,
    expanded_noSidebar:       assertAllChildrenDetached,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertEntireTreeMoved,
    expanded_outsideSidebar:  assertEntireTreeMoved,
    collapsed_noSidebar:      assertEntireTreeMoved,
    expanded_noSidebar:       assertEntireTreeMoved,
  });
}

export async function testConsistentMode_group() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CONSISTENT;

  configs.closeParentBehavior_insideSidebar_expanded = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB;
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertClosedParentIsReplacedWithGroup,
    collapsed_outsideSidebar: assertEntireTreeClosed,
    expanded_outsideSidebar:  assertClosedParentIsReplacedWithGroup,
    collapsed_noSidebar:      assertEntireTreeClosed,
    expanded_noSidebar:       assertClosedParentIsReplacedWithGroup,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertEntireTreeMoved,
    expanded_outsideSidebar:  assertEntireTreeMoved,
    collapsed_noSidebar:      assertEntireTreeMoved,
    expanded_noSidebar:       assertEntireTreeMoved,
  });
}


// Suffixes of following tests just mean the first parameter.
// Behaviors are rotated for better coverage.

export async function testParallelMode_entireTree() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_PARALLEL;

  await Utils.setConfigs({
    closeParentBehavior_insideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
    closeParentBehavior_outsideSidebar_expanded: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    moveParentBehavior_outsideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
  });
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertEntireTreeClosed,
    collapsed_outsideSidebar: assertFirstChildPromoted,
    expanded_outsideSidebar:  assertFirstChildPromoted,
    collapsed_noSidebar:      assertFirstChildPromoted,
    expanded_noSidebar:       assertFirstChildPromoted,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertAllChildrenPromoted,
    expanded_outsideSidebar:  assertAllChildrenPromoted,
    collapsed_noSidebar:      assertAllChildrenPromoted,
    expanded_noSidebar:       assertAllChildrenPromoted,
  });
}

export async function testParallelMode_firstChild() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_PARALLEL;

  await Utils.setConfigs({
    closeParentBehavior_insideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    closeParentBehavior_outsideSidebar_expanded: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
    moveParentBehavior_outsideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
  });
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertFirstChildPromoted,
    collapsed_outsideSidebar: assertAllChildrenPromoted,
    expanded_outsideSidebar:  assertAllChildrenPromoted,
    collapsed_noSidebar:      assertAllChildrenPromoted,
    expanded_noSidebar:       assertAllChildrenPromoted,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertClosedParentIsReplacedWithGroup,
    expanded_outsideSidebar:  assertClosedParentIsReplacedWithGroup,
    collapsed_noSidebar:      assertClosedParentIsReplacedWithGroup,
    expanded_noSidebar:       assertClosedParentIsReplacedWithGroup,
  });
}

export async function testParallelMode_allChildren() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_PARALLEL;

  await Utils.setConfigs({
    closeParentBehavior_insideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
    closeParentBehavior_outsideSidebar_expanded: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
    moveParentBehavior_outsideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
  });
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertAllChildrenPromoted,
    collapsed_outsideSidebar: assertAllChildrenDetached,
    expanded_outsideSidebar:  assertAllChildrenDetached,
    collapsed_noSidebar:      assertAllChildrenDetached,
    expanded_noSidebar:       assertAllChildrenDetached,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertEntireTreeMoved,
    expanded_outsideSidebar:  assertEntireTreeMoved,
    collapsed_noSidebar:      assertEntireTreeMoved,
    expanded_noSidebar:       assertEntireTreeMoved,
  });
}

export async function testParallelMode_detach() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_PARALLEL;

  await Utils.setConfigs({
    closeParentBehavior_insideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
    closeParentBehavior_outsideSidebar_expanded: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
  });
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertAllChildrenDetached,
    collapsed_outsideSidebar: assertFirstChildPromoted,
    expanded_outsideSidebar:  assertFirstChildPromoted,
    collapsed_noSidebar:      assertFirstChildPromoted,
    expanded_noSidebar:       assertFirstChildPromoted,
  });
}

export async function testParallelMode_group() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_PARALLEL;

  await Utils.setConfigs({
    closeParentBehavior_insideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    closeParentBehavior_outsideSidebar_expanded: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
  });
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertClosedParentIsReplacedWithGroup,
    collapsed_outsideSidebar: assertEntireTreeClosed,
    expanded_outsideSidebar:  assertEntireTreeClosed,
    collapsed_noSidebar:      assertEntireTreeClosed,
    expanded_noSidebar:       assertEntireTreeClosed,
  });
}


export async function testCustomMode_entireTree() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CUSTOM;

  await Utils.setConfigs({
    closeParentBehavior_insideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
    closeParentBehavior_outsideSidebar_collapsed: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    closeParentBehavior_outsideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
    closeParentBehavior_noSidebar_collapsed:      Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
    closeParentBehavior_noSidebar_expanded:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    moveParentBehavior_outsideSidebar_collapsed:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
    moveParentBehavior_outsideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    moveParentBehavior_noSidebar_collapsed:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
    moveParentBehavior_noSidebar_expanded:        Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
  });
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertEntireTreeClosed,
    collapsed_outsideSidebar: assertFirstChildPromoted,
    expanded_outsideSidebar:  assertAllChildrenPromoted,
    collapsed_noSidebar:      assertAllChildrenDetached,
    expanded_noSidebar:       assertClosedParentIsReplacedWithGroup,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertEntireTreeMoved,
    expanded_outsideSidebar:  assertFirstChildPromoted,
    collapsed_noSidebar:      assertAllChildrenPromoted,
    expanded_noSidebar:       assertAllChildrenDetached,
  });
}

export async function testCustomMode_firstChild() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CUSTOM;

  await Utils.setConfigs({
    closeParentBehavior_insideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    closeParentBehavior_outsideSidebar_collapsed: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
    closeParentBehavior_outsideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
    closeParentBehavior_noSidebar_collapsed:      Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    closeParentBehavior_noSidebar_expanded:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
    moveParentBehavior_outsideSidebar_collapsed:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    moveParentBehavior_outsideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
    moveParentBehavior_noSidebar_collapsed:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
    moveParentBehavior_noSidebar_expanded:        Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
  });
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertFirstChildPromoted,
    collapsed_outsideSidebar: assertAllChildrenPromoted,
    expanded_outsideSidebar:  assertAllChildrenDetached,
    collapsed_noSidebar:      assertClosedParentIsReplacedWithGroup,
    expanded_noSidebar:       assertEntireTreeClosed,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertFirstChildPromoted,
    expanded_outsideSidebar:  assertAllChildrenPromoted,
    collapsed_noSidebar:      assertAllChildrenDetached,
    expanded_noSidebar:       assertClosedParentIsReplacedWithGroup,
  });
}

export async function testCustomMode_allChildren() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CUSTOM;

  await Utils.setConfigs({
    closeParentBehavior_insideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
    closeParentBehavior_outsideSidebar_collapsed: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
    closeParentBehavior_outsideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    closeParentBehavior_noSidebar_collapsed:      Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
    closeParentBehavior_noSidebar_expanded:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    moveParentBehavior_outsideSidebar_collapsed:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
    moveParentBehavior_outsideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
    moveParentBehavior_noSidebar_collapsed:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    moveParentBehavior_noSidebar_expanded:        Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
  });
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertAllChildrenPromoted,
    collapsed_outsideSidebar: assertAllChildrenDetached,
    expanded_outsideSidebar:  assertClosedParentIsReplacedWithGroup,
    collapsed_noSidebar:      assertEntireTreeClosed,
    expanded_noSidebar:       assertFirstChildPromoted,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertAllChildrenPromoted,
    expanded_outsideSidebar:  assertAllChildrenDetached,
    collapsed_noSidebar:      assertClosedParentIsReplacedWithGroup,
    expanded_noSidebar:       assertEntireTreeMoved,
  });
}

export async function testCustomMode_detach() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CUSTOM;

  await Utils.setConfigs({
    closeParentBehavior_insideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
    closeParentBehavior_outsideSidebar_collapsed: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    closeParentBehavior_outsideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
    closeParentBehavior_noSidebar_collapsed:      Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    closeParentBehavior_noSidebar_expanded:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
    moveParentBehavior_outsideSidebar_collapsed:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
    moveParentBehavior_outsideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    moveParentBehavior_noSidebar_collapsed:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
    moveParentBehavior_noSidebar_expanded:        Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
  });
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertAllChildrenDetached,
    collapsed_outsideSidebar: assertClosedParentIsReplacedWithGroup,
    expanded_outsideSidebar:  assertEntireTreeClosed,
    collapsed_noSidebar:      assertFirstChildPromoted,
    expanded_noSidebar:       assertAllChildrenPromoted,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertAllChildrenDetached,
    expanded_outsideSidebar:  assertClosedParentIsReplacedWithGroup,
    collapsed_noSidebar:      assertEntireTreeMoved,
    expanded_noSidebar:       assertFirstChildPromoted,
  });
}

export async function testCustomMode_group() {
  configs.parentTabOperationBehaviorMode = Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CUSTOM;

  await Utils.setConfigs({
    closeParentBehavior_insideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    closeParentBehavior_outsideSidebar_collapsed: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
    closeParentBehavior_outsideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    closeParentBehavior_noSidebar_collapsed:      Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
    closeParentBehavior_noSidebar_expanded:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_DETACH_ALL_CHILDREN,
    moveParentBehavior_outsideSidebar_collapsed:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    moveParentBehavior_outsideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
    moveParentBehavior_noSidebar_collapsed:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    moveParentBehavior_noSidebar_expanded:        Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_ALL_CHILDREN,
  });
  await assertCloseBehaved({
    collapsed_insideSidebar:  assertEntireTreeClosed,
    expanded_insideSidebar:   assertClosedParentIsReplacedWithGroup,
    collapsed_outsideSidebar: assertEntireTreeClosed,
    expanded_outsideSidebar:  assertFirstChildPromoted,
    collapsed_noSidebar:      assertAllChildrenPromoted,
    expanded_noSidebar:       assertAllChildrenDetached,
  });
  await assertMoveBehaved({
    collapsed_outsideSidebar: assertClosedParentIsReplacedWithGroup,
    expanded_outsideSidebar:  assertEntireTreeMoved,
    collapsed_noSidebar:      assertFirstChildPromoted,
    expanded_noSidebar:       assertAllChildrenPromoted,
  });
}


export async function testPromoteOnlyFirstChildWhenClosedParentIsLastChild() {
  await Utils.setConfigs({
    warnOnCloseTabs:                               false,
    parentTabOperationBehaviorMode:                Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CONSISTENT,
    closeParentBehavior_insideSidebar_expanded:    Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    promoteAllChildrenWhenClosedParentIsLastChild: false,
  });
  configs.sidebarVirtuallyOpenedWindows = [win.id];

  let tabs = await Utils.prepareTabsInWindow(
    { A: { index: 1 },
      B: { index: 2, openerTabId: 'A' },
      C: { index: 3, openerTabId: 'B' },
      D: { index: 4, openerTabId: 'B' },
      E: { index: 5 },
      F: { index: 6, openerTabId: 'E' },
      G: { index: 7, openerTabId: 'E' } },
    win.id,
    [ 'A',
      'A => B',
      'A => B => C',
      'A => B => D',
      'E',
      'E => F',
      'E => G' ]
  );
  await expandAll(win.id);

  await browser.tabs.remove([tabs.B.id, tabs.E.id]);
  await wait(500);

  delete tabs.B;
  delete tabs.E;
  tabs = await Utils.refreshTabs(tabs);
  {
    const { A, C, D, F, G } = tabs;
    is([
      `${A.id}`,
      `${A.id} => ${C.id}`,
      `${A.id} => ${C.id} => ${D.id}`,
      `${F.id}`,
      `${F.id} => ${G.id}`,
    ], Utils.treeStructure([A, C, D, F, G]),
       'only first child must be promoted');
  }
}

export async function testPromoteAllChildrenWhenClosedParentIsLastChild() {
  await Utils.setConfigs({
    warnOnCloseTabs:                               false,
    parentTabOperationBehaviorMode:                Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CONSISTENT,
    closeParentBehavior_insideSidebar_expanded:    Constants.kPARENT_TAB_OPERATION_BEHAVIOR_PROMOTE_FIRST_CHILD,
    promoteAllChildrenWhenClosedParentIsLastChild: true,
  });
  configs.sidebarVirtuallyOpenedWindows = [win.id];

  let tabs = await Utils.prepareTabsInWindow(
    { A: { index: 1 },
      B: { index: 2, openerTabId: 'A' },
      C: { index: 3, openerTabId: 'B' },
      D: { index: 4, openerTabId: 'B' },
      E: { index: 5 },
      F: { index: 6, openerTabId: 'E' },
      G: { index: 7, openerTabId: 'E' } },
    win.id,
    [ 'A',
      'A => B',
      'A => B => C',
      'A => B => D',
      'E',
      'E => F',
      'E => G' ]
  );
  await expandAll(win.id);

  await browser.tabs.remove([tabs.B.id, tabs.E.id]);
  await wait(500);

  delete tabs.B;
  delete tabs.E;
  tabs = await Utils.refreshTabs(tabs);
  {
    const { A, C, D, F, G } = tabs;
    is([
      `${A.id}`,
      `${A.id} => ${C.id}`,
      `${A.id} => ${D.id}`,
      `${F.id}`,
      `${F.id} => ${G.id}`,
    ], Utils.treeStructure([A, C, D, F, G]),
       'all children must be promoted only when it is the last child');
  }
}

// https://github.com/piroor/treestyletab/issues/2819
export async function testKeepChildrenForTemporaryAggressiveGroupWithCloseParentWithAllChildrenBehavior() {
  await Utils.setConfigs({
    warnOnCloseTabs:                              false,
    parentTabOperationBehaviorMode:               Constants.kPARENT_TAB_OPERATION_BEHAVIOR_MODE_CUSTOM,
    closeParentBehavior_insideSidebar_expanded:   Constants.kPARENT_TAB_OPERATION_BEHAVIOR_ENTIRE_TREE,
    closeParentBehavior_outsideSidebar_collapsed: Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    closeParentBehavior_outsideSidebar_expanded:  Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    closeParentBehavior_noSidebar_collapsed:      Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
    closeParentBehavior_noSidebar_expanded:       Constants.kPARENT_TAB_OPERATION_BEHAVIOR_REPLACE_WITH_GROUP_TAB,
  });
  configs.sidebarVirtuallyOpenedWindows = [win.id];

  const tabs = await Utils.prepareTabsInWindow(
    { A: { index: 1, url: `${Constants.kSHORTHAND_URIS.group}?temporaryAggressive=true` },
      B: { index: 2, openerTabId: 'A' },
      C: { index: 3, openerTabId: 'A' } },
    win.id,
    [ 'A',
      'A => B',
      'A => C' ]
  );
  await expandAll(win.id);

  const beforeTabs = await browser.tabs.query({ windowId: win.id });
  await browser.tabs.remove(tabs.C.id);
  await wait(500);
  const afterTabs = await browser.tabs.query({ windowId: win.id });
  is(beforeTabs.length - 2,
     afterTabs.length,
     'only the group parent tab should be cleaned up');
  is(afterTabs[afterTabs.length - 1].id,
     tabs.B.id,
     'other children of the group parent tab must be kept');
}


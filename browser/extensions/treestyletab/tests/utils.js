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
import { is /*, ok, ng*/ } from './assert.js';

import * as Constants from '/common/constants.js';
//import * as Tree from '/background/tree.js';
import Tab from '/common/Tab.js';

export async function prepareTabsInWindow(definition, windowId, expectedStructure) {
  let tabs = await createTabs(definition, { windowId });
  tabs = await refreshTabs(tabs);
  const actualTabs = new Set();
  const expected = expectedStructure.map(line => {
    return line.replace(/\b[a-z]+\b/gi, matched => {
      const tab = tabs[matched];
      if (!actualTabs.has(tab))
        actualTabs.add(tab);
      return tab.id;
    });
  });
  is(expected,
     treeStructure(Array.from(actualTabs)),
     'tabs must be initialized with specified structure');
  return tabs;
}

export async function createTab(params = {}) {
  return browser.tabs.create(params);
}

export async function createTabs(definitions, commonParams = {}) {
  const oldAutoGroupNewTabs = configs.autoGroupNewTabs;
  if (oldAutoGroupNewTabs)
    await setConfigs({ autoGroupNewTabs: false });

  let tabs;
  let toBeActiveTabId;
  let treeChanged = false;
  if (Array.isArray(definitions)) {
    tabs = Promise.all(definitions.map(async (definition, index) => {
      if (!definition.url)
        definition.url = `about:blank?${index}`;
      const params = { ...commonParams, ...definition };
      if (params.openerTabId)
        treeChanged = true;
      const tab = await createTab({
        ...params,
        active: false // prepare all tabs in background, otherwise they may be misordered!
      });
      if (definition.active)
        toBeActiveTabId = tab.id;
      return tab;
    }));
  }

  if (typeof definitions == 'object') {
    tabs = {};
    for (const name of Object.keys(definitions)) {
      const definition = definitions[name];
      if (definition.openerTabId in tabs)
        definition.openerTabId = tabs[definition.openerTabId].id;
      if (!definition.url)
        definition.url = `about:blank?${name}`;
      const params = { ...commonParams, ...definition };
      if (params.openerTabId)
        treeChanged = true;
      tabs[name] = await createTab({
        ...params,
        active: false // prepare all tabs in background, otherwise they may be misordered!
      });
      await wait(100);
      if (definition.active)
        toBeActiveTabId = tabs[name].id;
    }
  }

  if (!tabs)
    throw new Error('Invalid tab definitions: ', definitions);

  if (toBeActiveTabId)
    await browser.tabs.update(toBeActiveTabId, { active: true });

  if (treeChanged) // wait until tree information is applied
    await wait(1000);

  if (oldAutoGroupNewTabs)
    await setConfigs({ autoGroupNewTabs: oldAutoGroupNewTabs });

  return tabs;
}

export async function refreshTabs(tabs) {
  if (Array.isArray(tabs)) {
    tabs = await browser.runtime.sendMessage({
      type:   Constants.kCOMMAND_PULL_TABS,
      tabIds: tabs.map(tab => tab.id)
    });
    return tabs.map(tab => Tab.import(tab));
  }

  if (typeof tabs == 'object') {
    const refreshedTabsArray = await browser.runtime.sendMessage({
      type:   Constants.kCOMMAND_PULL_TABS,
      tabIds: Object.values(tabs).map(tab => tab.id)
    });
    const refreshedTabs = {};
    const idToName = {};
    for (const name of Object.keys(tabs)) {
      idToName[tabs[name].id] = name;
    }
    for (const tab of refreshedTabsArray) {
      refreshedTabs[idToName[tab.id]] = Tab.import(tab);
    }
    return refreshedTabs;
  }

  throw new Error('Invalid tab collection: ', tabs);
}

export function treeStructure(tabs) {
  const tabsById = {};
  for (const tab of tabs) {
    tabsById[tab.id] = tab;
  }
  const outputNestedRelation = (tab) => {
    if (!tab)
      return '?';
    if (tab.openerTabId && tab.openerTabId != tab.id)
      return `${outputNestedRelation(tabsById[tab.openerTabId])} => ${tab.id}`;
    else if (tab.$TST && tab.$TST.parentId && tab.$TST.parentId != tab.id)
      return `${outputNestedRelation(tabsById[tab.$TST.parentId])} => ${tab.id}`;
    return `${tab.id}`;
  };
  return tabs.slice(0).sort((a, b) => a.index - b.index).map(outputNestedRelation);
}

export async function tabsOrder(tabs) {
  if (Array.isArray(tabs)) {
    tabs = await browser.runtime.sendMessage({
      type:   Constants.kCOMMAND_PULL_TABS,
      tabIds: tabs.map(tab => tab.id || tab)
    });
    return Tab.sort(tabs).map(tab => tab.id);
  }

  if (typeof tabs == 'object') {
    const refreshedTabsArray = await browser.runtime.sendMessage({
      type:   Constants.kCOMMAND_PULL_TABS,
      tabIds: Object.values(tabs).map(tab => tab.id)
    });
    return Tab.sort(refreshedTabsArray).map(tab => tab.id);
  }

  throw new Error('Invalid tab collection: ', tabs);
}

export async function setConfigs(values) {
  const uniqueValue = Date.now() + ':' + parseInt(Math.random() * 65000);
  // wait until updated configs are delivered to other namespaces
  return new Promise((resolve, _reject) => {
    const onMessage = (message, _sender) => {
      if (!message ||
          !message.type ||
          message.type != Constants.kCOMMAND_NOTIFY_TEST_KEY_CHANGED ||
          message.value != uniqueValue)
        return;
      browser.runtime.onMessage.removeListener(onMessage);
      resolve();
    };
    browser.runtime.onMessage.addListener(onMessage);

    for (const key of Object.keys(values)) {
      configs[key] = values[key];
    }
    configs.testKey = uniqueValue;
  });
}

export async function doAndGetNewTabs(task, queryToFindTabs) {
  if (!queryToFindTabs || Object.keys(queryToFindTabs).length == 0)
    throw new Error(`doAndGetNewTabs requires valid query to find tabs. given query: ${JSON.stringify(queryToFindTabs)}`);
  await wait(150); // wait until currently opened tabs are completely tracked
  const oldAllTabIds = new Set((await browser.tabs.query(queryToFindTabs)).map(tab => tab.id));
  await task();
  await wait(150); // wait until new tabs are tracked
  const allTabs = await browser.tabs.query(queryToFindTabs);
  const newTabs = allTabs.filter(tab => !oldAllTabIds.has(tab.id));
  return refreshTabs(newTabs);
}

export async function callAPI(message) {
  return browser.runtime.sendMessage({
    ...message,
    type: `treestyletab:api:${message.type}`
  });
}

export async function waitUntilAllTabChangesFinished(operation) {
  return new Promise(async (resolve, reject) => {
    let changeCount = 0;
    let operationFinished = false;
    const onChanged = () => {
      changeCount++;
      setTimeout(() => {
        changeCount--;
        if (changeCount > 0)
          return;
        browser.tabs.onCreated.removeListener(onChanged);
        browser.tabs.onRemoved.removeListener(onChanged);
        browser.tabs.onMoved.removeListener(onChanged);
        if (operationFinished)
          resolve();
      }, 500);
    };
    browser.tabs.onCreated.addListener(onChanged);
    browser.tabs.onRemoved.addListener(onChanged);
    browser.tabs.onMoved.addListener(onChanged);
    if (typeof operation == 'function') {
      try {
        await operation();
      }
      catch(error) {
        operationFinished = true;
        return reject(error);
      }
    }
    operationFinished = true;
    if (changeCount == 0)
      resolve();
  });
}

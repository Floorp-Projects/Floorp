/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  configs
} from './common.js';

function log(...args) {
  internalLogger('common/api-tabs', ...args);
}

export async function getIndexes(...queriedTabIds) {
  log('getIndexes ', queriedTabIds);
  if (queriedTabIds.length == 0)
    return [];

  const indexes = await Promise.all(queriedTabIds.map((tabId) => {
    return browser.tabs.get(tabId)
      .catch(e => {
        handleMissingTabError(e);
        return null;
      });
  }));
  return indexes.map(tab => tab ? tab.index : -1);
}

export async function blur(tab, unactivatableTabs = []) {
  const unactivatableIndices  = Array.from(new Set([tab, ...unactivatableTabs].map(tab => tab.index)));
  const minUnactivatableIndex = Math.min.apply(null, unactivatableIndices);
  const maxUnactivatableIndex = Math.max.apply(null, unactivatableIndices);
  const unactivatableTabById  = new Map();
  for (const tab of unactivatableTabs) {
    unactivatableTabById.set(tab.id, tab);
  }

  const allTabs     = await browser.tabs.query({ windowId: tab.windowId });
  const unactivatableTabIds = new Set([tab.id, ...unactivatableTabs.map(tab => tab.id)]);
  if (allTabs.length == unactivatableTabIds.size)
    return; // there is no other focusible tab!

  const restTabs    = allTabs.filter(tab => unactivatableTabIds.has(tab.id));
  const middleTabs  = restTabs.filter(tab => tab.index > minUnactivatableIndex || tab.index < maxUnactivatableIndex);
  const previousTab = minUnactivatableIndex > 0 && allTabs[minUnactivatableIndex - 1];
  const nextTab     = maxUnactivatableIndex < allTabs.length - 1 && allTabs[maxUnactivatableIndex + 1];
  const allTabById  = new Map();
  for (const tab of allTabs) {
    allTabById.set(tab.id, tab);
  }

  const scannedTabIds = new Set();
  let successorTab = tab;
  do {
    if (scannedTabIds.has(successorTab.id))
      break; // prevent infinite loop!
    scannedTabIds.add(successorTab.id);
    let nextSuccessorTab = unactivatableTabById.get(successorTab.successorTabId);
    if (nextSuccessorTab) {
      successorTab = nextSuccessorTab;
      continue;
    }
    nextSuccessorTab = allTabById.get(successorTab.successorTabId) ||
      nextTab ||
      (middleTabs.length > 0 && middleTabs[0]) ||
      previousTab ||
      restTabs[0];
    if (unactivatableTabById.has(nextSuccessorTab.id)) {
      successorTab = nextSuccessorTab;
      continue;
    }
    await browser.tabs.update(nextSuccessorTab.id, { active: true });
    break;
  }
  while (successorTab);
}

// workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1394477 + fix pinned/unpinned status
export async function safeMoveAcrossWindows(tabIds, moveOptions) {
  log('safeMoveAcrossWindows ', tabIds, moveOptions);
  if (!Array.isArray(tabIds))
    tabIds = [tabIds];
  const tabs = await Promise.all(tabIds.map(id => browser.tabs.get(id).catch(handleMissingTabError)));
  const activeTab = tabs.find(tab => tab.active);
  if (activeTab)
    await blur(activeTab, tabs);
  const window = await browser.windows.get(moveOptions.windowId || tabs[0].windowId, { populate: true });
  return (await Promise.all(tabs.map(async (tab, index) => {
    try {
      const destIndex = moveOptions.index + index;
      if (tab.pinned) {
        if (window.tabs[destIndex - 1] &&
            window.tabs[destIndex - 1].pinned != tab.pinned)
          await browser.tabs.update(tab.id, { pinned: false });
      }
      else {
        if (window.tabs[destIndex] &&
            window.tabs[destIndex].pinned != tab.pinned)
          await browser.tabs.update(tab.id, { pinned: true });
      }
      let movedTab = await browser.tabs.move(tab.id, {
        ...moveOptions,
        index: destIndex
      });
      log(`safeMoveAcrossWindows: movedTab[${index}] = `, movedTab);
      if (Array.isArray(movedTab))
        movedTab = movedTab[0];
      return movedTab;
    }
    catch(e) {
      handleMissingTabError(e);
      return null;
    }
  }))).filter(tab => !!tab);
}

export function isMissingTabError(error) {
  return (
    error &&
    error.message &&
    error.message.includes('Invalid tab ID:')
  );
}
export function handleMissingTabError(error) {
  if (!isMissingTabError(error))
    throw error;
  // otherwise, this error is caused from a tab already closed.
  // we just ignore it.
  //console.log('Invalid Tab ID error on: ' + error.stack);
}

export function isUnloadedError(error) {
  return (
    error &&
    error.message &&
    error.message.includes('can\'t access dead object')
  );
}
export function handleUnloadedError(error) {
  if (!isUnloadedError(error))
    throw error;
}

export function isMissingHostPermissionError(error) {
  return (
    error &&
    error.message &&
    error.message.includes('Missing host permission for the tab')
  );
}
export function handleMissingHostPermissionError(error) {
  if (!isMissingHostPermissionError(error))
    throw error;
}

export function createErrorHandler(...handlers) {
  const stack = configs.debug && new Error().stack;
  return (error) => {
    try {
      if (handlers.length > 0) {
        let unhandledCount = 0;
        handlers.forEach(handler => {
          try {
            handler(error);
          }
          catch(_error) {
            unhandledCount++;
          }
        });
        if (unhandledCount == handlers.length) // not handled
          throw error;
      }
      else {
        throw error;
      }
    }
    catch(newError){
      if (!configs.debug)
        throw newError;
      if (error == newError)
        console.log('Unhandled Error: ', error, stack);
      else
        console.log('Unhandled Error: ', error, newError, stack);
    }
  };
}

export function createErrorSuppressor(...handlers) {
  const stack = configs.debug && new Error().stack;
  return (error) => {
    try {
      if (handlers.length > 0) {
        let unhandledCount = 0;
        handlers.forEach(handler => {
          try {
            handler(error);
          }
          catch(_error) {
            unhandledCount++;
          }
        });
        if (unhandledCount == handlers.length) // not handled
          throw error;
      }
      else {
        throw error;
      }
    }
    catch(newError){
      if (error &&
          error.message &&
          error.message.indexOf('Could not establish connection. Receiving end does not exist.') == 0)
        return;
      if (!configs.debug)
        return;
      if (error == newError)
        console.log('Unhandled Error: ', error, stack);
      else
        console.log('Unhandled Error: ', error, newError, stack);
    }
  };
}

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

// workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1394477 + fix pinned/unpinned status
export async function safeMoveAcrossWindows(tabIds, moveOptions) {
  log('safeMoveAcrossWindows ', tabIds, moveOptions);
  if (!Array.isArray(tabIds))
    tabIds = [tabIds];
  const tabs = await Promise.all(tabIds.map(id => browser.tabs.get(id).catch(handleMissingTabError)));
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

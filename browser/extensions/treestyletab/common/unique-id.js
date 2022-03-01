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
} from './common.js';
import * as Constants from './constants.js';
import * as TabsStore from './tabs-store.js';
import * as ApiTabs from './api-tabs.js';

// eslint-disable-next-line no-unused-vars
function log(...args) {
  internalLogger('common/unique-id', ...args);
}

//===================================================================
// Unique Tab ID
//===================================================================

// for generated IDs
const kID_ADJECTIVES = `
Agile
Breezy
Cheerful
Dapper
Edgy
Feisty
Gutsy
Hoary
Intrepid
Jaunty
Karmic
Lucid
Marveric
Natty
Oneiric
Precise
Quantal
Raring
Saucy
Trusty
Utopic
Vivid
Warty
Xenial
Yakkety
Zesty
`.toLowerCase().trim().split(/\s+/);

const kID_NOUNS = `
Alpaca
Badger
Cat
Drake
Eft
Fawn
Gibbon
Heron
Ibis
Jackalope
Koala
Lynx
Meerkat
Narwhal
Ocelot
Pangolin
Quetzal
Ringtail
Salamander
Tahr
Unicorn
Vervet
Werwolf
Xerus
Yak
Zapus
`.toLowerCase().trim().split(/\s+/);

let mReadyToDetectDuplicatedTab = false;

export function readyToDetectDuplicatedTab() {
  mReadyToDetectDuplicatedTab = true;
}

export async function request(tabOrId, options = {}) {
  if (typeof options != 'object')
    options = {};

  let tab = tabOrId;
  if (typeof tabOrId == 'number')
    tab = TabsStore.tabs.get(tabOrId);

  if (TabsStore.getCurrentWindowId()) {
    return browser.runtime.sendMessage({
      type:  Constants.kCOMMAND_REQUEST_UNIQUE_ID,
      tabId: tab.id
    }).catch(ApiTabs.createErrorHandler());
  }

  let originalId    = null;
  let originalTabId = null;
  let duplicated    = false;
  if (!options.forceNew) {
    // https://github.com/piroor/treestyletab/issues/2845
    // This delay may break initial restoration of tabs, so we should
    // ignore it until all restoration processes are finished.
    if (mReadyToDetectDuplicatedTab &&
        configs.delayForDuplicatedTabDetection > 0)
      await wait(configs.delayForDuplicatedTabDetection);

    let oldId = await browser.sessions.getTabValue(tab.id, Constants.kPERSISTENT_ID).catch(ApiTabs.createErrorHandler());
    if (oldId && !oldId.tabId) // ignore broken information!
      oldId = null;

    if (oldId) {
      // If the tab detected from stored tabId is different, it is duplicated tab.
      try {
        const tabWithOldId = TabsStore.tabs.get(oldId.tabId);
        if (!tabWithOldId)
          throw new Error(`Invalid tab ID: ${oldId.tabId}`);
        originalId = (tabWithOldId.$TST.uniqueId || await tabWithOldId.$TST.promisedUniqueId).id;
        duplicated = tab && tabWithOldId.id != tab.id && originalId == oldId.id;
        if (duplicated)
          originalTabId = oldId.tabId;
        else
          throw new Error(`Invalid tab ID: ${oldId.tabId}`);
      }
      catch(e) {
        ApiTabs.handleMissingTabError(e);
        // It fails if the tab doesn't exist.
        // There is no live tab for the tabId, thus
        // this seems to be a tab restored from session.
        // We need to update the related tab id.
        browser.sessions.setTabValue(tab.id, Constants.kPERSISTENT_ID, {
          id:    oldId.id,
          tabId: tab.id
        }).catch(ApiTabs.createErrorSuppressor());
        return {
          id:            oldId.id,
          originalId:    null,
          originalTabId: oldId.tabId,
          restored:      true
        };
      }
    }
  }

  const adjective   = kID_ADJECTIVES[Math.floor(Math.random() * kID_ADJECTIVES.length)];
  const noun        = kID_NOUNS[Math.floor(Math.random() * kID_NOUNS.length)];
  const randomValue = Math.floor(Math.random() * 1000);
  const id          = `tab-${adjective}-${noun}-${Date.now()}-${randomValue}`;
  // tabId is for detecttion of duplicated tabs
  await browser.sessions.setTabValue(tab.id, Constants.kPERSISTENT_ID, { id, tabId: tab.id }).catch(ApiTabs.createErrorSuppressor());
  return { id, originalId, originalTabId, duplicated };
}

export async function getFromTabs(tabs) {
  return Promise.all(tabs.map(tab =>
    browser.sessions.getTabValue(tab.id, Constants.kPERSISTENT_ID).catch(ApiTabs.createErrorHandler())
  ));
}

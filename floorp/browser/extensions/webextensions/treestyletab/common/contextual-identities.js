/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import EventListenerManager from '/extlib/EventListenerManager.js';

import {
  configs,
  log as internalLogger,
  isWindows,
} from './common.js';
import * as ApiTabs from '/common/api-tabs.js';

// eslint-disable-next-line no-unused-vars
function log(...args) {
  internalLogger('common/contextual-identities', ...args);
}

const mContextualIdentities = new Map();

export function get(id) {
  return mContextualIdentities.get(id);
}

export function getIdFromName(name) {
  for (const identity of mContextualIdentities.values()) {
    if (identity.name.toLowerCase() == name.toLowerCase())
      return identity.cookieStoreId;
  }
  return null;
}

// Respect container type stored by Container Bookmarks
// https://addons.mozilla.org/firefox/addon/container-bookmarks/
export function getIdFromBookmark(bookmark) {
  const containerMatcher = new RegExp(`#${configs.containerRedirectKey}-(.+)$`);
  const matchedContainer = bookmark.url.match(containerMatcher);
  if (!matchedContainer)
    return {};

  const idPart = matchedContainer[matchedContainer.length-1];
  const url    = bookmark.url.replace(containerMatcher, '');

  // old method
  const identity = mContextualIdentities.get(decodeURIComponent(idPart));
  if (identity) {
    return {
      cookieStoreId: identity.cookieStoreId,
      url,
    };
  }

  for (const [cookieStoreId, identity] of mContextualIdentities.entries()) {
    if (idPart != encodeURIComponent(identity.name.toLowerCase().replace(/\s/g, '-')))
      continue;
    return {
      cookieStoreId,
      url,
    };
  }

  return {};
}

export function getColorInfo() {
  const colors = {};
  const customColors = [];
  forEach(identity => {
    if (!identity.colorCode)
      return;
    let colorValue = identity.colorCode;
    if (identity.color) {
      const customColor = `--contextual-identity-color-${identity.color}`;
      customColors.push(`${customColor}: ${identity.colorCode};`);
      colorValue = `var(${customColor})`;
    }
    colors[identity.cookieStoreId] = colorValue;
  });

  return {
    colors,
    colorDeclarations: customColors.length > 0 ? `:root { ${customColors.join('\n')} }` : ''
  };
}

export function getCount() {
  return mContextualIdentities.size;
}

export function forEach(callback) {
  for (const identity of mContextualIdentities.values()) {
    callback(identity);
  }
}

export function startObserve() {
  if (!browser.contextualIdentities)
    return;
  browser.contextualIdentities.onCreated.addListener(onContextualIdentityCreated);
  browser.contextualIdentities.onRemoved.addListener(onContextualIdentityRemoved);
  browser.contextualIdentities.onUpdated.addListener(onContextualIdentityUpdated);
}

export function endObserve() {
  if (!browser.contextualIdentities)
    return;
  browser.contextualIdentities.onCreated.removeListener(onContextualIdentityCreated);
  browser.contextualIdentities.onRemoved.removeListener(onContextualIdentityRemoved);
  browser.contextualIdentities.onUpdated.removeListener(onContextualIdentityUpdated);
}

export async function init() {
  if (!browser.contextualIdentities)
    return;
  const identities = await browser.contextualIdentities.query({}).catch(ApiTabs.createErrorHandler());
  for (const identity of identities) {
    mContextualIdentities.set(identity.cookieStoreId, fixupIcon(identity));
  }
}

function fixupIcon(identity) {
  if (identity.icon && identity.color)
    identity.iconUrl = `/resources/icons/contextual-identities/${identity.icon}.svg#${safeColor(identity.color)}`;
  return identity;
}

const mDarkModeMedia = window.matchMedia('(prefers-color-scheme: dark)');
mDarkModeMedia.addListener(async _event => {
  await init();
  forEach(identity => onContextualIdentityUpdated({ contextualIdentity: identity }));
});

function safeColor(color) {
  switch (color) {
    case 'blue':
    case 'turquoise':
    case 'green':
    case 'yellow':
    case 'orange':
    case 'red':
    case 'pink':
    case 'purple':
      return color;

    case 'toolbar':
    default:
      return !isWindows() && mDarkModeMedia.matches ? 'toolbar-dark' : 'toolbar-light';
  }
}

export const onUpdated = new EventListenerManager();

function onContextualIdentityCreated(createdInfo) {
  const identity = createdInfo.contextualIdentity;
  mContextualIdentities.set(identity.cookieStoreId, fixupIcon(identity));
  onUpdated.dispatch();
}

function onContextualIdentityRemoved(removedInfo) {
  const identity = removedInfo.contextualIdentity;
  delete mContextualIdentities.delete(identity.cookieStoreId);
  onUpdated.dispatch();
}

function onContextualIdentityUpdated(updatedInfo) {
  const identity = updatedInfo.contextualIdentity;
  mContextualIdentities.set(identity.cookieStoreId, fixupIcon(identity));
  onUpdated.dispatch();
}

/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import EventListenerManager from '/extlib/EventListenerManager.js';
import MenuUI from '/extlib/MenuUI.js';

import {
  log as internalLogger,
  wait,
  configs,
  shouldApplyAnimation,
  compareAsNumber,
} from '/common/common.js';
import * as ApiTabs from '/common/api-tabs.js';
import * as Constants from '/common/constants.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TSTAPI from '/common/tst-api.js';

import * as BackgroundConnection from './background-connection.js';
import * as Size from './size.js';

function log(...args) {
  internalLogger('sidebar/subpanel', ...args);
}

export const onResized = new EventListenerManager();

let mTargetWindow;
let mInitialized = false;

const mContainer       = document.querySelector('#subpanel-container');
const mHeader          = document.querySelector('#subpanel-header');
const mSelector        = document.querySelector('#subpanel-selector');
const mSelectorAnchor  = document.querySelector('#subpanel-selector-anchor');
const mToggler         = document.querySelector('#subpanel-toggler');

// Don't put iframe statically, because statically embedded iframe
// produces reflowing on the startup unexpectedly.
const mSubPanel = document.createElement('iframe');
mSubPanel.setAttribute('id', 'subpanel');
mSubPanel.setAttribute('src', 'about:blank');

let mHeight = 0;
let mDragStartY = 0;
let mDragStartHeight = 0;
let mProviderId = null;

updateLayout();

export async function init() {
  if (mInitialized)
    return;
  mInitialized = true;
  mTargetWindow = TabsStore.getCurrentWindowId();

  const [providerId, height] = await Promise.all([
    browser.sessions.getWindowValue(mTargetWindow, Constants.kWINDOW_STATE_SUBPANEL_PROVIDER_ID).catch(ApiTabs.createErrorHandler()),
    browser.sessions.getWindowValue(mTargetWindow, Constants.kWINDOW_STATE_SUBPANEL_HEIGHT).catch(ApiTabs.createErrorHandler())
  ]);
  const providerSpecificHeight = providerId && await browser.sessions.getWindowValue(mTargetWindow, `${Constants.kWINDOW_STATE_SUBPANEL_HEIGHT}:${providerId}`).catch(ApiTabs.createErrorHandler());
  mHeight = (typeof providerSpecificHeight == 'number') ?
    providerSpecificHeight :
    (typeof height == 'number') ?
      height :
      Math.max(configs.lastSubPanelHeight, 0);

  log('initialize ', { providerId, height: mHeight });

  mContainer.appendChild(mSubPanel);

  browser.runtime.onMessage.addListener((message, _sender, _respond) => {
    if (!message ||
        typeof message.type != 'string' ||
        message.type.indexOf('treestyletab:') != 0)
      return;

    //log('onMessage: ', message, sender);
    switch (message.type) {
      case TSTAPI.kCOMMAND_BROADCAST_API_REGISTERED:
        wait(0).then(async () => { // wait until addons are updated
          updateSelector();
          const provider = TSTAPI.getAddon(message.sender.id);
          if (provider &&
              provider.subPanel &&
              (mProviderId == provider.id ||
               provider.newlyInstalled)) {
            if (mHeight == 0)
              mHeight = getDefaultHeight();
            await applyProvider(provider.id);
          }
        });
        break;

      case TSTAPI.kCOMMAND_BROADCAST_API_UNREGISTERED:
        wait(0).then(() => { // wait until addons are updated
          updateSelector();
          if (message.sender.id == mProviderId)
            restoreLastProvider();
        });
        break;
    }
  });

  log('initialize: finish ');
}

TSTAPI.onInitialized.addListener(async () => {
  await init();
  updateSelector();

  const providerId = await browser.sessions.getWindowValue(mTargetWindow, Constants.kWINDOW_STATE_SUBPANEL_PROVIDER_ID).catch(ApiTabs.createErrorHandler());
  if (providerId)
    await applyProvider(providerId);
  else
    restoreLastProvider();
});

function getProviderIconUrl(provider) {
  if (!provider.icons || typeof provider.icons != 'object')
    return null;

  if ('16' in provider.icons)
    return provider.icons['16'];

  const sizes = Object.keys(provider.icons, size => parseInt(size)).sort(compareAsNumber);
  if (sizes.length == 0)
    return null;

  // find a size most nearest to 16
  sizes.sort((a, b) => {
    if (a < 16) {
      if (b >= 16)
        return 1;
      else
        return b - a;
    }
    if (b < 16) {
      if (a >= 16)
        return -1;
      else
        return b - a;
    }
    return a - b;
  });
  return provider.icons[sizes[0]];
}

async function applyProvider(id) {
  const provider = TSTAPI.getAddon(id);
  log('applyProvider ', id, provider);
  if (provider &&
      provider.subPanel) {
    log('applyProvider: load ', id);
    configs.lastSelectedSubPanelProviderId = mProviderId = id;
    const lastHeight = await browser.sessions.getWindowValue(mTargetWindow, `${Constants.kWINDOW_STATE_SUBPANEL_HEIGHT}:${id}`).catch(ApiTabs.createErrorHandler());
    for (const item of mSelector.querySelectorAll('.radio')) {
      item.classList.remove('checked');
    }
    const activeItem = mSelector.querySelector(`[data-value="${id}"]`);
    if (activeItem)
      activeItem.classList.add('checked');
    browser.sessions.setWindowValue(mTargetWindow, Constants.kWINDOW_STATE_SUBPANEL_PROVIDER_ID, id).catch(ApiTabs.createErrorHandler());

    const icon = mSelectorAnchor.querySelector('.icon > img');
    const iconUrl = getProviderIconUrl(provider);
    if (iconUrl)
      icon.src = iconUrl;
    else
      icon.removeAttribute('src');

    mSelectorAnchor.querySelector('.label').textContent = provider.subPanel.title || provider.name || provider.id;

    if ('fixedHeight' in provider.subPanel) {
      if (typeof provider.subPanel.fixedHeight == 'number')
        mHeight = provider.subPanel.fixedHeight;
      else
        mHeight = Size.calc(provider.subPanel.fixedHeight);
      mHeader.classList.remove('resizable');
    }
    else {
      mHeader.classList.add('resizable');
      if (typeof lastHeight == 'number') {
        mHeight = lastHeight;
      }
      else if ('initialHeight' in provider.subPanel) {
        if (typeof provider.subPanel.initialHeight == 'number')
          mHeight = provider.subPanel.initialHeight;
        else
          mHeight = Size.calc(provider.subPanel.initialHeight);
      }
    }

    const url = new URL(provider.subPanel.url);
    url.searchParams.set('windowId', mTargetWindow);
    provider.subPanel.url = url.href;

    if (mHeight > 0)
      load(provider.subPanel);
    else
      load();
  }
  else {
    log('applyProvider: unload missing/invalid provider ', id);
    const icon = mSelectorAnchor.querySelector('.icon > img');
    icon.removeAttribute('src');
    mSelectorAnchor.querySelector('.label').textContent = '';
    mHeader.classList.add('resizable');
    load();
  }
}

async function restoreLastProvider() {
  const lastProvider = TSTAPI.getAddon(configs.lastSelectedSubPanelProviderId);
  log('restoreLastProvider ', lastProvider);
  if (lastProvider && lastProvider.subPanel)
    await applyProvider(lastProvider.id);
  else if (mSelector.hasChildNodes())
    await applyProvider(mSelector.firstChild.dataset.value);
  else
    await applyProvider(mProviderId = null);
}

function getDefaultHeight() {
  return Math.floor(window.innerHeight * 0.5);
}

async function load(params) {
  params = params || {};
  const url = params.url || 'about:blank';
  if (url == mSubPanel.src &&
      url != 'about:blank') {
    mSubPanel.src = 'about:blank?'; // force reload
    await wait(0);
  }
  mSubPanel.src = url;
  updateLayout();
}

function updateLayout() {
  if (!mProviderId && !mSelector.hasChildNodes()) {
    mContainer.classList.add('collapsed');
    document.documentElement.style.setProperty('--subpanel-area-size', '0px');
  }
  else {
    mHeight = Math.max(0, mHeight);
    mContainer.classList.toggle('collapsed', mHeight == 0);
    const headerSize = mHeader.getBoundingClientRect().height;
    const maxHeight = window.innerHeight * Math.max(0, Math.min(1, configs.maxSubPanelSizeRatio));
    const appliedHeight = Math.min(maxHeight, mHeight);
    document.documentElement.style.setProperty('--subpanel-content-size', `${appliedHeight}px`);
    document.documentElement.style.setProperty('--subpanel-area-size', `${appliedHeight + headerSize}px`);

    if (mHeight > 0 &&
        (!mSubPanel.src || mSubPanel.src == 'about:blank')) {
      // delayed load
      const provider = TSTAPI.getAddon(mProviderId);
      if (provider && provider.subPanel)
        mSubPanel.src = provider.subPanel.url;
    }
    else if (mHeight == 0) {
      mSubPanel.src = 'about:blank';
    }
  }

  if (!mInitialized)
    return;

  onResized.dispatch();

  if (mProviderId) {
    browser.sessions.setWindowValue(mTargetWindow, Constants.kWINDOW_STATE_SUBPANEL_HEIGHT, mHeight).catch(ApiTabs.createErrorHandler());
    browser.sessions.setWindowValue(mTargetWindow, `${Constants.kWINDOW_STATE_SUBPANEL_HEIGHT}:${mProviderId}`, mHeight).catch(ApiTabs.createErrorHandler());
  }
}

async function toggle() {
  const lastEffectiveHeight = await browser.sessions.getWindowValue(mTargetWindow, Constants.kWINDOW_STATE_SUBPANEL_EFFECTIVE_HEIGHT).catch(ApiTabs.createErrorHandler());
  if (mHeight > 0)
    browser.sessions.setWindowValue(mTargetWindow, Constants.kWINDOW_STATE_SUBPANEL_EFFECTIVE_HEIGHT, mHeight).catch(ApiTabs.createErrorHandler());
  mHeight = mHeight > 0 ? 0 : (lastEffectiveHeight || getDefaultHeight());
  updateLayout();
}

// We should save the last height only when it is changed by the user intentonally.
function saveLastHeight() {
  configs.lastSubPanelHeight = mContainer.classList.contains('collapsed') ? 0 : mHeight;
}

function isFiredOnClickable(event) {
  let target = event.target;
  if (!(target instanceof Element))
    target = target.parentNode;
  return !!target.closest('.clickable');
}

function isResizable() {
  const provider = mProviderId && TSTAPI.getAddon(mProviderId);
  return !provider || !provider.subPanel || !('fixedHeight' in provider.subPanel);
}

mHeader.addEventListener('mousedown', event => {
  if (isFiredOnClickable(event))
    return;
  event.stopPropagation();
  event.preventDefault();
  mHeader.setCapture(true);
  mDragStartY = event.clientY;
  mDragStartHeight = mHeight;
  mHeader.addEventListener('mousemove', onMouseMove);
});

mHeader.addEventListener('mouseup', event => {
  if (isFiredOnClickable(event))
    return;
  mHeader.removeEventListener('mousemove', onMouseMove);
  event.stopPropagation();
  event.preventDefault();
  document.releaseCapture();
  if (!isResizable())
    return;
  mHeight = mDragStartHeight - (event.clientY - mDragStartY);
  updateLayout();
  saveLastHeight();
});

mHeader.addEventListener('dblclick', async event => {
  if (isFiredOnClickable(event))
    return;
  event.stopPropagation();
  event.preventDefault();
  await toggle();
  saveLastHeight();
});

mToggler.addEventListener('click', async event => {
  event.stopPropagation();
  event.preventDefault();
  await toggle();
  saveLastHeight();
});

window.addEventListener('resize', _event => {
  updateLayout();
});

function onMouseMove(event) {
  event.stopPropagation();
  event.preventDefault();
  if (!isResizable())
    return;
  mHeight = mDragStartHeight - (event.clientY - mDragStartY);
  updateLayout();
}


function updateSelector() {
  log('updateSelector start');
  const range = document.createRange();
  range.selectNodeContents(mSelector);
  range.deleteContents();

  const items = [];
  for (const [id, addon] of TSTAPI.getAddons()) {
    if (!addon.subPanel)
      continue;
    log('  subpanel provider detected: ', addon);
    const item = document.createElement('li');
    item.classList.add('radio');
    item.dataset.value = id;
    const iconContainer = item.appendChild(document.createElement('span'));
    iconContainer.classList.add('icon');
    const icon = iconContainer.appendChild(document.createElement('img'));
    const url = getProviderIconUrl(addon);
    if (url)
      icon.src = url;
    item.appendChild(document.createTextNode(addon.subPanel.title || addon.name || id));
    items.push(item);
  }

  items.sort((a, b) => a.textContent < b.textContent ? -1 : 1);

  const itemsFragment = document.createDocumentFragment();
  for (const item of items) {
    itemsFragment.appendChild(item);
  }
  range.insertNode(itemsFragment);
  range.detach();
  log('updateSelector end');
}

mSelector.ui = new MenuUI({
  root:       mSelector,
  appearance: 'panel',
  onCommand:  onSelect,
  animationDuration: shouldApplyAnimation() ? configs.collapseDuration : 0.001
});

async function onSelect(item, _event) {
  if (item.dataset.value) {
    await applyProvider(item.dataset.value);
    saveLastHeight();
  }
  mSelector.ui.close();
}

BackgroundConnection.onMessage.addListener(async message => {
  switch (message.type) {
    case Constants.kCOMMAND_TOGGLE_SUBPANEL:
      toggle();
      break;

    case Constants.kCOMMAND_SWITCH_SUBPANEL:
      mSelector.ui.open({ anchor: mSelectorAnchor });
      break;

    case Constants.kCOMMAND_INCREASE_SUBPANEL:
      mHeight += Size.getTabHeight();
      updateLayout();
      break;

    case Constants.kCOMMAND_DECREASE_SUBPANEL:
      mHeight -= Size.getTabHeight();
      updateLayout();
      break;
  }
});

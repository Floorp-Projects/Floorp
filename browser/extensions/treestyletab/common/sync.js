/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  configs,
  notify,
  wait,
  getChunkedConfig,
  setChunkedConfig,
  isLinux,
} from './common.js';
import * as Constants from '/common/constants.js';
import EventListenerManager from '/extlib/EventListenerManager.js';

function log(...args) {
  internalLogger('common/sync', ...args);
}

export const onMessage = new EventListenerManager();
export const onNewDevice = new EventListenerManager();
export const onUpdatedDevice = new EventListenerManager();
export const onObsoleteDevice = new EventListenerManager();

let mMyDeviceInfo = null;

async function getMyDeviceInfo() {
  if (!configs.syncDeviceInfo || !configs.syncDeviceInfo.id) {
    const newDeviceInfo = await generateDeviceInfo();
    if (!configs.syncDeviceInfo)
      configs.syncDeviceInfo = newDeviceInfo;
  }
  return mMyDeviceInfo = configs.syncDeviceInfo;
}

async function ensureDeviceInfoInitialized() {
  await getMyDeviceInfo();
}

export async function waitUntilDeviceInfoInitialized() {
  while (!configs.syncDeviceInfo) {
    await wait(100);
  }
  mMyDeviceInfo = configs.syncDeviceInfo;
}

configs.$loaded.then(async () => {
  await ensureDeviceInfoInitialized();
});

export async function init() {
  await updateSelf();
  await updateDevices();
  reserveToReceiveMessage();
  window.setInterval(updateSelf, 1000 * 60 * 60 * 24); // update info every day!

  configs.$addObserver(key => {
    switch (key) {
      case 'syncOtherDevicesDetected':
        if (!configs.syncAvailableNotified) {
          configs.syncAvailableNotified = true;
          notify({
            title:   browser.i18n.getMessage('syncAvailable_notification_title'),
            message: browser.i18n.getMessage(`syncAvailable_notification_message${isLinux() ? '_linux' : ''}`),
            url:     `${Constants.kSHORTHAND_URIS.options}#syncTabsToDeviceOptions`,
            timeout: configs.syncAvailableNotificationTimeout
          });
        }
        return;

      case 'syncDevices':
        // This may happen when all configs are reset.
        // We need to try updating devices after syncDeviceInfo is completely cleared.
        wait(100).then(updateDevices);
        break;

      case 'syncDeviceInfo':
        mMyDeviceInfo = null;
        updateSelf();
        break;

      default:
        if (key.startsWith('chunkedSyncData'))
          reserveToReceiveMessage();
        break;
    }
  });
}

export async function generateDeviceInfo({ name, icon } = {}) {
  const [platformInfo, browserInfo] = await Promise.all([
    browser.runtime.getPlatformInfo(),
    browser.runtime.getBrowserInfo()
  ]);
  return {
    id:   `device-${Date.now()}-${Math.round(Math.random() * 65000)}`,
    name: name === undefined ?
      browser.i18n.getMessage('syncDeviceDefaultName', [toHumanReadableOSName(platformInfo.os), browserInfo.name]) :
      (name || null),
    icon: icon || 'device-desktop'
  };
}

// https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/PlatformOs
function toHumanReadableOSName(os) {
  switch (os) {
    case 'mac': return 'macOS';
    case 'win': return 'Windows';
    case 'android': return 'Android';
    case 'cros': return 'Chrome OS';
    case 'linux': return 'Linux';
    case 'openbsd': return 'Open/FreeBSD';
    default: return 'Unknown Platform';
  }
}

configs.$addObserver(key => {
  switch (key) {
    case 'syncUnsendableUrlPattern':
      isSendableTab.unsendableUrlMatcher = null;
      break;

    default:
      break;
  }
});

async function updateSelf() {
  if (updateSelf.updating)
    return;

  updateSelf.updating = true;

  await ensureDeviceInfoInitialized();
  configs.syncDeviceInfo = mMyDeviceInfo = {
    ...clone(configs.syncDeviceInfo),
    timestamp: Date.now()
  };

  await updateDevices();

  setTimeout(() => {
    updateSelf.updating = false;
  }, 250);
}

async function updateDevices() {
  if (updateDevices.updating)
    return;
  updateDevices.updating = true;
  await waitUntilDeviceInfoInitialized();

  const remote = clone(configs.syncDevices);
  const local  = clone(configs.syncDevicesLocalCache);
  log('devices updated: ', local, remote);
  for (const [id, info] of Object.entries(remote)) {
    if (id == mMyDeviceInfo.id)
      continue;
    local[id] = info;
    if (id in local) {
      log('updated device: ', info);
      onUpdatedDevice.dispatch(info);
    }
    else {
      log('new device: ', info);
      onNewDevice.dispatch(info);
    }
  }

  for (const [id, info] of Object.entries(local)) {
    if (id in remote ||
        id == mMyDeviceInfo.id)
      continue;
    log('obsolete device: ', info);
    delete local[id];
    onObsoleteDevice.dispatch(info);
  }

  if (configs.syncDeviceExpirationDays > 0) {
    const expireDateInSeconds = Date.now() - (1000 * 60 * 60 * configs.syncDeviceExpirationDays);
    for (const [id, info] of Object.entries(local)) {
      if (info &&
          info.timestamp < expireDateInSeconds) {
        delete local[id];
        log('expired device: ', info);
        onObsoleteDevice.dispatch(info);
      }
    }
  }

  local[mMyDeviceInfo.id] = clone(mMyDeviceInfo);
  log('store myself: ', mMyDeviceInfo, local[mMyDeviceInfo.id]);

  if (!configs.syncOtherDevicesDetected && Object.keys(local).length > 1)
    configs.syncOtherDevicesDetected = true;

  configs.syncDevices = local;
  configs.syncDevicesLocalCache = clone(local);
  setTimeout(() => {
    updateDevices.updating = false;
  }, 250);
}

function reserveToReceiveMessage() {
  if (reserveToReceiveMessage.reserved)
    clearTimeout(reserveToReceiveMessage.reserved);
  reserveToReceiveMessage.reserved = setTimeout(() => {
    delete reserveToReceiveMessage.reserved;
    receiveMessage();
  }, 250);
}

async function receiveMessage() {
  const myDeviceInfo = await getMyDeviceInfo();
  try {
    const messages = readMessages();
    log('receiveMessage: queued messages => ', messages);
    const restMessages = messages.filter(message => {
      if (message.timestamp <= configs.syncLastMessageTimestamp)
        return false;
      if (message.to == myDeviceInfo.id) {
        log('receiveMessage receive: ', message);
        configs.syncLastMessageTimestamp = message.timestamp;
        onMessage.dispatch(message);
        return false;
      }
      return true;
    });
    log('receiveMessage: restMessages => ', restMessages);
    if (restMessages.length != messages.length)
      writeMessages(restMessages);
  }
  catch(error) {
    log('receiveMessage fatal error: ', error);
    writeMessages([]);
  }
}

export async function sendMessage(to, data) {
  const myDeviceInfo = await getMyDeviceInfo();
  try {
    const messages = readMessages();
    messages.push({
      timestamp: Date.now(),
      from:      myDeviceInfo.id,
      to,
      data
    });
    log('sendMessage: queued messages => ', messages);
    writeMessages(messages);
  }
  catch(error) {
    console.log('Sync.sendMessage: failed to send message ', error);
    writeMessages([]);
  }
}

function readMessages() {
  try {
    return uniqMessages([
      ...JSON.parse(getChunkedConfig('chunkedSyncDataLocal') || '[]'),
      ...JSON.parse(getChunkedConfig('chunkedSyncData') || '[]')
    ]);
  }
  catch(error) {
    log('failed to read messages: ', error);
    return [];
  }
}

function writeMessages(messages) {
  const stringified = JSON.stringify(messages || []);
  setChunkedConfig('chunkedSyncDataLocal', stringified);
  setChunkedConfig('chunkedSyncData', stringified);
}

function uniqMessages(messages) {
  const knownMessages = new Set();
  return messages.filter(message => {
    const key = JSON.stringify(message);
    if (knownMessages.has(key))
      return false;
    knownMessages.add(key);
    return true;
  });
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

export function getOtherDevices() {
  if (!mMyDeviceInfo)
    throw new Error('Not initialized yet. You need to call this after the device info is initialized.');
  const devices = configs.syncDevices || {};
  const result = [];
  for (const [id, info] of Object.entries(devices)) {
    if (id == mMyDeviceInfo.id ||
        !info.id /* ignore invalid device info accidentally saved (see also https://github.com/piroor/treestyletab/issues/2922 ) */)
      continue;
    result.push(info);
  }
  return result.sort((a, b) => a.name > b.name);
}

export function getDeviceName(id) {
  const devices = configs.syncDevices || {};
  if (!(id in devices) || !devices[id])
    return browser.i18n.getMessage('syncDeviceUnknownDevice');
  return String(devices[id].name || '').trim() || browser.i18n.getMessage('syncDeviceMissingDeviceName');
}

// https://searchfox.org/mozilla-central/rev/d866b96d74ec2a63f09ee418f048d23f4fd379a2/browser/base/content/browser-sync.js#1176
export function isSendableTab(tab) {
  if (!tab.url ||
      tab.url.length > 65535)
    return false;

  if (!isSendableTab.unsendableUrlMatcher)
    isSendableTab.unsendableUrlMatcher = new RegExp(configs.syncUnsendableUrlPattern);
  return !isSendableTab.unsendableUrlMatcher.test(tab.url);
}

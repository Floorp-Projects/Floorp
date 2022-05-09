/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  log as internalLogger,
  mapAndFilterUniq,
  configs
} from '/common/common.js';
import * as Constants from '/common/constants.js';
import * as TabsStore from '/common/tabs-store.js';
import * as TSTAPI from '/common/tst-api.js';

import EventListenerManager from '/extlib/EventListenerManager.js';

function log(...args) {
  internalLogger('sidebar/background-connection', ...args);
}

export const onMessage = new EventListenerManager();

let mConnectionPort = null;
let mHeartbeatTimer = null;

export function connect() {
  if (mConnectionPort)
    return;
  const type = /windowId=([1-9][0-9]*)/i.test(location.search) ? 'unknown' : 'sidebar';
  mConnectionPort = browser.runtime.connect({
    name: `${Constants.kCOMMAND_REQUEST_CONNECT_PREFIX}${TabsStore.getCurrentWindowId()}:${type}`
  });
  mConnectionPort.onMessage.addListener(onConnectionMessage);
  mConnectionPort.onDisconnect.addListener(() => {
    log(`Disconnected accidentally: try to reconnect.`);
    location.reload();
  });
  if (mHeartbeatTimer)
    clearInterval(mHeartbeatTimer);
  mHeartbeatTimer = setInterval(() => {
    sendMessage({
      type: Constants.kCOMMAND_HEARTBEAT
    });
  }, configs.heartbeatInterval);
}

let mPromisedStartedResolver;
let mPromisedStarted = new Promise((resolve, _reject) => {
  mPromisedStartedResolver = resolve;
});

export function start() {
  if (!mPromisedStartedResolver)
    return;
  mPromisedStartedResolver();
  mPromisedStartedResolver = undefined;
  mPromisedStarted = undefined;
}

export const counts = {};

let mReservedMessages = [];
let mOnFrame;

export function sendMessage(message) {
  if (configs.loggingConnectionMessages) {
    counts[message.type] = counts[message.type] || 0;
    counts[message.type]++;
  }
  // Se should not send messages immediately, instead we should throttle
  // it and bulk-send multiple messages, for better user experience.
  // Sending too much messages in one event loop may block everything
  // and makes Firefox like frozen.
  //mConnectionPort.postMessage(message);
  mReservedMessages.push(message);
  if (!mOnFrame) {
    mOnFrame = () => {
      mOnFrame = null;
      const messages = mReservedMessages;
      mReservedMessages = [];
      mConnectionPort.postMessage(messages);
      if (configs.debug) {
        const types = mapAndFilterUniq(messages,
                                       message => message.type || undefined).join(', ');
        log(`${messages.length} messages sent (${types}):`, messages);
      }
    };
    // Because sidebar is always visible, we may not need to avoid using
    // window.requestAnimationFrame. I just use a timer instead just for
    // a unity with common/sidebar-connection.js.
    //window.requestAnimationFrame(mOnFrame);
    setTimeout(mOnFrame, 0);
  }
}

async function onConnectionMessage(message) {
  if (Array.isArray(message))
    return message.forEach(onConnectionMessage);

  switch (message.type) {
    case 'echo': // for testing
      mConnectionPort.postMessage(message);
      break;

    case 'external':
      TSTAPI.onMessageExternal.dispatch(message.message, message.sender);
      break;

    default:
      if (mPromisedStarted)
        await mPromisedStarted;
      onMessage.dispatch(message);
      break;
  }
}


// Mechanism to apply only most recently notified message.
// See also: https://github.com/piroor/treestyletab/issues/2568#issuecomment-657188062

const mBufferedMessages = new Map();

export function handleBufferedMessage(message, key) {
  const bufferKey = `${message.type}:${key}`;
  const hasLastMessage = mBufferedMessages.has(bufferKey);
  mBufferedMessages.set(bufferKey, message);
  return hasLastMessage;
}

export function fetchBufferedMessage(type, key) {
  const bufferKey = `${type}:${key}`;
  const message = mBufferedMessages.get(bufferKey);
  mBufferedMessages.delete(bufferKey);
  return message;
}


//===================================================================
// Logging
//===================================================================

browser.runtime.onMessage.addListener((message, _sender) => {
  if (!message ||
      typeof message != 'object' ||
      message.type != Constants.kCOMMAND_REQUEST_CONNECTION_MESSAGE_LOGS)
    return;

  browser.runtime.sendMessage({
    type: Constants.kCOMMAND_RESPONSE_CONNECTION_MESSAGE_LOGS,
    logs: JSON.parse(JSON.stringify(counts)),
    windowId: TabsStore.getCurrentWindowId()
  });
});

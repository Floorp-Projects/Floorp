/*
 * Copyright 2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

Components.utils.import('resource://gre/modules/Services.jsm');
Components.utils.import('chrome://shumway/content/SpecialInflate.jsm');
Components.utils.import('chrome://shumway/content/RtmpUtils.jsm');

var externalInterfaceWrapper = {
  callback: function (call) {
    if (!shumwayComAdapter.onExternalCallback) {
      return undefined;
    }
    return shumwayComAdapter.onExternalCallback(
      Components.utils.cloneInto(JSON.parse(call), content));
  }
};

// The object allows resending of external interface, clipboard and other
// control messages between unprivileged content and ShumwayStreamConverter.
var shumwayComAdapter;

function sendMessage(action, data, sync, callbackCookie) {
  var detail = {action: action, data: data, sync: sync};
  if (callbackCookie !== undefined) {
    detail.callback = true;
    detail.cookie = callbackCookie;
  }
  if (!sync) {
    sendAsyncMessage('Shumway:message', detail);
    return;
  }
  var result = sendSyncMessage('Shumway:message', detail);
  return Components.utils.cloneInto(result, content);
}

addMessageListener('Shumway:init', function (message) {
  sendAsyncMessage('Shumway:running', {}, {
    externalInterface: externalInterfaceWrapper
  });

  // Exposing ShumwayCom object/adapter to the unprivileged content -- setting
  // up Xray wrappers.
  shumwayComAdapter = Components.utils.createObjectIn(content, {defineAs: 'ShumwayCom'});
  Components.utils.exportFunction(sendMessage, shumwayComAdapter, {defineAs: 'sendMessage'});
  Object.defineProperties(shumwayComAdapter, {
    onLoadFileCallback: { value: null, writable: true },
    onExternalCallback: { value: null, writable: true },
    onMessageCallback: { value: null, writable: true }
  });
  Components.utils.makeObjectPropsNormal(shumwayComAdapter);

  // Exposing createSpecialInflate function for DEFLATE stream decoding using
  // Gecko API.
  if (SpecialInflateUtils.isSpecialInflateEnabled) {
    Components.utils.exportFunction(function () {
      return SpecialInflateUtils.createWrappedSpecialInflate(content);
    }, content, {defineAs: 'createSpecialInflate'});
  }

  if (RtmpUtils.isRtmpEnabled) {
    Components.utils.exportFunction(function (params) {
      return RtmpUtils.createSocket(content, params);
    }, content, {defineAs: 'createRtmpSocket'});
    Components.utils.exportFunction(function () {
      return RtmpUtils.createXHR(content);
    }, content, {defineAs: 'createRtmpXHR'});
  }

  content.wrappedJSObject.runViewer();
});

addMessageListener('Shumway:loadFile', function (message) {
  if (!shumwayComAdapter.onLoadFileCallback) {
    return;
  }
  shumwayComAdapter.onLoadFileCallback(Components.utils.cloneInto(message.data, content));
});

addMessageListener('Shumway:messageCallback', function (message) {
  if (!shumwayComAdapter.onMessageCallback) {
    return;
  }
  shumwayComAdapter.onMessageCallback(message.data.cookie,
    Components.utils.cloneInto(message.data.response, content));
});

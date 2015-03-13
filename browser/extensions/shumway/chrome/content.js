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
Components.utils.import('chrome://shumway/content/ShumwayCom.jsm');

var externalInterfaceWrapper = {
  callback: function (call) {
    if (!shumwayComAdapterHooks.onExternalCallback) {
      return undefined;
    }
    return shumwayComAdapterHooks.onExternalCallback(
      Components.utils.cloneInto(JSON.parse(call), content));
  }
};

var shumwayComAdapterHooks = {};

function sendMessage(action, data, sync) {
  var detail = {action: action, data: data, sync: sync};
  if (!sync) {
    sendAsyncMessage('Shumway:message', detail);
    return;
  }
  var result = String(sendSyncMessage('Shumway:message', detail));
  result = result == 'undefined' ? undefined : JSON.parse(result);
  return Components.utils.cloneInto(result, content);
}

function enableDebug() {
  sendAsyncMessage('Shumway:enableDebug', null);
}

addMessageListener('Shumway:init', function (message) {
  var environment = message.data;

  sendAsyncMessage('Shumway:running', {}, {
    externalInterface: externalInterfaceWrapper
  });

  ShumwayCom.createAdapter(content.wrappedJSObject, {
    sendMessage: sendMessage,
    enableDebug: enableDebug,
    getEnvironment: function () { return environment; }
  }, shumwayComAdapterHooks);

  content.wrappedJSObject.runViewer();
});

addMessageListener('Shumway:loadFile', function (message) {
  if (!shumwayComAdapterHooks.onLoadFileCallback) {
    return;
  }
  shumwayComAdapterHooks.onLoadFileCallback(Components.utils.cloneInto(message.data, content));
});

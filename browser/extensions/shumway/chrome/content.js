/*
 * Copyright 2015 Mozilla Foundation
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
Components.utils.import('resource://gre/modules/Promise.jsm');

Components.utils.import('chrome://shumway/content/ShumwayCom.jsm');

var messageManager, viewerReady;
// Checking if we loading content.js in the OOP/mozbrowser or jsplugins.
// TODO remove mozbrowser logic when we switch to jsplugins only support
if (typeof document === 'undefined') { // mozbrowser OOP frame script
  messageManager = this;
  viewerReady = Promise.resolve(content);
  messageManager.sendAsyncMessage('Shumway:constructed', null);
} else { // jsplugins instance
  messageManager = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                .getInterface(Components.interfaces.nsIDocShell)
                .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                .getInterface(Components.interfaces.nsIContentFrameMessageManager);

  var viewer = document.getElementById('viewer');
  viewerReady = new Promise(function (resolve) {
    viewer.addEventListener('load', function () {
      messageManager.sendAsyncMessage('Shumway:constructed', null);
      resolve(viewer.contentWindow);
    });
  });
}


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
    messageManager.sendAsyncMessage('Shumway:message', detail);
    return;
  }
  var result = String(messageManager.sendSyncMessage('Shumway:message', detail));
  result = result == 'undefined' ? undefined : JSON.parse(result);
  return Components.utils.cloneInto(result, content);
}

function enableDebug() {
  messageManager.sendAsyncMessage('Shumway:enableDebug', null);
}

messageManager.addMessageListener('Shumway:init', function (message) {
  var environment = message.data;

  messageManager.sendAsyncMessage('Shumway:running', {}, {
    externalInterface: externalInterfaceWrapper
  });

  viewerReady.then(function (viewerWindow) {
    ShumwayCom.createAdapter(viewerWindow.wrappedJSObject, {
      sendMessage: sendMessage,
      enableDebug: enableDebug,
      getEnvironment: function () { return environment; }
    }, shumwayComAdapterHooks);

    viewerWindow.wrappedJSObject.runViewer();
  });
});

messageManager.addMessageListener('Shumway:loadFile', function (message) {
  if (!shumwayComAdapterHooks.onLoadFileCallback) {
    return;
  }
  shumwayComAdapterHooks.onLoadFileCallback(Components.utils.cloneInto(message.data, content));
});

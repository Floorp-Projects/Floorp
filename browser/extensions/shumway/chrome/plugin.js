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

Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');
Components.utils.import('resource://gre/modules/Services.jsm');
Components.utils.import('resource://gre/modules/Promise.jsm');

Components.utils.import('chrome://shumway/content/StartupInfo.jsm');
Components.utils.import('chrome://shumway/content/ShumwayCom.jsm');

XPCOMUtils.defineLazyModuleGetter(this, 'PrivateBrowsingUtils',
  'resource://gre/modules/PrivateBrowsingUtils.jsm');

function log(str) {
  var msg = 'plugin.js: ' + str;
  Services.console.logStringMessage(msg);
  dump(msg + '\n');
}

function runViewer() {
  function handlerOOP() {
    var frameLoader = pluginElement.frameLoader;
    var messageManager = frameLoader.messageManager;

    var externalInterface;

    messageManager.addMessageListener('Shumway:running', function (message) {
      externalInterface = message.objects.externalInterface;
    });

    messageManager.addMessageListener('Shumway:message', function (message) {
      var data = message.data;
      var result = shumwayActions.invoke(data.action, data.data);
      if (message.sync) {
        return result === undefined ? 'undefined' : JSON.stringify(result);
      }
    });

    messageManager.addMessageListener('Shumway:enableDebug', function (message) {
      enableDebug();
    });

    shumwayActions.onExternalCallback = function (call) {
      return externalInterface.callback(JSON.stringify(call));
    };

    shumwayActions.onLoadFileCallback = function (args) {
      messageManager.sendAsyncMessage('Shumway:loadFile', args);
    };

    messageManager.addMessageListener('Shumway:constructed', function (message) {
      messageManager.sendAsyncMessage('Shumway:init', getEnvironment());
    });
  }

  function getEnvironment() {
    return {
      swfUrl: startupInfo.url,
      privateBrowsing: startupInfo.privateBrowsing
    };
  }

  function enableDebug() {
    DebugUtils.enableDebug(startupInfo.url);
    setTimeout(function () {
      // TODO fix plugin instance reloading for jsplugins
    }, 1000);
  }

  var startupInfo = getStartupInfo(pluginElement);
  if (!startupInfo.url) {
    // Special case when movie URL is not specified, e.g. swfobject
    // checks only version. No need to instantiate the flash plugin.
    if (startupInfo.embedTag) {
      setupSimpleExternalInterface(startupInfo.embedTag);
    }
    return;
  }

  var document = pluginElement.ownerDocument;
  var window = document.defaultView;
  var shumwayActions = ShumwayCom.createActions(startupInfo, window, document);

  handlerOOP();

  // TODO fix remote debugging for jsplugins
}

function setupSimpleExternalInterface(embedTag) {
  Components.utils.exportFunction(function (variable) {
    switch (variable) {
      case '$version':
        return 'SHUMWAY 10,0,0';
      default:
        log('Unsupported GetVariable() call: ' + variable);
        return undefined;
    }
  }, embedTag.wrappedJSObject, {defineAs: 'GetVariable'});
}

runViewer();


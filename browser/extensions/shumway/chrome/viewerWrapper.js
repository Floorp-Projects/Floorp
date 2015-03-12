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

var viewer = document.getElementById('viewer'), onLoaded;
var promise = new Promise(function (resolve) {
  onLoaded = resolve;
});
viewer.addEventListener('load', function () {
  onLoaded(false);
});
viewer.addEventListener('mozbrowserloadend', function () {
  onLoaded(true);
});

Components.utils.import('chrome://shumway/content/ShumwayCom.jsm');

function runViewer() {
  function handler() {
    function sendMessage(action, data, sync) {
      var result = shumwayActions.invoke(action, data);
      return Components.utils.cloneInto(result, childWindow);
    }

    var childWindow = viewer.contentWindow.wrappedJSObject;

    var shumwayComAdapterHooks = {};
    ShumwayCom.createAdapter(childWindow, {
      sendMessage: sendMessage,
      enableDebug: enableDebug,
      getEnvironment: getEnvironment,
    }, shumwayComAdapterHooks);

    shumwayActions.onExternalCallback = function (call) {
      return shumwayComAdapterHooks.onExternalCallback(Components.utils.cloneInto(call, childWindow));
    };

    shumwayActions.onLoadFileCallback = function (args) {
      shumwayComAdapterHooks.onLoadFileCallback(Components.utils.cloneInto(args, childWindow));
    };

    childWindow.runViewer();
  }

  function handlerOOP() {
    var frameLoader = viewer.QueryInterface(Components.interfaces.nsIFrameLoaderOwner).frameLoader;
    var messageManager = frameLoader.messageManager;
    messageManager.loadFrameScript('chrome://shumway/content/content.js', false);

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

    messageManager.sendAsyncMessage('Shumway:init', getEnvironment());
  }


  function handleDebug(connection) {
    viewer.parentNode.removeChild(viewer); // we don't need viewer anymore
    document.body.className = 'remoteDebug';

    function sendMessage(data) {
      return shumwayActions.invoke(data.id, data.data);
    }

    connection.onData = function (data) {
      switch (data.action) {
        case 'sendMessage':
          return sendMessage(data);
        case 'reload':
          document.body.className = 'remoteReload';
          setTimeout(function () {
            window.top.location.reload();
          }, 1000);
          return;
      }
    };

    shumwayActions.onExternalCallback = function (call) {
      return connection.send({action: 'onExternalCallback', detail: call});
    };

    shumwayActions.onLoadFileCallback = function (args) {
      if (args.array) {
        args.array = Array.prototype.slice.call(args.array, 0);
      }
      return connection.send({action: 'onLoadFileCallback', detail: args}, true);
    };

    connection.send({action: 'runViewer'}, true);
  }

  function getEnvironment() {
    return {
      swfUrl: window.shumwayStartupInfo.url,
      privateBrowsing: window.shumwayStartupInfo.privateBrowsing
    };
  }

  function enableDebug() {
    DebugUtils.enableDebug(window.shumwayStartupInfo.url);
    setTimeout(function () {
      window.top.location.reload();
    }, 1000);
  }

  var startupInfo = window.shumwayStartupInfo;
  var shumwayActions = ShumwayCom.createActions(startupInfo, window, document);

  promise.then(function (oop) {
    if (DebugUtils.isEnabled) {
      DebugUtils.createDebuggerConnection(window.shumwayStartupInfo.url).then(
          function (debuggerConnection) {
        if (debuggerConnection) {
          handleDebug(debuggerConnection);
        } else if (oop) {
          handlerOOP();
        } else {
          handler();
        }
      });
      return;
    }

    if (oop) {
      handlerOOP();
    } else {
      handler();
    }
  });
}

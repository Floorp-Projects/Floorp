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

window.notifyShumwayMessage = function (detail) { };
window.onExternalCallback = null;
window.onMessageCallback = null;
window.onLoadFileCallback = null;

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

Components.utils.import('chrome://shumway/content/SpecialInflate.jsm');
Components.utils.import('chrome://shumway/content/RtmpUtils.jsm');

function runViewer() {
  function handler() {
    function sendMessage(action, data, sync, callbackCookie) {
      var detail = {action: action, data: data, sync: sync};
      if (callbackCookie !== undefined) {
        detail.callback = true;
        detail.cookie = callbackCookie;
      }
      var result = window.notifyShumwayMessage(detail);
      return Components.utils.cloneInto(result, childWindow);
    }

    var childWindow = viewer.contentWindow.wrappedJSObject;

    // Exposing ShumwayCom object/adapter to the unprivileged content -- setting
    // up Xray wrappers. This allows resending of external interface, clipboard
    // and other control messages between unprivileged content and
    // ShumwayStreamConverter.
    var shumwayComAdapter = Components.utils.createObjectIn(childWindow, {defineAs: 'ShumwayCom'});
    Components.utils.exportFunction(sendMessage, shumwayComAdapter, {defineAs: 'sendMessage'});
    Components.utils.exportFunction(enableDebug, shumwayComAdapter, {defineAs: 'enableDebug'});
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
        return SpecialInflateUtils.createWrappedSpecialInflate(childWindow);
      }, childWindow, {defineAs: 'createSpecialInflate'});
    }

    if (RtmpUtils.isRtmpEnabled) {
      Components.utils.exportFunction(function (params) {
        return RtmpUtils.createSocket(childWindow, params);
      }, childWindow, {defineAs: 'createRtmpSocket'});
      Components.utils.exportFunction(function () {
        return RtmpUtils.createXHR(childWindow);
      }, childWindow, {defineAs: 'createRtmpXHR'});
    }

    window.onExternalCallback = function (call) {
      return shumwayComAdapter.onExternalCallback(Components.utils.cloneInto(call, childWindow));
    };

    window.onMessageCallback = function (response) {
      shumwayComAdapter.onMessageCallback(Components.utils.cloneInto(response, childWindow));
    };

    window.onLoadFileCallback = function (args) {
      shumwayComAdapter.onLoadFileCallback(Components.utils.cloneInto(args, childWindow));
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
      var detail = {
        action: message.data.action,
        data: message.data.data,
        sync: message.data.sync
      };
      if (message.data.callback) {
        detail.callback = true;
        detail.cookie = message.data.cookie;
      }

      return window.notifyShumwayMessage(detail);
    });

    messageManager.addMessageListener('Shumway:enableDebug', function (message) {
      enableDebug();
    });

    window.onExternalCallback = function (call) {
      return externalInterface.callback(JSON.stringify(call));
    };

    window.onMessageCallback = function (response) {
      messageManager.sendAsyncMessage('Shumway:messageCallback', {
        cookie: response.cookie,
        response: response.response
      });
    };

    window.onLoadFileCallback = function (args) {
      messageManager.sendAsyncMessage('Shumway:loadFile', args);
    };

    messageManager.sendAsyncMessage('Shumway:init', {});
  }


  function handleDebug(connection) {
    viewer.parentNode.removeChild(viewer); // we don't need viewer anymore
    document.body.className = 'remoteDebug';

    function sendMessage(data) {
      var detail = {
        action: data.action,
        data: data.data,
        sync: data.sync
      };
      if (data.callback) {
        detail.callback = true;
        detail.cookie = data.cookie;
      }
      return window.notifyShumwayMessage(detail);
    }

    connection.onData = function (data) {
      switch (data.action) {
        case 'sendMessage':
          return sendMessage(data.detail);
        case 'reload':
          document.body.className = 'remoteReload';
          setTimeout(function () {
            window.top.location.reload();
          }, 1000);
          return;
      }
    };

    window.onExternalCallback = function (call) {
      return connection.send({action: 'onExternalCallback', detail: call});
    };

    window.onMessageCallback = function (response) {
      return connection.send({action: 'onMessageCallback', detail: response});
    };

    window.onLoadFileCallback = function (args) {
      if (args.array) {
        args.array = Array.prototype.slice.call(args.array, 0);
      }
      return connection.send({action: 'onLoadFileCallback', detail: args}, true);
    };

    connection.send({action: 'runViewer'}, true);
  }

  function enableDebug() {
    DebugUtils.enableDebug(window.swfUrlLoading);
    setTimeout(function () {
      window.top.location.reload();
    }, 1000);
  }

  promise.then(function (oop) {
    if (DebugUtils.isEnabled) {
      DebugUtils.createDebuggerConnection(window.swfUrlLoading).then(function (debuggerConnection) {
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

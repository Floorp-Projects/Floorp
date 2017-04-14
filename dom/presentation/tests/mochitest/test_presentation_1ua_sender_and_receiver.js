/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

function debug(str) {
  // info(str);
}

var gScript = SpecialPowers.loadChromeScript(SimpleTest.getTestFileURL('PresentationSessionChromeScript1UA.js'));
var receiverUrl = SimpleTest.getTestFileURL('file_presentation_1ua_receiver.html');
var request;
var connection;
var receiverIframe;
var presentationId;
const DATA_ARRAY = [0, 255, 254, 0, 1, 2, 3, 0, 255, 255, 254, 0];
const DATA_ARRAY_BUFFER = new ArrayBuffer(DATA_ARRAY.length);
const TYPED_DATA_ARRAY = new Uint8Array(DATA_ARRAY_BUFFER);
TYPED_DATA_ARRAY.set(DATA_ARRAY);

function postMessageToIframe(aType) {
  receiverIframe.src = receiverUrl + "#" +
                       encodeURIComponent(JSON.stringify({ type: aType }));
}

function setup() {

  gScript.addMessageListener('device-prompt', function devicePromptHandler() {
    debug('Got message: device-prompt');
    gScript.removeMessageListener('device-prompt', devicePromptHandler);
    gScript.sendAsyncMessage('trigger-device-prompt-select');
  });

  gScript.addMessageListener('control-channel-established', function controlChannelEstablishedHandler() {
    gScript.removeMessageListener('control-channel-established',
                                  controlChannelEstablishedHandler);
    gScript.sendAsyncMessage("trigger-control-channel-open");
  });

  gScript.addMessageListener('sender-launch', function senderLaunchHandler(url) {
    debug('Got message: sender-launch');
    gScript.removeMessageListener('sender-launch', senderLaunchHandler);
    is(url, receiverUrl, 'Receiver: should receive the same url');
    receiverIframe = document.createElement('iframe');
    receiverIframe.setAttribute('src', receiverUrl);
    receiverIframe.setAttribute("mozbrowser", "true");
    receiverIframe.setAttribute("mozpresentation", receiverUrl);
    var oop = location.pathname.indexOf('_inproc') == -1;
    receiverIframe.setAttribute("remote", oop);

    // This event is triggered when the iframe calls "alert".
    receiverIframe.addEventListener("mozbrowsershowmodalprompt", function receiverListener(evt) {
      var message = evt.detail.message;
      debug('Got iframe message: ' + message);
      if (/^OK /.exec(message)) {
        ok(true, message.replace(/^OK /, ""));
      } else if (/^KO /.exec(message)) {
        ok(false, message.replace(/^KO /, ""));
      } else if (/^INFO /.exec(message)) {
        info(message.replace(/^INFO /, ""));
      } else if (/^COMMAND /.exec(message)) {
        var command = JSON.parse(message.replace(/^COMMAND /, ""));
        gScript.sendAsyncMessage(command.name, command.data);
      } else if (/^DONE$/.exec(message)) {
        receiverIframe.removeEventListener("mozbrowsershowmodalprompt",
                                            receiverListener);
      }
    });

    var promise = new Promise(function(aResolve, aReject) {
      document.body.appendChild(receiverIframe);
      aResolve(receiverIframe);
    });

    var obs = SpecialPowers.Cc["@mozilla.org/observer-service;1"]
                           .getService(SpecialPowers.Ci.nsIObserverService);
    obs.notifyObservers(promise, 'setup-request-promise', null);
  });

  gScript.addMessageListener('promise-setup-ready', function promiseSetupReadyHandler() {
    debug('Got message: promise-setup-ready');
    gScript.removeMessageListener('promise-setup-ready', promiseSetupReadyHandler);
    gScript.sendAsyncMessage('trigger-on-session-request', receiverUrl);
  });

  return Promise.resolve();
}

function testCreateRequest() {
  return new Promise(function(aResolve, aReject) {
    info('Sender: --- testCreateRequest ---');
    request = new PresentationRequest("file_presentation_1ua_receiver.html");
    request.getAvailability().then((aAvailability) => {
      is(aAvailability.value, false, "Sender: should have no available device after setup");
      aAvailability.onchange = function() {
        aAvailability.onchange = null;
        ok(aAvailability.value, "Sender: Device should be available.");
        aResolve();
      }

      gScript.sendAsyncMessage('trigger-device-add');
    }).catch((aError) => {
      ok(false, "Sender: Error occurred when getting availability: " + aError);
      teardown();
      aReject();
    });
  });
}

function testStartConnection() {
  return new Promise(function(aResolve, aReject) {
    request.start().then((aConnection) => {
      connection = aConnection;
      ok(connection, "Sender: Connection should be available.");
      ok(connection.id, "Sender: Connection ID should be set.");
      is(connection.state, "connecting", "The initial state should be connecting.");
      is(connection.url, receiverUrl, "request URL should be expanded to absolute URL");
      connection.onconnect = function() {
        connection.onconnect = null;
        is(connection.state, "connected", "Connection should be connected.");
        presentationId = connection.id;
        aResolve();
      };
    }).catch((aError) => {
      ok(false, "Sender: Error occurred when establishing a connection: " + aError);
      teardown();
      aReject();
    });

    let request2 = new PresentationRequest("/");
    request2.start().then(() => {
      ok(false, "Sender: session start should fail while there is an unsettled promise.");
    }).catch((aError) => {
      is(aError.name, "OperationError", "Expect to get OperationError.");
    });
  });
}

function testSendMessage() {
  return new Promise(function(aResolve, aReject) {
    info('Sender: --- testSendMessage ---');
    gScript.addMessageListener('trigger-message-from-sender', function triggerMessageFromSenderHandler() {
      debug('Got message: trigger-message-from-sender');
      gScript.removeMessageListener('trigger-message-from-sender', triggerMessageFromSenderHandler);
      info('Send message to receiver');
      connection.send('msg-sender-to-receiver');
    });

    gScript.addMessageListener('message-from-sender-received', function messageFromSenderReceivedHandler() {
      debug('Got message: message-from-sender-received');
      gScript.removeMessageListener('message-from-sender-received', messageFromSenderReceivedHandler);
      aResolve();
    });
  });
}

function testIncomingMessage() {
  return new Promise(function(aResolve, aReject) {
    info('Sender: --- testIncomingMessage ---');
    connection.addEventListener('message', function(evt) {
      let msg = evt.data;
      is(msg, "msg-receiver-to-sender", "Sender: Sender should receive message from Receiver");
      postMessageToIframe('message-from-receiver-received');
      aResolve();
    }, {once: true});
    postMessageToIframe('trigger-message-from-receiver');
  });
}

function testSendBlobMessage() {
  return new Promise(function(aResolve, aReject) {
    info('Sender: --- testSendBlobMessage ---');
    connection.addEventListener('message', function(evt) {
      let msg = evt.data;
      is(msg, "testIncomingBlobMessage", "Sender: Sender should receive message from Receiver");
      let blob = new Blob(["Hello World"], {type : 'text/plain'});
      connection.send(blob);
      aResolve();
    }, {once: true});
  });
}

function testSendArrayBuffer() {
  return new Promise(function(aResolve, aReject) {
    info('Sender: --- testSendArrayBuffer ---');
    connection.addEventListener('message', function(evt) {
      let msg = evt.data;
      is(msg, "testIncomingArrayBuffer", "Sender: Sender should receive message from Receiver");
      connection.send(DATA_ARRAY_BUFFER);
      aResolve();
    }, {once: true});
  });
}

function testSendArrayBufferView() {
  return new Promise(function(aResolve, aReject) {
    info('Sender: --- testSendArrayBufferView ---');
    connection.addEventListener('message', function(evt) {
      let msg = evt.data;
      is(msg, "testIncomingArrayBufferView", "Sender: Sender should receive message from Receiver");
      connection.send(TYPED_DATA_ARRAY);
      aResolve();
    }, {once: true});
  });
}

function testCloseConnection() {
  info('Sender: --- testCloseConnection ---');
  // Test terminate immediate after close.
  function controlChannelEstablishedHandler()
  {
    gScript.removeMessageListener('control-channel-established',
                                  controlChannelEstablishedHandler);
    ok(false, "terminate after close should do nothing");
  }
  gScript.addMessageListener('ready-to-close', function onReadyToClose() {
    gScript.removeMessageListener('ready-to-close', onReadyToClose);
    connection.close();

    gScript.addMessageListener('control-channel-established', controlChannelEstablishedHandler);
    connection.terminate();
  });

  return Promise.all([
    new Promise(function(aResolve, aReject) {
      connection.onclose = function() {
        connection.onclose = null;
        is(connection.state, 'closed', 'Sender: Connection should be closed.');
        gScript.removeMessageListener('control-channel-established',
                                      controlChannelEstablishedHandler);
        aResolve();
      };
    }),
    new Promise(function(aResolve, aReject) {
      let timeout = setTimeout(function() {
        gScript.removeMessageListener('device-disconnected',
                                      deviceDisconnectedHandler);
        ok(true, "terminate after close should not trigger device.disconnect");
        aResolve();
      }, 3000);

      function deviceDisconnectedHandler() {
        gScript.removeMessageListener('device-disconnected',
                                      deviceDisconnectedHandler);
        ok(false, "terminate after close should not trigger device.disconnect");
        clearTimeout(timeout);
        aResolve();
      }

      gScript.addMessageListener('device-disconnected', deviceDisconnectedHandler);
    }),
    new Promise(function(aResolve, aReject) {
      gScript.addMessageListener('receiver-closed', function onReceiverClosed() {
        gScript.removeMessageListener('receiver-closed', onReceiverClosed);
        gScript.removeMessageListener('control-channel-established',
                                      controlChannelEstablishedHandler);
        aResolve();
      });
    }),
  ]);
}

function testTerminateAfterClose() {
  info('Sender: --- testTerminateAfterClose ---');
  return Promise.race([
      new Promise(function(aResolve, aReject) {
        connection.onterminate = function() {
          connection.onterminate = null;
          ok(false, 'terminate after close should do nothing');
          aResolve();
        };
        connection.terminate();
      }),
      new Promise(function(aResolve, aReject) {
        setTimeout(function() {
          is(connection.state, 'closed', 'Sender: Connection should be closed.');
          aResolve();
        }, 3000);
      }),
  ]);
}

function testReconnect() {
  return new Promise(function(aResolve, aReject) {
    info('Sender: --- testReconnect ---');
    gScript.addMessageListener('control-channel-established', function controlChannelEstablished() {
      gScript.removeMessageListener('control-channel-established', controlChannelEstablished);
      gScript.sendAsyncMessage("trigger-control-channel-open");
    });

    gScript.addMessageListener('start-reconnect', function startReconnectHandler(url) {
      debug('Got message: start-reconnect');
      gScript.removeMessageListener('start-reconnect', startReconnectHandler);
      is(url, receiverUrl, "URLs should be the same.")
      gScript.sendAsyncMessage('trigger-reconnected-acked', url);
    });

    gScript.addMessageListener('ready-to-reconnect', function onReadyToReconnect() {
      gScript.removeMessageListener('ready-to-reconnect', onReadyToReconnect);
      request.reconnect(presentationId).then((aConnection) => {
        connection = aConnection;
        ok(connection, "Sender: Connection should be available.");
        is(connection.id, presentationId, "The presentationId should be the same.");
        is(connection.state, "connecting", "The initial state should be connecting.");
        connection.onconnect = function() {
          connection.onconnect = null;
          is(connection.state, "connected", "Connection should be connected.");
          aResolve();
        };
      }).catch((aError) => {
        ok(false, "Sender: Error occurred when establishing a connection: " + aError);
        teardown();
        aReject();
      });
    });

    postMessageToIframe('prepare-for-reconnect');
  });
}

function teardown() {
  gScript.addMessageListener('teardown-complete', function teardownCompleteHandler() {
    debug('Got message: teardown-complete');
    gScript.removeMessageListener('teardown-complete', teardownCompleteHandler);
    gScript.destroy();
    SimpleTest.finish();
  });

  gScript.sendAsyncMessage('teardown');
}

function runTests() {
  setup().then(testCreateRequest)
         .then(testStartConnection)
         .then(testSendMessage)
         .then(testIncomingMessage)
         .then(testSendBlobMessage)
         .then(testCloseConnection)
         .then(testReconnect)
         .then(testSendArrayBuffer)
         .then(testSendArrayBufferView)
         .then(testCloseConnection)
         .then(testTerminateAfterClose)
         .then(teardown);
}

SimpleTest.waitForExplicitFinish();
SimpleTest.requestFlakyTimeout('Test for guarantee not firing async event');
SpecialPowers.pushPermissions([
  {type: 'presentation-device-manage', allow: false, context: document},
  {type: "browser", allow: true, context: document},
], () => {
  SpecialPowers.pushPrefEnv({ 'set': [["dom.presentation.enabled", true],
                                      /* Mocked TCP session transport builder in the test */
                                      ["dom.presentation.session_transport.data_channel.enable", true],
                                      ["dom.presentation.controller.enabled", true],
                                      ["dom.presentation.receiver.enabled", true],
                                      ["dom.presentation.test.enabled", true],
                                      ["dom.presentation.test.stage", 0],
                                      ["dom.mozBrowserFramesEnabled", true],
                                      ["network.disable.ipc.security", true],
                                      ["media.navigator.permission.disabled", true]]},
                            runTests);
});

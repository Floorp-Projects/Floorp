'use strict';

SimpleTest.waitForExplicitFinish();
SimpleTest.requestFlakyTimeout('Test for guarantee not firing async event');

function debug(str) {
  // info(str);
}

var gScript = SpecialPowers.loadChromeScript(SimpleTest.getTestFileURL('PresentationSessionChromeScript1UA.js'));
var receiverUrl = SimpleTest.getTestFileURL('file_presentation_terminate.html');
var request;
var connection;
var receiverIframe;

function setup() {
  gScript.addMessageListener('device-prompt', function devicePromptHandler() {
    debug('Got message: device-prompt');
    gScript.removeMessageListener('device-prompt', devicePromptHandler);
    gScript.sendAsyncMessage('trigger-device-prompt-select');
  });

  gScript.addMessageListener('control-channel-established', function controlChannelEstablishedHandler() {
    gScript.removeMessageListener('control-channel-established',
                                  controlChannelEstablishedHandler);
    gScript.sendAsyncMessage('trigger-control-channel-open');
  });

  gScript.addMessageListener('sender-launch', function senderLaunchHandler(url) {
    debug('Got message: sender-launch');
    gScript.removeMessageListener('sender-launch', senderLaunchHandler);
    is(url, receiverUrl, 'Receiver: should receive the same url');
    receiverIframe = document.createElement('iframe');
    receiverIframe.setAttribute('mozbrowser', 'true');
    receiverIframe.setAttribute('mozpresentation', receiverUrl);
    var oop = location.pathname.indexOf('_inproc') == -1;
    receiverIframe.setAttribute('remote', oop);

    receiverIframe.setAttribute('src', receiverUrl);
    receiverIframe.addEventListener('mozbrowserloadend', function mozbrowserloadendHander() {
      receiverIframe.removeEventListener('mozbrowserloadend', mozbrowserloadendHander);
      info('Receiver loaded.');
    });

    // This event is triggered when the iframe calls 'alert'.
    receiverIframe.addEventListener('mozbrowsershowmodalprompt', function receiverListener(evt) {
      var message = evt.detail.message;
      if (/^OK /.exec(message)) {
        ok(true, message.replace(/^OK /, ''));
      } else if (/^KO /.exec(message)) {
        ok(false, message.replace(/^KO /, ''));
      } else if (/^INFO /.exec(message)) {
        info(message.replace(/^INFO /, ''));
      } else if (/^COMMAND /.exec(message)) {
        var command = JSON.parse(message.replace(/^COMMAND /, ''));
        gScript.sendAsyncMessage(command.name, command.data);
      } else if (/^DONE$/.exec(message)) {
        ok(true, 'Messaging from iframe complete.');
        receiverIframe.removeEventListener('mozbrowsershowmodalprompt',
                                            receiverListener);
      }
    }, false);

    var promise = new Promise(function(aResolve, aReject) {
      document.body.appendChild(receiverIframe);
      aResolve(receiverIframe);
    });

    var obs = SpecialPowers.Cc['@mozilla.org/observer-service;1']
                           .getService(SpecialPowers.Ci.nsIObserverService);
    obs.notifyObservers(promise, 'setup-request-promise', null);
  });

  gScript.addMessageListener('promise-setup-ready', function promiseSetupReadyHandler() {
    debug('Got message: promise-setup-ready');
    gScript.removeMessageListener('promise-setup-ready',
                                  promiseSetupReadyHandler);
    gScript.sendAsyncMessage('trigger-on-session-request', receiverUrl);
  });

  return Promise.resolve();
}

function testCreateRequest() {
  return new Promise(function(aResolve, aReject) {
    info('Sender: --- testCreateRequest ---');
    request = new PresentationRequest(receiverUrl);
    request.getAvailability().then((aAvailability) => {
      is(aAvailability.value, false, "Sender: should have no available device after setup");
      aAvailability.onchange = function() {
        aAvailability.onchange = null;
        ok(aAvailability.value, 'Sender: Device should be available.');
        aResolve();
      }

      gScript.sendAsyncMessage('trigger-device-add');
    }).catch((aError) => {
      ok(false, 'Sender: Error occurred when getting availability: ' + aError);
      teardown();
      aReject();
    });
  });
}

function testStartConnection() {
  return new Promise(function(aResolve, aReject) {
    request.start().then((aConnection) => {
      connection = aConnection;
      ok(connection, 'Sender: Connection should be available.');
      ok(connection.id, 'Sender: Connection ID should be set.');
      is(connection.state, 'connecting', 'Sender: The initial state should be connecting.');
      connection.onconnect = function() {
        connection.onconnect = null;
        is(connection.state, 'connected', 'Connection should be connected.');
        aResolve();
      };

      info('Sender: test terminate at connecting state');
      connection.onterminate = function() {
        connection.onterminate = null;
        ok(false, 'Should not be able to terminate at connecting state');
        aReject();
      }
      connection.terminate();
    }).catch((aError) => {
      ok(false, 'Sender: Error occurred when establishing a connection: ' + aError);
      teardown();
      aReject();
    });
  });
}

function testConnectionTerminate() {
  return new Promise(function(aResolve, aReject) {
    info('Sender: --- testConnectionTerminate---');
    connection.onterminate = function() {
      connection.onterminate = null;
      is(connection.state, 'terminated', 'Sender: Connection should be terminated.');
    };
    gScript.addMessageListener('control-channel-established', function controlChannelEstablishedHandler() {
      gScript.removeMessageListener('control-channel-established',
                                    controlChannelEstablishedHandler);
      gScript.sendAsyncMessage('trigger-control-channel-open');
    });
    gScript.addMessageListener('sender-terminate', function senderTerminateHandler() {
      gScript.removeMessageListener('sender-terminate',
                                    senderTerminateHandler);

      receiverIframe.addEventListener('mozbrowserclose', function() {
        ok(true, 'observe receiver page closing');
        aResolve();
      });

      gScript.sendAsyncMessage('trigger-on-terminate-request');
    });
    gScript.addMessageListener('ready-to-terminate', function onReadyToTerminate() {
      gScript.removeMessageListener('ready-to-terminate', onReadyToTerminate);
      connection.terminate();

      // test unexpected close right after terminate
      connection.onclose = function() {
        ok(false, 'close after terminate should do nothing');
      };
      connection.close();
    });
  });
}

function testSendAfterTerminate() {
  return new Promise(function(aResolve, aReject) {
    try {
      connection.send('something');
      ok(false, 'PresentationConnection.send should be failed');
    } catch (e) {
      is(e.name, 'InvalidStateError', 'Must throw InvalidStateError');
    }
    aResolve();
  });
}

function testCloseAfterTerminate() {
  return Promise.race([
      new Promise(function(aResolve, aReject) {
        connection.onclose = function() {
          connection.onclose = null;
          ok(false, 'close at terminated state should do nothing');
          aResolve();
        };
        connection.close();
      }),
      new Promise(function(aResolve, aReject) {
        setTimeout(function() {
          is(connection.state, 'terminated', 'Sender: Connection should be terminated.');
          aResolve();
        }, 3000);
      }),
  ]);
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
         .then(testConnectionTerminate)
         .then(testSendAfterTerminate)
         .then(testCloseAfterTerminate)
         .then(teardown);
}

SpecialPowers.pushPermissions([
  {type: 'presentation-device-manage', allow: false, context: document},
  {type: 'browser', allow: true, context: document},
], () => {
  SpecialPowers.pushPrefEnv({ 'set': [['dom.presentation.enabled', true],
                                      ["dom.presentation.controller.enabled", true],
                                      ["dom.presentation.receiver.enabled", true],
                                      ['dom.presentation.test.enabled', true],
                                      ['dom.mozBrowserFramesEnabled', true],
                                      ['dom.ipc.tabs.disabled', false],
                                      ['dom.presentation.test.stage', 0]]},
                            runTests);
});

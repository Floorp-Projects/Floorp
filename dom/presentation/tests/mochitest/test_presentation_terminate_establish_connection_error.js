'use strict';

SimpleTest.waitForExplicitFinish();
SimpleTest.requestFlakyTimeout('Test for guarantee not firing async event');

function debug(str) {
  // info(str);
}

var gScript = SpecialPowers.loadChromeScript(SimpleTest.getTestFileURL('PresentationSessionChromeScript1UA.js'));
var receiverUrl = SimpleTest.getTestFileURL('file_presentation_terminate_establish_connection_error.html');
var request;
var connection;
var receiverIframe;

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

  gScript.addMessageListener('offer-sent', function offerSentHandler() {
    debug('Got message: offer-sent');
    gScript.removeMessageListener('offer-sent', offerSentHandler);
    gScript.sendAsyncMessage('trigger-on-offer');
  });

  gScript.addMessageListener('answer-sent', function answerSentHandler() {
    debug('Got message: answer-sent');
    gScript.removeMessageListener('answer-sent', answerSentHandler);
    gScript.sendAsyncMessage('trigger-on-answer');
  });

  return Promise.resolve();
}

function testCreateRequest() {
  return new Promise(function(aResolve, aReject) {
    info('Sender: --- testCreateRequest ---');
    request = new PresentationRequest(receiverUrl);
    request.getAvailability().then((aAvailability) => {
      aAvailability.onchange = function() {
        aAvailability.onchange = null;
        ok(aAvailability.value, 'Sender: Device should be available.');
        aResolve();
      }
    }).catch((aError) => {
      ok(false, 'Sender: Error occurred when getting availability: ' + aError);
      teardown();
      aReject();
    });

    gScript.sendAsyncMessage('trigger-device-add');
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
    gScript.addMessageListener('prepare-for-terminate', function prepareForTerminateHandler() {
      debug('Got message: prepare-for-terminate');
      gScript.removeMessageListener('prepare-for-terminate', prepareForTerminateHandler);
      connection.onclose = function() {
        connection.onclose = null;
        is(connection.state, 'closed', 'Sender: Connection should be closed.');
      };
      gScript.sendAsyncMessage('trigger-control-channel-error');
      receiverIframe.addEventListener('mozbrowserclose', function() {
        ok(true, 'observe receiver page closing');
        aResolve();
      });
      postMessageToIframe('ready-to-terminate');
    });
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
         .then(testConnectionTerminate)
         .then(teardown);
}

SpecialPowers.pushPermissions([
  {type: 'presentation-device-manage', allow: false, context: document},
  {type: 'presentation', allow: true, context: document},
  {type: 'browser', allow: true, context: document},
], () => {
  SpecialPowers.pushPrefEnv({ 'set': [['dom.presentation.enabled', true],
                                      ['dom.presentation.test.enabled', true],
                                      ['dom.mozBrowserFramesEnabled', true],
                                      ['dom.ipc.tabs.disabled', false],
                                      ['dom.presentation.test.stage', 0]]},
                            runTests);
});

'use strict';

// resolve multiple promise in parallel
function expectAll(aValue) {
  let deferred = new Promise(function(resolve, reject) {
    let countdown = aValue.length;
    let resolutionValues = new Array(countdown);

    for (let i = 0; i < aValue.length; i++) {
      let index = i;
      aValue[i].then(function(val) {
        resolutionValues[index] = val;
        if (--countdown === 0) {
          resolve(resolutionValues);
        }
      }, reject);
    }
  });

  return deferred;
}

function TestInit() {
  let url = SimpleTest.getTestFileURL("RecordingStatusChromeScript.js")
  let script = SpecialPowers.loadChromeScript(url);

  let helper = {
    finish: function () {
      script.destroy();
    },
    fakeShutdown: function () {
      script.sendAsyncMessage('fake-content-shutdown', {});
    }
  };

  script.addMessageListener('chrome-event', function (message) {
    if (helper.hasOwnProperty('onEvent')) {
      helper.onEvent(message);
    } else {
      ok(false, 'unexpected message: ' + JSON.stringify(message));
    }
  });

  script.sendAsyncMessage("init-chrome-event", {
    type: 'recording-status'
  });

  return Promise.resolve(helper);
}

function expectEvent(expected, eventHelper) {
  return new Promise(function(resolve, reject) {
    eventHelper.onEvent = function(message) {
      delete eventHelper.onEvent;
      ok(message, JSON.stringify(message));
      is(message.type, 'recording-status', 'event type: ' + message.type);
      is(message.active, expected.active, 'recording active: ' + message.active);
      is(message.isAudio, expected.isAudio, 'audio recording active: ' + message.isAudio);
      is(message.isVideo, expected.isVideo, 'video recording active: ' + message.isVideo);
      resolve(eventHelper);
    };
    info('waiting for recording-status');
  });
}

function expectStream(params, callback) {
  return new Promise(function(resolve, reject) {
    var req = navigator.mozGetUserMedia(
      params,
      function(stream) {
        ok(true, 'create media stream');
        callback(stream);
        resolve();
      },
      function(err) {
        ok(false, 'fail to create media stream');
        reject(err);
      }
    );
    info('waiting for gUM result');
  });
}

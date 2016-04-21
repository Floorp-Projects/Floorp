"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

var fileURL = 'chrome://mochitests/content/chrome/dom/browser-element/mochitest/file_browserElement_AudioChannelMutedByDefault.html';
var testFrame;
var ac;

function alertListener(e) {
  var message = e.detail.message
  if (/^OK/.exec(message)) {
    ok(true, "Message from file : " + message);
  } else if (/^KO/.exec(message)) {
    error(message);
  } else if (/DONE/.exec(message)) {
    ok(true, "Audio playback success!");
    finish();
  } else {
    error("Undefined event.");
  }
}

function assert(aVal, aMessage) {
  return (!aVal) ? error(aMessage) : 0;
}

function error(aMessage) {
  ok(false, "Error : " + aMessage);
  finish();
}

function finish() {
  testFrame.removeEventListener('mozbrowsershowmodalprompt', alertListener);
  document.body.removeChild(testFrame);
  SimpleTest.finish();
}

function setCommand(aArg) {
  assert(!!ac, "Audio channel doesn't exist!");
  info("# Command = " + aArg);
  testFrame.src = fileURL + '#' + aArg;

  switch (aArg) {
    case 'play':
      ac.onactivestatechanged = () => {
        ac.onactivestatechanged = null;
        ok(true, "activestatechanged event received.");

        new Promise(function(r, rr) {
          ac.getMuted().onsuccess = function(e) {
            is(e.target.result, true, "Muted channel by default");
            r();
          }
        }).then(function() {
          ac.setMuted(false).onsuccess = function(e) {
            ok(true, "Unmuted the channel.");
          }
        });
      };
      break;
    default :
      error("Undefined command!");
  }
}

function runTests() {
  setCommand('play');
}

function setupTestFrame() {
  testFrame = document.createElement('iframe');
  testFrame.setAttribute('mozbrowser', 'true');
  testFrame.src = fileURL;

  function loadend() {
    testFrame.removeEventListener('mozbrowserloadend', loadend);
    ok("allowedAudioChannels" in testFrame, "allowedAudioChannels exist");
    var channels = testFrame.allowedAudioChannels;
    is(channels.length, 9, "9 audio channel by default");

    ac = channels[0];
    ok(ac instanceof BrowserElementAudioChannel, "Correct class");
    ok("getMuted" in ac, "ac.getMuted exists");
    ok("setMuted" in ac, "ac.setMuted exists");
    ok("onactivestatechanged" in ac, "onactivestatechanged exists");

    runTests();
  }

  info("Set EventListeners.");
  testFrame.addEventListener('mozbrowsershowmodalprompt', alertListener);
  testFrame.addEventListener('mozbrowserloadend', loadend);
  document.body.appendChild(testFrame);
}

addEventListener('testready', function() {
  SpecialPowers.pushPrefEnv({'set': [["b2g.system_startup_url", window.location.href],
                                     ["dom.audiochannel.mutedByDefault", true]]},
                            function() {
    SimpleTest.executeSoon(setupTestFrame);
  });
});

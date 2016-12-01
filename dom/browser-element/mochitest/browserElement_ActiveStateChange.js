"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

var fileURL = 'chrome://mochitests/content/chrome/dom/browser-element/mochitest/file_browserElement_ActiveStateChange.html';
var testFrame;
var ac;

function assert(aVal, aMessage) {
  return (!aVal) ? error(aMessage) : 0;
}

function error(aMessage) {
  ok(false, "Error : " + aMessage);
  finish();
}

function finish() {
  document.body.removeChild(testFrame);
  SimpleTest.finish();
}

function setCommand(aArg) {
  return new Promise(resolve => {
    assert(!!ac, "Audio channel doesn't exist!");
    info("# Command = " + aArg);

    testFrame.src = fileURL + '#' + aArg;
    var expectedActive = false;
    switch (aArg) {
      case 'play':
        expectedActive = true;
        break;
      case 'pause':
        expectedActive = false;
        break;
      default :
        error("Undefined command!");
    }

    ac.onactivestatechanged = () => {
      ac.onactivestatechanged = null;
      ac.isActive().onsuccess = (e) => {
        is(expectedActive, e.target.result,
           "Correct active state = " + expectedActive);
        resolve();
      };
    };
  });
}

async function runTests() {
  await setCommand('play');

  await setCommand('pause');

  finish();
}

function setupTestFrame() {
  testFrame = document.createElement('iframe');
  testFrame.setAttribute('mozbrowser', 'true');
  testFrame.src = fileURL;

  function loadend() {
    ok("allowedAudioChannels" in testFrame, "allowedAudioChannels exist");
    var channels = testFrame.allowedAudioChannels;
    is(channels.length, 9, "9 audio channel by default");

    ac = channels[0];

    ok(ac instanceof BrowserElementAudioChannel, "Correct class");
    ok("isActive" in ac, "isActive exists");
    ok("onactivestatechanged" in ac, "onactivestatechanged exists");

    runTests();
  }

  function alertError(e) {
    testFrame.removeEventListener('mozbrowsershowmodalprompt', alertError);
    var message = e.detail.message
    error(message);
  }

  testFrame.addEventListener('mozbrowserloadend', loadend, {once: true});
  testFrame.addEventListener('mozbrowsershowmodalprompt', alertError);
  document.body.appendChild(testFrame);
}

addEventListener('testready', function() {
  SpecialPowers.pushPrefEnv({'set': [["b2g.system_startup_url", window.location.href]]},
                            function() {
    SimpleTest.executeSoon(setupTestFrame);
  });
});


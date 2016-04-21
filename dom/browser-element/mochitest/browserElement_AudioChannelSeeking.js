"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

var fileURL = 'chrome://mochitests/content/chrome/dom/browser-element/mochitest/file_browserElement_AudioChannelSeeking.html';
var generator = runTests();
var testFrame;
var ac;

function alertListener(e) {
  var message = e.detail.message
  if (/^OK/.exec(message)) {
    ok(true, "Message from file : " + message);
    continueTest();
  } else if (/^KO/.exec(message)) {
    error(message);
  } else if (/^INFO/.exec(message)) {
    info("Message from file : " + message);
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

function continueTest() {
  try {
    generator.next();
  } catch (e if e instanceof StopIteration) {
    error("Stop test because of exception!");
  }
}

function finish() {
  testFrame.removeEventListener('mozbrowsershowmodalprompt', alertListener);
  ok(true, "Remove event-listener.");
  document.body.removeChild(testFrame);
  ok(true, "Remove test-frame from document.");
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
        ok(true, "Receive onactivestatechanged after audio started.");
        continueTest();
      };
      break;
    case 'seeking':
      ac.onactivestatechanged = () => {
        ac.onactivestatechanged = null;
        error("Should not receive onactivestatechanged during seeking!");
      };
      break;
    case 'pause':
      ac.onactivestatechanged = null;
      break;
    default :
      error("Undefined command!");
  }
}

function runTests() {
  setCommand('play');
  yield undefined;

  setCommand('seeking');
  yield undefined;

  setCommand('seeking');
  yield undefined;

  setCommand('seeking');
  yield undefined;

  setCommand('pause');
  yield undefined;

  finish();
  yield undefined;
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
    ok("onactivestatechanged" in ac, "onactivestatechanged exists");

    continueTest();
  }

  testFrame.addEventListener('mozbrowsershowmodalprompt', alertListener);
  testFrame.addEventListener('mozbrowserloadend', loadend);
  ok(true, "Add event-listeners.");

  document.body.appendChild(testFrame);
  ok(true, "Append test-frame to document.");
}

addEventListener('testready', function() {
  SpecialPowers.pushPrefEnv({'set': [["b2g.system_startup_url", window.location.href]]},
                            function() {
    SimpleTest.executeSoon(setupTestFrame);
  });
});

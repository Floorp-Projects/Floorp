"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

var fileURL = 'chrome://mochitests/content/chrome/dom/browser-element/mochitest/file_browserElement_NoAudioTrack.html';
var generator = runTests();
var testFrame;

function alertListener(e) {
  var message = e.detail.message;
  if (/^OK/.exec(message)) {
    ok(true, "Message from file : " + message);
    continueTest();
  } else if (/^KO/.exec(message)) {
    error(message);
  } else {
    error("Undefined event.");
  }
}

function error(aMessage) {
  ok(false, "Error : " + aMessage);
  finish();
}

function continueTest() {
  generator.next();
}

function finish() {
  testFrame.removeEventListener('mozbrowsershowmodalprompt', alertListener);
  ok(true, "Remove event-listener.");
  document.body.removeChild(testFrame);
  ok(true, "Remove test-frame from document.");
  SimpleTest.finish();
}

function setCommand(aArg) {
  info("# Command = " + aArg);
  testFrame.src = fileURL + '#' + aArg;
}

function* runTests() {
  setCommand('play');
  yield undefined;

  // wait a second to make sure that onactivestatechanged isn't dispatched.
  setCommand('idle');
  yield undefined;

  finish();
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

    var ac = channels[0];
    ok(ac instanceof BrowserElementAudioChannel, "Correct class");
    ok("onactivestatechanged" in ac, "onactivestatechanged exists");

    ac.onactivestatechanged = () => {
      ac.onactivestatechanged = null;
      ok(true, "Should receive onactivestatechanged!");
    };

    continueTest();
  }

  testFrame.addEventListener('mozbrowserloadend', loadend);
  testFrame.addEventListener('mozbrowsershowmodalprompt', alertListener);
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

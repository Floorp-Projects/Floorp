"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var fileURL = 'http://example.org/tests/dom/browser-element/mochitest/file_browserElement_NoAudioTrack.html';
var generator = runTests();
var testFrame;

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
  document.body.removeChild(testFrame);
  SimpleTest.finish();
}

function setCommand(aArg) {
  info("# Command = " + aArg);
  testFrame.src = fileURL + '#' + aArg;

  // Yield to the event loop a few times to make sure that onactivestatechanged
  // is not dispatched.
  SimpleTest.executeSoon(function() {
    SimpleTest.executeSoon(function() {
      SimpleTest.executeSoon(function() {
        continueTest();
      });
    });
  });
}

function runTests() {
  setCommand('play');
  yield undefined;

  setCommand('pause');
  yield undefined;

  finish();
  yield undefined;
}

function setupTestFrame() {
  testFrame = document.createElement('iframe');
  testFrame.setAttribute('mozbrowser', 'true');
  testFrame.setAttribute('mozapp', 'http://example.org/manifest.webapp');
  testFrame.src = fileURL;

  function loadend() {
    testFrame.removeEventListener('mozbrowserloadend', loadend);
    ok("allowedAudioChannels" in testFrame, "allowedAudioChannels exist");
    var channels = testFrame.allowedAudioChannels;
    is(channels.length, 1, "1 audio channel by default");

    var ac = channels[0];
    ok(ac instanceof BrowserElementAudioChannel, "Correct class");
    ok("onactivestatechanged" in ac, "onactivestatechanged exists");

    ac.onactivestatechanged = () => {
      ac.onactivestatechanged = null;
      error("Should not receive onactivestatechanged!");
    };

    continueTest();
  }

  testFrame.addEventListener('mozbrowserloadend', loadend);
  document.body.appendChild(testFrame);
}

addEventListener('testready', function() {
  SpecialPowers.pushPrefEnv({'set': [["b2g.system_manifest_url", "http://mochi.test:8888/manifest.webapp"]]},
                            function() {
    SimpleTest.executeSoon(setupTestFrame);
  });
});

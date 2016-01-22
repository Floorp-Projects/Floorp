"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var fileURL = 'http://example.org/tests/dom/browser-element/mochitest/file_browserElement_NoAudioTrack.html';
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
  info("# Command = " + aArg);
  testFrame.src = fileURL + '#' + aArg;
}

function runTests() {
  setCommand('play');
  yield undefined;

  // wait a second to make sure that onactivestatechanged isn't dispatched.
  setCommand('idle');
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
  testFrame.addEventListener('mozbrowsershowmodalprompt', alertListener);
  ok(true, "Add event-listeners.");

  document.body.appendChild(testFrame);
  ok(true, "Append test-frame to document.");
}

addEventListener('testready', function() {
  SpecialPowers.pushPrefEnv({'set': [["b2g.system_manifest_url", "http://mochi.test:8888/manifest.webapp"]]},
                            function() {
    SimpleTest.executeSoon(setupTestFrame);
  });
});

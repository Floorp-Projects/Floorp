"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var fileURL = 'http://example.org/tests/dom/browser-element/mochitest/file_browserElement_ActiveStateChangeOnChangingMutedOrVolume.html';
var generator = runTests();
var testFrame;
var ac;

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
  document.body.removeChild(testFrame);
  SimpleTest.finish();
}

function setCommand(aArg) {
  assert(!!ac, "Audio channel doesn't exist!");
  info("# Command = " + aArg);

  testFrame.src = fileURL + '#' + aArg;
  var expectedActive = false;
  switch (aArg) {
    case 'play':
    case 'unmute':
    case 'volume-1':
      expectedActive = true;
      break;
    case 'pause':
    case 'mute':
    case 'volume-0':
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
      continueTest();
    }
  };
}

function runTests() {
  setCommand('play');
  yield undefined;

  setCommand('mute');
  yield undefined;

  setCommand('unmute');
  yield undefined;

  setCommand('volume-0');
  yield undefined;

  setCommand('volume-1');
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

    ac = channels[0];

    ok(ac instanceof BrowserElementAudioChannel, "Correct class");
    ok("isActive" in ac, "isActive exists");
    ok("onactivestatechanged" in ac, "onactivestatechanged exists");

    generator.next();
  }

  function alertError(e) {
    testFrame.removeEventListener('mozbrowsershowmodalprompt', alertError);
    var message = e.detail.message
    error(message);
  }

  testFrame.addEventListener('mozbrowserloadend', loadend);
  testFrame.addEventListener('mozbrowsershowmodalprompt', alertError);
  document.body.appendChild(testFrame);
}

addEventListener('testready', function() {
  SpecialPowers.pushPrefEnv({'set': [["b2g.system_manifest_url", "http://mochi.test:8888/manifest.webapp"]]},
                            function() {
    SimpleTest.executeSoon(setupTestFrame);
  });
});


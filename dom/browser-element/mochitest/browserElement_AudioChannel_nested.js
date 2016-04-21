/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1113086 - tests for AudioChannel API into BrowserElement

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

function runTests() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');

  var listener = function(e) {
    var message = e.detail.message;
    if (/^OK/.exec(message)) {
      ok(true, "Message from app: " + message);
    } else if (/^KO/.exec(message)) {
      ok(false, "Message from app: " + message);
    } else if (/DONE/.exec(message)) {
      ok(true, "Messaging from app complete");
      iframe.removeEventListener('mozbrowsershowmodalprompt', listener);
      SimpleTest.finish();
    }
  }

  function audio_loadend() {
    ok("mute" in iframe, "iframe.mute exists");
    ok("unmute" in iframe, "iframe.unmute exists");
    ok("getMuted" in iframe, "iframe.getMuted exists");
    ok("getVolume" in iframe, "iframe.getVolume exists");
    ok("setVolume" in iframe, "iframe.setVolume exists");

    ok("allowedAudioChannels" in iframe, "allowedAudioChannels exist");
    var channels = iframe.allowedAudioChannels;
    is(channels.length, 9, "9 audio channel by default");

    var ac = channels[0];

    ok(ac instanceof BrowserElementAudioChannel, "Correct class");
    ok("getVolume" in ac, "ac.getVolume exists");
    ok("setVolume" in ac, "ac.setVolume exists");
    ok("getMuted" in ac, "ac.getMuted exists");
    ok("setMuted" in ac, "ac.setMuted exists");
    ok("isActive" in ac, "ac.isActive exists");

    info("Setting the volume...");
    ac.setVolume(0.5);

    ac.onactivestatechanged = function() {
      ok(true, "activestatechanged event received.");
      ac.onactivestatechanged = null;
    }
  }

  iframe.addEventListener('mozbrowserloadend', audio_loadend);
  iframe.addEventListener('mozbrowsershowmodalprompt', listener, false);
  document.body.appendChild(iframe);

  iframe.src = 'chrome://mochitests/content/chrome/dom/browser-element/mochitest/file_browserElement_AudioChannel_nested.html';
}

addEventListener('testready', function() {
  SpecialPowers.pushPrefEnv({'set': [["b2g.system_startup_url", window.location.href]]},
                            function() {
    SimpleTest.executeSoon(runTests);
  });
});

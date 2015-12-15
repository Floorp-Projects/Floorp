/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1113086 - tests for AudioChannel API into BrowserElement

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTests() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  iframe.setAttribute('mozapp', 'http://example.org/manifest.webapp');

  var listener = function(e) {
    var message = e.detail.message;
    if (/^OK/.exec(message)) {
      ok(true, "Message from app: " + message);
    } else if (/^KO/.exec(message)) {
      ok(false, "Message from app: " + message);
    } else if (/DONE/.exec(message)) {
      ok(true, "Messaging from app complete");
      iframe.removeEventListener('mozbrowsershowmodalprompt', listener);
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
    is(channels.length, 1, "1 audio channel by default");

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
      SimpleTest.finish();
    }
  }

  iframe.addEventListener('mozbrowserloadend', audio_loadend);
  iframe.addEventListener('mozbrowsershowmodalprompt', listener, false);
  document.body.appendChild(iframe);

  var context = { 'url': 'http://example.org',
                  'appId': SpecialPowers.Ci.nsIScriptSecurityManager.NO_APP_ID,
                  'isInBrowserElement': true };
  SpecialPowers.pushPermissions([
    {'type': 'browser', 'allow': 1, 'context': context},
    {'type': 'embed-apps', 'allow': 1, 'context': context}
  ], function() {
    iframe.src = 'http://example.org/tests/dom/browser-element/mochitest/file_browserElement_AudioChannel_nested.html';
  });
}

addEventListener('testready', function() {
  SimpleTest.executeSoon(runTests);
});

/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1113086 - tests for AudioChannel API into BrowserElement

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

SpecialPowers.setBoolPref("media.useAudioChannelService", true);

function noaudio() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  iframe.setAttribute('mozapp', 'http://example.org/manifest.webapp');
  iframe.src = 'http://example.org/tests/dom/browser-element/mochitest/file_empty.html';

  function noaudio_loadend() {
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

    new Promise(function(r, rr) {
      var req = ac.getVolume();
      ok(req instanceof DOMRequest, "This is a domRequest.");
      req.onsuccess = function(e) {
        is(e.target.result, 1.0, "The default volume should be 1.0");
        r();
      }
    })

    .then(function() {
      return new Promise(function(r, rr) {
        ac.getMuted().onsuccess = function(e) {
          is(e.target.result, false, "The default muted value should be false");
          r();
        }
      });
    })

    .then(function() {
      return new Promise(function(r, rr) {
        ac.setVolume(0.8).onsuccess = function() { r(); }
      });
    })

    .then(function() {
      return new Promise(function(r, rr) {
        ac.getVolume().onsuccess = function(e) {
          // the actual value is 0.800000011920929..
          ok(Math.abs(0.8 - e.target.result) < 0.01, "The new volume should be 0.8: " + e.target.result);
          r();
        }
      });
    })

    .then(function() {
      return new Promise(function(r, rr) {
        ac.setVolume(1.0).onsuccess = function() { r(); }
      });
    })

    .then(function() {
      return new Promise(function(r, rr) {
        ac.setMuted(true).onsuccess = function() { r(); }
      });
    })

    .then(function() {
      return new Promise(function(r, rr) {
        ac.getMuted().onsuccess = function(e) {
          is(e.target.result, true, "The new muted value should be true");
          r();
        }
      });
    })

    .then(function() {
      return new Promise(function(r, rr) {
        ac.isActive().onsuccess = function(e) {
          is(e.target.result, false, "ac.isActive is false: no audio element active.");
          r();
        }
      });
    })

    .then(runTests);
  }

  iframe.addEventListener('mozbrowserloadend', noaudio_loadend);
  document.body.appendChild(iframe);
}

function audio() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  iframe.setAttribute('mozapp', 'http://example.org/manifest.webapp');
  iframe.src = 'http://example.org/tests/dom/browser-element/mochitest/iframe_file_audio.html';

  function audio_loadend() {
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

    ac.onactivestatechanged = function() {
      ok("activestatechanged event received.");
      ac.onactivestatechanged = null;
      runTests();
    }
  }

  iframe.addEventListener('mozbrowserloadend', audio_loadend);
  document.body.appendChild(iframe);
}

var tests = [ noaudio, audio ];

function runTests() {
  if (tests.length == 0) {
    SimpleTest.finish();
    return;
  }

  var test = tests.shift();
  test();
}


addEventListener('load', function() {
  SimpleTest.executeSoon(runTests);
});


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = SpecialPowers.Cc;
var Ci = SpecialPowers.Ci;
var Cr = SpecialPowers.Cr;

// Specifies whether we are using fake streams to run this automation
var FAKE_ENABLED = true;
try {
  var audioDevice = SpecialPowers.getCharPref('media.audio_loopback_dev');
  var videoDevice = SpecialPowers.getCharPref('media.video_loopback_dev');
  dump('TEST DEVICES: Using media devices:\n');
  dump('audio: ' + audioDevice + '\nvideo: ' + videoDevice + '\n');
  FAKE_ENABLED = false;
} catch (e) {
  dump('TEST DEVICES: No test devices found (in media.{audio,video}_loopback_dev, using fake streams.\n');
  FAKE_ENABLED = true;
}


/**
 * Create the necessary HTML elements for head and body as used by Mochitests
 *
 * @param {object} meta
 *        Meta information of the test
 * @param {string} meta.title
 *        Description of the test
 * @param {string} [meta.bug]
 *        Bug the test was created for
 * @param {boolean} [meta.visible=false]
 *        Visibility of the media elements
 */
function createHTML(meta) {
  var test = document.getElementById('test');

  // Create the head content
  var elem = document.createElement('meta');
  elem.setAttribute('charset', 'utf-8');
  document.head.appendChild(elem);

  var title = document.createElement('title');
  title.textContent = meta.title;
  document.head.appendChild(title);

  // Create the body content
  var anchor = document.createElement('a');
  anchor.setAttribute('target', '_blank');

  if (meta.bug) {
    anchor.setAttribute('href', 'https://bugzilla.mozilla.org/show_bug.cgi?id=' + meta.bug);
  }

  anchor.textContent = meta.title;
  document.body.insertBefore(anchor, test);

  var display = document.createElement('p');
  display.setAttribute('id', 'display');
  document.body.insertBefore(display, test);

  var content = document.createElement('div');
  content.setAttribute('id', 'content');
  content.style.display = meta.visible ? 'block' : "none";
  document.body.appendChild(content);
}


/**
 * Create the HTML element if it doesn't exist yet and attach
 * it to the content node.
 *
 * @param {string} type
 *        Type of media element to create ('audio' or 'video')
 * @param {string} label
 *        Description to use for the element
 * @return {HTMLMediaElement} The created HTML media element
 */
function createMediaElement(type, label) {
  var id = label + '_' + type;
  var element = document.getElementById(id);

  // Sanity check that we haven't created the element already
  if (element)
    return element;

  element = document.createElement(type === 'audio' ? 'audio' : 'video');
  element.setAttribute('id', id);
  element.setAttribute('height', 100);
  element.setAttribute('width', 150);
  element.setAttribute('controls', 'controls');
  document.getElementById('content').appendChild(element);

  return element;
}


/**
 * Wrapper function for mozGetUserMedia to allow a singular area of control
 * for determining whether we run this with fake devices or not.
 *
 * @param {Dictionary} constraints
 *        The constraints for this mozGetUserMedia callback
 * @param {Function} onSuccess
 *        The success callback if the stream is successfully retrieved
 * @param {Function} onError
 *        The error callback if the stream fails to be retrieved
 */
function getUserMedia(constraints, onSuccess, onError) {
  if (!("fake" in constraints) && FAKE_ENABLED) {
    constraints["fake"] = FAKE_ENABLED;
  }

  info("Call getUserMedia for " + JSON.stringify(constraints));
  navigator.mozGetUserMedia(constraints, onSuccess, onError);
}


/**
 * Setup any Mochitest for WebRTC by enabling the preference for
 * peer connections. As by bug 797979 it will also enable mozGetUserMedia()
 * and disable the mozGetUserMedia() permission checking.
 *
 * @param {Function} aCallback
 *        Test method to execute after initialization
 */
function runTest(aCallback) {
  if (window.SimpleTest) {
    // Running as a Mochitest.
    SimpleTest.waitForExplicitFinish();
    SpecialPowers.pushPrefEnv({'set': [
      ['dom.messageChannel.enabled', true],
      ['media.peerconnection.enabled', true],
      ['media.peerconnection.identity.enabled', true],
      ['media.peerconnection.identity.timeout', 12000],
      ['media.navigator.permission.disabled', true]]
    }, function () {
      try {
        aCallback();
      }
      catch (err) {
        generateErrorCallback()(err);
      }
    });
  } else {
    // Steeplechase, let it call the callback.
    window.run_test = function(is_initiator) {
      var options = {is_local: is_initiator,
                     is_remote: !is_initiator};
      aCallback(options);
    };
    // Also load the steeplechase test code.
    var s = document.createElement("script");
    s.src = "/test.js";
    document.head.appendChild(s);
  }
}

/**
 * Checks that the media stream tracks have the expected amount of tracks
 * with the correct kind and id based on the type and constraints given.
 *
 * @param {Object} constraints specifies whether the stream should have
 *                             audio, video, or both
 * @param {String} type the type of media stream tracks being checked
 * @param {sequence<MediaStreamTrack>} mediaStreamTracks the media stream
 *                                     tracks being checked
 */
function checkMediaStreamTracksByType(constraints, type, mediaStreamTracks) {
  if(constraints[type]) {
    is(mediaStreamTracks.length, 1, 'One ' + type + ' track shall be present');

    if(mediaStreamTracks.length) {
      is(mediaStreamTracks[0].kind, type, 'Track kind should be ' + type);
      ok(mediaStreamTracks[0].id, 'Track id should be defined');
    }
  } else {
    is(mediaStreamTracks.length, 0, 'No ' + type + ' tracks shall be present');
  }
}

/**
 * Check that the given media stream contains the expected media stream
 * tracks given the associated audio & video constraints provided.
 *
 * @param {Object} constraints specifies whether the stream should have
 *                             audio, video, or both
 * @param {MediaStream} mediaStream the media stream being checked
 */
function checkMediaStreamTracks(constraints, mediaStream) {
  checkMediaStreamTracksByType(constraints, 'audio',
    mediaStream.getAudioTracks());
  checkMediaStreamTracksByType(constraints, 'video',
    mediaStream.getVideoTracks());
}

/**
 * Utility methods
 */

/**
 * Returns the contents of a blob as text
 *
 * @param {Blob} blob
          The blob to retrieve the contents from
 * @param {Function} onSuccess
          Callback with the blobs content as parameter
 */
function getBlobContent(blob, onSuccess) {
  var reader = new FileReader();

  // Listen for 'onloadend' which will always be called after a success or failure
  reader.onloadend = function (event) {
    onSuccess(event.target.result);
  };

  reader.readAsText(blob);
}

/**
 * Generates a callback function fired only under unexpected circumstances
 * while running the tests. The generated function kills off the test as well
 * gracefully.
 *
 * @param {String} [message]
 *        An optional message to show if no object gets passed into the
 *        generated callback method.
 */
function generateErrorCallback(message) {
  var stack = new Error().stack.split("\n");
  stack.shift(); // Don't include this instantiation frame

  /**
   * @param {object} aObj
   *        The object fired back from the callback
   */
  return function (aObj) {
    if (aObj) {
      if (aObj.name && aObj.message) {
        ok(false, "Unexpected callback for '" + aObj.name +
           "' with message = '" + aObj.message + "' at " +
           JSON.stringify(stack));
      } else {
        ok(false, "Unexpected callback with = '" + aObj +
           "' at: " + JSON.stringify(stack));
      }
    } else {
      ok(false, "Unexpected callback with message = '" + message +
         "' at: " + JSON.stringify(stack));
    }
    SimpleTest.finish();
  }
}

/**
 * Generates a callback function fired only for unexpected events happening.
 *
 * @param {String} description
          Description of the object for which the event has been fired
 * @param {String} eventName
          Name of the unexpected event
 */
function unexpectedEventAndFinish(message, eventName) {
  var stack = new Error().stack.split("\n");
  stack.shift(); // Don't include this instantiation frame

  return function () {
    ok(false, "Unexpected event '" + eventName + "' fired with message = '" +
       message + "' at: " + JSON.stringify(stack));
    SimpleTest.finish();
  }
}

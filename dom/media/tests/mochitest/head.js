/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = SpecialPowers.Cc;
var Ci = SpecialPowers.Ci;
var Cr = SpecialPowers.Cr;

// Specifies whether we are using fake streams to run this automation
var FAKE_ENABLED = true;


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
  constraints["fake"] = FAKE_ENABLED;

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
  SimpleTest.waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({'set': [
    ['media.peerconnection.enabled', true],
    ['media.navigator.permission.disabled', true]]
  }, function () {
    try {
      aCallback();
    }
    catch (err) {
      unexpectedCallbackAndFinish(new Error)(err);
    }
  });
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
 * Generates a callback function fired only under unexpected circumstances
 * while running the tests. The generated function kills off the test as well
 * gracefully.
 *
 * @param {Error} error
 *        A new Error object, generated at the callback site, from which a
 *        filename and line number can be extracted for diagnostic purposes
 */
function unexpectedCallbackAndFinish(error) {
  /**
   * @param {object} aObj
   *        The object fired back from the callback
   */
  return function(aObj) {
    var where = error.fileName + ":" + error.lineNumber;
    if (aObj && aObj.name && aObj.message) {
      ok(false, "Unexpected error callback from " + where + " with name = '" +
                aObj.name + "', message = '" + aObj.message + "'");
    } else {
      ok(false, "Unexpected error callback from " + where + " with " + aObj);
    }
    SimpleTest.finish();
  }
}

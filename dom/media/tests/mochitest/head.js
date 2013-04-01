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
 * @param {Boolean} desktopSupportedOnly
 *        Specifies if the test currently is known to work on desktop only
 */
function runTest(aCallback, desktopSupportedOnly) {
  SimpleTest.waitForExplicitFinish();

  // If this is a desktop supported test and we're on android or b2g,
  // indicate that the test is not supported and skip the test
  if(desktopSupportedOnly && (navigator.userAgent.indexOf('Android') > -1 ||
     navigator.platform === '')) {
    ok(true, navigator.userAgent + ' currently not supported');
    SimpleTest.finish();
  } else {
    SpecialPowers.pushPrefEnv({'set': [
        ['media.peerconnection.enabled', true],
        ['media.navigator.permission.denied', true]]
      }, function () {
      try {
        aCallback();
      }
      catch (err) {
        unexpectedCallbackAndFinish(err);
      }
    });
  }
}


/**
 * A callback function fired only under unexpected circumstances while
 * running the tests. Kills off the test as well gracefully.
 *
 * @param {object} aObj
 *        The object fired back from the callback
 */
function unexpectedCallbackAndFinish(aObj) {
  if (aObj && aObj.name && aObj.message) {
    ok(false, "Unexpected error callback with name = '" + aObj.name +
              "', message = '" + aObj.message + "'");
  } else {
     ok(false, "Unexpected error callback with " + aObj);
  }
  SimpleTest.finish();
}

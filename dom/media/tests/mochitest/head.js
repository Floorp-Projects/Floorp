/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = SpecialPowers.Cc;
var Ci = SpecialPowers.Ci;
var Cr = SpecialPowers.Cr;

// Specifies whether we are using fake streams to run this automation
var FAKE_ENABLED = true;

/**
 * Setup any Mochitest for WebRTC by enabling the preference for
 * peer connections. As by bug 797979 it will also enable mozGetUserMedia().
 *
 * @param {Function} aCallback Test method to execute after initialization
 * @param {Boolean} desktopSupportedOnly specifies if the test currently
 *                                       is known to work on desktop only
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
      ['media.navigator.permission.denied', true]]}, function () {
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
 * Wrapper function for mozGetUserMedia to allow a singular area of control
 * for determining whether we run this with fake devices or not.
 *
 * @param {Dictionary} constraints the constraints for this mozGetUserMedia
 *                                 callback
 * @param {Function} onSuccess the success callback if the stream is
 *                             successfully retrieved
 * @param {Function} onError the error callback if the stream fails to be
 *                           retrieved
 */
function getUserMedia(constraints, onSuccess, onError) {
  constraints["fake"] = FAKE_ENABLED;
  navigator.mozGetUserMedia(constraints, onSuccess, onError);
}

/**
 * A callback function fired only under unexpected circumstances while
 * running the tests. Kills off the test as well gracefully.
 *
 * @param {object} aObj The object fired back from the callback
 */
function unexpectedCallbackAndFinish(aObj) {
  ok(false, "Unexpected error callback with " + aObj);
  SimpleTest.finish();
}

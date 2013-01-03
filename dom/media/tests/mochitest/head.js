/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = SpecialPowers.Cc;
var Ci = SpecialPowers.Ci;
var Cr = SpecialPowers.Cr;

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
    SpecialPowers.pushPrefEnv({'set': [['media.peerconnection.enabled', true]]}, function () {
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
 * @param {String} obj the object fired back from the callback
 */
function unexpectedCallbackAndFinish(obj) {
  ok(false, "Unexpected error callback with " + obj);
  SimpleTest.finish();
}

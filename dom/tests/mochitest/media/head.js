/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = SpecialPowers.Cc;
var Ci = SpecialPowers.Ci;
var Cr = SpecialPowers.Cr;

/**
 * Setup any Mochitest for WebRTC by enabling the preference for
 * peer connections. As by bug 797979 it will also enable mozGetUserMedia().
 * Additionally, we disable the permissions prompt for these mochitests.
 *
 * @param {Function} aCallback Test method to execute after initialization
 */
function runTest(aCallback) {
  SimpleTest.waitForExplicitFinish();

  SpecialPowers.pushPrefEnv({'set': [['media.peerconnection.enabled', true],
                            ['media.navigator.permission.disabled', true]]},
                            aCallback);
}

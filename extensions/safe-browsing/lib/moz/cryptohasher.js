/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


// A very thin wrapper around nsICryptoHash. It's not strictly
// necessary, but makes the code a bit cleaner and gives us the
// opportunity to verify that our implementations give the results that
// we expect, for example if we have to interoperate with a server.
//
// The digest* methods reset the state of the hasher, so it's
// necessary to call init() explicitly after them.
//
// Works only in Firefox 1.5+.
//
// IMPORTANT NOTE: Due to https://bugzilla.mozilla.org/show_bug.cgi?id=321024
// you cannot use the cryptohasher before app-startup. The symptom of doing
// so is a segfault in NSS.

/**
 * Instantiate a new hasher. You must explicitly call init() before use!
 */
function G_CryptoHasher() {
  this.debugZone = "cryptohasher";
  this.hasher_ = Cc["@mozilla.org/security/hash;1"]
                 .createInstance(Ci.nsICryptoHash);
  this.initialized_ = false;
}

G_CryptoHasher.algorithms = {
  MD2: Ci.nsICryptoHash.MD2,
  MD5: Ci.nsICryptoHash.MD5,
  SHA1: Ci.nsICryptoHash.SHA1,
  SHA256: Ci.nsICryptoHash.SHA256,
  SHA384: Ci.nsICryptoHash.SHA384,
  SHA512: Ci.nsICryptoHash.SHA512,
};

/**
 * Initialize the hasher. This function must be called after every call
 * to one of the digest* methods.
 *
 * @param algorithm Constant from G_CryptoHasher.algorithms specifying the
 *                  algorithm this hasher will use
 */ 
G_CryptoHasher.prototype.init = function(algorithm) {
  var validAlgorithm = false;
  for (var alg in G_CryptoHasher.algorithms)
    if (algorithm == G_CryptoHasher.algorithms[alg])
      validAlgorithm = true;

  if (!validAlgorithm)
    throw new Error("Invalid algorithm: " + algorithm);

  this.initialized_ = true;
  this.hasher_.init(algorithm);
}

/**
 * Update the hash's internal state with input given in a string. Can be
 * called multiple times for incrementeal hash updates.
 *
 * @param input String containing data to hash.
 */ 
G_CryptoHasher.prototype.updateFromString = function(input) {
  if (!this.initialized_)
    throw new Error("You must initialize the hasher first!");
  this.hasher_.update(input.split(""), input.length);
}

/**
 * Update the hash's internal state with input given in an array. Can be
 * called multiple times for incrementeal hash updates.
 *
 * @param input Array containing data to hash.
 */ 
G_CryptoHasher.prototype.updateFromArray = function(input) {
  if (!this.initialized_)
    throw new Error("You must initialize the hasher first!");
  this.hasher_.update(input, input.length);
}

/**
 * @returns The hash value as a string (sequence of 8-bit values)
 */ 
G_CryptoHasher.prototype.digestRaw = function() {
  return this.hasher_.finish(false /* not b64 encoded */);
}

/**
 * @returns The hash value as a base64-encoded string
 */ 
G_CryptoHasher.prototype.digestBase64 = function() {
  return this.hasher_.finish(true /* b64 encoded */);
}

/**
 * @returns The hash value as a hex-encoded string
 */ 
G_CryptoHasher.prototype.digestHex = function() {
  var raw = this.digestRaw();
  return this.toHex_(raw);
}

/**
 * Converts a sequence of values to a hex-encoded string. The input is a
 * a string, so you can stick 16-bit values in each character.
 *
 * @param str String to conver to hex. (Often this is just a sequence of
 *            16-bit values)
 *
 * @returns String containing the hex representation of the input
 */ 
G_CryptoHasher.prototype.toHex_ = function(str) {
  var hexchars = '0123456789ABCDEF';
  var hexrep = new Array(str.length * 2);

  for (var i = 0; i < str.length; ++i) {
    hexrep[i * 2] = hexchars.charAt((str.charCodeAt(i) >> 4) & 15);
    hexrep[i * 2 + 1] = hexchars.charAt(str.charCodeAt(i) & 15);
  }
  return hexrep.join('');
}

/**
 * Lame unittest function
 */
function TEST_G_CryptoHasher() {
  if (G_GDEBUG) {
    var z = "cryptohasher UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");  

    /* Add test vectors 
     * var hasher = new G_CryptoHasher();
     * hasher.updateFromtring("0");
     * hasher.updateFromtring("1");
     * etc
     * var digest = hasher.digestHex();
     */

    G_Debug(z, "PASSED");
  }
}

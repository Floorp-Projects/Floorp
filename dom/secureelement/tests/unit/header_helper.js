/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Strips spaces, and returns a byte array.
 */
function formatHexAndCreateByteArray(hexStr) {
  return SEUtils.hexStringToByteArray(hexStr.replace(/\s+/g, ""));
}

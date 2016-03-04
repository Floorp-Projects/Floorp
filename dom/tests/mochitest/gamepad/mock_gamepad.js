/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
var GamepadService = (function() {
  return SpecialPowers.Cc["@mozilla.org/gamepad-test;1"].getService(SpecialPowers.Ci.nsIGamepadServiceTest);
})();

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var GamepadService;

GamepadService = SpecialPowers.Cc["@mozilla.org/gamepad-test;1"].getService(SpecialPowers.Ci.nsIGamepadServiceTest);


var addGamepad = function(name, mapping, buttons, axes) {
  return new Promise((resolve, reject) => {
    resolve(GamepadService.addGamepad(name, mapping, buttons, axes));
  });
}

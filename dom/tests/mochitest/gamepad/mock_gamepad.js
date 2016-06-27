/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var GamepadService;

function runGamepadTest (callback) {
  SpecialPowers.pushPrefEnv({"set" : [["dom.gamepad.test.enabled", true]]},
                      () => {
                        GamepadService = navigator.requestGamepadServiceTest();
                        callback();
                      });
}
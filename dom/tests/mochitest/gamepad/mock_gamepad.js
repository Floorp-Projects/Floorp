/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Determine whether this process is a parent or child process.
let appInfo = SpecialPowers.Cc["@mozilla.org/xre/app-info;1"];
let isParentProcess =
      !appInfo || appInfo.getService(SpecialPowers.Ci.nsIXULRuntime)
  .processType == SpecialPowers.Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

var GamepadService;
var GamepadScript;

// sendAsyncMessage fails when running in process, so if we're not e10s, just
// use GamepadServiceTest directly.
if (isParentProcess) {
  GamepadService = SpecialPowers.Cc["@mozilla.org/gamepad-test;1"].getService(SpecialPowers.Ci.nsIGamepadServiceTest);
} else {
  // If we're e10s, use the proxy script to access GamepadServiceTest in the
  // parent process.
  let url = SimpleTest.getTestFileURL("gamepad_service_test_chrome_script.js");
  GamepadScript = SpecialPowers.loadChromeScript(url);

  GamepadService = {
    addGamepad: function (name, mapping, buttons, axes) {
      GamepadScript.sendAsyncMessage("add-gamepad", {
        name: name,
        mapping: mapping,
        buttons: buttons,
        axes: axes
      });
    },
    newButtonEvent: function (index, button, status) {
      GamepadScript.sendAsyncMessage("new-button-event", {
        index: index,
        button: button,
        status: status
      });
    },
    newButtonValueEvent: function (index, button, status, value) {
      GamepadScript.sendAsyncMessage("new-button-value-event", {
        index: index,
        button: button,
        status: status,
        value: value
      });
    },
    removeGamepad: function (index) {
      GamepadScript.sendAsyncMessage("remove-gamepad", {
        index: index
      });
    },
    finish: function () {
      GamepadScript.destroy();
    }
  };
}

var addGamepad = function(name, mapping, buttons, axes) {
  return new Promise((resolve, reject) => {
    if (isParentProcess) {
      resolve(GamepadService.addGamepad(name, mapping, buttons, axes));
    } else {
      var listener = (index) => {
        GamepadScript.removeMessageListener('new-gamepad-index', listener);
        resolve(index);
      };
      GamepadScript.addMessageListener('new-gamepad-index', listener);
      GamepadService.addGamepad(name, mapping, buttons, axes);
    }
  });
}

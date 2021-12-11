/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var GamepadService;

async function setGamepadPreferenceAndCreateIframe(iframeSrc) {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.gamepad.test.enabled", true]],
  });

  let iframe = document.createElement("iframe");
  iframe.src = iframeSrc;
  document.body.appendChild(iframe);
}

function runGamepadTest(callback) {
  GamepadService = navigator.requestGamepadServiceTest();
  callback();
}

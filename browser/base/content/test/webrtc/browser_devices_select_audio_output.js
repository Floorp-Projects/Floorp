/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(2);

const permissionError =
  "error: NotAllowedError: The request is not allowed " +
  "by the user agent or the platform in the current context.";

async function requestAudioOutputExpectingPrompt() {
  await Promise.all([
    promisePopupNotificationShown("webRTC-shareDevices"),
    expectObserverCalled("getUserMedia:request"),
    expectObserverCalled("recording-window-ended"),
    promiseRequestAudioOutput(),
  ]);

  is(
    PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
    "webRTC-shareSpeaker-notification-icon",
    "anchored to device icon"
  );
  checkDeviceSelectors(["speaker"]);
}

async function simulateAudioOutputRequest() {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function simPrompt() {
    const req = {
      type: "selectaudiooutput",
      windowID: content.windowGlobalChild.outerWindowId,
      devices: [
        {
          type: "audiooutput",
          rawName: "name 1",
          deviceIndex: 1,
          rawId: "id 1",
          QueryInterface: ChromeUtils.generateQI([Ci.nsIMediaDevice]),
        },
      ],
      getConstraints: () => ({}),
      isSecure: true,
      isHandlingUserInput: true,
    };
    const { WebRTCChild } = SpecialPowers.ChromeUtils.import(
      "resource:///actors/WebRTCChild.jsm"
    );
    WebRTCChild.observe(req, "getUserMedia:request");
  });
}

async function allow() {
  const observerPromise = expectObserverCalled("getUserMedia:response:allow");
  await promiseMessage("ok", () => {
    PopupNotifications.panel.firstElementChild.button.click();
  });
  await observerPromise;
}

async function deny() {
  const observerPromise = expectObserverCalled("getUserMedia:response:deny");
  await promiseMessage(permissionError, () => {
    activateSecondaryAction(kActionDeny);
  });
  await observerPromise;
}

async function escapePrompt() {
  const observerPromise = expectObserverCalled("getUserMedia:response:deny");
  EventUtils.synthesizeKey("KEY_Escape");
  await observerPromise;
}

async function escape() {
  await Promise.all([promiseMessage(permissionError), escapePrompt()]);
}

var gTests = [
  {
    desc: 'User clicks "Allow"',
    run: async function checkAllow() {
      await requestAudioOutputExpectingPrompt();
      await allow();
      info("selectAudioOutput() with no deviceId again should prompt again.");
      await requestAudioOutputExpectingPrompt();
      await allow();
    },
  },
  {
    desc: 'User clicks "Block"',
    run: async function checkBlock() {
      await requestAudioOutputExpectingPrompt();
      await deny();
    },
  },
  {
    desc: 'User presses "Esc"',
    run: async function checkEsc() {
      await requestAudioOutputExpectingPrompt();
      await escape();
      info("selectAudioOutput() after Esc should prompt again.");
      await requestAudioOutputExpectingPrompt();
      await allow();
    },
  },
  {
    desc: "Single Device",
    run: async function checkSingle() {
      await Promise.all([
        promisePopupNotificationShown("webRTC-shareDevices"),
        simulateAudioOutputRequest(),
      ]);
      checkDeviceSelectors(["speaker"]);
      await escapePrompt();
    },
  },
];

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({ set: [["media.setsinkid.enabled", true]] });
  await runTests(gTests);
});

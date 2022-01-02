/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var gTests = [
  {
    desc: "test queueing allow video behind allow video",
    run: async function testQueuingAllowVideoBehindAllowVideo() {
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promiseRequestDevice(false, true);
      await promise;
      checkDeviceSelectors(["camera"]);
      await observerPromise;

      let promises = [
        expectObserverCalled("getUserMedia:request"),
        expectObserverCalled("getUserMedia:response:allow", 2),
        expectObserverCalled("recording-device-events", 2),
      ];

      await promiseMessage(
        "ok",
        () => {
          PopupNotifications.panel.firstElementChild.button.click();
        },
        2
      );
      await Promise.all(promises);

      await promiseNoPopupNotification("webRTC-shareDevices");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { video: true },
        "expected camera to be shared"
      );

      // close all streams
      await closeStream();
    },
  },
];

add_task(async function test() {
  SimpleTest.requestCompleteLog();
  await runTests(gTests);
});

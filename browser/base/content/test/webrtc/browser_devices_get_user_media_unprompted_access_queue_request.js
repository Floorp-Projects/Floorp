/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var gTests = [

{
  desc: "test queueing allow video behind allow video",
  run: async function testQueuingAllowVideoBehindAllowVideo() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true);
    await promiseRequestDevice(false, true);
    await promise;
    checkDeviceSelectors(false, true);
    await expectObserverCalled("getUserMedia:request");

    let promiseOK = promiseSpecificMessageReceived("ok", 2);
    PopupNotifications.panel.firstChild.button.click();
    await promiseOK;

    await promiseNoPopupNotification("webRTC-shareDevices");
    await expectObserverCalled("getUserMedia:request");
    await expectObserverCalled("getUserMedia:response:allow", 2);
    Assert.deepEqual((await getMediaCaptureState()), {video: true},
                     "expected camera to be shared");
    await expectObserverCalled("recording-device-events", 2);

    // close all streams
    await closeStream();
  }
}

];

add_task(async function test() {
  SimpleTest.requestCompleteLog();
  await runTests(gTests);
});

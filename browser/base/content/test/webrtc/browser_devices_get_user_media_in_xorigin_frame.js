/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTests = [
  {
    desc: "'Always Allow' disabled on third party pages",
    run: async function checkNoAlwaysOnThirdParty() {
      // Initially set both permissions to 'allow'.
      let Perms = Services.perms;
      let uri = Services.io.newURI("https://test1.example.com/");
      Perms.add(uri, "microphone", Perms.ALLOW_ACTION);
      Perms.add(uri, "camera", Perms.ALLOW_ACTION);

      // Request devices and expect a prompt despite the saved 'Allow' permission,
      // because we're a third party.
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      // Ensure that the 'Remember this decision' checkbox is absent.
      let notification = PopupNotifications.panel.firstElementChild;
      let checkbox = notification.checkbox;
      ok(!!checkbox, "checkbox is present");
      ok(checkbox.hidden, "checkbox is not visible");
      ok(!checkbox.checked, "checkbox not checked");

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () =>
        EventUtils.synthesizeMouseAtCenter(notification.button, {})
      );
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );
      await indicator;
      await checkSharingUI({ audio: true, video: true });

      // Cleanup.
      await closeStream(false, "frame1");
      Perms.remove(uri, "camera");
      Perms.remove(uri, "microphone");
    },
  },
  {
    desc: "'Always Allow' disabled when sharing screen in third party iframes",
    run: async function checkScreenSharing() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, "frame1", "screen");
      await promise;
      await expectObserverCalled("getUserMedia:request");

      checkDeviceSelectors(false, false, true);
      let notification = PopupNotifications.panel.firstElementChild;
      let iconclass = notification.getAttribute("iconclass");
      ok(iconclass.includes("screen-icon"), "panel using screen icon");

      // Ensure that the 'Remember this decision' checkbox is absent.
      let checkbox = notification.checkbox;
      ok(!!checkbox, "checkbox is present");
      ok(checkbox.hidden, "checkbox is not visible");
      ok(!checkbox.checked, "checkbox not checked");

      let menulist = document.getElementById("webRTC-selectWindow-menulist");
      let count = menulist.itemCount;
      ok(
        count >= 4,
        "There should be the 'Select Window or Screen' item, a separator and at least one window and one screen"
      );

      let noWindowOrScreenItem = menulist.getItemAtIndex(0);
      ok(
        noWindowOrScreenItem.hasAttribute("selected"),
        "the 'Select Window or Screen' item is selected"
      );
      is(
        menulist.selectedItem,
        noWindowOrScreenItem,
        "'Select Window or Screen' is the selected item"
      );
      is(menulist.value, -1, "no window or screen is selected by default");
      ok(
        noWindowOrScreenItem.disabled,
        "'Select Window or Screen' item is disabled"
      );
      ok(notification.button.disabled, "Allow button is disabled");
      ok(
        notification.hasAttribute("invalidselection"),
        "Notification is marked as invalid"
      );

      menulist.getItemAtIndex(count - 1).doCommand();
      ok(!notification.button.disabled, "Allow button is enabled");

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () =>
        EventUtils.synthesizeMouseAtCenter(notification.button, {})
      );
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { screen: "Screen" },
        "expected screen to be shared"
      );

      await indicator;
      await checkSharingUI({ screen: "Screen" });
      await closeStream(false, "frame1");
    },
  },
];

add_task(async function test() {
  await runTests(gTests, {
    relativeURI: "get_user_media_in_xorigin_frame.html",
  });
});

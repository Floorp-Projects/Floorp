/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const permissionError = "error: NotAllowedError: The request is not allowed " +
    "by the user agent or the platform in the current context.";

const notFoundError =
    "error: NotFoundError: The object can not be found here.";

var gTests = [

{
  desc: "getUserMedia screen only",
  run: async function checkScreenOnly() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true, null, "screen");
    await promise;
    await expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareScreen-notification-icon", "anchored to device icon");
    checkDeviceSelectors(false, false, true);
    let iconclass =
      PopupNotifications.panel.firstChild.getAttribute("iconclass");
    ok(iconclass.includes("screen-icon"), "panel using screen icon");

    let menulist =
      document.getElementById("webRTC-selectWindow-menulist");
    let count = menulist.itemCount;
    ok(count >= 3,
       "There should be the 'No Screen' item, a separator and at least one screen");

    let noScreenItem = menulist.getItemAtIndex(0);
    ok(noScreenItem.hasAttribute("selected"), "the 'No Screen' item is selected");
    is(menulist.value, -1, "no screen is selected by default");
    is(menulist.selectedItem, noScreenItem, "'No Screen' is the selected item");

    let separator = menulist.getItemAtIndex(1);
    is(separator.localName, "menuseparator", "the second item is a separator");

    ok(document.getElementById("webRTC-all-windows-shared").hidden,
       "the 'all windows will be shared' warning should be hidden while there's no selection");
    ok(document.getElementById("webRTC-preview").hidden,
       "the preview area is hidden");

    for (let i = 2; i < count; ++i) {
      let item = menulist.getItemAtIndex(i);
      is(parseInt(item.getAttribute("value")), i - 2, "the screen item has the correct index");
      is(item.getAttribute("devicetype"), "Screen", "the devicetype attribute is set correctly");
      ok(item.scary, "the screen item is marked as scary");
    }

    // Select a screen, a preview with a scary warning should appear.
    menulist.getItemAtIndex(2).doCommand();
    ok(!document.getElementById("webRTC-all-windows-shared").hidden,
       "the 'all windows will be shared' warning should now be visible");
    await promiseWaitForCondition(() => !document.getElementById("webRTC-preview").hidden, 100);
    ok(!document.getElementById("webRTC-preview").hidden,
       "the preview area is visible");
    ok(!document.getElementById("webRTC-previewWarning").hidden,
       "the scary warning is visible");

    // Select the 'No Screen' item again, the preview should be hidden.
    menulist.getItemAtIndex(0).doCommand();
    ok(document.getElementById("webRTC-all-windows-shared").hidden,
       "the 'all windows will be shared' warning should now be hidden");
    ok(document.getElementById("webRTC-preview").hidden,
       "the preview area is hidden");

    // Select the first screen again so that we can have a stream.
    menulist.getItemAtIndex(2).doCommand();

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {screen: "Screen"},
                     "expected screen to be shared");

    await indicator;
    await checkSharingUI({screen: "Screen"});
    await closeStream();
  }
},

{
  desc: "getUserMedia window only",
  run: async function checkWindowOnly() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true, null, "window");
    await promise;
    await expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareScreen-notification-icon", "anchored to device icon");
    checkDeviceSelectors(false, false, true);
    let iconclass =
      PopupNotifications.panel.firstChild.getAttribute("iconclass");
    ok(iconclass.includes("screen-icon"), "panel using screen icon");

    let menulist =
      document.getElementById("webRTC-selectWindow-menulist");
    let count = menulist.itemCount;
    ok(count >= 3,
       "There should be the 'No Window' item, a separator and at least one window");

    let noWindowItem = menulist.getItemAtIndex(0);
    ok(noWindowItem.hasAttribute("selected"), "the 'No Window' item is selected");
    is(menulist.value, -1, "no window is selected by default");
    is(menulist.selectedItem, noWindowItem, "'No Window' is the selected item");

    let separator = menulist.getItemAtIndex(1);
    is(separator.localName, "menuseparator", "the second item is a separator");

    ok(document.getElementById("webRTC-all-windows-shared").hidden,
       "the 'all windows will be shared' warning should be hidden");
    ok(document.getElementById("webRTC-preview").hidden,
       "the preview area is hidden");

    let scaryIndex, nonScaryIndex;
    for (let i = 2; i < count; ++i) {
      let item = menulist.getItemAtIndex(i);
      is(parseInt(item.getAttribute("value")), i - 2, "the window item has the correct index");
      is(item.getAttribute("devicetype"), "Window", "the devicetype attribute is set correctly");
      if (item.scary)
        scaryIndex = i;
      else
        nonScaryIndex = i;
    }
    ok(typeof scaryIndex == "number", "there's at least one scary window, as Firefox is running");

    // Select a scary window, a preview with a scary warning should appear.
    menulist.getItemAtIndex(scaryIndex).doCommand();
    ok(document.getElementById("webRTC-all-windows-shared").hidden,
       "the 'all windows will be shared' warning should still be hidden");
    await promiseWaitForCondition(() => !document.getElementById("webRTC-preview").hidden);
    ok(!document.getElementById("webRTC-preview").hidden,
       "the preview area is visible");
    ok(!document.getElementById("webRTC-previewWarning").hidden,
       "the scary warning is visible");

    // Select the 'No Window' item again, the preview should be hidden.
    menulist.getItemAtIndex(0).doCommand();
    ok(document.getElementById("webRTC-preview").hidden,
       "the preview area is hidden");

    // If we have a non-scary window, select it and verify the warning isn't displayed.
    // A non-scary window may not always exist on test slaves.
    if (typeof nonScaryIndex == "number") {
      menulist.getItemAtIndex(nonScaryIndex).doCommand();
      ok(document.getElementById("webRTC-all-windows-shared").hidden,
         "the 'all windows will be shared' warning should still be hidden");
      await promiseWaitForCondition(() => !document.getElementById("webRTC-preview").hidden);
      ok(!document.getElementById("webRTC-preview").hidden,
         "the preview area is visible");
      ok(document.getElementById("webRTC-previewWarning").hidden,
         "the scary warning is hidden");
    } else {
      info("no non-scary window available on this test slave");

      // Select the first window again so that we can have a stream.
      menulist.getItemAtIndex(scaryIndex).doCommand();
    }

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {screen: "Window"},
                     "expected window to be shared");

    await indicator;
    await checkSharingUI({screen: "Window"});
    await closeStream();
  }
},

{
  desc: "getUserMedia application only",
  run: async function checkAppOnly() {
    if (AppConstants.platform == "linux") {
      todo(false, "Bug 1323490 - On Linux 64, this test fails with 'NotFoundError' and causes the next test (screen+audio) to timeout");
      return;
    }

    let promptPromise = promisePopupNotificationShown("webRTC-shareDevices");
    let messagePromise = promiseMessageReceived();
    await promiseRequestDevice(false, true, null, "application");
    let message = await Promise.race([promptPromise, messagePromise]);
    if (message) {
      is(message, notFoundError,
         "NotFoundError, likely because there's no other application running.");
      return;
    }

    await expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareScreen-notification-icon", "anchored to device icon");
    checkDeviceSelectors(false, false, true);
    let iconclass =
      PopupNotifications.panel.firstChild.getAttribute("iconclass");
    ok(iconclass.includes("screen-icon"), "panel using screen icon");

    let menulist =
      document.getElementById("webRTC-selectWindow-menulist");
    let count = menulist.itemCount;
    ok(count >= 3,
       "There should be the 'No Application' item, a separator and at least one application");

    let noApplicationItem = menulist.getItemAtIndex(0);
    ok(noApplicationItem.hasAttribute("selected"), "the 'No Application' item is selected");
    is(menulist.value, -1, "no app is selected by default");
    is(menulist.selectedItem, noApplicationItem, "'No Application' is the selected item");

    let separator = menulist.getItemAtIndex(1);
    is(separator.localName, "menuseparator", "the second item is a separator");

    ok(document.getElementById("webRTC-all-windows-shared").hidden,
       "the 'all windows will be shared' warning should be hidden");
    ok(document.getElementById("webRTC-preview").hidden,
       "the preview area is hidden");

    let scaryIndex;
    for (let i = 2; i < count; ++i) {
      let item = menulist.getItemAtIndex(i);
      is(parseInt(item.getAttribute("value")), i - 2, "the app item has the correct index");
      is(item.getAttribute("devicetype"), "Application", "the devicetype attribute is set correctly");
      if (item.scary)
        scaryIndex = i;
    }
    ok(scaryIndex === undefined,
       "Firefox is currently excluding itself from the list of applications, " +
       "so no scary app should be listed");

    menulist.getItemAtIndex(2).doCommand();
    ok(document.getElementById("webRTC-all-windows-shared").hidden,
       "the 'all windows will be shared' warning should still be hidden");
    await promiseWaitForCondition(() => !document.getElementById("webRTC-preview").hidden);
    ok(!document.getElementById("webRTC-preview").hidden,
       "the preview area is visible");
    ok(document.getElementById("webRTC-previewWarning").hidden,
       "the scary warning is hidden");

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {screen: "Application"},
                     "expected application to be shared");

    await indicator;
    await checkSharingUI({screen: "Application"});
    await closeStream();
  }
},

{
  desc: "getUserMedia audio+screen",
  run: async function checkAudioVideo() {
    if (AppConstants.platform == "macosx") {
      todo(false, "Bug 1323481 - On Mac on treeherder, but not locally, requesting microphone + screen never makes the permission prompt appear, and so causes the test to timeout");
      return;
    }

    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(true, true, null, "screen");
    await promise;
    await expectObserverCalled("getUserMedia:request");

    is(PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
       "webRTC-shareScreen-notification-icon", "anchored to device icon");
    checkDeviceSelectors(true, false, true);
    let iconclass =
      PopupNotifications.panel.firstChild.getAttribute("iconclass");
    ok(iconclass.includes("screen-icon"), "panel using screen icon");

    let menulist =
      document.getElementById("webRTC-selectWindow-menulist");
    let count = menulist.itemCount;
    ok(count >= 3,
       "There should be the 'No Screen' item, a separator and at least one screen");

    // Select a screen, a preview with a scary warning should appear.
    menulist.getItemAtIndex(2).doCommand();
    ok(!document.getElementById("webRTC-all-windows-shared").hidden,
       "the 'all windows will be shared' warning should now be visible");
    await promiseWaitForCondition(() => !document.getElementById("webRTC-preview").hidden);
    ok(!document.getElementById("webRTC-preview").hidden,
       "the preview area is visible");
    ok(!document.getElementById("webRTC-previewWarning").hidden,
       "the scary warning is visible");

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()),
                     {audio: true, screen: "Screen"},
                     "expected screen and microphone to be shared");

    await indicator;
    await checkSharingUI({audio: true, screen: "Screen"});
    await closeStream();
  }
},


{
  desc: "getUserMedia screen: clicking through without selecting a screen denies",
  run: async function checkClickThroughDenies() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true, null, "screen");
    await promise;
    await expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, false, true);

    await promiseMessage(permissionError, () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    await expectObserverCalled("getUserMedia:response:deny");
    await expectObserverCalled("recording-window-ended");
    await checkNotSharing();
    SitePermissions.remove(null, "screen", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "camera", gBrowser.selectedBrowser);
  }
},


{
  desc: "getUserMedia screen, user clicks \"Don't Allow\"",
  run: async function checkDontShare() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true, null, "screen");
    await promise;
    await expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, false, true);

    await promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });

    await expectObserverCalled("getUserMedia:response:deny");
    await expectObserverCalled("recording-window-ended");
    await checkNotSharing();
    SitePermissions.remove(null, "screen", gBrowser.selectedBrowser);
    SitePermissions.remove(null, "camera", gBrowser.selectedBrowser);
  }
},

{
  desc: "getUserMedia audio+video+screen: stop sharing",
  run: async function checkStopSharing() {
    if (AppConstants.platform == "macosx") {
      todo(false, "Bug 1323481 - On Mac on treeherder, but not locally, requesting microphone + screen never makes the permission prompt appear, and so causes the test to timeout");
      return;
    }

    async function share(audio, video, screen) {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(audio, video || !!screen,
                                 null, screen && "screen");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(audio, video, screen);
      if (screen) {
        document.getElementById("webRTC-selectWindow-menulist")
                .getItemAtIndex(2).doCommand();
      }
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
    }

    async function check(expected = {}) {
      let shared = Object.keys(expected).join(" and ");
      if (shared) {
        Assert.deepEqual((await getMediaCaptureState()), expected,
                         "expected " + shared + " to be shared");
        await checkSharingUI(expected);
      } else {
        await checkNotSharing();
      }
    }

    info("Share screen and microphone");
    let indicator = promiseIndicatorWindow();
    await share(true, false, true);
    await indicator;
    await check({audio: true, screen: "Screen"});

    info("Share camera");
    await share(false, true);
    await check({video: true, audio: true, screen: "Screen"});

    info("Stop the screen share, mic+cam should continue");
    await stopSharing("screen", true);
    await check({video: true, audio: true});

    info("Stop the camera, everything should stop.");
    await stopSharing("camera");

    info("Now, share only the screen...");
    indicator = promiseIndicatorWindow();
    await share(false, false, true);
    await indicator;
    await check({screen: "Screen"});

    info("... and add camera and microphone in a second request.");
    await share(true, true);
    await check({video: true, audio: true, screen: "Screen"});

    info("Stop the camera, this should stop everything.");
    await stopSharing("camera");
  }
},

{
  desc: "getUserMedia screen: reloading the page removes all gUM UI",
  run: async function checkReloading() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true, null, "screen");
    await promise;
    await expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, false, true);
    document.getElementById("webRTC-selectWindow-menulist")
            .getItemAtIndex(2).doCommand();

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {screen: "Screen"},
                     "expected screen to be shared");

    await indicator;
    await checkSharingUI({screen: "Screen"});

    await reloadAndAssertClosedStreams();
  }
},

{
  desc: "test showControlCenter from screen icon",
  run: async function checkShowControlCenter() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true, null, "screen");
    await promise;
    await expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, false, true);
    document.getElementById("webRTC-selectWindow-menulist")
            .getItemAtIndex(2).doCommand();

    let indicator = promiseIndicatorWindow();
    await promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    await expectObserverCalled("getUserMedia:response:allow");
    await expectObserverCalled("recording-device-events");
    Assert.deepEqual((await getMediaCaptureState()), {screen: "Screen"},
                     "expected screen to be shared");
    await indicator;
    await checkSharingUI({screen: "Screen"});

    ok(gIdentityHandler._identityPopup.hidden, "control center should be hidden");
    if ("nsISystemStatusBar" in Ci) {
      let activeStreams = webrtcUI.getActiveStreams(false, false, true);
      webrtcUI.showSharingDoorhanger(activeStreams[0]);
    } else {
      let win =
        Services.wm.getMostRecentWindow("Browser:WebRTCGlobalIndicator");
      let elt = win.document.getElementById("screenShareButton");
      EventUtils.synthesizeMouseAtCenter(elt, {}, win);
      await promiseWaitForCondition(() => !gIdentityHandler._identityPopup.hidden);
    }
    ok(!gIdentityHandler._identityPopup.hidden, "control center should be open");

    gIdentityHandler._identityPopup.hidden = true;
    await expectNoObserverCalled();

    await closeStream();
  }
},

{
  desc: "Only persistent block is possible for screen sharing",
  run: async function checkPersistentPermissions() {
    let browser = gBrowser.selectedBrowser;
    let uri = browser.documentURI;
    let devicePerms = SitePermissions.get(uri, "screen", browser);
    is(devicePerms.state, SitePermissions.UNKNOWN,
       "starting without screen persistent permissions");

    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true, null, "screen");
    await promise;
    await expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, false, true);
    document.getElementById("webRTC-selectWindow-menulist")
            .getItemAtIndex(2).doCommand();

    // Ensure that checking the 'Remember this decision' checkbox disables
    // 'Allow'.
    let notification = PopupNotifications.panel.firstChild;
    ok(notification.hasAttribute("warninghidden"), "warning message is hidden");
    let checkbox = notification.checkbox;
    ok(!!checkbox, "checkbox is present");
    ok(!checkbox.checked, "checkbox is not checked");
    checkbox.click();
    ok(checkbox.checked, "checkbox now checked");
    ok(notification.button.disabled, "Allow button is disabled");
    ok(!notification.hasAttribute("warninghidden"), "warning message is shown");

    // Click "Don't Allow" to save a persistent block permission.
    await promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });
    await expectObserverCalled("getUserMedia:response:deny");
    await expectObserverCalled("recording-window-ended");
    await checkNotSharing();

    let permission = SitePermissions.get(uri, "screen", browser);
    is(permission.state, SitePermissions.BLOCK,
       "screen sharing is blocked");
    is(permission.scope, SitePermissions.SCOPE_PERSISTENT,
       "screen sharing is persistently blocked");

    // Request screensharing again, expect an immediate failure.
    promise = promiseMessage(permissionError);
    await promiseRequestDevice(false, true, null, "screen");
    await promise;
    await expectObserverCalled("recording-window-ended");

    // Now set the permission to allow and expect a prompt.
    SitePermissions.set(uri, "screen", SitePermissions.ALLOW);

    // Request devices and expect a prompt despite the saved 'Allow' permission.
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    await promiseRequestDevice(false, true, null, "screen");
    await promise;
    await expectObserverCalled("getUserMedia:request");

    // The 'remember' checkbox shouldn't be checked anymore.
    notification = PopupNotifications.panel.firstChild;
    ok(notification.hasAttribute("warninghidden"), "warning message is hidden");
    checkbox = notification.checkbox;
    ok(!!checkbox, "checkbox is present");
    ok(!checkbox.checked, "checkbox is not checked");

    // Deny the request to cleanup...
    await promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });
    await expectObserverCalled("getUserMedia:response:deny");
    await expectObserverCalled("recording-window-ended");
    SitePermissions.remove(uri, "screen", browser);
  }
}

];

add_task(async function test() {
  await runTests(gTests);
});

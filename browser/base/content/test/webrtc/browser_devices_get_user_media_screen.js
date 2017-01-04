/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

registerCleanupFunction(function() {
  gBrowser.removeCurrentTab();
});

const permissionError = "error: NotAllowedError: The request is not allowed " +
    "by the user agent or the platform in the current context.";

const notFoundError =
    "error: NotFoundError: The object can not be found here.";

var gTests = [

{
  desc: "getUserMedia screen only",
  run: function* checkScreenOnly() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true, null, "screen");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

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
    yield promiseWaitForCondition(() => !document.getElementById("webRTC-preview").hidden);
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
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {screen: "Screen"},
                     "expected screen to be shared");

    yield indicator;
    yield checkSharingUI({screen: "Screen"});
    yield closeStream();
  }
},

{
  desc: "getUserMedia window only",
  run: function* checkWindowOnly() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true, null, "window");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

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
    yield promiseWaitForCondition(() => !document.getElementById("webRTC-preview").hidden);
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
      yield promiseWaitForCondition(() => !document.getElementById("webRTC-preview").hidden);
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
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {screen: "Window"},
                     "expected window to be shared");

    yield indicator;
    yield checkSharingUI({screen: "Window"});
    yield closeStream();
  }
},

{
  desc: "getUserMedia application only",
  run: function* checkAppOnly() {
    if (AppConstants.platform == "linux") {
      todo(false, "Bug 1323490 - On Linux 64, this test fails with 'NotFoundError' and causes the next test (screen+audio) to timeout");
      return;
    }

    let promptPromise = promisePopupNotificationShown("webRTC-shareDevices");
    let messagePromise = promiseMessageReceived();
    yield promiseRequestDevice(false, true, null, "application");
    let message = yield Promise.race([promptPromise, messagePromise]);
    if (message) {
      is(message, notFoundError,
         "NotFoundError, likely because there's no other application running.");
      return;
    }

    yield expectObserverCalled("getUserMedia:request");

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
    yield promiseWaitForCondition(() => !document.getElementById("webRTC-preview").hidden);
    ok(!document.getElementById("webRTC-preview").hidden,
       "the preview area is visible");
    ok(document.getElementById("webRTC-previewWarning").hidden,
       "the scary warning is hidden");

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {screen: "Application"},
                     "expected application to be shared");

    yield indicator;
    yield checkSharingUI({screen: "Application"});
    yield closeStream();
  }
},

{
  desc: "getUserMedia audio+screen",
  run: function* checkAudioVideo() {
    if (AppConstants.platform == "macosx") {
      todo(false, "Bug 1323481 - On Mac on treeherder, but not locally, requesting microphone + screen never makes the permission prompt appear, and so causes the test to timeout");
      return;
    }

    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(true, true, null, "screen");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

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
    yield promiseWaitForCondition(() => !document.getElementById("webRTC-preview").hidden);
    ok(!document.getElementById("webRTC-preview").hidden,
       "the preview area is visible");
    ok(!document.getElementById("webRTC-previewWarning").hidden,
       "the scary warning is visible");

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()),
                     {audio: true, screen: "Screen"},
                     "expected screen and microphone to be shared");

    yield indicator;
    yield checkSharingUI({audio: true, screen: "Screen"});
    yield closeStream();
  }
},


{
  desc: "getUserMedia screen: clicking through without selecting a screen denies",
  run: function* checkReloading() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true, null, "screen");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, false, true);

    yield promiseMessage(permissionError, () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    yield expectObserverCalled("getUserMedia:response:deny");
    yield expectObserverCalled("recording-window-ended");
    yield checkNotSharing();
  }
},


{
  desc: "getUserMedia screen, user clicks \"Don't Allow\"",
  run: function* checkDontShare() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true, null, "screen");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, false, true);

    yield promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });

    yield expectObserverCalled("getUserMedia:response:deny");
    yield expectObserverCalled("recording-window-ended");
    yield checkNotSharing();
  }
},

{
  desc: "getUserMedia audio+video+screen: stop sharing",
  run: function* checkStopSharing() {
    if (AppConstants.platform == "macosx") {
      todo(false, "Bug 1323481 - On Mac on treeherder, but not locally, requesting microphone + screen never makes the permission prompt appear, and so causes the test to timeout");
      return;
    }

    function* share(audio, video, screen) {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      yield promiseRequestDevice(audio, video || !!screen,
                                 null, screen && "screen");
      yield promise;
      yield expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(audio, video, screen);
      if (screen) {
        document.getElementById("webRTC-selectWindow-menulist")
                .getItemAtIndex(2).doCommand();
      }
      yield promiseMessage("ok", () => {
        PopupNotifications.panel.firstChild.button.click();
      });
      yield expectObserverCalled("getUserMedia:response:allow");
      yield expectObserverCalled("recording-device-events");
    }

    function* check(expected = {}) {
      let shared = Object.keys(expected).join(" and ");
      if (shared) {
        Assert.deepEqual((yield getMediaCaptureState()), expected,
                         "expected " + shared + " to be shared");
        yield checkSharingUI(expected);
      } else {
        yield checkNotSharing();
      }
    }

    info("Share screen and microphone");
    let indicator = promiseIndicatorWindow();
    yield share(true, false, true);
    yield indicator;
    yield check({audio: true, screen: "Screen"});

    info("Share camera");
    yield share(false, true);
    yield check({video: true, audio: true, screen: "Screen"});

    info("Stop the screen share, mic+cam should continue");
    yield stopSharing("screen", true);
    yield check({video: true, audio: true});

    info("Stop the camera, everything should stop.");
    yield stopSharing("camera", false, true);

    info("Now, share only the screen...");
    indicator = promiseIndicatorWindow();
    yield share(false, false, true);
    yield indicator;
    yield check({screen: "Screen"});

    info("... and add camera and microphone in a second request.");
    yield share(true, true);
    yield check({video: true, audio: true, screen: "Screen"});

    info("Stop the camera, this should stop everything.");
    yield stopSharing("camera", false, true);
  }
},

{
  desc: "getUserMedia screen: reloading the page removes all gUM UI",
  run: function* checkReloading() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true, null, "screen");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, false, true);
    document.getElementById("webRTC-selectWindow-menulist")
            .getItemAtIndex(2).doCommand();

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {screen: "Screen"},
                     "expected screen to be shared");

    yield indicator;
    yield checkSharingUI({screen: "Screen"});

    yield reloadAndAssertClosedStreams();
  }
},

{
  desc: "test showControlCenter from screen icon",
  run: function* checkShowControlCenter() {
    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true, null, "screen");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
    checkDeviceSelectors(false, false, true);
    document.getElementById("webRTC-selectWindow-menulist")
            .getItemAtIndex(2).doCommand();

    let indicator = promiseIndicatorWindow();
    yield promiseMessage("ok", () => {
      PopupNotifications.panel.firstChild.button.click();
    });
    yield expectObserverCalled("getUserMedia:response:allow");
    yield expectObserverCalled("recording-device-events");
    Assert.deepEqual((yield getMediaCaptureState()), {screen: "Screen"},
                     "expected screen to be shared");
    yield indicator;
    yield checkSharingUI({screen: "Screen"});

    ok(gIdentityHandler._identityPopup.hidden, "control center should be hidden");
    if ("nsISystemStatusBar" in Ci) {
      let activeStreams = webrtcUI.getActiveStreams(false, false, true);
      webrtcUI.showSharingDoorhanger(activeStreams[0]);
    } else {
      let win =
        Services.wm.getMostRecentWindow("Browser:WebRTCGlobalIndicator");
      let elt = win.document.getElementById("screenShareButton");
      EventUtils.synthesizeMouseAtCenter(elt, {}, win);
      yield promiseWaitForCondition(() => !gIdentityHandler._identityPopup.hidden);
    }
    ok(!gIdentityHandler._identityPopup.hidden, "control center should be open");

    gIdentityHandler._identityPopup.hidden = true;
    yield expectNoObserverCalled();

    yield closeStream();
  }
},

{
  desc: "Only persistent block is possible for screen sharing",
  run: function* checkPersistentPermissions() {
    let Perms = Services.perms;
    let uri = gBrowser.selectedBrowser.documentURI;
    let devicePerms = Perms.testExactPermission(uri, "screen");
    is(devicePerms, Perms.UNKNOWN_ACTION,
       "starting without screen persistent permissions");

    let promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true, null, "screen");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");
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
    yield promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });
    yield expectObserverCalled("getUserMedia:response:deny");
    yield expectObserverCalled("recording-window-ended");
    yield checkNotSharing();

    is(Perms.testExactPermission(uri, "screen"), Perms.DENY_ACTION,
       "screen sharing is persistently blocked");

    // Request screensharing again, expect an immediate failure.
    promise = promiseMessage(permissionError);
    yield promiseRequestDevice(false, true, null, "screen");
    yield promise;
    yield expectObserverCalled("recording-window-ended");

    // Now set the permission to allow and expect a prompt.
    Perms.add(uri, "screen", Perms.ALLOW_ACTION);

    // Request devices and expect a prompt despite the saved 'Allow' permission.
    promise = promisePopupNotificationShown("webRTC-shareDevices");
    yield promiseRequestDevice(false, true, null, "screen");
    yield promise;
    yield expectObserverCalled("getUserMedia:request");

    // The 'remember' checkbox shouldn't be checked anymore.
    notification = PopupNotifications.panel.firstChild;
    ok(notification.hasAttribute("warninghidden"), "warning message is hidden");
    checkbox = notification.checkbox;
    ok(!!checkbox, "checkbox is present");
    ok(!checkbox.checked, "checkbox is not checked");

    // Deny the request to cleanup...
    yield promiseMessage(permissionError, () => {
      activateSecondaryAction(kActionDeny);
    });
    yield expectObserverCalled("getUserMedia:response:deny");
    yield expectObserverCalled("recording-window-ended");
    Perms.remove(uri, "screen");
  }
}

];

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  browser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

  browser.addEventListener("load", function onload() {
    browser.removeEventListener("load", onload, true);

    is(PopupNotifications._currentNotifications.length, 0,
       "should start the test without any prior popup notification");
    ok(gIdentityHandler._identityPopup.hidden,
       "should start the test with the control center hidden");

    Task.spawn(function* () {
      yield SpecialPowers.pushPrefEnv({"set": [[PREF_PERMISSION_FAKE, true]]});

      for (let testCase of gTests) {
        info(testCase.desc);
        yield testCase.run();

        // Cleanup before the next test
        yield expectNoObserverCalled();
      }
    }).then(finish, ex => {
     Cu.reportError(ex);
     ok(false, "Unexpected Exception: " + ex);
     finish();
    });
  }, true);
  let rootDir = getRootDirectory(gTestPath);
  rootDir = rootDir.replace("chrome://mochitests/content/",
                            "https://example.com/");
  content.location = rootDir + "get_user_media.html";
}

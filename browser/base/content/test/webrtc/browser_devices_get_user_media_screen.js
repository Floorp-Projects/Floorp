/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The rejection "The fetching process for the media resource was aborted by the
// user agent at the user's request." is left unhandled in some cases. This bug
// should be fixed, but for the moment this file is whitelisted.
//
// NOTE: Whitelisting a class of rejections should be limited. Normally you
//       should use "expectUncaughtRejection" to flag individual failures.
ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm", this);
PromiseTestUtils.whitelistRejectionsGlobally(/aborted by the user agent/);
ChromeUtils.import("resource:///modules/BrowserWindowTracker.jsm", this);

const permissionError =
  "error: NotAllowedError: The request is not allowed " +
  "by the user agent or the platform in the current context.";

const notFoundError = "error: NotFoundError: The object can not be found here.";

let env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);
const isHeadless = env.get("MOZ_HEADLESS");

var gTests = [
  {
    desc: "getUserMedia window/screen picking screen",
    run: async function checkWindowOrScreen() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await expectObserverCalled("getUserMedia:request");

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareScreen-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(false, false, true);
      let notification = PopupNotifications.panel.firstElementChild;
      let iconclass = notification.getAttribute("iconclass");
      ok(iconclass.includes("screen-icon"), "panel using screen icon");

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

      let separator = menulist.getItemAtIndex(1);
      is(
        separator.localName,
        "menuseparator",
        "the second item is a separator"
      );

      ok(
        document.getElementById("webRTC-all-windows-shared").hidden,
        "the 'all windows will be shared' warning should be hidden while there's no selection"
      );
      ok(
        document.getElementById("webRTC-preview").hidden,
        "the preview area is hidden"
      );

      let scaryScreenIndex;
      for (let i = 2; i < count; ++i) {
        let item = menulist.getItemAtIndex(i);
        is(
          parseInt(item.getAttribute("value")),
          i - 2,
          "the window/screen item has the correct index"
        );
        let type = item.getAttribute("devicetype");
        ok(
          ["window", "screen"].includes(type),
          "the devicetype attribute is set correctly"
        );
        if (type == "screen") {
          ok(item.scary, "the screen item is marked as scary");
          scaryScreenIndex = i;
        }
      }
      ok(
        typeof scaryScreenIndex == "number",
        "there's at least one scary screen, as as all screens are"
      );

      // Select a screen, a preview with a scary warning should appear.
      menulist.getItemAtIndex(scaryScreenIndex).doCommand();
      ok(
        !document.getElementById("webRTC-all-windows-shared").hidden,
        "the 'all windows will be shared' warning should now be visible"
      );
      await TestUtils.waitForCondition(
        () => !document.getElementById("webRTC-preview").hidden,
        "preview unhide",
        100,
        100
      );
      ok(
        !document.getElementById("webRTC-preview").hidden,
        "the preview area is visible"
      );
      ok(
        !document.getElementById("webRTC-previewWarning").hidden,
        "the scary warning is visible"
      );
      ok(!notification.button.disabled, "Allow button is enabled");

      // Select the 'Select Window or Screen' item again, the preview should be hidden.
      menulist.getItemAtIndex(0).doCommand();
      ok(
        document.getElementById("webRTC-all-windows-shared").hidden,
        "the 'all windows will be shared' warning should now be hidden"
      );
      ok(
        document.getElementById("webRTC-preview").hidden,
        "the preview area is hidden"
      );

      // Select the scary screen again so that we can have a stream.
      menulist.getItemAtIndex(scaryScreenIndex).doCommand();

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { screen: "Screen" },
        "expected screen to be shared"
      );

      await indicator;
      await checkSharingUI({ screen: "Screen" });

      // we always show prompt for screen sharing.
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await expectObserverCalled("getUserMedia:request");

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareScreen-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(false, false, true);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      SitePermissions.removeFromPrincipal(
        null,
        "screen",
        gBrowser.selectedBrowser
      );
      await closeStream();
    },
  },

  {
    desc: "getUserMedia window/screen picking window",
    run: async function checkWindowOrScreen() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "window");
      await promise;
      await expectObserverCalled("getUserMedia:request");

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareScreen-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(false, false, true);
      let notification = PopupNotifications.panel.firstElementChild;
      let iconclass = notification.getAttribute("iconclass");
      ok(iconclass.includes("screen-icon"), "panel using screen icon");

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

      let separator = menulist.getItemAtIndex(1);
      is(
        separator.localName,
        "menuseparator",
        "the second item is a separator"
      );

      ok(
        document.getElementById("webRTC-all-windows-shared").hidden,
        "the 'all windows will be shared' warning should be hidden while there's no selection"
      );
      ok(
        document.getElementById("webRTC-preview").hidden,
        "the preview area is hidden"
      );

      let scaryWindowIndexes = [],
        nonScaryWindowIndex,
        scaryScreenIndex;
      for (let i = 2; i < count; ++i) {
        let item = menulist.getItemAtIndex(i);
        is(
          parseInt(item.getAttribute("value")),
          i - 2,
          "the window/screen item has the correct index"
        );
        let type = item.getAttribute("devicetype");
        ok(
          ["window", "screen"].includes(type),
          "the devicetype attribute is set correctly"
        );
        if (type == "screen") {
          ok(item.scary, "the screen item is marked as scary");
          scaryScreenIndex = i;
        } else if (item.scary) {
          scaryWindowIndexes.push(i);
        } else {
          nonScaryWindowIndex = i;
        }
      }
      if (isHeadless) {
        is(
          scaryWindowIndexes.length,
          0,
          "there are no scary Firefox windows in headless mode"
        );
      } else {
        ok(
          scaryWindowIndexes.length,
          "there's at least one scary window, as Firefox is running"
        );
      }
      ok(
        typeof scaryScreenIndex == "number",
        "there's at least one scary screen, as all screens are"
      );

      if (!isHeadless) {
        // Select one scary window, a preview with a scary warning should appear.
        let scaryWindowIndex;
        for (scaryWindowIndex of scaryWindowIndexes) {
          menulist.getItemAtIndex(scaryWindowIndex).doCommand();
          ok(
            document.getElementById("webRTC-all-windows-shared").hidden,
            "the 'all windows will be shared' warning should still be hidden"
          );
          try {
            await TestUtils.waitForCondition(
              () => !document.getElementById("webRTC-preview").hidden,
              "",
              100,
              100
            );
            break;
          } catch (e) {
            // A "scary window" is Firefox. Multiple Firefox windows have been
            // observed to come and go during try runs, so we won't know which one
            // is ours. To avoid intermittents, we ignore preview failing due to
            // these going away on us, provided it succeeds on one of them.
          }
        }
        ok(
          !document.getElementById("webRTC-preview").hidden,
          "the preview area is visible"
        );
        ok(
          !document.getElementById("webRTC-previewWarning").hidden,
          "the scary warning is visible"
        );
        // Select the 'Select Window' item again, the preview should be hidden.
        menulist.getItemAtIndex(0).doCommand();
        ok(
          document.getElementById("webRTC-preview").hidden,
          "the preview area is hidden"
        );

        // Select the first window again so that we can have a stream.
        menulist.getItemAtIndex(scaryWindowIndex).doCommand();
      }

      // If we have a non-scary window, select it and verify the warning isn't displayed.
      // A non-scary window may not always exist on test slaves.
      if (typeof nonScaryWindowIndex == "number") {
        menulist.getItemAtIndex(nonScaryWindowIndex).doCommand();
        ok(
          document.getElementById("webRTC-all-windows-shared").hidden,
          "the 'all windows will be shared' warning should still be hidden"
        );
        await TestUtils.waitForCondition(
          () => !document.getElementById("webRTC-preview").hidden,
          "preview unhide",
          100,
          100
        );
        ok(
          !document.getElementById("webRTC-preview").hidden,
          "the preview area is visible"
        );
        ok(
          document.getElementById("webRTC-previewWarning").hidden,
          "the scary warning is hidden"
        );
      } else {
        info("no non-scary window available on this test slave");
      }

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { screen: "Window" },
        "expected screen to be shared"
      );

      await indicator;
      await checkSharingUI({ screen: "Window" });
      await closeStream();
    },
  },

  {
    desc: "getUserMedia audio + window/screen",
    run: async function checkAudioVideo() {
      if (AppConstants.platform == "macosx") {
        todo(
          false,
          "Bug 1323481 - On Mac on treeherder, but not locally, requesting microphone + screen never makes the permission prompt appear, and so causes the test to timeout"
        );
        return;
      }

      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, null, "window");
      await promise;
      await expectObserverCalled("getUserMedia:request");

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareScreen-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(true, false, true);
      let iconclass = PopupNotifications.panel.firstElementChild.getAttribute(
        "iconclass"
      );
      ok(iconclass.includes("screen-icon"), "panel using screen icon");

      let menulist = document.getElementById("webRTC-selectWindow-menulist");
      let count = menulist.itemCount;
      ok(
        count >= 4,
        "There should be the 'Select Window or Screen' item, a separator and at least one window and one screen"
      );

      // Select a screen, a preview with a scary warning should appear.
      menulist.getItemAtIndex(count - 1).doCommand();
      ok(
        !document.getElementById("webRTC-all-windows-shared").hidden,
        "the 'all windows will be shared' warning should now be visible"
      );
      await TestUtils.waitForCondition(
        () => !document.getElementById("webRTC-preview").hidden,
        "preview unhide",
        100,
        100
      );
      ok(
        !document.getElementById("webRTC-preview").hidden,
        "the preview area is visible"
      );
      ok(
        !document.getElementById("webRTC-previewWarning").hidden,
        "the scary warning is visible"
      );

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, screen: "Screen" },
        "expected screen and microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ audio: true, screen: "Screen" });
      await closeStream();
    },
  },

  {
    desc: 'getUserMedia screen, user clicks "Don\'t Allow"',
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
      SitePermissions.removeFromPrincipal(
        null,
        "screen",
        gBrowser.selectedBrowser
      );
      SitePermissions.removeFromPrincipal(
        null,
        "camera",
        gBrowser.selectedBrowser
      );
    },
  },

  {
    desc: "getUserMedia audio + window/screen: stop sharing",
    run: async function checkStopSharing() {
      if (AppConstants.platform == "macosx") {
        todo(
          false,
          "Bug 1323481 - On Mac on treeherder, but not locally, requesting microphone + screen never makes the permission prompt appear, and so causes the test to timeout"
        );
        return;
      }

      async function share(audio, video, screen) {
        let promise = promisePopupNotificationShown("webRTC-shareDevices");
        await promiseRequestDevice(
          audio,
          video || !!screen,
          null,
          screen && "window"
        );
        await promise;
        await expectObserverCalled("getUserMedia:request");
        checkDeviceSelectors(audio, video, screen);
        if (screen) {
          let menulist = document.getElementById(
            "webRTC-selectWindow-menulist"
          );
          menulist.getItemAtIndex(menulist.itemCount - 1).doCommand();
        }
        await promiseMessage("ok", () => {
          PopupNotifications.panel.firstElementChild.button.click();
        });
        await expectObserverCalled("getUserMedia:response:allow");
        await expectObserverCalled("recording-device-events");
      }

      async function check(expected = {}) {
        let shared = Object.keys(expected).join(" and ");
        if (shared) {
          Assert.deepEqual(
            await getMediaCaptureState(),
            expected,
            "expected " + shared + " to be shared"
          );
          await checkSharingUI(expected);
        } else {
          await checkNotSharing();
        }
      }

      info("Share screen and microphone");
      let indicator = promiseIndicatorWindow();
      await share(true, false, true);
      await indicator;
      await check({ audio: true, screen: "Screen" });

      info("Share camera");
      await share(false, true);
      await check({ video: true, audio: true, screen: "Screen" });

      info("Stop the screen share, mic+cam should continue");
      await stopSharing("screen", true);
      await check({ video: true, audio: true });

      info("Stop the camera, everything should stop.");
      await stopSharing("camera");

      info("Now, share only the screen...");
      indicator = promiseIndicatorWindow();
      await share(false, false, true);
      await indicator;
      await check({ screen: "Screen" });

      info("... and add camera and microphone in a second request.");
      await share(true, true);
      await check({ video: true, audio: true, screen: "Screen" });

      info("Stop the camera, this should stop everything.");
      await stopSharing("camera");
    },
  },

  {
    desc: "getUserMedia window/screen: reloading the page removes all gUM UI",
    run: async function checkReloading() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(false, false, true);
      let menulist = document.getElementById("webRTC-selectWindow-menulist");
      menulist.getItemAtIndex(menulist.itemCount - 1).doCommand();

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { screen: "Screen" },
        "expected screen to be shared"
      );

      await indicator;
      await checkSharingUI({ screen: "Screen" });

      await reloadAndAssertClosedStreams();
    },
  },

  {
    desc: "test showControlCenter from screen icon",
    run: async function checkShowControlCenter() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(false, false, true);
      let menulist = document.getElementById("webRTC-selectWindow-menulist");
      menulist.getItemAtIndex(menulist.itemCount - 1).doCommand();

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { screen: "Screen" },
        "expected screen to be shared"
      );
      await indicator;
      await checkSharingUI({ screen: "Screen" });

      ok(
        gIdentityHandler._identityPopup.hidden,
        "control center should be hidden"
      );
      if ("nsISystemStatusBar" in Ci) {
        let activeStreams = webrtcUI.getActiveStreams(false, false, true);
        webrtcUI.showSharingDoorhanger(activeStreams[0]);
      } else {
        let win = Services.wm.getMostRecentWindow(
          "Browser:WebRTCGlobalIndicator"
        );
        let elt = win.document.getElementById("screenShareButton");
        EventUtils.synthesizeMouseAtCenter(elt, {}, win);
        await TestUtils.waitForCondition(
          () => !gIdentityHandler._identityPopup.hidden
        );
      }
      ok(
        !gIdentityHandler._identityPopup.hidden,
        "control center should be open"
      );

      gIdentityHandler._identityPopup.hidden = true;
      await expectNoObserverCalled();

      await closeStream();
    },
  },

  {
    desc: "Only persistent block is possible for screen sharing",
    run: async function checkPersistentPermissions() {
      let browser = gBrowser.selectedBrowser;
      let devicePerms = SitePermissions.getForPrincipal(
        browser.contentPrincipal,
        "screen",
        browser
      );
      is(
        devicePerms.state,
        SitePermissions.UNKNOWN,
        "starting without screen persistent permissions"
      );

      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(false, false, true);
      document
        .getElementById("webRTC-selectWindow-menulist")
        .getItemAtIndex(2)
        .doCommand();

      // Ensure that checking the 'Remember this decision' checkbox disables
      // 'Allow'.
      let notification = PopupNotifications.panel.firstElementChild;
      ok(
        notification.hasAttribute("warninghidden"),
        "warning message is hidden"
      );
      let checkbox = notification.checkbox;
      ok(!!checkbox, "checkbox is present");
      ok(!checkbox.checked, "checkbox is not checked");
      checkbox.click();
      ok(checkbox.checked, "checkbox now checked");
      ok(notification.button.disabled, "Allow button is disabled");
      ok(
        !notification.hasAttribute("warninghidden"),
        "warning message is shown"
      );

      // Click "Don't Allow" to save a persistent block permission.
      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });
      await expectObserverCalled("getUserMedia:response:deny");
      await expectObserverCalled("recording-window-ended");
      await checkNotSharing();

      let permission = SitePermissions.getForPrincipal(
        browser.contentPrincipal,
        "screen",
        browser
      );
      is(permission.state, SitePermissions.BLOCK, "screen sharing is blocked");
      is(
        permission.scope,
        SitePermissions.SCOPE_PERSISTENT,
        "screen sharing is persistently blocked"
      );

      // Request screensharing again, expect an immediate failure.
      promise = promiseMessage(permissionError);
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await expectObserverCalled("recording-window-ended");

      // Now set the permission to allow and expect a prompt.
      SitePermissions.setForPrincipal(
        browser.contentPrincipal,
        "screen",
        SitePermissions.ALLOW
      );

      // Request devices and expect a prompt despite the saved 'Allow' permission.
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "screen");
      await promise;
      await expectObserverCalled("getUserMedia:request");

      // The 'remember' checkbox shouldn't be checked anymore.
      notification = PopupNotifications.panel.firstElementChild;
      ok(
        notification.hasAttribute("warninghidden"),
        "warning message is hidden"
      );
      checkbox = notification.checkbox;
      ok(!!checkbox, "checkbox is present");
      ok(!checkbox.checked, "checkbox is not checked");

      // Deny the request to cleanup...
      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });
      await expectObserverCalled("getUserMedia:response:deny");
      await expectObserverCalled("recording-window-ended");
      SitePermissions.removeFromPrincipal(
        browser.contentPrincipal,
        "screen",
        browser
      );
    },
  },

  {
    desc:
      "Switching between menu options maintains correct main action state while window sharing",
    run: async function checkDoorhangerState() {
      let win = await BrowserTestUtils.openNewBrowserWindow();
      await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:newtab");
      BrowserWindowTracker.orderedWindows[1].focus();

      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, null, "window");
      await promise;
      await expectObserverCalled("getUserMedia:request");

      let menulist = document.getElementById("webRTC-selectWindow-menulist");
      let notification = PopupNotifications.panel.firstElementChild;
      let checkbox = notification.checkbox;

      menulist.getItemAtIndex(2).doCommand();
      checkbox.click();
      ok(checkbox.checked, "checkbox now checked");
      ok(notification.button.disabled, "Allow button is disabled");
      ok(
        !notification.hasAttribute("warninghidden"),
        "warning message is shown"
      );

      menulist.getItemAtIndex(3).doCommand();
      ok(checkbox.checked, "checkbox still checked");
      ok(notification.button.disabled, "Allow button remains disabled");
      ok(
        !notification.hasAttribute("warninghidden"),
        "warning message is still shown"
      );

      win.close();
    },
  },
];

add_task(async function test() {
  await runTests(gTests);
});

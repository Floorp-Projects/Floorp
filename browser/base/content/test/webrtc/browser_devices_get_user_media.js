/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(2);

const permissionError =
  "error: NotAllowedError: The request is not allowed " +
  "by the user agent or the platform in the current context.";

var gTests = [
  {
    desc: "getUserMedia audio+video",
    run: async function checkAudioVideo() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareDevices-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(true, true);
      let iconclass = PopupNotifications.panel.firstElementChild.getAttribute(
        "iconclass"
      );
      ok(iconclass.includes("camera-icon"), "panel using devices icon");

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ audio: true, video: true });
      await closeStream();
    },
  },

  {
    desc: "getUserMedia audio only",
    run: async function checkAudioOnly() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true);
      await promise;
      await expectObserverCalled("getUserMedia:request");

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareMicrophone-notification-icon",
        "anchored to mic icon"
      );
      checkDeviceSelectors(true);
      let iconclass = PopupNotifications.panel.firstElementChild.getAttribute(
        "iconclass"
      );
      ok(iconclass.includes("microphone-icon"), "panel using microphone icon");

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true },
        "expected microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ audio: true });
      await closeStream();
    },
  },

  {
    desc: "getUserMedia video only",
    run: async function checkVideoOnly() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareDevices-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(false, true);
      let iconclass = PopupNotifications.panel.firstElementChild.getAttribute(
        "iconclass"
      );
      ok(iconclass.includes("camera-icon"), "panel using devices icon");

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { video: true },
        "expected camera to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true });
      await closeStream();
    },
  },

  {
    desc: 'getUserMedia audio+video, user clicks "Don\'t Share"',
    run: async function checkDontShare() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await expectObserverCalled("getUserMedia:response:deny");
      await expectObserverCalled("recording-window-ended");
      await checkNotSharing();

      // Verify that we set 'Temporarily blocked' permissions.
      let browser = gBrowser.selectedBrowser;
      let blockedPerms = document.getElementById(
        "blocked-permissions-container"
      );

      let { state, scope } = SitePermissions.getForPrincipal(
        null,
        "camera",
        browser
      );
      Assert.equal(state, SitePermissions.BLOCK);
      Assert.equal(scope, SitePermissions.SCOPE_TEMPORARY);
      ok(
        blockedPerms.querySelector(
          ".blocked-permission-icon.camera-icon[showing=true]"
        ),
        "the blocked camera icon is shown"
      );

      ({ state, scope } = SitePermissions.getForPrincipal(
        null,
        "microphone",
        browser
      ));
      Assert.equal(state, SitePermissions.BLOCK);
      Assert.equal(scope, SitePermissions.SCOPE_TEMPORARY);
      ok(
        blockedPerms.querySelector(
          ".blocked-permission-icon.microphone-icon[showing=true]"
        ),
        "the blocked microphone icon is shown"
      );

      info("requesting devices again to check temporarily blocked permissions");
      promise = promiseMessage(permissionError);
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      await expectObserverCalled("getUserMedia:response:deny");
      await expectObserverCalled("recording-window-ended");
      await checkNotSharing();

      SitePermissions.removeFromPrincipal(
        browser.contentPrincipal,
        "camera",
        browser
      );
      SitePermissions.removeFromPrincipal(
        browser.contentPrincipal,
        "microphone",
        browser
      );
    },
  },

  {
    desc: "getUserMedia audio+video: stop sharing",
    run: async function checkStopSharing() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true, audio: true });

      await stopSharing();

      // the stream is already closed, but this will do some cleanup anyway
      await closeStream(true);

      // After stop sharing, gUM(audio+camera) causes a prompt.
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

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
      SitePermissions.removeFromPrincipal(
        null,
        "microphone",
        gBrowser.selectedBrowser
      );
    },
  },

  {
    desc: "getUserMedia audio+video: reloading the page removes all gUM UI",
    run: async function checkReloading() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true, audio: true });

      await reloadAndAssertClosedStreams();

      // After the reload, gUM(audio+camera) causes a prompt.
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(true, true);

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
      SitePermissions.removeFromPrincipal(
        null,
        "microphone",
        gBrowser.selectedBrowser
      );
    },
  },

  {
    desc: "getUserMedia prompt: Always/Never Share",
    run: async function checkRememberCheckbox() {
      let elt = id => document.getElementById(id);

      async function checkPerm(
        aRequestAudio,
        aRequestVideo,
        aExpectedAudioPerm,
        aExpectedVideoPerm,
        aNever
      ) {
        let promise = promisePopupNotificationShown("webRTC-shareDevices");
        await promiseRequestDevice(aRequestAudio, aRequestVideo);
        await promise;
        await expectObserverCalled("getUserMedia:request");

        is(
          elt("webRTC-selectMicrophone").hidden,
          !aRequestAudio,
          "microphone selector expected to be " +
            (aRequestAudio ? "visible" : "hidden")
        );

        is(
          elt("webRTC-selectCamera").hidden,
          !aRequestVideo,
          "camera selector expected to be " +
            (aRequestVideo ? "visible" : "hidden")
        );

        let expectedMessage = aNever ? permissionError : "ok";
        await promiseMessage(expectedMessage, () => {
          activateSecondaryAction(aNever ? kActionNever : kActionAlways);
        });
        let expected = {};
        if (expectedMessage == "ok") {
          await expectObserverCalled("getUserMedia:response:allow");
          await expectObserverCalled("recording-device-events");
          if (aRequestVideo) {
            expected.video = true;
          }
          if (aRequestAudio) {
            expected.audio = true;
          }
        } else {
          await expectObserverCalled("getUserMedia:response:deny");
          await expectObserverCalled("recording-window-ended");
        }
        Assert.deepEqual(
          await getMediaCaptureState(),
          expected,
          "expected " + Object.keys(expected).join(" and ") + " to be shared"
        );

        function checkDevicePermissions(aDevice, aExpected) {
          let uri = gBrowser.selectedBrowser.documentURI;
          let devicePerms = PermissionTestUtils.testExactPermission(
            uri,
            aDevice
          );
          if (aExpected === undefined) {
            is(
              devicePerms,
              Services.perms.UNKNOWN_ACTION,
              "no " + aDevice + " persistent permissions"
            );
          } else {
            is(
              devicePerms,
              aExpected
                ? Services.perms.ALLOW_ACTION
                : Services.perms.DENY_ACTION,
              aDevice + " persistently " + (aExpected ? "allowed" : "denied")
            );
          }
          PermissionTestUtils.remove(uri, aDevice);
        }
        checkDevicePermissions("microphone", aExpectedAudioPerm);
        checkDevicePermissions("camera", aExpectedVideoPerm);

        if (expectedMessage == "ok") {
          await closeStream();
        }
      }

      // 3 cases where the user accepts the device prompt.
      info("audio+video, user grants, expect both Services.perms set to allow");
      await checkPerm(true, true, true, true);
      info(
        "audio only, user grants, check audio perm set to allow, video perm not set"
      );
      await checkPerm(true, false, true, undefined);
      info(
        "video only, user grants, check video perm set to allow, audio perm not set"
      );
      await checkPerm(false, true, undefined, true);

      // 3 cases where the user rejects the device request by using 'Never Share'.
      info(
        "audio only, user denies, expect audio perm set to deny, video not set"
      );
      await checkPerm(true, false, false, undefined, true);
      info(
        "video only, user denies, expect video perm set to deny, audio perm not set"
      );
      await checkPerm(false, true, undefined, false, true);
      info("audio+video, user denies, expect both Services.perms set to deny");
      await checkPerm(true, true, false, false, true);
    },
  },

  {
    desc: "getUserMedia without prompt: use persistent permissions",
    run: async function checkUsePersistentPermissions() {
      async function usePerm(
        aAllowAudio,
        aAllowVideo,
        aRequestAudio,
        aRequestVideo,
        aExpectStream
      ) {
        let uri = gBrowser.selectedBrowser.documentURI;

        if (aAllowAudio !== undefined) {
          PermissionTestUtils.add(
            uri,
            "microphone",
            aAllowAudio
              ? Services.perms.ALLOW_ACTION
              : Services.perms.DENY_ACTION
          );
        }
        if (aAllowVideo !== undefined) {
          PermissionTestUtils.add(
            uri,
            "camera",
            aAllowVideo
              ? Services.perms.ALLOW_ACTION
              : Services.perms.DENY_ACTION
          );
        }

        if (aExpectStream === undefined) {
          // Check that we get a prompt.
          let promise = promisePopupNotificationShown("webRTC-shareDevices");
          await promiseRequestDevice(aRequestAudio, aRequestVideo);
          await promise;
          await expectObserverCalled("getUserMedia:request");

          // Deny the request to cleanup...
          await promiseMessage(permissionError, () => {
            activateSecondaryAction(kActionDeny);
          });
          await expectObserverCalled("getUserMedia:response:deny");
          await expectObserverCalled("recording-window-ended");
          let browser = gBrowser.selectedBrowser;
          SitePermissions.removeFromPrincipal(null, "camera", browser);
          SitePermissions.removeFromPrincipal(null, "microphone", browser);
        } else {
          let expectedMessage = aExpectStream ? "ok" : permissionError;
          let promise = promiseMessage(expectedMessage);
          await promiseRequestDevice(aRequestAudio, aRequestVideo);
          await promise;

          if (expectedMessage == "ok") {
            await expectObserverCalled("getUserMedia:request");
            await promiseNoPopupNotification("webRTC-shareDevices");
            await expectObserverCalled("getUserMedia:response:allow");
            await expectObserverCalled("recording-device-events");

            // Check what's actually shared.
            let expected = {};
            if (aAllowVideo && aRequestVideo) {
              expected.video = true;
            }
            if (aAllowAudio && aRequestAudio) {
              expected.audio = true;
            }
            Assert.deepEqual(
              await getMediaCaptureState(),
              expected,
              "expected " +
                Object.keys(expected).join(" and ") +
                " to be shared"
            );

            await closeStream();
          } else {
            await expectObserverCalled("recording-window-ended");
          }
        }

        PermissionTestUtils.remove(uri, "camera");
        PermissionTestUtils.remove(uri, "microphone");
      }

      // Set both permissions identically
      info("allow audio+video, request audio+video, expect ok (audio+video)");
      await usePerm(true, true, true, true, true);
      info("deny audio+video, request audio+video, expect denied");
      await usePerm(false, false, true, true, false);

      // Allow audio, deny video.
      info("allow audio, deny video, request audio+video, expect denied");
      await usePerm(true, false, true, true, false);
      info("allow audio, deny video, request audio, expect ok (audio)");
      await usePerm(true, false, true, false, true);
      info("allow audio, deny video, request video, expect denied");
      await usePerm(true, false, false, true, false);

      // Deny audio, allow video.
      info("deny audio, allow video, request audio+video, expect denied");
      await usePerm(false, true, true, true, false);
      info("deny audio, allow video, request audio, expect denied");
      await usePerm(false, true, true, false, false);
      info("deny audio, allow video, request video, expect ok (video)");
      await usePerm(false, true, false, true, true);

      // Allow audio, video not set.
      info("allow audio, request audio+video, expect prompt");
      await usePerm(true, undefined, true, true, undefined);
      info("allow audio, request audio, expect ok (audio)");
      await usePerm(true, undefined, true, false, true);
      info("allow audio, request video, expect prompt");
      await usePerm(true, undefined, false, true, undefined);

      // Deny audio, video not set.
      info("deny audio, request audio+video, expect denied");
      await usePerm(false, undefined, true, true, false);
      info("deny audio, request audio, expect denied");
      await usePerm(false, undefined, true, false, false);
      info("deny audio, request video, expect prompt");
      await usePerm(false, undefined, false, true, undefined);

      // Allow video, audio not set.
      info("allow video, request audio+video, expect prompt");
      await usePerm(undefined, true, true, true, undefined);
      info("allow video, request audio, expect prompt");
      await usePerm(undefined, true, true, false, undefined);
      info("allow video, request video, expect ok (video)");
      await usePerm(undefined, true, false, true, true);

      // Deny video, audio not set.
      info("deny video, request audio+video, expect denied");
      await usePerm(undefined, false, true, true, false);
      info("deny video, request audio, expect prompt");
      await usePerm(undefined, false, true, false, undefined);
      info("deny video, request video, expect denied");
      await usePerm(undefined, false, false, true, false);
    },
  },

  {
    desc: "Stop Sharing removes persistent permissions",
    run: async function checkStopSharingRemovesPersistentPermissions() {
      async function stopAndCheckPerm(aRequestAudio, aRequestVideo) {
        let uri = gBrowser.selectedBrowser.documentURI;

        // Initially set both permissions to 'allow'.
        PermissionTestUtils.add(uri, "microphone", Services.perms.ALLOW_ACTION);
        PermissionTestUtils.add(uri, "camera", Services.perms.ALLOW_ACTION);

        let indicator = promiseIndicatorWindow();
        // Start sharing what's been requested.
        let promise = promiseMessage("ok");
        await promiseRequestDevice(aRequestAudio, aRequestVideo);
        await promise;

        await expectObserverCalled("getUserMedia:request");
        await expectObserverCalled("getUserMedia:response:allow");
        await expectObserverCalled("recording-device-events");
        await indicator;
        await checkSharingUI({ video: aRequestVideo, audio: aRequestAudio });

        await stopSharing(aRequestVideo ? "camera" : "microphone");

        // Check that permissions have been removed as expected.
        let audioPerm = PermissionTestUtils.testExactPermission(
          uri,
          "microphone"
        );
        if (aRequestAudio) {
          is(
            audioPerm,
            Services.perms.UNKNOWN_ACTION,
            "microphone permissions removed"
          );
        } else {
          is(
            audioPerm,
            Services.perms.ALLOW_ACTION,
            "microphone permissions untouched"
          );
        }

        let videoPerm = PermissionTestUtils.testExactPermission(uri, "camera");
        if (aRequestVideo) {
          is(
            videoPerm,
            Services.perms.UNKNOWN_ACTION,
            "camera permissions removed"
          );
        } else {
          is(
            videoPerm,
            Services.perms.ALLOW_ACTION,
            "camera permissions untouched"
          );
        }

        // Cleanup.
        await closeStream(true);

        PermissionTestUtils.remove(uri, "camera");
        PermissionTestUtils.remove(uri, "microphone");
      }

      info("request audio+video, stop sharing resets both");
      await stopAndCheckPerm(true, true);
      info("request audio, stop sharing resets audio only");
      await stopAndCheckPerm(true, false);
      info("request video, stop sharing resets video only");
      await stopAndCheckPerm(false, true);
    },
  },

  {
    desc: "test showControlCenter",
    run: async function checkShowControlCenter() {
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");
      checkDeviceSelectors(false, true);

      let indicator = promiseIndicatorWindow();
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await expectObserverCalled("getUserMedia:response:allow");
      await expectObserverCalled("recording-device-events");
      Assert.deepEqual(
        await getMediaCaptureState(),
        { video: true },
        "expected camera to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true });

      ok(
        gIdentityHandler._identityPopup.hidden,
        "control center should be hidden"
      );
      if ("nsISystemStatusBar" in Ci) {
        let activeStreams = webrtcUI.getActiveStreams(true, false, false);
        webrtcUI.showSharingDoorhanger(activeStreams[0]);
      } else {
        let win = Services.wm.getMostRecentWindow(
          "Browser:WebRTCGlobalIndicator"
        );
        let elt = win.document.getElementById("audioVideoButton");
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
    desc: "'Always Allow' disabled on http pages",
    run: async function checkNoAlwaysOnHttp() {
      // Load an http page instead of the https version.
      await SpecialPowers.pushPrefEnv({
        set: [
          ["media.devices.insecure.enabled", true],
          ["media.getusermedia.insecure.enabled", true],
        ],
      });
      let browser = gBrowser.selectedBrowser;
      BrowserTestUtils.loadURI(
        browser,
        browser.documentURI.spec.replace("https://", "http://")
      );
      await BrowserTestUtils.browserLoaded(browser);

      // Initially set both permissions to 'allow'.
      let uri = browser.documentURI;
      PermissionTestUtils.add(uri, "microphone", Services.perms.ALLOW_ACTION);
      PermissionTestUtils.add(uri, "camera", Services.perms.ALLOW_ACTION);

      // Request devices and expect a prompt despite the saved 'Allow' permission,
      // because the connection isn't secure.
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await expectObserverCalled("getUserMedia:request");

      // Ensure that checking the 'Remember this decision' checkbox disables
      // 'Allow'.
      let notification = PopupNotifications.panel.firstElementChild;
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

      // Cleanup.
      await closeStream(true);
      PermissionTestUtils.remove(uri, "camera");
      PermissionTestUtils.remove(uri, "microphone");
    },
  },
];

add_task(async function test() {
  await runTests(gTests);
});

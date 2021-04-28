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
      let observerPromise = expectObserverCalled("getUserMedia:request");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareDevices-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(true, true);

      // With Proton enabled, the icon does not appear in the panel.
      if (!gProtonDoorhangers) {
        let iconclass = PopupNotifications.panel.firstElementChild.getAttribute(
          "iconclass"
        );
        ok(iconclass.includes("camera-icon"), "panel using devices icon");
      }

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");

      promise = promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
      await promise;

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
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true);
      await promise;
      await observerPromise;

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareMicrophone-notification-icon",
        "anchored to mic icon"
      );
      checkDeviceSelectors(true);

      // With Proton enabled, the icon does not appear in the panel.
      if (!gProtonDoorhangers) {
        let iconclass = PopupNotifications.panel.firstElementChild.getAttribute(
          "iconclass"
        );
        ok(
          iconclass.includes("microphone-icon"),
          "panel using microphone icon"
        );
      }

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");

      promise = promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
      await promise;
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
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await observerPromise;

      is(
        PopupNotifications.getNotification("webRTC-shareDevices").anchorID,
        "webRTC-shareDevices-notification-icon",
        "anchored to device icon"
      );
      checkDeviceSelectors(false, true);

      // With Proton enabled, the icon does not appear in the panel.
      if (!gProtonDoorhangers) {
        let iconclass = PopupNotifications.panel.firstElementChild.getAttribute(
          "iconclass"
        );
        ok(iconclass.includes("camera-icon"), "panel using devices icon");
      }

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
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
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      let observerPromise1 = expectObserverCalled("getUserMedia:response:deny");
      let observerPromise2 = expectObserverCalled("recording-window-ended");
      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });
      await observerPromise1;
      await observerPromise2;

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
      observerPromise1 = expectObserverCalled("getUserMedia:request");
      observerPromise2 = expectObserverCalled("getUserMedia:response:deny");
      let observerPromise3 = expectObserverCalled("recording-window-ended");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise1;
      await observerPromise2;
      await observerPromise3;
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
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
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
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      observerPromise1 = expectObserverCalled("getUserMedia:response:deny");
      observerPromise2 = expectObserverCalled("recording-window-ended");
      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await observerPromise1;
      await observerPromise2;
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
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true, audio: true });

      await reloadAndAssertClosedStreams();

      observerPromise = expectObserverCalled("getUserMedia:request");
      // After the reload, gUM(audio+camera) causes a prompt.
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      observerPromise1 = expectObserverCalled("getUserMedia:response:deny");
      observerPromise2 = expectObserverCalled("recording-window-ended");

      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await observerPromise1;
      await observerPromise2;
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
        let observerPromise = expectObserverCalled("getUserMedia:request");
        let promise = promisePopupNotificationShown("webRTC-shareDevices");
        await promiseRequestDevice(aRequestAudio, aRequestVideo);
        await promise;
        await observerPromise;

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

        let expected = {};
        let observerPromises = [];
        let expectedMessage = aNever ? permissionError : "ok";
        if (expectedMessage == "ok") {
          observerPromises.push(
            expectObserverCalled("getUserMedia:response:allow")
          );
          observerPromises.push(
            expectObserverCalled("recording-device-events")
          );
          if (aRequestVideo) {
            expected.video = true;
          }
          if (aRequestAudio) {
            expected.audio = true;
          }
        } else {
          observerPromises.push(
            expectObserverCalled("getUserMedia:response:deny")
          );
          observerPromises.push(expectObserverCalled("recording-window-ended"));
        }
        await promiseMessage(expectedMessage, () => {
          activateSecondaryAction(aNever ? kActionNever : kActionAlways);
        });
        await Promise.all(observerPromises);
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
          let observerPromise = expectObserverCalled("getUserMedia:request");
          let promise = promisePopupNotificationShown("webRTC-shareDevices");
          await promiseRequestDevice(aRequestAudio, aRequestVideo);
          await promise;
          await observerPromise;

          // Deny the request to cleanup...
          let observerPromise1 = expectObserverCalled(
            "getUserMedia:response:deny"
          );
          let observerPromise2 = expectObserverCalled("recording-window-ended");
          await promiseMessage(permissionError, () => {
            activateSecondaryAction(kActionDeny);
          });
          await observerPromise1;
          await observerPromise2;

          let browser = gBrowser.selectedBrowser;
          SitePermissions.removeFromPrincipal(null, "camera", browser);
          SitePermissions.removeFromPrincipal(null, "microphone", browser);
        } else {
          let expectedMessage = aExpectStream ? "ok" : permissionError;

          let observerPromises = [];
          if (expectedMessage == "ok") {
            observerPromises.push(expectObserverCalled("getUserMedia:request"));
            observerPromises.push(
              expectObserverCalled("getUserMedia:response:allow")
            );
            observerPromises.push(
              expectObserverCalled("recording-device-events")
            );
          } else {
            observerPromises.push(
              expectObserverCalled("recording-window-ended")
            );
          }

          let promise = promiseMessage(expectedMessage);
          await promiseRequestDevice(aRequestAudio, aRequestVideo);
          await promise;
          await Promise.all(observerPromises);

          if (expectedMessage == "ok") {
            await promiseNoPopupNotification("webRTC-shareDevices");

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
    desc: "Stop Sharing removes permissions",
    run: async function checkStopSharingRemovesPermissions() {
      async function stopAndCheckPerm(
        aRequestAudio,
        aRequestVideo,
        aStopAudio = aRequestAudio,
        aStopVideo = aRequestVideo
      ) {
        let uri = gBrowser.selectedBrowser.documentURI;

        // Initially set both permissions to 'allow'.
        PermissionTestUtils.add(uri, "microphone", Services.perms.ALLOW_ACTION);
        PermissionTestUtils.add(uri, "camera", Services.perms.ALLOW_ACTION);
        // Also set device-specific temporary allows.
        SitePermissions.setForPrincipal(
          gBrowser.contentPrincipal,
          "microphone^myDevice",
          SitePermissions.ALLOW,
          SitePermissions.SCOPE_TEMPORARY,
          gBrowser.selectedBrowser,
          10000000
        );
        SitePermissions.setForPrincipal(
          gBrowser.contentPrincipal,
          "camera^myDevice2",
          SitePermissions.ALLOW,
          SitePermissions.SCOPE_TEMPORARY,
          gBrowser.selectedBrowser,
          10000000
        );

        if (aRequestAudio || aRequestVideo) {
          let indicator = promiseIndicatorWindow();
          let observerPromise1 = expectObserverCalled("getUserMedia:request");
          let observerPromise2 = expectObserverCalled(
            "getUserMedia:response:allow"
          );
          let observerPromise3 = expectObserverCalled(
            "recording-device-events"
          );
          // Start sharing what's been requested.
          let promise = promiseMessage("ok");
          await promiseRequestDevice(aRequestAudio, aRequestVideo);
          await promise;
          await observerPromise1;
          await observerPromise2;
          await observerPromise3;

          await indicator;
          await checkSharingUI(
            { video: aRequestVideo, audio: aRequestAudio },
            undefined,
            undefined,
            {
              video: { scope: SitePermissions.SCOPE_PERSISTENT },
              audio: { scope: SitePermissions.SCOPE_PERSISTENT },
            }
          );
          await stopSharing(aStopVideo ? "camera" : "microphone");
        } else {
          await revokePermission(aStopVideo ? "camera" : "microphone");
        }

        // Check that permissions have been removed as expected.
        let audioPerm = SitePermissions.getForPrincipal(
          gBrowser.contentPrincipal,
          "microphone",
          gBrowser.selectedBrowser
        );
        let audioPermDevice = SitePermissions.getForPrincipal(
          gBrowser.contentPrincipal,
          "microphone^myDevice",
          gBrowser.selectedBrowser
        );

        if (
          aRequestAudio ||
          aRequestVideo ||
          aStopAudio ||
          (aStopVideo && aRequestAudio)
        ) {
          Assert.deepEqual(
            audioPerm,
            {
              state: SitePermissions.UNKNOWN,
              scope: SitePermissions.SCOPE_PERSISTENT,
            },
            "microphone permissions removed"
          );
          Assert.deepEqual(
            audioPermDevice,
            {
              state: SitePermissions.UNKNOWN,
              scope: SitePermissions.SCOPE_PERSISTENT,
            },
            "microphone device-specific permissions removed"
          );
        } else {
          Assert.deepEqual(
            audioPerm,
            {
              state: SitePermissions.ALLOW,
              scope: SitePermissions.SCOPE_PERSISTENT,
            },
            "microphone permissions untouched"
          );
          Assert.deepEqual(
            audioPermDevice,
            {
              state: SitePermissions.ALLOW,
              scope: SitePermissions.SCOPE_TEMPORARY,
            },
            "microphone device-specific permissions untouched"
          );
        }

        let videoPerm = SitePermissions.getForPrincipal(
          gBrowser.contentPrincipal,
          "camera",
          gBrowser.selectedBrowser
        );
        let videoPermDevice = SitePermissions.getForPrincipal(
          gBrowser.contentPrincipal,
          "camera^myDevice2",
          gBrowser.selectedBrowser
        );
        if (
          aRequestAudio ||
          aRequestVideo ||
          aStopVideo ||
          (aStopAudio && aRequestVideo)
        ) {
          Assert.deepEqual(
            videoPerm,
            {
              state: SitePermissions.UNKNOWN,
              scope: SitePermissions.SCOPE_PERSISTENT,
            },
            "camera permissions removed"
          );
          Assert.deepEqual(
            videoPermDevice,
            {
              state: SitePermissions.UNKNOWN,
              scope: SitePermissions.SCOPE_PERSISTENT,
            },
            "camera device-specific permissions removed"
          );
        } else {
          Assert.deepEqual(
            videoPerm,
            {
              state: SitePermissions.ALLOW,
              scope: SitePermissions.SCOPE_PERSISTENT,
            },
            "camera permissions untouched"
          );
          Assert.deepEqual(
            videoPermDevice,
            {
              state: SitePermissions.ALLOW,
              scope: SitePermissions.SCOPE_TEMPORARY,
            },
            "camera device-specific permissions untouched"
          );
        }
        await checkNotSharing();

        // Cleanup.
        await closeStream(true);

        SitePermissions.removeFromPrincipal(
          gBrowser.contentPrincipal,
          "camera",
          gBrowser.selectedBrowser
        );
        SitePermissions.removeFromPrincipal(
          gBrowser.contentPrincipal,
          "microphone",
          gBrowser.selectedBrowser
        );
      }

      info("request audio+video, stop sharing video resets both");
      await stopAndCheckPerm(true, true);
      info("request audio only, stop sharing audio resets both");
      await stopAndCheckPerm(true, false);
      info("request video only, stop sharing video resets both");
      await stopAndCheckPerm(false, true);
      info("request audio only, stop sharing video resets both");
      await stopAndCheckPerm(true, false, false, true);
      info("request video only, stop sharing audio resets both");
      await stopAndCheckPerm(false, true, true, false);
      info("request neither, stop audio affects audio only");
      await stopAndCheckPerm(false, false, true, false);
      info("request neither, stop video affects video only");
      await stopAndCheckPerm(false, false, false, true);
    },
  },

  {
    desc: "test showPermissionPanel",
    run: async function checkShowPermissionPanel() {
      if (!USING_LEGACY_INDICATOR) {
        // The indicator only links to the permission panel for the
        // legacy indicator.
        return;
      }

      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true);
      await promise;
      await observerPromise;
      checkDeviceSelectors(false, true);

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () => {
        PopupNotifications.panel.firstElementChild.button.click();
      });
      await observerPromise1;
      await observerPromise2;
      Assert.deepEqual(
        await getMediaCaptureState(),
        { video: true },
        "expected camera to be shared"
      );

      await indicator;
      await checkSharingUI({ video: true });

      ok(permissionPopupHidden(), "permission panel should be hidden");
      if (IS_MAC) {
        let activeStreams = webrtcUI.getActiveStreams(true, false, false);
        webrtcUI.showSharingDoorhanger(activeStreams[0]);
      } else {
        let win = Services.wm.getMostRecentWindow(
          "Browser:WebRTCGlobalIndicator"
        );

        let elt = win.document.getElementById("audioVideoButton");
        EventUtils.synthesizeMouseAtCenter(elt, {}, win);
      }

      await TestUtils.waitForCondition(
        () => !permissionPopupHidden(),
        "wait for permission panel to open"
      );
      ok(!permissionPopupHidden(), "permission panel should be open");

      gPermissionPanel._permissionPopup.hidePopup();

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

      // Disable while loading a new page
      await disableObserverVerification();

      let browser = gBrowser.selectedBrowser;
      BrowserTestUtils.loadURI(
        browser,
        browser.documentURI.spec.replace("https://", "http://")
      );
      await BrowserTestUtils.browserLoaded(browser);

      await enableObserverVerification();

      // Initially set both permissions to 'allow'.
      let uri = browser.documentURI;
      PermissionTestUtils.add(uri, "microphone", Services.perms.ALLOW_ACTION);
      PermissionTestUtils.add(uri, "camera", Services.perms.ALLOW_ACTION);

      // Request devices and expect a prompt despite the saved 'Allow' permission,
      // because the connection isn't secure.
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;

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

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const permissionError =
  "error: NotAllowedError: The request is not allowed " +
  "by the user agent or the platform in the current context.";

const PromptResult = {
  ALLOW: "allow",
  DENY: "deny",
  PROMPT: "prompt",
};

const Perms = Services.perms;

async function promptNoDelegate(aThirdPartyOrgin, audio = true, video = true) {
  // Persistent allowed first party origin
  const uri = gBrowser.selectedBrowser.documentURI;
  if (audio) {
    PermissionTestUtils.add(uri, "microphone", Services.perms.ALLOW_ACTION);
  }
  if (video) {
    PermissionTestUtils.add(uri, "camera", Services.perms.ALLOW_ACTION);
  }

  // Check that we get a prompt.
  const observerPromise = expectObserverCalled("getUserMedia:request");
  const promise = promisePopupNotificationShown("webRTC-shareDevices");
  await promiseRequestDevice(audio, video, "frame4");
  await promise;
  await observerPromise;

  // The 'Remember this decision' checkbox is hidden.
  const notification = PopupNotifications.panel.firstElementChild;
  const checkbox = notification.checkbox;
  ok(!!checkbox, "checkbox is present");
  ok(checkbox.hidden, "checkbox is not visible");
  ok(!checkbox.checked, "checkbox not checked");

  // Check the label of the notification should be the first party
  is(
    PopupNotifications.getNotification("webRTC-shareDevices").options.name,
    uri.host,
    "Use first party's origin"
  );

  // Check the secondName of the notification should be the third party
  is(
    PopupNotifications.getNotification("webRTC-shareDevices").options
      .secondName,
    aThirdPartyOrgin,
    "Use third party's origin as secondName"
  );

  let indicator = promiseIndicatorWindow();
  let observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
  let observerPromise2 = expectObserverCalled("recording-device-events");
  await promiseMessage("ok", () =>
    EventUtils.synthesizeMouseAtCenter(notification.button, {})
  );
  await observerPromise1;
  await observerPromise2;
  let state = await getMediaCaptureState();
  is(
    !!state.audio,
    audio,
    `expected microphone to be ${audio ? "" : "not"} shared`
  );
  is(
    !!state.video,
    video,
    `expected camera to be ${video ? "" : "not"} shared`
  );
  await indicator;
  await checkSharingUI({ audio, video }, undefined, undefined, {
    video: { scope: SitePermissions.SCOPE_PERSISTENT },
    audio: { scope: SitePermissions.SCOPE_PERSISTENT },
  });

  // Cleanup.
  await closeStream(false, "frame4");

  PermissionTestUtils.remove(uri, "camera");
  PermissionTestUtils.remove(uri, "microphone");
}

async function promptNoDelegateScreenSharing(aThirdPartyOrgin) {
  // Persistent allow screen sharing
  const uri = gBrowser.selectedBrowser.documentURI;
  PermissionTestUtils.add(uri, "screen", Services.perms.ALLOW_ACTION);

  const observerPromise = expectObserverCalled("getUserMedia:request");
  const promise = promisePopupNotificationShown("webRTC-shareDevices");
  await promiseRequestDevice(false, true, "frame4", "screen");
  await promise;
  await observerPromise;

  checkDeviceSelectors(false, false, true);
  const notification = PopupNotifications.panel.firstElementChild;

  // With Proton enabled, the icon does not appear in the panel.
  if (!gProtonDoorhangers) {
    const iconclass = notification.getAttribute("iconclass");
    ok(iconclass.includes("screen-icon"), "panel using screen icon");
  }

  // The 'Remember this decision' checkbox is hidden.
  const checkbox = notification.checkbox;
  ok(!!checkbox, "checkbox is present");

  if (ALLOW_SILENCING_NOTIFICATIONS) {
    ok(!checkbox.hidden, "Notification silencing checkbox is visible");
  } else {
    ok(checkbox.hidden, "checkbox is not visible");
  }

  ok(!checkbox.checked, "checkbox not checked");

  // Check the label of the notification should be the first party
  is(
    PopupNotifications.getNotification("webRTC-shareDevices").options.name,
    uri.host,
    "Use first party's origin"
  );

  // Check the secondName of the notification should be the third party
  is(
    PopupNotifications.getNotification("webRTC-shareDevices").options
      .secondName,
    aThirdPartyOrgin,
    "Use third party's origin as secondName"
  );

  const menulist = document.getElementById("webRTC-selectWindow-menulist");
  const count = menulist.itemCount;
  menulist.getItemAtIndex(count - 1).doCommand();
  ok(!notification.button.disabled, "Allow button is enabled");

  const indicator = promiseIndicatorWindow();
  const observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
  const observerPromise2 = expectObserverCalled("recording-device-events");
  await promiseMessage("ok", () =>
    EventUtils.synthesizeMouseAtCenter(notification.button, {})
  );
  await observerPromise1;
  await observerPromise2;
  Assert.deepEqual(
    await getMediaCaptureState(),
    { screen: "Screen" },
    "expected screen to be shared"
  );

  await indicator;
  await checkSharingUI({ screen: "Screen" }, undefined, undefined, {
    screen: { scope: SitePermissions.SCOPE_PERSISTENT },
  });
  await closeStream(false, "frame4");

  PermissionTestUtils.remove(uri, "screen");
}

var gTests = [
  {
    desc:
      "'Always Allow' enabled on third party pages, when origin is explicitly allowed",
    run: async function checkNoAlwaysOnThirdParty() {
      // Initially set both permissions to 'prompt'.
      const uri = gBrowser.selectedBrowser.documentURI;
      PermissionTestUtils.add(uri, "microphone", Services.perms.PROMPT_ACTION);
      PermissionTestUtils.add(uri, "camera", Services.perms.PROMPT_ACTION);

      const observerPromise = expectObserverCalled("getUserMedia:request");
      const promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame1");
      await promise;
      await observerPromise;
      checkDeviceSelectors(true, true);

      // The 'Remember this decision' checkbox is visible.
      const notification = PopupNotifications.panel.firstElementChild;
      const checkbox = notification.checkbox;
      ok(!!checkbox, "checkbox is present");
      ok(!checkbox.hidden, "checkbox is visible");
      ok(!checkbox.checked, "checkbox not checked");

      // Check the label of the notification should be the first party
      is(
        PopupNotifications.getNotification("webRTC-shareDevices").options.name,
        uri.host,
        "Use first party's origin"
      );

      const indicator = promiseIndicatorWindow();
      const observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      const observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () =>
        EventUtils.synthesizeMouseAtCenter(notification.button, {})
      );
      await observerPromise1;
      await observerPromise2;
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );
      await indicator;
      await checkSharingUI({ audio: true, video: true });

      // Cleanup.
      await closeStream(false, "frame1");
      PermissionTestUtils.remove(uri, "camera");
      PermissionTestUtils.remove(uri, "microphone");
    },
  },
  {
    desc:
      "'Always Allow' disabled when sharing screen in third party iframes, when origin is explicitly allowed",
    run: async function checkScreenSharing() {
      const observerPromise = expectObserverCalled("getUserMedia:request");
      const promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(false, true, "frame1", "screen");
      await promise;
      await observerPromise;

      checkDeviceSelectors(false, false, true);
      const notification = PopupNotifications.panel.firstElementChild;

      // With Proton enabled, the icon does not appear in the panel.
      if (!gProtonDoorhangers) {
        const iconclass = notification.getAttribute("iconclass");
        ok(iconclass.includes("screen-icon"), "panel using screen icon");
      }

      // The 'Remember this decision' checkbox is visible.
      const checkbox = notification.checkbox;
      ok(!!checkbox, "checkbox is present");
      ok(!checkbox.hidden, "checkbox is visible");
      ok(!checkbox.checked, "checkbox not checked");

      const menulist = document.getElementById("webRTC-selectWindow-menulist");
      const count = menulist.itemCount;
      ok(
        count >= 4,
        "There should be the 'Select Window or Screen' item, a separator and at least one window and one screen"
      );

      const noWindowOrScreenItem = menulist.getItemAtIndex(0);
      ok(
        noWindowOrScreenItem.hasAttribute("selected"),
        "the 'Select Window or Screen' item is selected"
      );
      is(
        menulist.selectedItem,
        noWindowOrScreenItem,
        "'Select Window or Screen' is the selected item"
      );
      is(menulist.value, "-1", "no window or screen is selected by default");
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

      const indicator = promiseIndicatorWindow();
      const observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      const observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () =>
        EventUtils.synthesizeMouseAtCenter(notification.button, {})
      );
      await observerPromise1;
      await observerPromise2;
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

  {
    desc: "getUserMedia use persistent permissions from first party",
    run: async function checkUsePersistentPermissionsFirstParty() {
      async function checkPersistentPermission(
        aPermission,
        aRequestType,
        aIframeId,
        aExpect
      ) {
        info(
          `Test persistent permission ${aPermission} type ${aRequestType} expect ${aExpect}`
        );
        const uri = gBrowser.selectedBrowser.documentURI;
        // Persistent allow/deny for first party uri
        PermissionTestUtils.add(uri, aRequestType, aPermission);

        let audio = aRequestType == "microphone";
        let video = aRequestType == "camera";
        const screen = aRequestType == "screen" ? "screen" : undefined;
        if (screen) {
          audio = false;
          video = true;
        }
        if (aExpect == PromptResult.PROMPT) {
          // Check that we get a prompt.
          const observerPromise = expectObserverCalled("getUserMedia:request");
          const observerPromise1 = expectObserverCalled(
            "getUserMedia:response:deny"
          );
          const observerPromise2 = expectObserverCalled(
            "recording-window-ended"
          );
          const promise = promisePopupNotificationShown("webRTC-shareDevices");
          await promiseRequestDevice(audio, video, aIframeId, screen);
          await promise;
          await observerPromise;

          // Check the label of the notification should be the first party
          is(
            PopupNotifications.getNotification("webRTC-shareDevices").options
              .name,
            uri.host,
            "Use first party's origin"
          );

          // Deny the request to cleanup...
          await promiseMessage(permissionError, () => {
            activateSecondaryAction(kActionDeny);
          });
          await observerPromise1;
          await observerPromise2;
          let browser = gBrowser.selectedBrowser;
          SitePermissions.removeFromPrincipal(null, aRequestType, browser);
        } else if (aExpect == PromptResult.ALLOW) {
          const observerPromise = expectObserverCalled("getUserMedia:request");
          const observerPromise1 = expectObserverCalled(
            "getUserMedia:response:allow"
          );
          const observerPromise2 = expectObserverCalled(
            "recording-device-events"
          );
          const promise = promiseMessage("ok");
          await promiseRequestDevice(audio, video, aIframeId, screen);
          await promise;
          await observerPromise;

          await promiseNoPopupNotification("webRTC-shareDevices");
          await observerPromise1;
          await observerPromise2;

          let expected = {};
          if (audio) {
            expected.audio = audio;
          }
          if (video) {
            expected.video = video;
          }

          Assert.deepEqual(
            await getMediaCaptureState(),
            expected,
            "expected " + Object.keys(expected).join(" and ") + " to be shared"
          );

          await closeStream(false, aIframeId);
        } else if (aExpect == PromptResult.DENY) {
          const observerPromise = expectObserverCalled(
            "recording-window-ended"
          );
          const promise = promiseMessage(permissionError);
          await promiseRequestDevice(audio, video, aIframeId, screen);
          await promise;
          await observerPromise;
        }

        PermissionTestUtils.remove(uri, aRequestType);
      }

      await checkPersistentPermission(
        Perms.PROMPT_ACTION,
        "camera",
        "frame1",
        PromptResult.PROMPT
      );
      await checkPersistentPermission(
        Perms.DENY_ACTION,
        "camera",
        "frame1",
        PromptResult.DENY
      );
      await checkPersistentPermission(
        Perms.ALLOW_ACTION,
        "camera",
        "frame1",
        PromptResult.ALLOW
      );

      await checkPersistentPermission(
        Perms.PROMPT_ACTION,
        "microphone",
        "frame1",
        PromptResult.PROMPT
      );
      await checkPersistentPermission(
        Perms.DENY_ACTION,
        "microphone",
        "frame1",
        PromptResult.DENY
      );
      await checkPersistentPermission(
        Perms.ALLOW_ACTION,
        "microphone",
        "frame1",
        PromptResult.ALLOW
      );

      // Wildcard attributes still get delegation when their src is unchanged.
      await checkPersistentPermission(
        Perms.PROMPT_ACTION,
        "camera",
        "frame4",
        PromptResult.PROMPT
      );
      await checkPersistentPermission(
        Perms.DENY_ACTION,
        "camera",
        "frame4",
        PromptResult.DENY
      );
      await checkPersistentPermission(
        Perms.ALLOW_ACTION,
        "camera",
        "frame4",
        PromptResult.ALLOW
      );

      // Wildcard attributes still get delegation when their src is unchanged.
      await checkPersistentPermission(
        Perms.PROMPT_ACTION,
        "microphone",
        "frame4",
        PromptResult.PROMPT
      );
      await checkPersistentPermission(
        Perms.DENY_ACTION,
        "microphone",
        "frame4",
        PromptResult.DENY
      );
      await checkPersistentPermission(
        Perms.ALLOW_ACTION,
        "microphone",
        "frame4",
        PromptResult.ALLOW
      );

      await checkPersistentPermission(
        Perms.PROMPT_ACTION,
        "screen",
        "frame1",
        PromptResult.PROMPT
      );
      await checkPersistentPermission(
        Perms.DENY_ACTION,
        "screen",
        "frame1",
        PromptResult.DENY
      );
      // Always prompt screen sharing
      await checkPersistentPermission(
        Perms.ALLOW_ACTION,
        "screen",
        "frame1",
        PromptResult.PROMPT
      );
      await checkPersistentPermission(
        Perms.ALLOW_ACTION,
        "screen",
        "frame4",
        PromptResult.PROMPT
      );

      // Denied by default if allow is not defined
      await checkPersistentPermission(
        Perms.PROMPT_ACTION,
        "camera",
        "frame3",
        PromptResult.DENY
      );
      await checkPersistentPermission(
        Perms.DENY_ACTION,
        "camera",
        "frame3",
        PromptResult.DENY
      );
      await checkPersistentPermission(
        Perms.ALLOW_ACTION,
        "camera",
        "frame3",
        PromptResult.DENY
      );

      await checkPersistentPermission(
        Perms.PROMPT_ACTION,
        "microphone",
        "frame3",
        PromptResult.DENY
      );
      await checkPersistentPermission(
        Perms.DENY_ACTION,
        "microphone",
        "frame3",
        PromptResult.DENY
      );
      await checkPersistentPermission(
        Perms.ALLOW_ACTION,
        "microphone",
        "frame3",
        PromptResult.DENY
      );

      await checkPersistentPermission(
        Perms.PROMPT_ACTION,
        "screen",
        "frame3",
        PromptResult.DENY
      );
      await checkPersistentPermission(
        Perms.DENY_ACTION,
        "screen",
        "frame3",
        PromptResult.DENY
      );
      await checkPersistentPermission(
        Perms.ALLOW_ACTION,
        "screen",
        "frame3",
        PromptResult.DENY
      );
    },
  },

  {
    desc: "getUserMedia use temporary blocked permissions from first party",
    run: async function checkUseTempPermissionsBlockFirstParty() {
      async function checkTempPermission(aRequestType) {
        let browser = gBrowser.selectedBrowser;
        let observerPromise = expectObserverCalled("getUserMedia:request");
        let observerPromise1 = expectObserverCalled(
          "getUserMedia:response:deny"
        );
        let observerPromise2 = expectObserverCalled("recording-window-ended");
        let promise = promisePopupNotificationShown("webRTC-shareDevices");
        let audio = aRequestType == "microphone";
        let video = aRequestType == "camera";
        const screen = aRequestType == "screen" ? "screen" : undefined;
        if (screen) {
          audio = false;
          video = true;
        }

        await promiseRequestDevice(audio, video, null, screen);
        await promise;
        await observerPromise;

        // Temporarily grant/deny from top level
        // Only need to check allow and deny temporary permissions
        await promiseMessage(permissionError, () => {
          activateSecondaryAction(kActionDeny);
        });
        await observerPromise1;
        await observerPromise2;
        await checkNotSharing();

        observerPromise = expectObserverCalled("getUserMedia:request");
        observerPromise1 = expectObserverCalled("getUserMedia:response:deny");
        observerPromise2 = expectObserverCalled("recording-window-ended");
        promise = promiseMessage(permissionError);
        await promiseRequestDevice(audio, video, "frame1", screen);
        await promise;

        await observerPromise;
        await observerPromise1;
        await observerPromise2;

        SitePermissions.removeFromPrincipal(null, aRequestType, browser);
      }

      // At the moment we only save temporary deny
      await checkTempPermission("camera");
      await checkTempPermission("microphone");
      await checkTempPermission("screen");
    },
  },
  {
    desc:
      "Don't reprompt while actively sharing in maybe unsafe permission delegation",
    run: async function checkNoRepromptNoDelegate() {
      // Change location to ensure that we're treated as potentially unsafe.
      await promiseChangeLocationFrame(
        "frame4",
        "https://test2.example.com/browser/browser/base/content/test/webrtc/get_user_media.html"
      );

      // Check that we get a prompt.
      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame4");
      await promise;
      await observerPromise;

      // Check the secondName of the notification should be the third party
      is(
        PopupNotifications.getNotification("webRTC-shareDevices").options
          .secondName,
        "test2.example.com",
        "Use third party's origin as secondName"
      );

      const notification = PopupNotifications.panel.firstElementChild;
      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () =>
        EventUtils.synthesizeMouseAtCenter(notification.button, {})
      );
      await observerPromise1;
      await observerPromise2;

      let state = await getMediaCaptureState();
      is(!!state.audio, true, "expected microphone to be shared");
      is(!!state.video, true, "expected camera to be shared");
      await indicator;
      await checkSharingUI({ audio: true, video: true });

      // Check that we now don't get a prompt.
      observerPromise = expectObserverCalled("getUserMedia:request");
      observerPromise1 = expectObserverCalled("getUserMedia:response:allow");
      observerPromise2 = expectObserverCalled("recording-device-events");
      promise = promiseMessage("ok");
      await promiseRequestDevice(true, true, "frame4");
      await promise;
      await observerPromise;

      await promiseNoPopupNotification("webRTC-shareDevices");
      await observerPromise1;
      await observerPromise2;

      state = await getMediaCaptureState();
      is(!!state.audio, true, "expected microphone to be shared");
      is(!!state.video, true, "expected camera to be shared");
      await checkSharingUI({ audio: true, video: true });

      // Cleanup.
      await closeStream(false, "frame4");
    },
  },
  {
    desc:
      "Change location, prompt and display both first party and third party origin in maybe unsafe permission delegation",
    run: async function checkPromptNoDelegateChangeLoxation() {
      await promiseChangeLocationFrame(
        "frame4",
        "https://test2.example.com/browser/browser/base/content/test/webrtc/get_user_media.html"
      );
      await promptNoDelegate("test2.example.com");
    },
  },
  {
    desc:
      "Change location, prompt and display both first party and third party origin when sharing screen in unsafe permission delegation",
    run: async function checkPromptNoDelegateScreenSharingChangeLocation() {
      await promiseChangeLocationFrame(
        "frame4",
        "https://test2.example.com/browser/browser/base/content/test/webrtc/get_user_media.html"
      );
      await promptNoDelegateScreenSharing("test2.example.com");
    },
  },
  {
    desc:
      "Prompt and display both first party and third party origin and temporary deny in frame does not change permission scope",
    skipObserverVerification: true,
    run: async function checkPromptBothOriginsTempDenyFrame() {
      // Change location to ensure that we're treated as potentially unsafe.
      await promiseChangeLocationFrame(
        "frame4",
        "https://test2.example.com/browser/browser/base/content/test/webrtc/get_user_media.html"
      );

      // Persistent allowed first party origin
      let browser = gBrowser.selectedBrowser;
      let uri = gBrowser.selectedBrowser.documentURI;
      let principal = Services.scriptSecurityManager.createContentPrincipal(
        uri,
        {}
      );

      let observerPromise = expectObserverCalled("getUserMedia:request");
      let promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true);
      await promise;
      await observerPromise;

      // Ensure that checking the 'Remember this decision'
      let notification = PopupNotifications.panel.firstElementChild;
      let checkbox = notification.checkbox;
      ok(!!checkbox, "checkbox is present");
      ok(!checkbox.checked, "checkbox is not checked");
      checkbox.click();

      let indicator = promiseIndicatorWindow();
      let observerPromise1 = expectObserverCalled(
        "getUserMedia:response:allow"
      );
      let observerPromise2 = expectObserverCalled("recording-device-events");
      await promiseMessage("ok", () =>
        EventUtils.synthesizeMouseAtCenter(notification.button, {})
      );
      await observerPromise1;
      await observerPromise2;
      Assert.deepEqual(
        await getMediaCaptureState(),
        { audio: true, video: true },
        "expected camera and microphone to be shared"
      );
      await indicator;
      await checkSharingUI({ audio: true, video: true }, undefined, undefined, {
        audio: { scope: SitePermissions.SCOPE_PERSISTENT },
        video: { scope: SitePermissions.SCOPE_PERSISTENT },
      });
      await closeStream(true);

      // Check that we get a prompt.
      observerPromise = expectObserverCalled("getUserMedia:request");
      promise = promisePopupNotificationShown("webRTC-shareDevices");
      await promiseRequestDevice(true, true, "frame4");
      await promise;
      await observerPromise;

      // The 'Remember this decision' checkbox is hidden.
      notification = PopupNotifications.panel.firstElementChild;
      checkbox = notification.checkbox;
      ok(!!checkbox, "checkbox is present");
      ok(checkbox.hidden, "checkbox is not visible");

      observerPromise1 = expectObserverCalled("getUserMedia:response:deny");
      observerPromise2 = expectObserverCalled("recording-window-ended");
      await promiseMessage(permissionError, () => {
        activateSecondaryAction(kActionDeny);
      });

      await observerPromise1;
      await observerPromise2;
      await checkNotSharing();

      // Make sure we are not changing the scope and state of persistent
      // permission
      let { state, scope } = SitePermissions.getForPrincipal(
        principal,
        "camera",
        browser
      );
      Assert.equal(state, SitePermissions.ALLOW);
      Assert.equal(scope, SitePermissions.SCOPE_PERSISTENT);

      ({ state, scope } = SitePermissions.getForPrincipal(
        principal,
        "microphone",
        browser
      ));
      Assert.equal(state, SitePermissions.ALLOW);
      Assert.equal(scope, SitePermissions.SCOPE_PERSISTENT);

      PermissionTestUtils.remove(uri, "camera");
      PermissionTestUtils.remove(uri, "microphone");
    },
  },
];

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["permissions.delegation.enabled", true],
      ["dom.security.featurePolicy.header.enabled", true],
      ["dom.security.featurePolicy.webidl.enabled", true],
    ],
  });

  await runTests(gTests, {
    relativeURI: "get_user_media_in_xorigin_frame.html",
  });
});

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

function expectObserverCalledAncestor(aTopic, browsingContext) {
  if (!gMultiProcessBrowser) {
    return expectObserverCalledInProcess(aTopic);
  }

  return BrowserTestUtils.contentTopicObserved(browsingContext, aTopic);
}

function enableObserverVerificationAncestor(browsingContext) {
  // Skip these checks in single process mode as it isn't worth implementing it.
  if (!gMultiProcessBrowser) {
    return Promise.resolve();
  }

  return BrowserTestUtils.startObservingTopics(browsingContext, observerTopics);
}

function disableObserverVerificationAncestor(browsingContextt) {
  if (!gMultiProcessBrowser) {
    return Promise.resolve();
  }

  return BrowserTestUtils.stopObservingTopics(
    browsingContextt,
    observerTopics
  ).catch(reason => {
    ok(false, "Failed " + reason);
  });
}

function promiseRequestDeviceAncestor(
  aRequestAudio,
  aRequestVideo,
  aType,
  aBrowser,
  aBadDevice = false
) {
  info("requesting devices");
  return SpecialPowers.spawn(
    aBrowser,
    [{ aRequestAudio, aRequestVideo, aType, aBadDevice }],
    async function (args) {
      let global =
        content.wrappedJSObject.document.getElementById("frame4").contentWindow;
      global.requestDevice(
        args.aRequestAudio,
        args.aRequestVideo,
        args.aType,
        args.aBadDevice
      );
    }
  );
}

async function closeStreamAncestor(browser) {
  let observerPromises = [];
  observerPromises.push(
    expectObserverCalledAncestor("recording-device-events", browser)
  );
  observerPromises.push(
    expectObserverCalledAncestor("recording-window-ended", browser)
  );

  info("closing the stream");
  await SpecialPowers.spawn(browser, [], async () => {
    let global =
      content.wrappedJSObject.document.getElementById("frame4").contentWindow;
    global.closeStream();
  });

  await Promise.all(observerPromises);

  await assertWebRTCIndicatorStatus(null);
}

var gTests = [
  {
    desc: "getUserMedia use persistent permissions from first party if third party is explicitly trusted",
    skipObserverVerification: true,
    run: async function checkPermissionsAncestorChain() {
      async function checkPermission(aPermission, aRequestType, aExpect) {
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
        const iframeAncestor = await SpecialPowers.spawn(
          gBrowser.selectedBrowser,
          [],
          () => {
            return content.document.getElementById("frameAncestor")
              .browsingContext;
          }
        );

        if (aExpect == PromptResult.PROMPT) {
          // Check that we get a prompt.
          const observerPromise = expectObserverCalledAncestor(
            "getUserMedia:request",
            iframeAncestor
          );
          const promise = promisePopupNotificationShown("webRTC-shareDevices");

          await promiseRequestDeviceAncestor(
            audio,
            video,
            screen,
            iframeAncestor
          );
          await promise;
          await observerPromise;

          // Check the label of the notification should be the first party
          is(
            PopupNotifications.getNotification("webRTC-shareDevices").options
              .name,
            uri.host,
            "Use first party's origin"
          );
          const observerPromise1 = expectObserverCalledAncestor(
            "getUserMedia:response:deny",
            iframeAncestor
          );
          const observerPromise2 = expectObserverCalledAncestor(
            "recording-window-ended",
            iframeAncestor
          );
          // Deny the request to cleanup...
          activateSecondaryAction(kActionDeny);
          await observerPromise1;
          await observerPromise2;
          let browser = gBrowser.selectedBrowser;
          SitePermissions.removeFromPrincipal(null, aRequestType, browser);
        } else if (aExpect == PromptResult.ALLOW) {
          const observerPromise = expectObserverCalledAncestor(
            "getUserMedia:request",
            iframeAncestor
          );
          const observerPromise1 = expectObserverCalledAncestor(
            "getUserMedia:response:allow",
            iframeAncestor
          );
          const observerPromise2 = expectObserverCalledAncestor(
            "recording-device-events",
            iframeAncestor
          );
          await promiseRequestDeviceAncestor(
            audio,
            video,
            screen,
            iframeAncestor
          );
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

          await closeStreamAncestor(iframeAncestor);
        } else if (aExpect == PromptResult.DENY) {
          const observerPromise = expectObserverCalledAncestor(
            "recording-window-ended",
            iframeAncestor
          );
          await promiseRequestDeviceAncestor(
            audio,
            video,
            screen,
            iframeAncestor
          );
          await observerPromise;
        }

        PermissionTestUtils.remove(uri, aRequestType);
      }

      await checkPermission(Perms.PROMPT_ACTION, "camera", PromptResult.PROMPT);
      await checkPermission(Perms.DENY_ACTION, "camera", PromptResult.DENY);
      await checkPermission(Perms.ALLOW_ACTION, "camera", PromptResult.ALLOW);

      await checkPermission(
        Perms.PROMPT_ACTION,
        "microphone",
        PromptResult.PROMPT
      );
      await checkPermission(Perms.DENY_ACTION, "microphone", PromptResult.DENY);
      await checkPermission(
        Perms.ALLOW_ACTION,
        "microphone",
        PromptResult.ALLOW
      );

      await checkPermission(Perms.PROMPT_ACTION, "screen", PromptResult.PROMPT);
      await checkPermission(Perms.DENY_ACTION, "screen", PromptResult.DENY);
      // Always prompt screen sharing
      await checkPermission(Perms.ALLOW_ACTION, "screen", PromptResult.PROMPT);
    },
  },
];

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.featurePolicy.header.enabled", true],
      ["dom.security.featurePolicy.webidl.enabled", true],
    ],
  });

  await runTests(gTests, {
    relativeURI: "get_user_media_in_xorigin_frame_ancestor.html",
  });
});

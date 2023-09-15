const PAGE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";
const PAGE2_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_main_frame_with_multiple_child_session_frames.html";
const IFRAME_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_iframe_media.html";
const CORS_IFRAME_URL =
  "https://example.org/browser/dom/media/mediacontrol/tests/browser/file_iframe_media.html";
const CORS_IFRAME2_URL =
  "https://test1.example.org/browser/dom/media/mediacontrol/tests/browser/file_iframe_media.html";
const videoId = "video";

/**
 * This test is used to check the scenario when we should use the customized
 * action handler and the the default action handler (play/pause/stop).
 * If a frame (DOM Window, it could be main frame or an iframe) has active media
 * session, then it should use the customized action handler it it has one.
 * Otherwise, the default action handler should be used.
 */
add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

add_task(async function triggerDefaultActionHandler() {
  // Default handler should be triggered no matter if media session exists or not.
  const kCreateMediaSession = [true, false];
  for (const shouldCreateSession of kCreateMediaSession) {
    info(`open page and start media`);
    const tab = await createLoadedTabWrapper(PAGE_URL);
    await playMedia(tab, videoId);

    if (shouldCreateSession) {
      info(
        `media has started, so created session should become active session`
      );
      await Promise.all([
        waitUntilActiveMediaSessionChanged(),
        createMediaSession(tab),
      ]);
    }

    info(`test 'pause' action`);
    await simulateMediaAction(tab, "pause");

    info(`default action handler should pause media`);
    await checkOrWaitUntilMediaPauses(tab, { videoId });

    info(`test 'play' action`);
    await simulateMediaAction(tab, "play");

    info(`default action handler should resume media`);
    await checkOrWaitUntilMediaPlays(tab, { videoId });

    info(`test 'stop' action`);
    await simulateMediaAction(tab, "stop");

    info(`default action handler should pause media`);
    await checkOrWaitUntilMediaPauses(tab, { videoId });

    const controller = tab.linkedBrowser.browsingContext.mediaController;
    ok(
      !controller.isActive,
      `controller should be deactivated after receiving stop`
    );

    info(`remove tab`);
    await tab.close();
  }
});

add_task(async function triggerNonDefaultHandlerWhenSetCustomizedHandler() {
  info(`open page and start media`);
  const tab = await createLoadedTabWrapper(PAGE_URL);
  await Promise.all([
    new Promise(r => (tab.controller.onactivated = r)),
    startMedia(tab, { videoId }),
  ]);

  const kActions = ["play", "pause", "stop"];
  for (const action of kActions) {
    info(`set action handler for '${action}'`);
    await setActionHandler(tab, action);

    info(`press '${action}' should trigger action handler (not a default one)`);
    await simulateMediaAction(tab, action);
    await waitUntilActionHandlerIsTriggered(tab, action);

    info(`action handler doesn't pause media, media should keep playing`);
    await checkOrWaitUntilMediaPlays(tab, { videoId });
  }

  info(`remove tab`);
  await tab.close();
});

add_task(
  async function triggerDefaultHandlerToPausePlaybackOnInactiveSession() {
    const kIframeUrls = [IFRAME_URL, CORS_IFRAME_URL];
    for (const url of kIframeUrls) {
      const kActions = ["play", "pause", "stop"];
      for (const action of kActions) {
        info(`open page and load iframe`);
        const tab = await createLoadedTabWrapper(PAGE_URL);
        const frameId = "iframe";
        await loadIframe(tab, frameId, url);

        info(`start media from iframe would make it become active session`);
        await Promise.all([
          new Promise(r => (tab.controller.onactivated = r)),
          startMedia(tab, { frameId }),
        ]);

        info(`press '${action}' should trigger iframe's action handler`);
        await setActionHandler(tab, action, frameId);
        await simulateMediaAction(tab, action);
        await waitUntilActionHandlerIsTriggered(tab, action, frameId);

        info(`start media from main frame so iframe would become inactive`);
        // When action is `play`, controller is already playing, because above
        // code won't pause media. So we need to wait for the active session
        // changed to ensure the following tests can be executed on the right
        // browsing context.
        let waitForControllerStatusChanged =
          action == "play"
            ? waitUntilActiveMediaSessionChanged()
            : ensureControllerIsPlaying(tab.controller);
        await Promise.all([
          waitForControllerStatusChanged,
          startMedia(tab, { videoId }),
        ]);

        if (action == "play") {
          info(`pause media first in order to test 'play'`);
          await pauseAllMedia(tab);

          info(
            `press '${action}' would trigger default andler on main frame because it doesn't set action handler`
          );
          await simulateMediaAction(tab, action);
          await checkOrWaitUntilMediaPlays(tab, { videoId });

          info(
            `default handler should also be triggered on inactive iframe, which would resume media`
          );
          await checkOrWaitUntilMediaPlays(tab, { frameId });
        } else {
          info(
            `press '${action}' would trigger default andler on main frame because it doesn't set action handler`
          );
          await simulateMediaAction(tab, action);
          await checkOrWaitUntilMediaPauses(tab, { videoId });

          info(
            `default handler should also be triggered on inactive iframe, which would pause media`
          );
          await checkOrWaitUntilMediaPauses(tab, { frameId });
        }

        info(`remove tab`);
        await tab.close();
      }
    }
  }
);

add_task(async function onlyResumeActiveMediaSession() {
  info(`open page and load iframes`);
  const tab = await createLoadedTabWrapper(PAGE2_URL);
  const frame1Id = "frame1";
  const frame2Id = "frame2";
  await loadIframe(tab, frame1Id, CORS_IFRAME_URL);
  await loadIframe(tab, frame2Id, CORS_IFRAME2_URL);

  info(`start media from iframe1 would make it become active session`);
  await createMediaSession(tab, frame1Id);
  await Promise.all([
    waitUntilActiveMediaSessionChanged(),
    startMedia(tab, { frameId: frame1Id }),
  ]);

  info(`start media from iframe2 would make it become active session`);
  await createMediaSession(tab, frame2Id);
  await Promise.all([
    waitUntilActiveMediaSessionChanged(),
    startMedia(tab, { frameId: frame2Id }),
  ]);

  info(`press 'pause' should pause both iframes`);
  await simulateMediaAction(tab, "pause");
  await checkOrWaitUntilMediaPauses(tab, { frameId: frame1Id });
  await checkOrWaitUntilMediaPauses(tab, { frameId: frame2Id });

  info(
    `press 'play' should only resume iframe2 which has active media session`
  );
  await simulateMediaAction(tab, "play");
  await checkOrWaitUntilMediaPauses(tab, { frameId: frame1Id });
  await checkOrWaitUntilMediaPlays(tab, { frameId: frame2Id });

  info(`remove tab`);
  await tab.close();
});

/**
 * The following are helper functions.
 */
function startMedia(tab, { videoId, frameId }) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [videoId, frameId],
    (videoId, frameId) => {
      if (frameId) {
        return content.messageHelper(
          content.document.getElementById(frameId),
          "play",
          "played"
        );
      }
      return content.document.getElementById(videoId).play();
    }
  );
}

function pauseAllMedia(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.messageHelper(
      content.document.getElementById("iframe"),
      "pause",
      "paused"
    );
    const videos = content.document.getElementsByTagName("video");
    for (let video of videos) {
      video.pause();
    }
  });
}

function createMediaSession(tab, frameId = null) {
  info(`create media session`);
  return SpecialPowers.spawn(tab.linkedBrowser, [frameId], async frameId => {
    if (frameId) {
      await content.messageHelper(
        content.document.getElementById(frameId),
        "create-media-session",
        "created-media-session"
      );
      return;
    }
    // simply calling a media session would create an instance.
    content.navigator.mediaSession;
  });
}

function checkOrWaitUntilMediaPauses(tab, { videoId, frameId }) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [videoId, frameId],
    (videoId, frameId) => {
      if (frameId) {
        return content.messageHelper(
          content.document.getElementById(frameId),
          "check-pause",
          "checked-pause"
        );
      }
      return new Promise(r => {
        const video = content.document.getElementById(videoId);
        if (video.paused) {
          ok(true, `media stopped playing`);
          r();
        } else {
          info(`wait until media stops playing`);
          video.onpause = () => {
            video.onpause = null;
            ok(true, `media stopped playing`);
            r();
          };
        }
      });
    }
  );
}

function checkOrWaitUntilMediaPlays(tab, { videoId, frameId }) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [videoId, frameId],
    (videoId, frameId) => {
      if (frameId) {
        return content.messageHelper(
          content.document.getElementById(frameId),
          "check-playing",
          "checked-playing"
        );
      }
      return new Promise(r => {
        const video = content.document.getElementById(videoId);
        if (!video.paused) {
          ok(true, `media is playing`);
          r();
        } else {
          info(`wait until media starts playing`);
          video.onplay = () => {
            video.onplay = null;
            ok(true, `media starts playing`);
            r();
          };
        }
      });
    }
  );
}

function setActionHandler(tab, action, frameId = null) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [action, frameId],
    async (action, frameId) => {
      if (frameId) {
        await content.messageHelper(
          content.document.getElementById(frameId),
          {
            cmd: "setActionHandler",
            action,
          },
          "setActionHandler-done"
        );
        return;
      }
      // Create this on the first function call
      if (content.actionHandlerPromises === undefined) {
        content.actionHandlerPromises = {};
      }
      content.actionHandlerPromises[action] = new Promise(r => {
        content.navigator.mediaSession.setActionHandler(action, () => {
          info(`receive ${action}`);
          r();
        });
      });
    }
  );
}

async function waitUntilActionHandlerIsTriggered(tab, action, frameId = null) {
  info(`wait until '${action}' action handler is triggered`);
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [action, frameId],
    (action, frameId) => {
      if (frameId) {
        return content.messageHelper(
          content.document.getElementById(frameId),
          {
            cmd: "checkActionHandler",
            action,
          },
          "checkActionHandler-done"
        );
      }
      const actionTriggerPromise = content.actionHandlerPromises[action];
      ok(actionTriggerPromise, `Has created promise for ${action}`);
      return actionTriggerPromise;
    }
  );
}

async function simulateMediaAction(tab, action) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  if (!controller.isActive) {
    await new Promise(r => (controller.onactivated = r));
  }
  MediaControlService.generateMediaControlKey(action);
}

function loadIframe(tab, iframeId, url) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [iframeId, url],
    async (iframeId, url) => {
      const iframe = content.document.getElementById(iframeId);
      info(`load iframe with url '${url}'`);
      iframe.src = url;
      await new Promise(r => (iframe.onload = r));
      // create a helper to simplify communication process with iframe
      content.messageHelper = (target, sentMessage, expectedResponse) => {
        target.contentWindow.postMessage(sentMessage, "*");
        return new Promise(r => {
          content.onmessage = event => {
            if (event.data == expectedResponse) {
              ok(true, `Received response ${expectedResponse}`);
              content.onmessage = null;
              r();
            }
          };
        });
      };
    }
  );
}

function waitUntilActiveMediaSessionChanged() {
  return BrowserUtils.promiseObserved("active-media-session-changed");
}

function ensureControllerIsPlaying(controller) {
  return new Promise(r => {
    if (controller.isPlaying) {
      r();
      return;
    }
    controller.onplaybackstatechange = () => {
      if (controller.isPlaying) {
        r();
      }
    };
  });
}

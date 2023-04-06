"use strict";

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const videoUrl =
  "https://www.mozilla.org/tests/dom/media/webaudio/test/noaudio.webm";

function testAutoplayPermission(browser) {
  let principal = browser.contentPrincipal;
  is(
    PermissionTestUtils.testPermission(principal, "autoplay-media"),
    Services.perms.ALLOW_ACTION,
    `Autoplay is allowed on ${principal.origin}`
  );
}

async function openAWWithVideo({
  autoPlay = false,
  video_url = videoUrl,
  ...rest
} = {}) {
  const content = [
    {
      id: "VIDEO_ONBOARDING",
      content: {
        position: "center",
        logo: {},
        title: "Video onboarding",
        secondary_button: { label: "Skip video", action: { navigate: true } },
        video_container: {
          video_url,
          action: { navigate: true },
          autoPlay,
          ...rest,
        },
      },
    },
  ];
  await setAboutWelcomeMultiStage(JSON.stringify(content));
  let { cleanup, browser } = await openMRAboutWelcome();
  return {
    browser,
    content,
    async cleanup() {
      await SpecialPowers.popPrefEnv();
      await cleanup();
    },
  };
}

add_task(async function test_aboutwelcome_video_autoplay() {
  let { cleanup, browser } = await openAWWithVideo({ autoPlay: true });

  testAutoplayPermission(browser);

  await SpecialPowers.spawn(browser, [videoUrl], async url => {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector("main.with-video"),
      "Waiting for video onboarding screen"
    );
    let video = content.document.querySelector(`video[src='${url}'][autoplay]`);
    await ContentTaskUtils.waitForCondition(
      () =>
        video.currentTime > 0 &&
        !video.paused &&
        !video.ended &&
        video.readyState > 2,
      "Waiting for video to play"
    );
    ok(!video.error, "Video should not have an error");
  });

  await cleanup();
});

add_task(async function test_aboutwelcome_video_no_autoplay() {
  let { cleanup, browser } = await openAWWithVideo();

  testAutoplayPermission(browser);

  await SpecialPowers.spawn(browser, [videoUrl], async url => {
    let video = await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector(`video[src='${url}']:not([autoplay])`),
      "Waiting for video element to render"
    );
    await ContentTaskUtils.waitForCondition(
      () => video.paused && !video.ended && video.readyState > 2,
      "Waiting for video to be playable but not playing"
    );
    ok(!video.error, "Video should not have an error");
  });

  await cleanup();
});

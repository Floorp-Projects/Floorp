/*
 * Description of the Test:
 * We load an https page which uses a CSP including block-all-mixed-content.
 * The page embedded an audio, img and video. ML2 should upgrade them and
 * CSP should not be triggered.
 */

const PRE_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

let gExpectedMessages = 0;
let gExpectBlockAllMsg = false;

function onConsoleMessage({ message }) {
  // Check if csp warns about block-all-mixed content being obsolete
  if (message.includes("Content-Security-Policy")) {
    if (gExpectBlockAllMsg) {
      ok(
        message.includes("block-all-mixed-content obsolete"),
        "CSP warns about block-all-mixed content being obsolete"
      );
    } else {
      ok(
        message.includes("Blocking insecure request"),
        "CSP error about blocking insecure request"
      );
    }
  }
  if (message.includes("Mixed Content:")) {
    ok(
      message.includes("Upgrading insecure display request"),
      "msg included a mixed content upgrade"
    );
    gExpectedMessages--;
  }
}

async function checkLoadedElements() {
  let url =
    PRE_PATH +
    "file_csp_block_all_mixedcontent_and_mixed_content_display_upgrade.html";
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
      waitForLoad: true,
    },
    async function (browser) {
      return ContentTask.spawn(browser, [], async function () {
        console.log(content.document.innerHTML);

        // Check image loaded
        let image = content.document.getElementById("some-img");
        let imageLoaded = image && image.complete && image.naturalHeight !== 0;
        // Check audio loaded
        let audio = content.document.getElementById("some-audio");
        let audioLoaded = audio && audio.readyState >= 2;
        // Check video loaded
        let video = content.document.getElementById("some-video");
        //let videoPlayable = await once(video, "loadeddata").then(_ => true);
        let videoLoaded = video && video.readyState === 4;
        return { audio: audioLoaded, img: imageLoaded, video: videoLoaded };
      });
    }
  );
}

add_task(async function test_upgrade_all() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.mixed_content.upgrade_display_content", true],
      // Not enabled by default outside Nightly.
      ["security.mixed_content.upgrade_display_content.image", true],
    ],
  });
  Services.console.registerListener(onConsoleMessage);

  // Starting the test
  gExpectedMessages = 3;
  gExpectBlockAllMsg = true;

  let loadedElements = await checkLoadedElements();
  is(loadedElements.img, true, "Image loaded and was upgraded");
  is(loadedElements.video, true, "Video loaded and was upgraded");
  is(loadedElements.audio, true, "Audio loaded and was upgraded");

  await BrowserTestUtils.waitForCondition(() => gExpectedMessages === 0);

  // Clean up
  Services.console.unregisterListener(onConsoleMessage);
  SpecialPowers.popPrefEnv();
});

add_task(async function test_dont_upgrade_image() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.mixed_content.upgrade_display_content", true],
      ["security.mixed_content.upgrade_display_content.image", false],
    ],
  });
  Services.console.registerListener(onConsoleMessage);

  // Starting the test
  gExpectedMessages = 2;
  gExpectBlockAllMsg = false;

  let loadedElements = await checkLoadedElements();
  is(loadedElements.img, false, "Image was not loaded");
  is(loadedElements.video, true, "Video loaded and was upgraded");
  is(loadedElements.audio, true, "Audio loaded and was upgraded");

  await BrowserTestUtils.waitForCondition(() => gExpectedMessages === 0);

  // Clean up
  Services.console.unregisterListener(onConsoleMessage);
  SpecialPowers.popPrefEnv();
});

add_task(async function test_dont_upgrade_audio() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.mixed_content.upgrade_display_content", true],
      ["security.mixed_content.upgrade_display_content.image", true],
      ["security.mixed_content.upgrade_display_content.audio", false],
    ],
  });
  Services.console.registerListener(onConsoleMessage);

  // Starting the test
  gExpectedMessages = 2;
  gExpectBlockAllMsg = false;

  let loadedElements = await checkLoadedElements();
  is(loadedElements.img, true, "Image loaded and was upgraded");
  is(loadedElements.video, true, "Video loaded and was upgraded");
  is(loadedElements.audio, false, "Audio was not loaded");

  await BrowserTestUtils.waitForCondition(() => gExpectedMessages === 0);

  // Clean up
  Services.console.unregisterListener(onConsoleMessage);
  SpecialPowers.popPrefEnv();
});

add_task(async function test_dont_upgrade_video() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.mixed_content.upgrade_display_content", true],
      ["security.mixed_content.upgrade_display_content.image", true],
      ["security.mixed_content.upgrade_display_content.video", false],
    ],
  });
  Services.console.registerListener(onConsoleMessage);

  // Starting the test
  gExpectedMessages = 2;
  gExpectBlockAllMsg = false;

  let loadedElements = await checkLoadedElements();
  is(loadedElements.img, true, "Image loaded and was upgraded");
  is(loadedElements.video, false, "Video was not loaded");
  is(loadedElements.audio, true, "Audio loaded and was upgraded");

  await BrowserTestUtils.waitForCondition(() => gExpectedMessages === 0);

  // Clean up
  Services.console.unregisterListener(onConsoleMessage);
  SpecialPowers.popPrefEnv();
});

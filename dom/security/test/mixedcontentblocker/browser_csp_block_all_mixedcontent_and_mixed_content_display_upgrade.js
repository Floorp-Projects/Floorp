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
var gTestBrowser = null;
let expectedMessages = 3;
function on_new_message(msgObj) {
  const message = msgObj.message;

  // Check if csp warns about block-all-mixed content being obsolete
  if (message.includes("Content-Security-Policy")) {
    ok(
      message.includes("block-all-mixed-content obsolete"),
      "CSP warns about block-all-mixed content being obsolete"
    );
  }
  if (message.includes("Mixed Content:")) {
    ok(
      message.includes("Upgrading insecure display request"),
      "msg included a mixed content upgrade"
    );
    expectedMessages--;
  }
}

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["security.mixed_content.upgrade_display_content", true]],
  });
  Services.console.registerListener(on_new_message);
  // Starting the test
  var url =
    PRE_PATH +
    "file_csp_block_all_mixedcontent_and_mixed_content_display_upgrade.html";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
      waitForLoad: true,
    },
    async function (browser) {
      let loadedElements = await ContentTask.spawn(
        browser,
        [],
        async function () {
          // Check image loaded
          let image = content.document.getElementById("some-img");
          let imageLoaded =
            image && image.complete && image.naturalHeight !== 0;
          // Check audio loaded
          let audio = content.document.getElementById("some-audio");
          let audioLoaded = audio && audio.readyState >= 2;
          // Check video loaded
          let video = content.document.getElementById("some-video");
          //let videoPlayable = await once(video, "loadeddata").then(_ => true);
          let videoLoaded = video && video.readyState === 4;
          return { audio: audioLoaded, img: imageLoaded, video: videoLoaded };
        }
      );
      is(true, loadedElements.img, "Image loaded and was upgraded " + url);
      is(true, loadedElements.video, "Video loaded and was upgraded " + url);
      is(true, loadedElements.audio, "Audio loaded and was upgraded " + url);
    }
  );

  await BrowserTestUtils.waitForCondition(() => expectedMessages === 0);

  // Clean up
  Services.console.unregisterListener(on_new_message);
});

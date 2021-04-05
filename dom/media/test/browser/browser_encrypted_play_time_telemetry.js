/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test verifies that telemetry gathered around encrypted media playtime
// is gathered as expected.

"use strict";

/* import-globals-from ../eme_standalone.js */

// Clears any existing telemetry data that has been accumulated. Returns a
// promise the will be resolved once the telemetry store is clear.
async function clearTelemetry() {
  // There's an arbitrary interval of 2 seconds in which the content
  // processes sync their event data with the parent process, we wait
  // this out to ensure that we clear everything that is left over from
  // previous tests and don't receive random events in the middle of our tests.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 2000));

  Services.telemetry.clearEvents();
  await TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_ALL_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  });
}

// Opens a tab and plays clearkey encrypted media in it. Resolves the returned
// promise once playback is complete.
async function playEmeMedia() {
  // Open a tab with an empty page.
  const emptyPageUri =
    "https://example.com/browser/dom/media/test/browser/file_empty_page.html";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    emptyPageUri
  );

  const emeHelperUri =
    gTestPath.substr(0, gTestPath.lastIndexOf("/")) + "/eme_standalone.js";
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [emeHelperUri],
    async _emeHelperUri => {
      // Begin helper functions.
      async function once(target, name) {
        return new Promise(r =>
          target.addEventListener(name, r, { once: true })
        );
      }

      // Helper to clone data into content so the EME helper can use the data.
      function cloneIntoContent(data) {
        return Cu.cloneInto(data, content.wrappedJSObject);
      }
      // End helper functions.

      // Load the EME helper into content.
      Services.scriptloader.loadSubScript(_emeHelperUri, content);
      // Setup EME with the helper.
      let video = content.document.createElement("video");
      content.document.body.appendChild(video);
      let emeHelper = new content.wrappedJSObject.EmeHelper();
      emeHelper.SetKeySystem(
        content.wrappedJSObject.EmeHelper.GetClearkeyKeySystemString()
      );
      emeHelper.SetInitDataTypes(cloneIntoContent(["webm"]));
      emeHelper.SetVideoCapabilities(
        cloneIntoContent([{ contentType: 'video/webm; codecs="vp9"' }])
      );
      emeHelper.AddKeyIdAndKey(
        "2cdb0ed6119853e7850671c3e9906c3c",
        "808b9adac384de1e4f56140f4ad76194"
      );
      emeHelper.onerror = error => {
        is(false, `Got unexpected error from EME helper: ${error}`);
      };
      await emeHelper.ConfigureEme(video);
      // Done setting up EME.

      // Setup MSE.
      const ms = new content.wrappedJSObject.MediaSource();
      video.src = content.wrappedJSObject.URL.createObjectURL(ms);
      await once(ms, "sourceopen");
      const sb = ms.addSourceBuffer("video/webm");
      const videoFile = "sintel-short-clearkey-subsample-encrypted-video.webm";
      let fetchResponse = await content.fetch(videoFile);
      sb.appendBuffer(await fetchResponse.arrayBuffer());
      await once(sb, "updateend");
      ms.endOfStream();
      await once(ms, "sourceended");
      let endedPromise = once(video, "ended");
      // Done setting up MSE.

      // Play the video and wait for it to finish.
      await video.play();
      await endedPromise;
    }
  );

  BrowserTestUtils.removeTab(tab);
}

// Verify the telemetry gathered has the expected values.
async function verifyTelemetry() {
  // The telemetry was gathered in the content process, so we have to wait
  // until is arrived in the parent to check it. At time of writing there's
  // not a more elegant way of doing this than polling.
  let playTimeSums = await TestUtils.waitForCondition(() => {
    let histograms = Services.telemetry.getSnapshotForHistograms("main", true)
      .content;
    // All the histogram data should come at the same time, so we just check
    // for playtime here, but will read more.
    if (histograms.VIDEO_PLAY_TIME_MS) {
      // We only expect to have one value for each histogram, so returning the
      // sums is a short hand for returning that one value.
      return {
        VIDEO_PLAY_TIME_MS: histograms.VIDEO_PLAY_TIME_MS.sum,
        VIDEO_ENCRYPTED_PLAY_TIME_MS:
          histograms.VIDEO_ENCRYPTED_PLAY_TIME_MS.sum,
        VIDEO_CLEARKEY_PLAY_TIME_MS: histograms.VIDEO_CLEARKEY_PLAY_TIME_MS.sum,
      };
    }
    return null;
  }, "recorded telemetry from playing media");

  // Finally, verify the telemetry data.
  ok(playTimeSums, "Should get play time telemetry");
  is(
    playTimeSums.VIDEO_PLAY_TIME_MS,
    playTimeSums.VIDEO_ENCRYPTED_PLAY_TIME_MS,
    "Play time should be the same as encrypted play time"
  );
  is(
    playTimeSums.VIDEO_PLAY_TIME_MS,
    playTimeSums.VIDEO_CLEARKEY_PLAY_TIME_MS,
    "Play time should be the same as clearkey play time"
  );
  ok(
    playTimeSums.VIDEO_PLAY_TIME_MS > 0,
    "Should have a play time greater than zero"
  );
}

add_task(async function testEncryptedMediaTelemetryProbes() {
  await clearTelemetry();
  await playEmeMedia();
  await verifyTelemetry();
});

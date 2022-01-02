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
  return TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_ALL_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  });
}

// Opens a tab containing a blank page, returns a promise that will resolve
// to that tab.
async function openTab() {
  const emptyPageUri =
    "https://example.com/browser/dom/media/test/browser/file_empty_page.html";
  return BrowserTestUtils.openNewForegroundTab(window.gBrowser, emptyPageUri);
}

// Creates and configures a video element for EME playback in `tab`. Does not
// start playback for the element. Returns a promise that will resolve once
// the element is setup and ready for playback.
async function loadEmeVideo(tab) {
  const emeHelperUri =
    gTestPath.substr(0, gTestPath.lastIndexOf("/")) + "/eme_standalone.js";
  return SpecialPowers.spawn(
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
      video.id = "media";
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
    }
  );
}

// Plays the media in `tab` until the 'ended' event is fire. Returns a promise
// that resolves once that state has been reached.
async function playMediaThrough(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    let video = content.document.getElementById("media");
    await Promise.all([new Promise(r => (video.onended = r)), video.play()]);
  });
}

// Plays the media in `tab` until the 'timeupdate' event is fire. Returns a
// promise that resolves once that state has been reached.
async function playMediaToTimeUpdate(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    let video = content.document.getElementById("media");
    await Promise.all([
      new Promise(r => (video.ontimeupdate = r)),
      video.play(),
    ]);
  });
}

// Aborts existing loads and replaces the media on the media element with an
// unencrypted file.
async function replaceMediaWithUnencrypted(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    let video = content.document.getElementById("media");
    video.src = "gizmo.mp4";
    video.load();
  });
}

// Clears/nulls the media keys on the media in `tab`.
async function clearMediaKeys(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    let video = content.document.getElementById("media");
    await video.setMediaKeys(null);
  });
}

// Wait for telemetry information to be received from the content process
// then get the relevant histograms for the tests and return the sums of
// those histograms. If a histogram does not exist this will return a 0
// sum. Returns a promise the resolves to an object with sums for
// - VIDEO_PLAY_TIME_MS
// - VIDEO_ENCRYPTED_PLAY_TIME_MS
// - VIDEO_CLEARKEY_PLAY_TIME_MS
// This function clears the histograms as it gets them.
async function getTelemetrySums() {
  // The telemetry was gathered in the content process, so we have to wait
  // until is arrived in the parent to check it. At time of writing there's
  // not a more elegant way of doing this than polling.
  return TestUtils.waitForCondition(() => {
    let histograms = Services.telemetry.getSnapshotForHistograms("main", true)
      .content;
    // All the histogram data should come at the same time, so we just check
    // for playtime here as we always expect it in these tests, but we'll
    // grab other values if present.
    if (histograms.VIDEO_PLAY_TIME_MS) {
      // We only expect to have one value for each histogram, so returning the
      // sums is a short hand for returning that one value.
      return {
        VIDEO_PLAY_TIME_MS: histograms.VIDEO_PLAY_TIME_MS.sum,
        VIDEO_ENCRYPTED_PLAY_TIME_MS: histograms.VIDEO_ENCRYPTED_PLAY_TIME_MS
          ? histograms.VIDEO_ENCRYPTED_PLAY_TIME_MS.sum
          : 0,
        VIDEO_CLEARKEY_PLAY_TIME_MS: histograms.VIDEO_CLEARKEY_PLAY_TIME_MS
          ? histograms.VIDEO_CLEARKEY_PLAY_TIME_MS.sum
          : 0,
      };
    }
    return null;
  }, "recorded telemetry from playing media");
}

// Clear telemetry before other tests. Internally the tests clear the telemetry
// when they check it, so we shouldn't need to do this between tests.
add_task(clearTelemetry);

add_task(async function testEncryptedMediaPlayback() {
  let testTab = await openTab();

  await loadEmeVideo(testTab);
  await playMediaThrough(testTab);

  BrowserTestUtils.removeTab(testTab);

  let telemetrySums = await getTelemetrySums();

  ok(telemetrySums, "Should get play time telemetry");
  is(
    telemetrySums.VIDEO_PLAY_TIME_MS,
    telemetrySums.VIDEO_ENCRYPTED_PLAY_TIME_MS,
    "Play time should be the same as encrypted play time"
  );
  is(
    telemetrySums.VIDEO_PLAY_TIME_MS,
    telemetrySums.VIDEO_CLEARKEY_PLAY_TIME_MS,
    "Play time should be the same as clearkey play time"
  );
  ok(
    telemetrySums.VIDEO_PLAY_TIME_MS > 0,
    "Should have a play time greater than zero"
  );
});

add_task(async function testChangingFromEncryptedToUnencrypted() {
  let testTab = await openTab();

  await loadEmeVideo(testTab);
  await replaceMediaWithUnencrypted(testTab);
  await playMediaToTimeUpdate(testTab);

  BrowserTestUtils.removeTab(testTab);

  let telemetrySums = await getTelemetrySums();

  ok(telemetrySums, "Should get play time telemetry");
  is(
    telemetrySums.VIDEO_ENCRYPTED_PLAY_TIME_MS,
    0,
    "Encrypted play time should be 0"
  );
  is(
    telemetrySums.VIDEO_PLAY_TIME_MS,
    telemetrySums.VIDEO_CLEARKEY_PLAY_TIME_MS,
    "Play time should be the same as clearkey play time because the media element still has a media keys attached"
  );
  ok(
    telemetrySums.VIDEO_PLAY_TIME_MS > 0,
    "Should have a play time greater than zero"
  );
});

add_task(
  async function testChangingFromEncryptedToUnencryptedAndClearingMediaKeys() {
    let testTab = await openTab();

    await loadEmeVideo(testTab);
    await replaceMediaWithUnencrypted(testTab);
    await clearMediaKeys(testTab);
    await playMediaToTimeUpdate(testTab);

    BrowserTestUtils.removeTab(testTab);

    let telemetrySums = await getTelemetrySums();

    ok(telemetrySums, "Should get play time telemetry");
    is(
      telemetrySums.VIDEO_ENCRYPTED_PLAY_TIME_MS,
      0,
      "Encrypted play time should be 0"
    );
    is(
      telemetrySums.VIDEO_CLEARKEY_PLAY_TIME_MS,
      0,
      "Clearkey play time should be 0"
    );
    ok(
      telemetrySums.VIDEO_PLAY_TIME_MS > 0,
      "Should have a play time greater than zero"
    );
  }
);

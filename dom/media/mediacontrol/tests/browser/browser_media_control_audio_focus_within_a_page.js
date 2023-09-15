const mainPageURL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_main_frame_with_multiple_child_session_frames.html";
const frameURL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_iframe_media.html";

const frame1 = "frame1";
const frame2 = "frame2";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * This test is used to check the behaviors when we play media from different
 * frames. When a page contains multiple frames, if those frames are using the
 * media session and set the metadata, then we have to know which frame owns the
 * audio focus that would be the last tab playing media. When the frame owns
 * audio focus, it means its metadata would be displayed on the virtual control
 * interface if it has a media session.
 */
add_task(async function testAudioFocusChangesAmongMultipleFrames() {
  /**
   * Play the media from the main frame, so it would own the audio focus and
   * its metadata should be shown on the virtual control interface. As the main
   * frame doesn't use media session, the current metadata would be the default
   * metadata.
   */
  const tab = await createLoadedTabWrapper(mainPageURL);
  await playAndWaitUntilMetadataChanged(tab);
  await isGivenTabUsingDefaultMetadata(tab);

  /**
   * Play media for frame1, so the audio focus switches to frame1 because it's
   * the last tab playing media and frame1's metadata should be displayed.
   */
  await loadPageForFrame(tab, frame1, frameURL);
  let metadata = await setMetadataAndGetReturnResult(tab, frame1);
  await playAndWaitUntilMetadataChanged(tab, frame1);
  isCurrentMetadataEqualTo(metadata);

  /**
   * Play media for frame2, so the audio focus switches to frame2 because it's
   * the last tab playing media and frame2's metadata should be displayed.
   */
  await loadPageForFrame(tab, frame2, frameURL);
  metadata = await setMetadataAndGetReturnResult(tab, frame2);
  await playAndWaitUntilMetadataChanged(tab, frame2);
  isCurrentMetadataEqualTo(metadata);

  /**
   * Remove tab and end test.
   */
  await tab.close();
});

add_task(async function testAudioFocusChangesAfterPausingAudioFocusOwner() {
  /**
   * Play the media from the main frame, so it would own the audio focus and
   * its metadata should be shown on the virtual control interface. As the main
   * frame doesn't use media session, the current metadata would be the default
   * metadata.
   */
  const tab = await createLoadedTabWrapper(mainPageURL);
  await playAndWaitUntilMetadataChanged(tab);
  await isGivenTabUsingDefaultMetadata(tab);

  /**
   * Play media for frame1, so the audio focus switches to frame1 because it's
   * the last tab playing media and frame1's metadata should be displayed.
   */
  await loadPageForFrame(tab, frame1, frameURL);
  let metadata = await setMetadataAndGetReturnResult(tab, frame1);
  await playAndWaitUntilMetadataChanged(tab, frame1);
  isCurrentMetadataEqualTo(metadata);

  /**
   * Pause media for frame1, so the audio focus switches back to the main frame
   * which is still playing media.
   */
  await pauseAndWaitUntilMetadataChangedFrom(tab, frame1);
  await isGivenTabUsingDefaultMetadata(tab);

  /**
   * Remove tab and end test.
   */
  await tab.close();
});

add_task(async function testAudioFocusUnchangesAfterPausingAudioFocusOwner() {
  /**
   * Play the media from the main frame, so it would own the audio focus and
   * its metadata should be shown on the virtual control interface. As the main
   * frame doesn't use media session, the current metadata would be the default
   * metadata.
   */
  const tab = await createLoadedTabWrapper(mainPageURL);
  await playAndWaitUntilMetadataChanged(tab);
  await isGivenTabUsingDefaultMetadata(tab);

  /**
   * Play media for frame1, so the audio focus switches to frame1 because it's
   * the last tab playing media and frame1's metadata should be displayed.
   */
  await loadPageForFrame(tab, frame1, frameURL);
  let metadata = await setMetadataAndGetReturnResult(tab, frame1);
  await playAndWaitUntilMetadataChanged(tab, frame1);
  isCurrentMetadataEqualTo(metadata);

  /**
   * Pause main frame's media first. When pausing frame1's media, there are not
   * other frames playing media, so frame1 still owns the audio focus and its
   * metadata should be displayed.
   */
  await pauseMediaFrom(tab);
  isCurrentMetadataEqualTo(metadata);

  /**
   * Remove tab and end test.
   */
  await tab.close();
});

add_task(
  async function testSwitchAudioFocusToMainFrameAfterRemovingAudioFocusOwner() {
    /**
     * Play the media from the main frame, so it would own the audio focus and
     * its metadata should be displayed on the virtual control interface. As the
     * main frame doesn't use media session, the current metadata would be the
     * default metadata.
     */
    const tab = await createLoadedTabWrapper(mainPageURL);
    await playAndWaitUntilMetadataChanged(tab);
    await isGivenTabUsingDefaultMetadata(tab);

    /**
     * Play media for frame1, so the audio focus switches to frame1 because it's
     * the last tab playing media and frame1's metadata should be displayed.
     */
    await loadPageForFrame(tab, frame1, frameURL);
    let metadata = await setMetadataAndGetReturnResult(tab, frame1);
    await playAndWaitUntilMetadataChanged(tab, frame1);
    isCurrentMetadataEqualTo(metadata);

    /**
     * Remove frame1, the audio focus would switch to the main frame which
     * metadata should be displayed.
     */
    await Promise.all([
      waitUntilDisplayedMetadataChanged(),
      removeFrame(tab, frame1),
    ]);
    await isGivenTabUsingDefaultMetadata(tab);

    /**
     * Remove tab and end test.
     */
    await tab.close();
  }
);

add_task(
  async function testSwitchAudioFocusToIframeAfterRemovingAudioFocusOwner() {
    /**
     * Play media for frame1, so frame1 owns the audio focus and frame1's metadata
     * should be displayed.
     */
    const tab = await createLoadedTabWrapper(mainPageURL);
    await loadPageForFrame(tab, frame1, frameURL);
    let metadataFrame1 = await setMetadataAndGetReturnResult(tab, frame1);
    await playAndWaitUntilMetadataChanged(tab, frame1);
    isCurrentMetadataEqualTo(metadataFrame1);

    /**
     * Play media for frame2, so the audio focus switches to frame2 because it's
     * the last tab playing media and frame2's metadata should be displayed.
     */
    await loadPageForFrame(tab, frame2, frameURL);
    let metadataFrame2 = await setMetadataAndGetReturnResult(tab, frame2);
    await playAndWaitUntilMetadataChanged(tab, frame2);
    isCurrentMetadataEqualTo(metadataFrame2);

    /**
     * Remove frame2, the audio focus would switch to frame1 which metadata should
     * be displayed.
     */
    await Promise.all([
      waitUntilDisplayedMetadataChanged(),
      removeFrame(tab, frame2),
    ]);
    isCurrentMetadataEqualTo(metadataFrame1);

    /**
     * Remove tab and end test.
     */
    await tab.close();
  }
);

add_task(async function testNoAudioFocusAfterRemovingAudioFocusOwner() {
  /**
   * Play the media from the main frame, so it would own the audio focus and
   * its metadata should be shown on the virtual control interface. As the main
   * frame doesn't use media session, the current metadata would be the default
   * metadata.
   */
  const tab = await createLoadedTabWrapper(mainPageURL);
  await playAndWaitUntilMetadataChanged(tab);
  await isGivenTabUsingDefaultMetadata(tab);

  /**
   * Play media for frame1, so the audio focus switches to frame1 because it's
   * the last tab playing media and frame1's metadata should be displayed.
   */
  await loadPageForFrame(tab, frame1, frameURL);
  let metadata = await setMetadataAndGetReturnResult(tab, frame1);
  await playAndWaitUntilMetadataChanged(tab, frame1);
  isCurrentMetadataEqualTo(metadata);

  /**
   * Pause media in main frame and then remove frame1. As the frame which owns
   * the audio focus is removed and no frame is still playing media, the current
   * metadata would be the default metadata.
   */
  await pauseMediaFrom(tab);
  await Promise.all([
    waitUntilDisplayedMetadataChanged(),
    removeFrame(tab, frame1),
  ]);
  await isGivenTabUsingDefaultMetadata(tab);

  /**
   * Remove tab and end test.
   */
  await tab.close();
});

/**
 * The following are helper functions.
 */
function loadPageForFrame(tab, frameId, pageUrl) {
  info(`start to load page for ${frameId}`);
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [frameId, pageUrl],
    async (id, url) => {
      const iframe = content.document.getElementById(id);
      if (!iframe) {
        ok(false, `can not get iframe '${id}'`);
      }
      iframe.src = url;
      await new Promise(r => (iframe.onload = r));
      // Set the document title that would be used as the value for properties
      // in frame's medadata.
      iframe.contentDocument.title = id;
    }
  );
}

function playMediaFrom(tab, frameId = undefined) {
  return SpecialPowers.spawn(tab.linkedBrowser, [frameId], id => {
    if (id == undefined) {
      info(`start to play media from main frame`);
      const video = content.document.getElementById("video");
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      return video.play();
    }

    info(`start to play media from ${id}`);
    const iframe = content.document.getElementById(id);
    if (!iframe) {
      ok(false, `can not get ${id}`);
    }
    iframe.contentWindow.postMessage("play", "*");
    return new Promise(r => {
      content.onmessage = event => {
        is(event.data, "played", `media started playing in ${id}`);
        r();
      };
    });
  });
}

function playAndWaitUntilMetadataChanged(tab, frameId = undefined) {
  const metadataChanged = frameId
    ? new Promise(r => (tab.controller.onmetadatachange = r))
    : waitUntilDisplayedMetadataChanged();
  return Promise.all([metadataChanged, playMediaFrom(tab, frameId)]);
}

function pauseMediaFrom(tab, frameId = undefined) {
  return SpecialPowers.spawn(tab.linkedBrowser, [frameId], id => {
    if (id == undefined) {
      info(`start to pause media from in frame`);
      const video = content.document.getElementById("video");
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      return video.pause();
    }

    info(`start to pause media in ${id}`);
    const iframe = content.document.getElementById(id);
    if (!iframe) {
      ok(false, `can not get ${id}`);
    }
    iframe.contentWindow.postMessage("pause", "*");
    return new Promise(r => {
      content.onmessage = event => {
        is(event.data, "paused", `media paused in ${id}`);
        r();
      };
    });
  });
}

function pauseAndWaitUntilMetadataChangedFrom(tab, frameId = undefined) {
  const metadataChanged = waitUntilDisplayedMetadataChanged();
  return Promise.all([metadataChanged, pauseMediaFrom(tab, frameId)]);
}

function setMetadataAndGetReturnResult(tab, frameId) {
  info(`start to set metadata for ${frameId}`);
  return SpecialPowers.spawn(tab.linkedBrowser, [frameId], id => {
    const iframe = content.document.getElementById(id);
    if (!iframe) {
      ok(false, `can not get ${id}`);
    }
    iframe.contentWindow.postMessage("setMetadata", "*");
    info(`wait until we get metadata for ${id}`);
    return new Promise(r => {
      content.onmessage = event => {
        ok(
          event.data.title && event.data.artist && event.data.album,
          "correct return format"
        );
        r(event.data);
      };
    });
  });
}

function removeFrame(tab, frameId) {
  info(`remove ${frameId}`);
  return SpecialPowers.spawn(tab.linkedBrowser, [frameId], id => {
    const iframe = content.document.getElementById(id);
    if (!iframe) {
      ok(false, `can not get ${id}`);
    }
    content.document.body.removeChild(iframe);
  });
}

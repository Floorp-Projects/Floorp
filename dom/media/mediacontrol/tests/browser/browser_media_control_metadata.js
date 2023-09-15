const PAGE_NON_AUTOPLAY =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";
const PAGE_EMPTY_TITLE_URL =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_empty_title.html";

const testVideoId = "video";
const defaultFaviconName = "defaultFavicon.svg";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["media.mediacontrol.testingevents.enabled", true]],
  });
});

/**
 * This test includes many different test cases of checking the current media
 * metadata from the tab which is being controlled by the media control. Each
 * `add_task(..)` is a different testing scenario.
 *
 * Media metadata is the information that can tell user about what title, artist,
 * album and even art work the tab is currently playing to. The metadta is
 * usually set from MediaSession API, but if the site doesn't use that API, we
 * would also generate a default metadata instead.
 *
 * The current metadata would only be available after the page starts playing
 * media at least once, if the page hasn't started any media before, then the
 * current metadata is always empty.
 *
 * For following situations, we would create a default metadata which title is
 * website's title and artwork is from our default favicon icon.
 * (1) the page doesn't use MediaSession API
 * (2) media session doesn't has metadata
 * (3) media session has an empty metadata
 *
 * Otherwise, the current metadata would be media session's metadata from the
 * tab which is currently controlled by the media control.
 */
add_task(async function testDefaultMetadataForPageWithoutMediaSession() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`should use default metadata because of lacking of media session`);
  await isGivenTabUsingDefaultMetadata(tab);

  info(`remove tab`);
  await tab.close();
});

add_task(
  async function testDefaultMetadataForEmptyTitlePageWithoutMediaSession() {
    info(`open media page`);
    const tab = await createLoadedTabWrapper(PAGE_EMPTY_TITLE_URL);

    info(`start media`);
    await playMedia(tab, testVideoId);

    info(`should use default metadata because of lacking of media session`);
    await isGivenTabUsingDefaultMetadata(tab);

    info(`remove tab`);
    await tab.close();
  }
);

add_task(async function testDefaultMetadataForPageUsingEmptyMetadata() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`create empty media metadata`);
  await setMediaMetadata(tab, {
    title: "",
    artist: "",
    album: "",
    artwork: [],
  });

  info(`should use default metadata because of empty media metadata`);
  await isGivenTabUsingDefaultMetadata(tab);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testDefaultMetadataForPageUsingNullMetadata() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`create empty media metadata`);
  await setNullMediaMetadata(tab);

  info(`should use default metadata because of lacking of media metadata`);
  await isGivenTabUsingDefaultMetadata(tab);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testMetadataWithEmptyTitleAndArtwork() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`create media metadata with empty title and artwork`);
  await setMediaMetadata(tab, {
    title: "",
    artist: "foo",
    album: "bar",
    artwork: [],
  });

  info(`should use default metadata because of empty title and artwork`);
  await isGivenTabUsingDefaultMetadata(tab);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testMetadataWithoutTitleAndArtwork() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`create media metadata with empty title and artwork`);
  await setMediaMetadata(tab, {
    artist: "foo",
    album: "bar",
  });

  info(`should use default metadata because of lacking of title and artwork`);
  await isGivenTabUsingDefaultMetadata(tab);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testMetadataInPrivateBrowsing() {
  info(`create a private window`);
  const inputWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY, { inputWindow });

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`set metadata`);
  let metadata = {
    title: "foo",
    artist: "bar",
    album: "foo",
    artwork: [{ src: "bar.jpg", sizes: "128x128", type: "image/jpeg" }],
  };
  await setMediaMetadata(tab, metadata);

  info(`should use default metadata because of in private browsing mode`);
  await isGivenTabUsingDefaultMetadata(tab, { isPrivateBrowsing: true });

  info(`remove tab`);
  await tab.close();

  info(`close private window`);
  await BrowserTestUtils.closeWindow(inputWindow);
});

add_task(async function testSetMetadataFromMediaSessionAPI() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`set metadata`);
  let metadata = {
    title: "foo",
    artist: "bar",
    album: "foo",
    artwork: [{ src: "bar.jpg", sizes: "128x128", type: "image/jpeg" }],
  };
  await setMediaMetadata(tab, metadata);

  info(`check if current active metadata is equal to what we've set before`);
  await isCurrentMetadataEqualTo(metadata);

  info(`set metadata again to see if current metadata would change`);
  metadata = {
    title: "foo2",
    artist: "bar2",
    album: "foo2",
    artwork: [{ src: "bar2.jpg", sizes: "129x129", type: "image/jpeg" }],
  };
  await setMediaMetadata(tab, metadata);

  info(`check if current active metadata is equal to what we've set before`);
  await isCurrentMetadataEqualTo(metadata);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testSetMetadataBeforeMediaStarts() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY, {
    needCheck: false,
  });

  info(`set metadata`);
  let metadata = {
    title: "foo",
    artist: "bar",
    album: "foo",
    artwork: [{ src: "bar.jpg", sizes: "128x128", type: "image/jpeg" }],
  };
  await setMediaMetadata(tab, metadata, { notExpectChange: true });

  info(`current media metadata should be empty before media starts`);
  isCurrentMetadataEmpty();

  info(`remove tab`);
  await tab.close();
});

add_task(async function testSetMetadataAfterMediaPaused() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media in order to let this tab be controlled`);
  await playMedia(tab, testVideoId);

  info(`pause media`);
  await pauseMedia(tab, testVideoId);

  info(`set metadata after media is paused`);
  let metadata = {
    title: "foo",
    artist: "bar",
    album: "foo",
    artwork: [{ src: "bar.jpg", sizes: "128x128", type: "image/jpeg" }],
  };
  await setMediaMetadata(tab, metadata);

  info(`check if current active metadata is equal to what we've set before`);
  await isCurrentMetadataEqualTo(metadata);

  info(`remove tab`);
  await tab.close();
});

add_task(async function testSetMetadataAmongMultipleTabs() {
  info(`open media page in tab1`);
  const tab1 = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media in tab1`);
  await playMedia(tab1, testVideoId);

  info(`set metadata for tab1`);
  let metadata = {
    title: "foo",
    artist: "bar",
    album: "foo",
    artwork: [{ src: "bar.jpg", sizes: "128x128", type: "image/jpeg" }],
  };
  await setMediaMetadata(tab1, metadata);

  info(`check if current active metadata is equal to what we've set before`);
  await isCurrentMetadataEqualTo(metadata);

  info(`open another page in tab2`);
  const tab2 = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media in tab2`);
  await playMedia(tab2, testVideoId);

  info(`set metadata for tab2`);
  metadata = {
    title: "foo2",
    artist: "bar2",
    album: "foo2",
    artwork: [{ src: "bar2.jpg", sizes: "129x129", type: "image/jpeg" }],
  };
  await setMediaMetadata(tab2, metadata);

  info(`current active metadata should become metadata from tab2`);
  await isCurrentMetadataEqualTo(metadata);

  info(
    `update metadata for tab1, which should not affect current metadata ` +
      `because media session in tab2 is the one we're controlling right now`
  );
  await setMediaMetadata(tab1, {
    title: "foo3",
    artist: "bar3",
    album: "foo3",
    artwork: [{ src: "bar3.jpg", sizes: "130x130", type: "image/jpeg" }],
  });

  info(`current active metadata should still be metadata from tab2`);
  await isCurrentMetadataEqualTo(metadata);

  info(`remove tabs`);
  await Promise.all([tab1.close(), tab2.close()]);
});

add_task(async function testMetadataAfterTabNavigation() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`set metadata`);
  let metadata = {
    title: "foo",
    artist: "bar",
    album: "foo",
    artwork: [{ src: "bar.jpg", sizes: "128x128", type: "image/jpeg" }],
  };
  await setMediaMetadata(tab, metadata);

  info(`check if current active metadata is equal to what we've set before`);
  await isCurrentMetadataEqualTo(metadata);

  info(`navigate tab to blank page`);
  await Promise.all([
    new Promise(r => (tab.controller.ondeactivated = r)),
    BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, "about:blank"),
  ]);

  info(`current media metadata should be reset`);
  isCurrentMetadataEmpty();

  info(`remove tab`);
  await tab.close();
});

add_task(async function testUpdateDefaultMetadataWhenPageTitleChanges() {
  info(`open media page`);
  const tab = await createLoadedTabWrapper(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab, testVideoId);

  info(`should use default metadata because of lacking of media session`);
  await isGivenTabUsingDefaultMetadata(tab);

  info(`default metadata should be updated after page title changes`);
  await changePageTitle(tab, { shouldAffectMetadata: true });
  await isGivenTabUsingDefaultMetadata(tab);

  info(`after setting metadata, title change won't affect current metadata`);
  const metadata = {
    title: "foo",
    artist: "bar",
    album: "foo",
    artwork: [{ src: "bar.jpg", sizes: "128x128", type: "image/jpeg" }],
  };
  await setMediaMetadata(tab, metadata);
  await changePageTitle(tab, { shouldAffectMetadata: false });
  await isCurrentMetadataEqualTo(metadata);

  info(`remove tab`);
  await tab.close();
});

/**
 * The following are helper functions.
 */
function setMediaMetadata(tab, metadata, { notExpectChange } = {}) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  const metadatachangePromise = notExpectChange
    ? Promise.resolve()
    : new Promise(r => (controller.onmetadatachange = r));
  return Promise.all([
    metadatachangePromise,
    SpecialPowers.spawn(tab.linkedBrowser, [metadata], data => {
      content.navigator.mediaSession.metadata = new content.MediaMetadata(data);
    }),
  ]);
}

function setNullMediaMetadata(tab) {
  const promise = SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.navigator.mediaSession.metadata = null;
  });
  return Promise.all([promise, waitUntilControllerMetadataChanged()]);
}

async function changePageTitle(tab, { shouldAffectMetadata } = {}) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  const shouldWaitMetadataChangePromise = shouldAffectMetadata
    ? new Promise(r => (controller.onmetadatachange = r))
    : Promise.resolve();
  await Promise.all([
    shouldWaitMetadataChangePromise,
    SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
      content.document.title = "new title";
    }),
  ]);
}

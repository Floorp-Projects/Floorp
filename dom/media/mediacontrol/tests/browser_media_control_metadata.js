/* eslint-disable no-undef */
const PAGE_NON_AUTOPLAY =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_non_autoplay.html";

const testVideoId = "video";
const defaultFaviconName = "defaultFavicon.svg";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.mediacontrol.testingevents.enabled", true],
      ["dom.media.mediasession.enabled", true],
    ],
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
  const tab = await createTabAndLoad(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab);

  info(`should use default metadata because of lacking of media session`);
  await isUsingDefaultMetadata(tab);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testDefaultMetadataForPageUsingEmptyMetadata() {
  info(`open media page`);
  const tab = await createTabAndLoad(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab);

  info(`create empty media metadata`);
  await setMediaMetadata(tab, {
    title: "",
    artist: "",
    album: "",
    artwork: [],
  });

  info(`should use default metadata because of empty media metadata`);
  await isUsingDefaultMetadata(tab);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testDefaultMetadataForPageUsingNullMetadata() {
  info(`open media page`);
  const tab = await createTabAndLoad(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab);

  info(`create empty media metadata`);
  await setNullMediaMetadata(tab);

  info(`should use default metadata because of lacking of media metadata`);
  await isUsingDefaultMetadata(tab);

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testSetMetadataFromMediaSessionAPI() {
  info(`open media page`);
  const tab = await createTabAndLoad(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab);

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
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testSetMetadataBeforeMediaStarts() {
  info(`open media page`);
  const tab = await createTabAndLoad(PAGE_NON_AUTOPLAY);

  info(`set metadata`);
  let metadata = {
    title: "foo",
    artist: "bar",
    album: "foo",
    artwork: [{ src: "bar.jpg", sizes: "128x128", type: "image/jpeg" }],
  };
  await setMediaMetadata(tab, metadata);

  info(`current media metadata should be empty before media starts`);
  isCurrentMetadataEmpty();

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testSetMetadataAfterMediaPaused() {
  info(`open media page`);
  const tab = await createTabAndLoad(PAGE_NON_AUTOPLAY);

  info(`start media in order to let this tab be controlled`);
  await playMedia(tab);

  info(`pause media`);
  await pauseMedia(tab);

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
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testSetMetadataAmongMultipleTabs() {
  info(`open media page in tab1`);
  const tab1 = await createTabAndLoad(PAGE_NON_AUTOPLAY);

  info(`start media in tab1`);
  await playMedia(tab1);

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
  const tab2 = await createTabAndLoad(PAGE_NON_AUTOPLAY);

  info(`set metadata for tab2`);
  metadata = {
    title: "foo2",
    artist: "bar2",
    album: "foo2",
    artwork: [{ src: "bar2.jpg", sizes: "129x129", type: "image/jpeg" }],
  };
  await setMediaMetadata(tab2, metadata);

  info(`start media in tab2`);
  await playMedia(tab2);

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
  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
});

add_task(async function testMetadataAfterTabNavigation() {
  info(`open media page`);
  const tab = await createTabAndLoad(PAGE_NON_AUTOPLAY);

  info(`start media`);
  await playMedia(tab);

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
    BrowserTestUtils.loadURI(tab.linkedBrowser, "about:blank"),
    waitUntilMainMediaControllerChanged(),
  ]);

  info(`current media metadata should be reset`);
  isCurrentMetadataEmpty();

  info(`remove tab`);
  await BrowserTestUtils.removeTab(tab);
});

/**
 * The following are helper functions.
 */
function waitUntilMainMediaControllerChanged() {
  return BrowserUtils.promiseObserved("main-media-controller-changed");
}

function waitUntilControllerMetadataChanged() {
  return BrowserUtils.promiseObserved(
    "media-session-controller-metadata-changed"
  );
}

function playMedia(tab) {
  const playPromise = SpecialPowers.spawn(
    tab.linkedBrowser,
    [testVideoId],
    Id => {
      const video = content.document.getElementById(Id);
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      return video.play();
    }
  );
  return Promise.all([playPromise, waitUntilMainMediaControllerChanged()]);
}

function pauseMedia(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [testVideoId], Id => {
    const video = content.document.getElementById(Id);
    if (!video) {
      ok(false, `can't get the media element!`);
    }
    ok(!video.paused, `video is playing before calling pause`);
    video.pause();
  });
}

async function isUsingDefaultMetadata(tab) {
  let metadata = ChromeUtils.getCurrentActiveMediaMetadata();
  await SpecialPowers.spawn(tab.linkedBrowser, [metadata.title], title => {
    is(title, content.document.title, "Using website title as a default title");
  });
  is(metadata.artwork.length, 1, "Default metada contains one artwork");
  ok(
    metadata.artwork[0].src.includes(defaultFaviconName),
    "Using default favicon as a default art work"
  );
}

function setMediaMetadata(tab, metadata) {
  const promise = SpecialPowers.spawn(tab.linkedBrowser, [metadata], data => {
    content.navigator.mediaSession.metadata = new content.MediaMetadata(data);
  });
  return Promise.all([promise, waitUntilControllerMetadataChanged()]);
}

function setNullMediaMetadata(tab) {
  const promise = SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.navigator.mediaSession.metadata = null;
  });
  return Promise.all([promise, waitUntilControllerMetadataChanged()]);
}

function isCurrentMetadataEqualTo(metadata) {
  const current = ChromeUtils.getCurrentActiveMediaMetadata();
  is(
    current.title,
    metadata.title,
    `tile '${current.title}' is equal to ${metadata.title}`
  );
  is(
    current.artist,
    metadata.artist,
    `artist '${current.artist}' is equal to ${metadata.artist}`
  );
  is(
    current.album,
    metadata.album,
    `album '${current.album}' is equal to ${metadata.album}`
  );
  is(
    current.artwork.length,
    metadata.artwork.length,
    `artwork length '${current.artwork.length}' is equal to ${metadata.artwork.length}`
  );
  for (let idx = 0; idx < metadata.artwork.length; idx++) {
    // the current src we got would be a completed path of the image, so we do
    // not check if they are equal, we check if the current src includes the
    // metadata's file name. Eg. "http://foo/bar.jpg" v.s. "bar.jpg"
    ok(
      current.artwork[idx].src.includes(metadata.artwork[idx].src),
      `artwork src '${current.artwork[idx].src}' includes ${metadata.artwork[idx].src}`
    );
    is(
      current.artwork[idx].sizes,
      metadata.artwork[idx].sizes,
      `artwork sizes '${current.artwork[idx].sizes}' is equal to ${metadata.artwork[idx].sizes}`
    );
    is(
      current.artwork[idx].type,
      metadata.artwork[idx].type,
      `artwork type '${current.artwork[idx].type}' is equal to ${metadata.artwork[idx].type}`
    );
  }
}

function isCurrentMetadataEmpty() {
  const current = ChromeUtils.getCurrentActiveMediaMetadata();
  is(current.title, "", `current title should be empty`);
  is(current.artist, "", `current title should be empty`);
  is(current.album, "", `current album should be empty`);
  is(current.artwork.length, 0, `current artwork should be empty`);
}

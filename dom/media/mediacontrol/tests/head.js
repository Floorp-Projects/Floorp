/* eslint-disable no-undef */

async function createTabAndLoad(url, inputWindow = null) {
  const browser = inputWindow ? inputWindow.gBrowser : window.gBrowser;
  let tab = await BrowserTestUtils.openNewForegroundTab(browser, url);
  return tab;
}

function checkOrWaitUntilMediaStartedPlaying(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], Id => {
    return new Promise(resolve => {
      const video = content.document.getElementById(Id);
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      if (!video.paused) {
        ok(true, `media started playing`);
        resolve();
      } else {
        info(`wait until media starts playing`);
        video.onplaying = () => {
          video.onplaying = null;
          ok(true, `media started playing`);
          resolve();
        };
      }
    });
  });
}

function checkOrWaitUntilMediaStoppedPlaying(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], Id => {
    return new Promise(resolve => {
      const video = content.document.getElementById(Id);
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      if (video.paused) {
        ok(true, `media stopped playing`);
        resolve();
      } else {
        info(`wait until media stops playing`);
        video.onpause = () => {
          video.onpause = null;
          ok(true, `media stopped playing`);
          resolve();
        };
      }
    });
  });
}

function isCurrentMetadataEmpty() {
  const current = ChromeUtils.getCurrentActiveMediaMetadata();
  is(current.title, "", `current title should be empty`);
  is(current.artist, "", `current title should be empty`);
  is(current.album, "", `current album should be empty`);
  is(current.artwork.length, 0, `current artwork should be empty`);
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

function waitUntilMainMediaControllerPlaybackChanged() {
  return BrowserUtils.promiseObserved("main-media-controller-playback-changed");
}

function waitUntilMainMediaControllerChanged() {
  return BrowserUtils.promiseObserved("main-media-controller-changed");
}

function waitUntilControllerMetadataChanged() {
  return BrowserUtils.promiseObserved(
    "media-session-controller-metadata-changed"
  );
}

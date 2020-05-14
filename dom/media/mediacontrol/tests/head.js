/* eslint-disable no-undef */

/**
 * Create a new tab and load the content from a given window (optional), if
 * caller doesn't provide any window, then we would create tab in the current
 * window.
 *
 * @param {string} url
 *        The URL that tab is going to load
 * @param {window} inputWindow [optional]
 *        The window that uses to create a tab
 * @return {tab}
 *         Return a loaded tab created from the given window
 */
async function createTabAndLoad(url, inputWindow = null) {
  const browser = inputWindow ? inputWindow.gBrowser : window.gBrowser;
  let tab = await BrowserTestUtils.openNewForegroundTab(browser, url);
  return tab;
}

/**
 * Returns a promise that resolves when generated media control keys has
 * triggered the main media controller's corresponding method and changes its
 * playback state.
 *
 * @param {string} event
 *        The event name of the media control key
 * @return {Promise}
 *         Resolve when the main controller receives the media control key event
 *         and change its playback state.
 */
function generateMediaControlKeyEvent(event) {
  const playbackStateChanged = waitUntilMainMediaControllerPlaybackChanged();
  ChromeUtils.generateMediaControlKeysTestEvent(event);
  return playbackStateChanged;
}

/**
 * Play the specific media and wait until it plays successfully and the main
 * controller has been updated.
 *
 * @param {tab} tab
 *        The tab that contains the media which we would play
 * @param {string} elementId
 *        The element Id of the media which we would play
 * @return {Promise}
 *         Resolve when the media has been starting playing and the main
 *         controller has been updated.
 */
function playMedia(tab, elementId) {
  const playPromise = SpecialPowers.spawn(
    tab.linkedBrowser,
    [elementId],
    Id => {
      const video = content.document.getElementById(Id);
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      return video.play();
    }
  );
  return Promise.all([
    playPromise,
    waitUntilMainMediaControllerPlaybackChanged(),
  ]);
}

/**
 * Pause the specific media and wait until it pauses successfully and the main
 * controller has been updated.
 *
 * @param {tab} tab
 *        The tab that contains the media which we would pause
 * @param {string} elementId
 *        The element Id of the media which we would pause
 * @return {Promise}
 *         Resolve when the media has been paused and the main controller has
 *         been updated.
 */
function pauseMedia(tab, elementId) {
  const pausePromise = SpecialPowers.spawn(
    tab.linkedBrowser,
    [elementId],
    Id => {
      const video = content.document.getElementById(Id);
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      ok(!video.paused, `video is playing before calling pause`);
      video.pause();
    }
  );
  return Promise.all([
    pausePromise,
    waitUntilMainMediaControllerPlaybackChanged(),
  ]);
}

/**
 * Returns a promise that resolves when the specific media starts playing.
 *
 * @param {tab} tab
 *        The tab that contains the media which we would check
 * @param {string} elementId
 *        The element Id of the media which we would check
 * @return {Promise}
 *         Resolve when the media has been starting playing.
 */
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

/**
 * Returns a promise that resolves when the specific media stops playing.
 *
 * @param {tab} tab
 *        The tab that contains the media which we would check
 * @param {string} elementId
 *        The element Id of the media which we would check
 * @return {Promise}
 *         Resolve when the media has been stopped playing.
 */
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

/**
 * Check if the active metadata is empty.
 */
function isCurrentMetadataEmpty() {
  const current = ChromeUtils.getCurrentActiveMediaMetadata();
  is(current.title, "", `current title should be empty`);
  is(current.artist, "", `current title should be empty`);
  is(current.album, "", `current album should be empty`);
  is(current.artwork.length, 0, `current artwork should be empty`);
}

/**
 * Check if the active metadata is equal to the given metadata.artwork
 *
 * @param {object} metadata
 *        The metadata that would be compared with the active metadata
 */
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

/**
 * Wait until the main media controller changes its playback state, we would
 * observe that by listening for `main-media-controller-playback-changed`
 * notification.
 *
 * @return {Promise}
 *         Resolve when observing `main-media-controller-playback-changed`
 */
function waitUntilMainMediaControllerPlaybackChanged() {
  return BrowserUtils.promiseObserved("main-media-controller-playback-changed");
}

/**
 * Wait until the main media controller has been changed, we would observe that
 * by listening for the `main-media-controller-changed` notification.
 *
 * @return {Promise}
 *         Resolve when observing `main-media-controller-changed`
 */
function waitUntilMainMediaControllerChanged() {
  return BrowserUtils.promiseObserved("main-media-controller-changed");
}

/**
 * Wait until the main session controller changes its metadata, we would observe
 * that by listening for `media-session-controller-metadata-changed`
 * notification.
 *
 * @return {Promise}
 *         Resolve when observing `media-session-controller-metadata-changed`
 */
function waitUntilControllerMetadataChanged() {
  return BrowserUtils.promiseObserved(
    "media-session-controller-metadata-changed"
  );
}

/**
 * Wait until media controller amount changes, we would observe that by
 * listening for `media-controller-amount-changed` notification.
 *
 * @return {Promise}
 *         Resolve when observing `media-controller-amount-changed`
 */
function waitUntilMediaControllerAmountChanged() {
  return BrowserUtils.promiseObserved("media-controller-amount-changed");
}

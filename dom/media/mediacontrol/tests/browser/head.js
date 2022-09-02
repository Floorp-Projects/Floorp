/**
 * This function would create a new foreround tab and load the url for it. In
 * addition, instead of returning a tab element, we return a tab wrapper that
 * helps us to automatically detect if the media controller of that tab
 * dispatches the first (activated) and the last event (deactivated) correctly.
 * @ param url
 *         the page url which tab would load
 * @ param input window (optional)
 *         if it exists, the tab would be created from the input window. If not,
 *         then the tab would be created in current window.
 * @ param needCheck (optional)
 *         it decides if we would perform the check for the first and last event
 *         on the media controller. It's default true.
 */
async function createLoadedTabWrapper(
  url,
  { inputWindow = window, needCheck = true } = {}
) {
  class tabWrapper {
    constructor(tab, needCheck) {
      this._tab = tab;
      this._controller = tab.linkedBrowser.browsingContext.mediaController;
      this._firstEvent = "";
      this._lastEvent = "";
      this._events = [
        "activated",
        "deactivated",
        "metadatachange",
        "playbackstatechange",
        "positionstatechange",
        "supportedkeyschange",
      ];
      this._needCheck = needCheck;
      if (this._needCheck) {
        this._registerAllEvents();
      }
    }
    _registerAllEvents() {
      for (let event of this._events) {
        this._controller.addEventListener(event, this._handleEvent.bind(this));
      }
    }
    _unregisterAllEvents() {
      for (let event of this._events) {
        this._controller.removeEventListener(
          event,
          this._handleEvent.bind(this)
        );
      }
    }
    _handleEvent(event) {
      info(`handle event=${event.type}`);
      if (this._firstEvent === "") {
        this._firstEvent = event.type;
      }
      this._lastEvent = event.type;
    }
    get linkedBrowser() {
      return this._tab.linkedBrowser;
    }
    get controller() {
      return this._controller;
    }
    get tabElement() {
      return this._tab;
    }
    async close() {
      info(`wait until finishing close tab wrapper`);
      const deactivationPromise = this._controller.isActive
        ? new Promise(r => (this._controller.ondeactivated = r))
        : Promise.resolve();
      BrowserTestUtils.removeTab(this._tab);
      await deactivationPromise;
      if (this._needCheck) {
        is(this._firstEvent, "activated", "First event should be 'activated'");
        is(
          this._lastEvent,
          "deactivated",
          "Last event should be 'deactivated'"
        );
        this._unregisterAllEvents();
      }
    }
  }
  const browser = inputWindow ? inputWindow.gBrowser : window.gBrowser;
  let tab = await BrowserTestUtils.openNewForegroundTab(browser, url);
  return new tabWrapper(tab, needCheck);
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
  const playbackStateChanged = waitUntilDisplayedPlaybackChanged();
  MediaControlService.generateMediaControlKey(event);
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
async function playMedia(tab, elementId) {
  const playbackStatePromise = waitUntilDisplayedPlaybackChanged();
  await SpecialPowers.spawn(tab.linkedBrowser, [elementId], async Id => {
    const video = content.document.getElementById(Id);
    if (!video) {
      ok(false, `can't get the media element!`);
    }
    ok(
      await video.play().then(
        _ => true,
        _ => false
      ),
      "video started playing"
    );
  });
  return playbackStatePromise;
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
  return Promise.all([pausePromise, waitUntilDisplayedPlaybackChanged()]);
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
  const current = MediaControlService.getCurrentActiveMediaMetadata();
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
  const current = MediaControlService.getCurrentActiveMediaMetadata();
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
 * Check if the given tab is using the default metadata. If the tab is being
 * used in the private browsing mode, `isPrivateBrowsing` should be definded in
 * the `options`.
 */
async function isGivenTabUsingDefaultMetadata(tab, options = {}) {
  const localization = new Localization([
    "branding/brand.ftl",
    "dom/media.ftl",
  ]);
  const fallbackTitle = await localization.formatValue(
    "mediastatus-fallback-title"
  );
  ok(fallbackTitle.length, "l10n fallback title is not empty");

  const metadata = tab.linkedBrowser.browsingContext.mediaController.getMetadata();

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [metadata.title, fallbackTitle, options.isPrivateBrowsing],
    (title, fallbackTitle, isPrivateBrowsing) => {
      if (isPrivateBrowsing || !content.document.title.length) {
        is(title, fallbackTitle, "Using a generic default fallback title");
      } else {
        is(
          title,
          content.document.title,
          "Using website title as a default title"
        );
      }
    }
  );
  is(metadata.artwork.length, 1, "Default metada contains one artwork");
  ok(
    metadata.artwork[0].src.includes("defaultFavicon.svg"),
    "Using default favicon as a default art work"
  );
}

/**
 * Wait until the main media controller changes its playback state, we would
 * observe that by listening for `media-displayed-playback-changed`
 * notification.
 *
 * @return {Promise}
 *         Resolve when observing `media-displayed-playback-changed`
 */
function waitUntilDisplayedPlaybackChanged() {
  return BrowserUtils.promiseObserved("media-displayed-playback-changed");
}

/**
 * Wait until the metadata that would be displayed on the virtual control
 * interface changes. we would observe that by listening for
 * `media-displayed-metadata-changed` notification.
 *
 * @return {Promise}
 *         Resolve when observing `media-displayed-metadata-changed`
 */
function waitUntilDisplayedMetadataChanged() {
  return BrowserUtils.promiseObserved("media-displayed-metadata-changed");
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
 * Wait until any media controller updates its metadata even if it's not the
 * main controller. The difference between this function and
 * `waitUntilDisplayedMetadataChanged()` is that the changed metadata might come
 * from non-main controller so it won't be show on the virtual control
 * interface. we would observe that by listening for
 * `media-session-controller-metadata-changed` notification.
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

/**
 * check if the media controll from given tab is active. If not, return a
 * promise and resolve it when controller become active.
 */
async function checkOrWaitUntilControllerBecomeActive(tab) {
  const controller = tab.linkedBrowser.browsingContext.mediaController;
  if (controller.isActive) {
    return;
  }
  await new Promise(r => (controller.onactivated = r));
}

"use strict";
const injector = require("inject!lib/HighlightsFeed.jsm");
const {Screenshots} = require("lib/Screenshots.jsm");
const {GlobalOverrider} = require("test/unit/utils");
const {actionTypes: at} = require("common/Actions.jsm");
const {Dedupe} = require("common/Dedupe.jsm");

const FAKE_LINKS = new Array(9).fill(null).map((v, i) => ({url: `http://www.site${i}.com`}));
const FAKE_IMAGE = "data123";

describe("Highlights Feed", () => {
  let HighlightsFeed;
  let HIGHLIGHTS_UPDATE_TIME;
  let SECTION_ID;
  let feed;
  let globals;
  let sandbox;
  let links;
  let clock;
  let fakeScreenshot;
  let fakeNewTabUtils;
  let filterAdultStub;
  let sectionsManagerStub;
  let shortURLStub;
  let profileAgeCreatedStub;

  const fetchHighlights = async() => {
    await feed.fetchHighlights();
    return sectionsManagerStub.updateSection.firstCall.args[1].rows;
  };

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    fakeNewTabUtils = {activityStreamLinks: {getHighlights: sandbox.spy(() => Promise.resolve(links))}};
    sectionsManagerStub = {
      onceInitialized: sinon.stub().callsFake(callback => callback()),
      enableSection: sinon.spy(),
      disableSection: sinon.spy(),
      updateSection: sinon.spy(),
      updateSectionCard: sinon.spy(),
      sections: new Map([["highlights", {}]])
    };
    fakeScreenshot = {
      getScreenshotForURL: sandbox.spy(() => Promise.resolve(FAKE_IMAGE)),
      maybeCacheScreenshot: Screenshots.maybeCacheScreenshot
    };
    filterAdultStub = sinon.stub().returns([]);
    shortURLStub = sinon.stub().callsFake(site => site.url.match(/\/([^/]+)/)[1]);

    const fakeProfileAgePromise = {};
    const fakeProfileAge = function() { return fakeProfileAgePromise; };
    profileAgeCreatedStub = sinon.stub().callsFake(() => Promise.resolve(42));
    sinon.stub(fakeProfileAgePromise, "created").get(profileAgeCreatedStub);

    globals.set("NewTabUtils", fakeNewTabUtils);
    globals.set("ProfileAge", fakeProfileAge);
    ({HighlightsFeed, HIGHLIGHTS_UPDATE_TIME, SECTION_ID} = injector({
      "lib/FilterAdult.jsm": {filterAdult: filterAdultStub},
      "lib/ShortURL.jsm": {shortURL: shortURLStub},
      "lib/SectionsManager.jsm": {SectionsManager: sectionsManagerStub},
      "lib/Screenshots.jsm": {Screenshots: fakeScreenshot},
      "common/Dedupe.jsm": {Dedupe}
    }));
    feed = new HighlightsFeed();
    feed.store = {
      dispatch: sinon.spy(),
      getState() { return this.state; },
      state: {
        Prefs: {values: {filterAdult: false}},
        TopSites: {
          initialized: true,
          rows: Array(12).fill(null).map((v, i) => ({url: `http://www.topsite${i}.com`}))
        }
      },
      subscribe: sinon.stub().callsFake(cb => { cb(); return () => {}; })
    };
    links = FAKE_LINKS;
    clock = sinon.useFakeTimers();
  });
  afterEach(() => {
    globals.restore();
    clock.restore();
  });

  describe("#init", () => {
    it("should create a HighlightsFeed", () => {
      assert.instanceOf(feed, HighlightsFeed);
    });
    it("should call SectionsManager.onceInitialized on INIT", () => {
      feed.onAction({type: at.INIT});
      assert.calledOnce(sectionsManagerStub.onceInitialized);
    });
    it("should enable its section", () => {
      feed.onAction({type: at.INIT});
      assert.calledOnce(sectionsManagerStub.enableSection);
      assert.calledWith(sectionsManagerStub.enableSection, SECTION_ID);
    });
    it("should fetch highlights on postInit", () => {
      feed.fetchHighlights = sinon.spy();
      feed.postInit();
      assert.calledOnce(feed.fetchHighlights);
    });
  });
  describe("#fetchHighlights", () => {
    it("should wait for TopSites to be initialised", async () => {
      feed.store.state.TopSites.initialized = false;
      // Initially TopSites is uninitialised and fetchHighlights should wait
      const firstFetch = feed.fetchHighlights();

      assert.calledOnce(feed.store.subscribe);
      assert.notCalled(fakeNewTabUtils.activityStreamLinks.getHighlights);

      // Initialisation causes the subscribe callback to be called and
      // fetchHighlights should continue
      feed.store.state.TopSites.initialized = true;
      const subscribeCallback = feed.store.subscribe.firstCall.args[0];
      await subscribeCallback();
      await firstFetch;
      await feed._getBookmarksThreshold();
      assert.calledOnce(fakeNewTabUtils.activityStreamLinks.getHighlights);

      // If TopSites is initialised in the first place it shouldn't wait
      feed.linksCache.expire();
      feed.store.subscribe.reset();
      fakeNewTabUtils.activityStreamLinks.getHighlights.reset();
      await feed.fetchHighlights();
      assert.notCalled(feed.store.subscribe);
      await feed._getBookmarksThreshold();
      assert.calledOnce(fakeNewTabUtils.activityStreamLinks.getHighlights);
    });
    it("should add hostname and hasImage to each link", async () => {
      links = [{url: "https://mozilla.org"}];

      const highlights = await fetchHighlights();

      assert.equal(highlights[0].hostname, "mozilla.org");
      assert.equal(highlights[0].hasImage, true);
    });
    it("should add an existing image if it exists to the link without calling fetchImage", async () => {
      links = [{url: "https://mozilla.org", image: FAKE_IMAGE}];
      sinon.spy(feed, "fetchImage");

      const highlights = await fetchHighlights();

      assert.equal(highlights[0].image, FAKE_IMAGE);
      assert.notCalled(feed.fetchImage);
    });
    it("should call fetchImage with the correct arguments for new links", async () => {
      links = [{url: "https://mozilla.org", preview_image_url: "https://mozilla.org/preview.jog"}];
      sinon.spy(feed, "fetchImage");

      await feed.fetchHighlights();

      assert.calledOnce(feed.fetchImage);
      const arg = feed.fetchImage.firstCall.args[0];
      assert.propertyVal(arg, "url", links[0].url);
      assert.propertyVal(arg, "preview_image_url", links[0].preview_image_url);
    });
    it("should not include any links already in Top Sites", async () => {
      links = [
        {url: "https://mozilla.org"},
        {url: "http://www.topsite0.com"},
        {url: "http://www.topsite1.com"},
        {url: "http://www.topsite2.com"}
      ];

      const highlights = await fetchHighlights();

      assert.equal(highlights.length, 1);
      assert.equal(highlights[0].url, links[0].url);
    });
    it("should not include history of same hostname as a bookmark", async () => {
      links = [
        {url: "https://site.com/bookmark", type: "bookmark"},
        {url: "https://site.com/history", type: "history"}
      ];

      const highlights = await fetchHighlights();

      assert.equal(highlights.length, 1);
      assert.equal(highlights[0].url, links[0].url);
    });
    it("should take the first history of a hostname", async () => {
      links = [
        {url: "https://site.com/first", type: "history"},
        {url: "https://site.com/second", type: "history"},
        {url: "https://other", type: "history"}
      ];

      const highlights = await fetchHighlights();

      assert.equal(highlights.length, 2);
      assert.equal(highlights[0].url, links[0].url);
      assert.equal(highlights[1].url, links[2].url);
    });
    it("should set type to bookmark if there is a bookmarkGuid", async () => {
      links = [{url: "https://mozilla.org", type: "history", bookmarkGuid: "1234567890"}];

      const highlights = await fetchHighlights();

      assert.equal(highlights[0].type, "bookmark");
    });
    it("should not filter out adult pages when pref is false", async() => {
      await feed.fetchHighlights();

      assert.notCalled(filterAdultStub);
    });
    it("should filter out adult pages when pref is true", async() => {
      feed.store.state.Prefs.values.filterAdult = true;

      const highlights = await fetchHighlights();

      // The stub filters out everything
      assert.calledOnce(filterAdultStub);
      assert.equal(highlights.length, 0);
    });
    it("should not expose internal link properties", async() => {
      const highlights = await fetchHighlights();

      const internal = Object.keys(highlights[0]).filter(key => key.startsWith("__"));
      assert.equal(internal.join(""), "");
    });
  });
  describe("#fetchImage", () => {
    const FAKE_URL = "https://mozilla.org";
    const FAKE_IMAGE_URL = "https://mozilla.org/preview.jpg";
    function fetchImage(page) {
      return feed.fetchImage(Object.assign({__sharedCache: {updateLink() {}}},
        page));
    }
    it("should capture the image, if available", async () => {
      await fetchImage({
        preview_image_url: FAKE_IMAGE_URL,
        url: FAKE_URL
      });

      assert.calledOnce(fakeScreenshot.getScreenshotForURL);
      assert.calledWith(fakeScreenshot.getScreenshotForURL, FAKE_IMAGE_URL);
    });
    it("should fall back to capturing a screenshot", async () => {
      await fetchImage({url: FAKE_URL});

      assert.calledOnce(fakeScreenshot.getScreenshotForURL);
      assert.calledWith(fakeScreenshot.getScreenshotForURL, FAKE_URL);
    });
    it("should call SectionsManager.updateSectionCard with the right arguments", async () => {
      await fetchImage({
        preview_image_url: FAKE_IMAGE_URL,
        url: FAKE_URL
      });

      assert.calledOnce(sectionsManagerStub.updateSectionCard);
      assert.calledWith(sectionsManagerStub.updateSectionCard, "highlights", FAKE_URL, {image: FAKE_IMAGE}, true);
    });
    it("should not update the card with the image", async () => {
      const card = {
        preview_image_url: FAKE_IMAGE_URL,
        url: FAKE_URL
      };

      await feed.fetchImage(card);

      assert.notProperty(card, "image");
    });
  });
  describe("#_getBookmarksThreshold", () => {
    it("should have the correct default", () => {
      assert.equal(feed._profileAge, 0);
    });
    it("should not call ProfileAge if _profileAge is set", async () => {
      feed._profileAge = 10;

      await feed._getBookmarksThreshold();

      assert.notCalled(profileAgeCreatedStub);
    });
    it("should call ProfileAge if _profileAge is not set", async () => {
      await feed._getBookmarksThreshold();

      assert.calledOnce(profileAgeCreatedStub);
    });
    it("should set _profileAge", async () => {
      await feed._getBookmarksThreshold();

      assert.notEqual(feed._profileAge, 0);
    });
  });
  describe("#uninit", () => {
    it("should disable its section", () => {
      feed.onAction({type: at.UNINIT});
      assert.calledOnce(sectionsManagerStub.disableSection);
      assert.calledWith(sectionsManagerStub.disableSection, SECTION_ID);
    });
  });
  describe("#onAction", () => {
    it("should fetch highlights on NEW_TAB_LOAD after update interval", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.NEW_TAB_LOAD});
      assert.notCalled(feed.fetchHighlights);

      clock.tick(HIGHLIGHTS_UPDATE_TIME);
      feed.onAction({type: at.NEW_TAB_LOAD});
      assert.calledOnce(feed.fetchHighlights);
    });
    it("should fetch highlights on NEW_TAB_LOAD if grid is empty", async () => {
      links = [];
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.NEW_TAB_LOAD});
      assert.calledOnce(feed.fetchHighlights);
    });
    it("should fetch highlights on NEW_TAB_LOAD if grid isn't full", async () => {
      links = new Array(8).fill(null).map((v, i) => ({url: `http://www.site${i}.com`}));
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.NEW_TAB_LOAD});
      assert.calledOnce(feed.fetchHighlights);
    });
    it("should fetch highlights on MIGRATION_COMPLETED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.MIGRATION_COMPLETED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, true);
    });
    it("should fetch highlights on PLACES_HISTORY_CLEARED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_HISTORY_CLEARED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, true);
    });
    it("should fetch highlights on PLACES_LINKS_DELETED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_LINKS_DELETED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, true);
    });
    it("should fetch highlights on PLACES_LINK_BLOCKED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_LINK_BLOCKED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, true);
    });
    it("should fetch highlights on PLACES_BOOKMARK_ADDED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_BOOKMARK_ADDED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, false);
    });
    it("should fetch highlights on PLACES_BOOKMARK_REMOVED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_BOOKMARK_REMOVED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, false);
    });
    it("should fetch highlights on TOP_SITES_UPDATED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.TOP_SITES_UPDATED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, false);
    });
  });
});

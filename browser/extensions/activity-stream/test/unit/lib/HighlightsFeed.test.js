"use strict";
const injector = require("inject!lib/HighlightsFeed.jsm");
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
  let sectionsManagerStub;
  let shortURLStub;

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
    fakeScreenshot = {getScreenshotForURL: sandbox.spy(() => Promise.resolve(FAKE_IMAGE))};
    shortURLStub = sinon.stub().callsFake(site => site.url.match(/\/([^/]+)/)[1]);
    globals.set("NewTabUtils", fakeNewTabUtils);
    ({HighlightsFeed, HIGHLIGHTS_UPDATE_TIME, SECTION_ID} = injector({
      "lib/ShortURL.jsm": {shortURL: shortURLStub},
      "lib/SectionsManager.jsm": {SectionsManager: sectionsManagerStub},
      "lib/Screenshots.jsm": {Screenshots: fakeScreenshot},
      "common/Dedupe.jsm": {Dedupe}
    }));
    feed = new HighlightsFeed();
    feed.store = {dispatch: sinon.spy(), getState() { return {TopSites: {rows: Array(12).fill(null).map((v, i) => ({url: `http://www.topsite${i}.com`}))}}; }};
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
    it("should *not* fetch highlights on init to avoid loading Places too early", () => {
      feed.fetchHighlights = sinon.spy();

      feed.onAction({type: at.INIT});

      assert.notCalled(feed.fetchHighlights);
    });
  });
  describe("#fetchHighlights", () => {
    it("should add hostname and hasImage to each link", async () => {
      links = [{url: "https://mozilla.org"}];
      await feed.fetchHighlights();
      assert.equal(feed.highlights[0].hostname, "mozilla.org");
      assert.equal(feed.highlights[0].hasImage, true);
    });
    it("should add the image from the imageCache if it exists to the link", async () => {
      links = [{url: "https://mozilla.org", preview_image_url: "https://mozilla.org/preview.jog"}];
      feed.imageCache = new Map([["https://mozilla.org", FAKE_IMAGE]]);
      await feed.fetchHighlights();
      assert.equal(feed.highlights[0].image, FAKE_IMAGE);
    });
    it("should call fetchImage with the correct arguments for each link", async () => {
      links = [{url: "https://mozilla.org", preview_image_url: "https://mozilla.org/preview.jog"}];
      sinon.spy(feed, "fetchImage");
      await feed.fetchHighlights();
      assert.calledOnce(feed.fetchImage);
      assert.calledWith(feed.fetchImage, links[0].url, links[0].preview_image_url);
    });
    it("should not include any links already in Top Sites", async () => {
      links = [
        {url: "https://mozilla.org"},
        {url: "http://www.topsite0.com"},
        {url: "http://www.topsite1.com"},
        {url: "http://www.topsite2.com"}
      ];
      await feed.fetchHighlights();
      assert.equal(feed.highlights.length, 1);
      assert.deepEqual(feed.highlights[0], links[0]);
    });
    it("should not include history of same hostname as a bookmark", async () => {
      links = [
        {url: "https://site.com/bookmark", type: "bookmark"},
        {url: "https://site.com/history", type: "history"}
      ];

      await feed.fetchHighlights();

      assert.equal(feed.highlights.length, 1);
      assert.deepEqual(feed.highlights[0], links[0]);
    });
    it("should take the first history of a hostname", async () => {
      links = [
        {url: "https://site.com/first", type: "history"},
        {url: "https://site.com/second", type: "history"},
        {url: "https://other", type: "history"}
      ];

      await feed.fetchHighlights();

      assert.equal(feed.highlights.length, 2);
      assert.deepEqual(feed.highlights[0], links[0]);
      assert.deepEqual(feed.highlights[1], links[2]);
    });
    it("should set type to bookmark if there is a bookmarkGuid", async () => {
      links = [{url: "https://mozilla.org", type: "history", bookmarkGuid: "1234567890"}];
      await feed.fetchHighlights();
      assert.equal(feed.highlights[0].type, "bookmark");
    });
    it("should clear the imageCache at the end", async () => {
      links = [{url: "https://mozilla.org", preview_image_url: "https://mozilla.org/preview.jpg"}];
      feed.imageCache = new Map([["https://mozilla.org", FAKE_IMAGE]]);
      // Stops fetchImage adding to the cache
      feed.fetchImage = () => {};
      await feed.fetchHighlights();
      assert.equal(feed.imageCache.size, 0);
    });
  });
  describe("#fetchImage", () => {
    const FAKE_URL = "https://mozilla.org";
    const FAKE_IMAGE_URL = "https://mozilla.org/preview.jpg";
    it("should capture the image, if available", async () => {
      await feed.fetchImage(FAKE_URL, FAKE_IMAGE_URL);
      assert.calledOnce(fakeScreenshot.getScreenshotForURL);
      assert.calledWith(fakeScreenshot.getScreenshotForURL, FAKE_IMAGE_URL);
    });
    it("should fall back to capturing a screenshot", async () => {
      await feed.fetchImage(FAKE_URL, undefined);
      assert.calledOnce(fakeScreenshot.getScreenshotForURL);
      assert.calledWith(fakeScreenshot.getScreenshotForURL, FAKE_URL);
    });
    it("should store the image in the imageCache", async () => {
      feed.imageCache.clear();
      await feed.fetchImage(FAKE_URL, FAKE_IMAGE_URL);
      assert.equal(feed.imageCache.get(FAKE_URL), FAKE_IMAGE);
    });
    it("should call SectionsManager.updateSectionCard with the right arguments", async () => {
      await feed.fetchImage(FAKE_URL, FAKE_IMAGE_URL);
      assert.calledOnce(sectionsManagerStub.updateSectionCard);
      assert.calledWith(sectionsManagerStub.updateSectionCard, "highlights", FAKE_URL, {image: FAKE_IMAGE}, true);
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
    it("should fetch highlights on PLACES_LINK_DELETED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_LINK_DELETED});
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

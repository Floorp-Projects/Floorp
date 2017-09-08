"use strict";
const injector = require("inject!lib/HighlightsFeed.jsm");
const {GlobalOverrider} = require("test/unit/utils");
const {actionTypes: at} = require("common/Actions.jsm");
const {Dedupe} = require("common/Dedupe.jsm");

const FAKE_LINKS = new Array(9).fill(null).map((v, i) => ({url: `http://www.site${i}.com`}));

describe("Top Sites Feed", () => {
  let HighlightsFeed;
  let HIGHLIGHTS_UPDATE_TIME;
  let SECTION_ID;
  let feed;
  let globals;
  let sandbox;
  let links;
  let clock;
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
      sections: new Map([["highlights", {}]])
    };
    shortURLStub = sinon.stub().callsFake(site => site.url);
    globals.set("NewTabUtils", fakeNewTabUtils);
    ({HighlightsFeed, HIGHLIGHTS_UPDATE_TIME, SECTION_ID} = injector({
      "lib/ShortURL.jsm": {shortURL: shortURLStub},
      "lib/SectionsManager.jsm": {SectionsManager: sectionsManagerStub},
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
    it("should add hostname and image to each link", async () => {
      links = [{url: "https://mozilla.org", preview_image_url: "https://mozilla.org/preview.jog"}];
      await feed.fetchHighlights();
      assert.equal(feed.highlights[0].hostname, links[0].url);
      assert.equal(feed.highlights[0].image, links[0].preview_image_url);
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
    it("should set type to bookmark if there is a bookmarkGuid", async () => {
      links = [{url: "https://mozilla.org", type: "history", bookmarkGuid: "1234567890"}];
      await feed.fetchHighlights();
      assert.equal(feed.highlights[0].type, "bookmark");
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

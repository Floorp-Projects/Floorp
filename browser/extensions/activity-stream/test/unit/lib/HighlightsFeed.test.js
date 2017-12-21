"use strict";

import {actionTypes as at} from "common/Actions.jsm";
import {Dedupe} from "common/Dedupe.jsm";
import {GlobalOverrider} from "test/unit/utils";
import injector from "inject!lib/HighlightsFeed.jsm";
import {Screenshots} from "lib/Screenshots.jsm";

const FAKE_LINKS = new Array(9).fill(null).map((v, i) => ({url: `http://www.site${i}.com`}));
const FAKE_IMAGE = "data123";

describe("Highlights Feed", () => {
  let HighlightsFeed;
  let SECTION_ID;
  let feed;
  let globals;
  let sandbox;
  let links;
  let fakeScreenshot;
  let fakeNewTabUtils;
  let filterAdultStub;
  let sectionsManagerStub;
  let shortURLStub;
  let fakePageThumbs;

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
      sections: new Map([["highlights", {order: 0}]])
    };
    fakeScreenshot = {
      getScreenshotForURL: sandbox.spy(() => Promise.resolve(FAKE_IMAGE)),
      maybeCacheScreenshot: Screenshots.maybeCacheScreenshot
    };
    filterAdultStub = sinon.stub().returns([]);
    shortURLStub = sinon.stub().callsFake(site => site.url.match(/\/([^/]+)/)[1]);
    fakePageThumbs = {
      addExpirationFilter: sinon.stub(),
      removeExpirationFilter: sinon.stub()
    };

    globals.set("NewTabUtils", fakeNewTabUtils);
    globals.set("PageThumbs", fakePageThumbs);
    ({HighlightsFeed, SECTION_ID} = injector({
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
        },
        Sections: [{initialized: false}]
      },
      subscribe: sinon.stub().callsFake(cb => { cb(); return () => {}; })
    };
    links = FAKE_LINKS;
  });
  afterEach(() => {
    globals.restore();
  });

  describe("#init", () => {
    it("should create a HighlightsFeed", () => {
      assert.instanceOf(feed, HighlightsFeed);
    });
    it("should register a expiration filter", () => {
      assert.calledOnce(fakePageThumbs.addExpirationFilter);
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
  describe("#filterForThumbnailExpiration", () => {
    it("should pass rows.urls to the callback provided", () => {
      const rows = [{url: "foo.com"}, {"url": "bar.com"}];
      feed.store.state.Sections = [{rows, initialized: true}];
      const stub = sinon.stub();

      feed.filterForThumbnailExpiration(stub);

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, rows.map(r => r.url));
    });
    it("should include preview_image_url (if present) in the callback results", () => {
      const rows = [{url: "foo.com"}, {"url": "bar.com", "preview_image_url": "bar.jpg"}];
      feed.store.state.Sections = [{rows, initialized: true}];
      const stub = sinon.stub();

      feed.filterForThumbnailExpiration(stub);

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, ["foo.com", "bar.com", "bar.jpg"]);
    });
    it("should pass an empty array if not initialized", () => {
      const rows = [{url: "foo.com"}, {"url": "bar.com"}];
      feed.store.state.Sections = [{rows, initialized: false}];
      const stub = sinon.stub();

      feed.filterForThumbnailExpiration(stub);

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, []);
    });
  });
  describe("#fetchHighlights", () => {
    const fetchHighlights = async options => {
      await feed.fetchHighlights(options);
      return sectionsManagerStub.updateSection.firstCall.args[1].rows;
    };

    it("should return early if if are not TopSites initialised", async () => {
      sandbox.spy(feed.linksCache, "request");
      feed.store.state.TopSites.initialized = false;

      // Initially TopSites is uninitialised and fetchHighlights should return.
      await feed.fetchHighlights();

      assert.notCalled(fakeNewTabUtils.activityStreamLinks.getHighlights);
      assert.notCalled(feed.linksCache.request);
    });
    it("should fetch Highlights if TopSites are initialised", async () => {
      sandbox.spy(feed.linksCache, "request");
      // fetchHighlights should continue
      feed.store.state.TopSites.initialized = true;

      await feed.fetchHighlights();

      assert.calledOnce(feed.linksCache.request);
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
    it("should include bookmark but not history already in Top Sites", async () => {
      links = [
        {url: "http://www.topsite0.com", type: "bookmark"},
        {url: "http://www.topsite1.com", type: "history"}
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
    it("should not filter out adult pages when pref is false", async () => {
      await feed.fetchHighlights();

      assert.notCalled(filterAdultStub);
    });
    it("should filter out adult pages when pref is true", async () => {
      feed.store.state.Prefs.values.filterAdult = true;

      const highlights = await fetchHighlights();

      // The stub filters out everything
      assert.calledOnce(filterAdultStub);
      assert.equal(highlights.length, 0);
    });
    it("should not expose internal link properties", async () => {
      const highlights = await fetchHighlights();

      const internal = Object.keys(highlights[0]).filter(key => key.startsWith("__"));
      assert.equal(internal.join(""), "");
    });
    it("should broadcast if feed is not initialized", async () => {
      links = [];
      await fetchHighlights();

      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.calledWithExactly(sectionsManagerStub.updateSection, SECTION_ID, {rows: []}, true);
    });
    it("should broadcast if options.broadcast is true", async () => {
      links = [];
      feed.store.state.Sections[0].initialized = true;
      await fetchHighlights({broadcast: true});

      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.calledWithExactly(sectionsManagerStub.updateSection, SECTION_ID, {rows: []}, true);
    });
    it("should not broadcast if options.broadcast is false and initialized is true", async () => {
      links = [];
      feed.store.state.Sections[0].initialized = true;
      await fetchHighlights({broadcast: false});

      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.calledWithExactly(sectionsManagerStub.updateSection, SECTION_ID, {rows: []}, false);
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
  describe("#uninit", () => {
    it("should disable its section", () => {
      feed.onAction({type: at.UNINIT});
      assert.calledOnce(sectionsManagerStub.disableSection);
      assert.calledWith(sectionsManagerStub.disableSection, SECTION_ID);
    });
    it("should remove the expiration filter", () => {
      feed.onAction({type: at.UNINIT});
      assert.calledOnce(fakePageThumbs.removeExpirationFilter);
    });
  });
  describe("#onAction", () => {
    it("should fetch highlights on SYSTEM_TICK", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.SYSTEM_TICK});

      assert.calledOnce(feed.fetchHighlights);
      assert.calledWithExactly(feed.fetchHighlights, {broadcast: false});
    });
    it("should fetch highlights on MIGRATION_COMPLETED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.MIGRATION_COMPLETED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should fetch highlights on PLACES_HISTORY_CLEARED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_HISTORY_CLEARED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should fetch highlights on PLACES_LINKS_DELETED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_LINKS_DELETED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should fetch highlights on PLACES_LINK_BLOCKED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_LINK_BLOCKED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should fetch highlights on PLACES_BOOKMARK_ADDED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_BOOKMARK_ADDED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: false});
    });
    it("should fetch highlights on PLACES_BOOKMARK_REMOVED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_BOOKMARK_REMOVED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: false});
    });
    it("should expire the cache on PLACES_BOOKMARK_REMOVED", async () => {
      sandbox.stub(feed.linksCache, "expire");

      feed.onAction({type: at.PLACES_BOOKMARK_REMOVED});

      assert.calledOnce(feed.linksCache.expire);
    });
    it("should call fetchHighlights with broadcast false on TOP_SITES_UPDATED", () => {
      sandbox.stub(feed, "fetchHighlights");
      feed.onAction({type: at.TOP_SITES_UPDATED});

      assert.calledOnce(feed.fetchHighlights);
      assert.calledWithExactly(feed.fetchHighlights, {broadcast: false});
    });
  });
});

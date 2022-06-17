import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { GlobalOverrider } from "test/unit/utils";
import injector from "inject!lib/PlacesFeed.jsm";

const FAKE_BOOKMARK = {
  bookmarkGuid: "xi31",
  bookmarkTitle: "Foo",
  dateAdded: 123214232,
  url: "foo.com",
};
const TYPE_BOOKMARK = 0; // This is fake, for testing
const SOURCES = {
  DEFAULT: 0,
  SYNC: 1,
  IMPORT: 2,
  RESTORE: 5,
  RESTORE_ON_STARTUP: 6,
};

const BLOCKED_EVENT = "newtab-linkBlocked"; // The event dispatched in NewTabUtils when a link is blocked;

const TOP_SITES_BLOCKED_SPONSORS_PREF = "browser.topsites.blockedSponsors";
const POCKET_SITE_PREF = "extensions.pocket.site";

describe("PlacesFeed", () => {
  let PlacesFeed;
  let BookmarksObserver;
  let PlacesObserver;
  let globals;
  let sandbox;
  let feed;
  let shortURLStub;
  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    globals.set("NewTabUtils", {
      activityStreamProvider: { getBookmark() {} },
      activityStreamLinks: {
        addBookmark: sandbox.spy(),
        deleteBookmark: sandbox.spy(),
        deleteHistoryEntry: sandbox.spy(),
        blockURL: sandbox.spy(),
        addPocketEntry: sandbox.spy(() => Promise.resolve()),
        deletePocketEntry: sandbox.spy(() => Promise.resolve()),
        archivePocketEntry: sandbox.spy(() => Promise.resolve()),
      },
    });
    globals.set("pktApi", {
      isUserLoggedIn: sandbox.spy(),
    });
    globals.set("ExperimentAPI", {
      getExperiment: sandbox.spy(),
    });
    globals.set("NimbusFeatures", {
      pocketNewtab: {
        getVariable: sandbox.spy(),
      },
    });
    globals.set("PartnerLinkAttribution", {
      makeRequest: sandbox.spy(),
    });
    sandbox
      .stub(global.PlacesUtils.bookmarks, "TYPE_BOOKMARK")
      .value(TYPE_BOOKMARK);
    sandbox.stub(global.PlacesUtils.bookmarks, "SOURCES").value(SOURCES);
    sandbox.spy(global.PlacesUtils.bookmarks, "addObserver");
    sandbox.spy(global.PlacesUtils.bookmarks, "removeObserver");
    sandbox.spy(global.PlacesUtils.history, "addObserver");
    sandbox.spy(global.PlacesUtils.history, "removeObserver");
    sandbox.spy(global.PlacesUtils.observers, "addListener");
    sandbox.spy(global.PlacesUtils.observers, "removeListener");
    sandbox.spy(global.Services.obs, "addObserver");
    sandbox.spy(global.Services.obs, "removeObserver");
    sandbox.spy(global.Cu, "reportError");
    shortURLStub = sandbox
      .stub()
      .callsFake(site =>
        site.url.replace(/(.com|.ca)/, "").replace("https://", "")
      );

    global.Services.io.newURI = spec => ({
      mutate: () => ({
        setRef: ref => ({
          finalize: () => ({
            ref,
            spec,
          }),
        }),
      }),
      spec,
      scheme: "https",
    });

    global.Cc["@mozilla.org/timer;1"] = {
      createInstance() {
        return {
          initWithCallback: sinon.stub().callsFake(callback => callback()),
          cancel: sinon.spy(),
        };
      },
    };
    ({ PlacesFeed } = injector({
      "lib/ShortURL.jsm": { shortURL: shortURLStub },
    }));
    BookmarksObserver = PlacesFeed.BookmarksObserver;
    PlacesObserver = PlacesFeed.PlacesObserver;
    feed = new PlacesFeed();
    feed.store = { dispatch: sinon.spy() };
  });
  afterEach(() => {
    globals.restore();
    sandbox.restore();
  });

  it("should have a BookmarksObserver that dispatch to the store", () => {
    assert.instanceOf(feed.bookmarksObserver, BookmarksObserver);
    const action = { type: "FOO" };

    feed.bookmarksObserver.dispatch(action);

    assert.calledOnce(feed.store.dispatch);
    assert.equal(feed.store.dispatch.firstCall.args[0].type, action.type);
  });

  it("should have a PlacesObserver that dispatches to the store", () => {
    assert.instanceOf(feed.placesObserver, PlacesObserver);
    const action = { type: "FOO" };

    feed.placesObserver.dispatch(action);

    assert.calledOnce(feed.store.dispatch);
    assert.equal(feed.store.dispatch.firstCall.args[0].type, action.type);
  });

  describe("#addToBlockedTopSitesSponsors", () => {
    let spy;
    beforeEach(() => {
      sandbox
        .stub(global.Services.prefs, "getStringPref")
        .withArgs(TOP_SITES_BLOCKED_SPONSORS_PREF)
        .returns(`["foo","bar"]`);
      spy = sandbox.spy(global.Services.prefs, "setStringPref");
    });

    it("should add the blocked sponsors to the blocklist", () => {
      feed.addToBlockedTopSitesSponsors([
        { url: "test.com" },
        { url: "test1.com" },
      ]);

      assert.calledOnce(spy);
      const [, sponsors] = spy.firstCall.args;
      assert.deepEqual(
        new Set(["foo", "bar", "test", "test1"]),
        new Set(JSON.parse(sponsors))
      );
    });

    it("should not add duplicate sponsors to the blocklist", () => {
      feed.addToBlockedTopSitesSponsors([
        { url: "foo.com" },
        { url: "bar.com" },
        { url: "test.com" },
      ]);

      assert.calledOnce(spy);
      const [, sponsors] = spy.firstCall.args;
      assert.deepEqual(
        new Set(["foo", "bar", "test"]),
        new Set(JSON.parse(sponsors))
      );
    });
  });

  describe("#onAction", () => {
    it("should add bookmark, history, places, blocked observers on INIT", () => {
      feed.onAction({ type: at.INIT });

      assert.calledWith(
        global.PlacesUtils.bookmarks.addObserver,
        feed.bookmarksObserver,
        true
      );
      assert.calledWith(
        global.PlacesUtils.observers.addListener,
        [
          "bookmark-added",
          "bookmark-removed",
          "history-cleared",
          "page-removed",
        ],
        feed.placesObserver.handlePlacesEvent
      );
      assert.calledWith(global.Services.obs.addObserver, feed, BLOCKED_EVENT);
    });
    it("should remove bookmark, history, places, blocked observers, and timers on UNINIT", () => {
      feed.placesChangedTimer = global.Cc[
        "@mozilla.org/timer;1"
      ].createInstance();
      let spy = feed.placesChangedTimer.cancel;
      feed.onAction({ type: at.UNINIT });

      assert.calledWith(
        global.PlacesUtils.bookmarks.removeObserver,
        feed.bookmarksObserver
      );
      assert.calledWith(
        global.PlacesUtils.observers.removeListener,
        [
          "bookmark-added",
          "bookmark-removed",
          "history-cleared",
          "page-removed",
        ],
        feed.placesObserver.handlePlacesEvent
      );
      assert.calledWith(
        global.Services.obs.removeObserver,
        feed,
        BLOCKED_EVENT
      );
      assert.equal(feed.placesChangedTimer, null);
      assert.calledOnce(spy);
    });
    it("should block a url on BLOCK_URL", () => {
      feed.onAction({
        type: at.BLOCK_URL,
        data: [{ url: "apple.com", pocket_id: 1234 }],
      });
      assert.calledWith(global.NewTabUtils.activityStreamLinks.blockURL, {
        url: "apple.com",
        pocket_id: 1234,
      });
    });
    it("should update the blocked top sites sponsors", () => {
      sandbox.stub(feed, "addToBlockedTopSitesSponsors");
      feed.onAction({
        type: at.BLOCK_URL,
        data: [{ url: "foo.com", pocket_id: 1234, isSponsoredTopSite: 1 }],
      });
      assert.calledWith(feed.addToBlockedTopSitesSponsors, [
        { url: "foo.com" },
      ]);
    });
    it("should bookmark a url on BOOKMARK_URL", () => {
      const data = { url: "pear.com", title: "A pear" };
      const _target = { browser: { ownerGlobal() {} } };
      feed.onAction({ type: at.BOOKMARK_URL, data, _target });
      assert.calledWith(
        global.NewTabUtils.activityStreamLinks.addBookmark,
        data,
        _target.browser.ownerGlobal
      );
    });
    it("should delete a bookmark on DELETE_BOOKMARK_BY_ID", () => {
      feed.onAction({ type: at.DELETE_BOOKMARK_BY_ID, data: "g123kd" });
      assert.calledWith(
        global.NewTabUtils.activityStreamLinks.deleteBookmark,
        "g123kd"
      );
    });
    it("should delete a history entry on DELETE_HISTORY_URL", () => {
      feed.onAction({
        type: at.DELETE_HISTORY_URL,
        data: { url: "guava.com", forceBlock: null },
      });
      assert.calledWith(
        global.NewTabUtils.activityStreamLinks.deleteHistoryEntry,
        "guava.com"
      );
      assert.notCalled(global.NewTabUtils.activityStreamLinks.blockURL);
    });
    it("should delete a history entry on DELETE_HISTORY_URL and force a site to be blocked if specified", () => {
      feed.onAction({
        type: at.DELETE_HISTORY_URL,
        data: { url: "guava.com", forceBlock: "g123kd" },
      });
      assert.calledWith(
        global.NewTabUtils.activityStreamLinks.deleteHistoryEntry,
        "guava.com"
      );
      assert.calledWith(global.NewTabUtils.activityStreamLinks.blockURL, {
        url: "guava.com",
        pocket_id: undefined,
      });
    });
    it("should call openTrustedLinkIn with the correct url, where and params on OPEN_NEW_WINDOW", () => {
      const openTrustedLinkIn = sinon.stub();
      const openWindowAction = {
        type: at.OPEN_NEW_WINDOW,
        data: { url: "https://foo.com" },
        _target: { browser: { ownerGlobal: { openTrustedLinkIn } } },
      };

      feed.onAction(openWindowAction);

      assert.calledOnce(openTrustedLinkIn);
      const [url, where, params] = openTrustedLinkIn.firstCall.args;
      assert.equal(url, "https://foo.com");
      assert.equal(where, "window");
      assert.propertyVal(params, "private", false);
      assert.propertyVal(params, "fromChrome", false);
    });
    it("should call openTrustedLinkIn with the correct url, where, params and privacy args on OPEN_PRIVATE_WINDOW", () => {
      const openTrustedLinkIn = sinon.stub();
      const openWindowAction = {
        type: at.OPEN_PRIVATE_WINDOW,
        data: { url: "https://foo.com" },
        _target: { browser: { ownerGlobal: { openTrustedLinkIn } } },
      };

      feed.onAction(openWindowAction);

      assert.calledOnce(openTrustedLinkIn);
      const [url, where, params] = openTrustedLinkIn.firstCall.args;
      assert.equal(url, "https://foo.com");
      assert.equal(where, "window");
      assert.propertyVal(params, "private", true);
      assert.propertyVal(params, "fromChrome", false);
    });
    it("should call openTrustedLinkIn with the correct url, where and params on OPEN_LINK", () => {
      const openTrustedLinkIn = sinon.stub();
      const openLinkAction = {
        type: at.OPEN_LINK,
        data: { url: "https://foo.com" },
        _target: {
          browser: {
            ownerGlobal: { openTrustedLinkIn, whereToOpenLink: e => "current" },
          },
        },
      };

      feed.onAction(openLinkAction);

      assert.calledOnce(openTrustedLinkIn);
      const [url, where, params] = openTrustedLinkIn.firstCall.args;
      assert.equal(url, "https://foo.com");
      assert.equal(where, "current");
      assert.propertyVal(params, "private", false);
      assert.propertyVal(params, "fromChrome", false);
    });
    it("should open link with referrer on OPEN_LINK", () => {
      const openTrustedLinkIn = sinon.stub();
      const openLinkAction = {
        type: at.OPEN_LINK,
        data: { url: "https://foo.com", referrer: "https://foo.com/ref" },
        _target: {
          browser: {
            ownerGlobal: { openTrustedLinkIn, whereToOpenLink: e => "tab" },
          },
        },
      };

      feed.onAction(openLinkAction);

      const [, , params] = openTrustedLinkIn.firstCall.args;
      assert.nestedPropertyVal(params, "referrerInfo.referrerPolicy", 5);
      assert.nestedPropertyVal(
        params,
        "referrerInfo.originalReferrer.spec",
        "https://foo.com/ref"
      );
    });
    it("should mark link with typed bonus as typed before opening OPEN_LINK", () => {
      const callOrder = [];
      sinon
        .stub(global.PlacesUtils.history, "markPageAsTyped")
        .callsFake(() => {
          callOrder.push("markPageAsTyped");
        });
      const openTrustedLinkIn = sinon.stub().callsFake(() => {
        callOrder.push("openTrustedLinkIn");
      });
      const openLinkAction = {
        type: at.OPEN_LINK,
        data: {
          typedBonus: true,
          url: "https://foo.com",
        },
        _target: {
          browser: {
            ownerGlobal: { openTrustedLinkIn, whereToOpenLink: e => "tab" },
          },
        },
      };

      feed.onAction(openLinkAction);

      assert.sameOrderedMembers(callOrder, [
        "markPageAsTyped",
        "openTrustedLinkIn",
      ]);
    });
    it("should open the pocket link if it's a pocket story on OPEN_LINK", () => {
      const openTrustedLinkIn = sinon.stub();
      const openLinkAction = {
        type: at.OPEN_LINK,
        data: {
          url: "https://foo.com",
          open_url: "getpocket.com/foo",
          type: "pocket",
        },
        _target: {
          browser: {
            ownerGlobal: { openTrustedLinkIn, whereToOpenLink: e => "current" },
          },
        },
      };

      feed.onAction(openLinkAction);

      assert.calledOnce(openTrustedLinkIn);
      const [url, where, params] = openTrustedLinkIn.firstCall.args;
      assert.equal(url, "getpocket.com/foo");
      assert.equal(where, "current");
      assert.propertyVal(params, "private", false);
    });
    it("should not open link if not http", () => {
      const openTrustedLinkIn = sinon.stub();
      global.Services.io.newURI = spec => ({
        mutate: () => ({
          setRef: ref => ({
            finalize: () => ({
              ref,
              spec,
            }),
          }),
        }),
        spec,
        scheme: "file",
      });
      const openLinkAction = {
        type: at.OPEN_LINK,
        data: { url: "file:///foo.com" },
        _target: {
          browser: {
            ownerGlobal: { openTrustedLinkIn, whereToOpenLink: e => "current" },
          },
        },
      };

      feed.onAction(openLinkAction);
      const [e] = global.Cu.reportError.firstCall.args;
      assert.equal(
        e.message,
        "Can't open link using file protocol from the new tab page."
      );
    });
    it("should call fillSearchTopSiteTerm on FILL_SEARCH_TERM", () => {
      sinon.stub(feed, "fillSearchTopSiteTerm");

      feed.onAction({ type: at.FILL_SEARCH_TERM });

      assert.calledOnce(feed.fillSearchTopSiteTerm);
    });
    it("should call openTrustedLinkIn with the correct SUMO url on ABOUT_SPONSORED_TOP_SITES", () => {
      const openTrustedLinkIn = sinon.stub();
      const openLinkAction = {
        type: at.ABOUT_SPONSORED_TOP_SITES,
        _target: {
          browser: {
            ownerGlobal: { openTrustedLinkIn },
          },
        },
      };

      feed.onAction(openLinkAction);

      assert.calledOnce(openTrustedLinkIn);
      const [url, where] = openTrustedLinkIn.firstCall.args;
      assert.equal(url.endsWith("sponsor-privacy"), true);
      assert.equal(where, "tab");
    });
    it("should set the URL bar value to the label value", async () => {
      const locationBar = { search: sandbox.stub() };
      const action = {
        type: at.FILL_SEARCH_TERM,
        data: { label: "@Foo" },
        _target: { browser: { ownerGlobal: { gURLBar: locationBar } } },
      };

      await feed.fillSearchTopSiteTerm(action);

      assert.calledOnce(locationBar.search);
      assert.calledWithExactly(locationBar.search, "@Foo", {
        searchEngine: null,
        searchModeEntry: "topsites_newtab",
      });
    });
    it("should call saveToPocket on SAVE_TO_POCKET", () => {
      const action = {
        type: at.SAVE_TO_POCKET,
        data: { site: { url: "raspberry.com", title: "raspberry" } },
        _target: { browser: {} },
      };
      sinon.stub(feed, "saveToPocket");
      feed.onAction(action);
      assert.calledWithExactly(
        feed.saveToPocket,
        action.data.site,
        action._target.browser
      );
    });
    it("should openTrustedLinkIn with sendToPocket if not logged in", () => {
      const openTrustedLinkIn = sinon.stub();
      global.NimbusFeatures.pocketNewtab.getVariable = sandbox
        .stub()
        .returns(true);
      global.pktApi.isUserLoggedIn = sandbox.stub().returns(false);
      global.ExperimentAPI.getExperiment = sandbox.stub().returns({
        slug: "slug",
        branch: { slug: "branch-slug" },
      });
      sandbox
        .stub(global.Services.prefs, "getStringPref")
        .withArgs(POCKET_SITE_PREF)
        .returns("getpocket.com");
      const action = {
        type: at.SAVE_TO_POCKET,
        data: { site: { url: "raspberry.com", title: "raspberry" } },
        _target: {
          browser: {
            ownerGlobal: {
              openTrustedLinkIn,
            },
          },
        },
      };
      feed.onAction(action);
      assert.calledOnce(openTrustedLinkIn);
      const [url, where] = openTrustedLinkIn.firstCall.args;
      assert.equal(
        url,
        "https://getpocket.com/ff_signup?utmSource=firefox_newtab_save_button&utmCampaign=slug&utmContent=branch-slug"
      );
      assert.equal(where, "tab");
    });
    it("should call NewTabUtils.activityStreamLinks.addPocketEntry if we are saving a pocket story", async () => {
      const action = {
        data: { site: { url: "raspberry.com", title: "raspberry" } },
        _target: { browser: {} },
      };
      await feed.saveToPocket(action.data.site, action._target.browser);
      assert.calledOnce(global.NewTabUtils.activityStreamLinks.addPocketEntry);
      assert.calledWithExactly(
        global.NewTabUtils.activityStreamLinks.addPocketEntry,
        action.data.site.url,
        action.data.site.title,
        action._target.browser
      );
    });
    it("should reject the promise if NewTabUtils.activityStreamLinks.addPocketEntry rejects", async () => {
      const e = new Error("Error");
      const action = {
        data: { site: { url: "raspberry.com", title: "raspberry" } },
        _target: { browser: {} },
      };
      global.NewTabUtils.activityStreamLinks.addPocketEntry = sandbox
        .stub()
        .rejects(e);
      await feed.saveToPocket(action.data.site, action._target.browser);
      assert.calledWith(global.Cu.reportError, e);
    });
    it("should broadcast to content if we successfully added a link to Pocket", async () => {
      // test in the form that the API returns data based on: https://getpocket.com/developer/docs/v3/add
      global.NewTabUtils.activityStreamLinks.addPocketEntry = sandbox
        .stub()
        .resolves({ item: { open_url: "pocket.com/itemID", item_id: 1234 } });
      const action = {
        data: { site: { url: "raspberry.com", title: "raspberry" } },
        _target: { browser: {} },
      };
      await feed.saveToPocket(action.data.site, action._target.browser);
      assert.equal(
        feed.store.dispatch.firstCall.args[0].type,
        at.PLACES_SAVED_TO_POCKET
      );
      assert.deepEqual(feed.store.dispatch.firstCall.args[0].data, {
        url: "raspberry.com",
        title: "raspberry",
        pocket_id: 1234,
        open_url: "pocket.com/itemID",
      });
    });
    it("should only broadcast if we got some data back from addPocketEntry", async () => {
      global.NewTabUtils.activityStreamLinks.addPocketEntry = sandbox
        .stub()
        .resolves(null);
      const action = {
        data: { site: { url: "raspberry.com", title: "raspberry" } },
        _target: { browser: {} },
      };
      await feed.saveToPocket(action.data.site, action._target.browser);
      assert.notCalled(feed.store.dispatch);
    });
    it("should call deleteFromPocket on DELETE_FROM_POCKET", () => {
      sandbox.stub(feed, "deleteFromPocket");
      feed.onAction({
        type: at.DELETE_FROM_POCKET,
        data: { pocket_id: 12345 },
      });

      assert.calledOnce(feed.deleteFromPocket);
      assert.calledWithExactly(feed.deleteFromPocket, 12345);
    });
    it("should catch if deletePocketEntry throws", async () => {
      const e = new Error("Error");
      global.NewTabUtils.activityStreamLinks.deletePocketEntry = sandbox
        .stub()
        .rejects(e);
      await feed.deleteFromPocket(12345);

      assert.calledWith(global.Cu.reportError, e);
    });
    it("should call NewTabUtils.deletePocketEntry and dispatch POCKET_LINK_DELETED_OR_ARCHIVED when deleting from Pocket", async () => {
      await feed.deleteFromPocket(12345);

      assert.calledOnce(
        global.NewTabUtils.activityStreamLinks.deletePocketEntry
      );
      assert.calledWith(
        global.NewTabUtils.activityStreamLinks.deletePocketEntry,
        12345
      );

      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        type: at.POCKET_LINK_DELETED_OR_ARCHIVED,
      });
    });
    it("should call archiveFromPocket on ARCHIVE_FROM_POCKET", async () => {
      sandbox.stub(feed, "archiveFromPocket");
      await feed.onAction({
        type: at.ARCHIVE_FROM_POCKET,
        data: { pocket_id: 12345 },
      });

      assert.calledOnce(feed.archiveFromPocket);
      assert.calledWithExactly(feed.archiveFromPocket, 12345);
    });
    it("should catch if archiveFromPocket throws", async () => {
      const e = new Error("Error");
      global.NewTabUtils.activityStreamLinks.archivePocketEntry = sandbox
        .stub()
        .rejects(e);
      await feed.archiveFromPocket(12345);

      assert.calledWith(global.Cu.reportError, e);
    });
    it("should call NewTabUtils.archivePocketEntry and dispatch POCKET_LINK_DELETED_OR_ARCHIVED when archiving from Pocket", async () => {
      await feed.archiveFromPocket(12345);

      assert.calledOnce(
        global.NewTabUtils.activityStreamLinks.archivePocketEntry
      );
      assert.calledWith(
        global.NewTabUtils.activityStreamLinks.archivePocketEntry,
        12345
      );

      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        type: at.POCKET_LINK_DELETED_OR_ARCHIVED,
      });
    });
    it("should call handoffSearchToAwesomebar on HANDOFF_SEARCH_TO_AWESOMEBAR", () => {
      const action = {
        type: at.HANDOFF_SEARCH_TO_AWESOMEBAR,
        data: { text: "f" },
        meta: { fromTarget: {} },
        _target: { browser: { ownerGlobal: { gURLBar: { focus: () => {} } } } },
      };
      sinon.stub(feed, "handoffSearchToAwesomebar");
      feed.onAction(action);
      assert.calledWith(feed.handoffSearchToAwesomebar, action);
    });
    it("should call makeAttributionRequest on PARTNER_LINK_ATTRIBUTION", () => {
      sinon.stub(feed, "makeAttributionRequest");
      let data = { targetURL: "https://partnersite.com", source: "topsites" };
      feed.onAction({
        type: at.PARTNER_LINK_ATTRIBUTION,
        data,
      });

      assert.calledOnce(feed.makeAttributionRequest);
      assert.calledWithExactly(feed.makeAttributionRequest, data);
    });
    it("should call PartnerLinkAttribution.makeRequest when calling makeAttributionRequest", () => {
      let data = { targetURL: "https://partnersite.com", source: "topsites" };
      feed.makeAttributionRequest(data);
      assert.calledOnce(global.PartnerLinkAttribution.makeRequest);
    });
  });

  describe("handoffSearchToAwesomebar", () => {
    let fakeUrlBar;
    let listeners;

    beforeEach(() => {
      fakeUrlBar = {
        focus: sinon.spy(),
        handoff: sinon.spy(),
        setHiddenFocus: sinon.spy(),
        removeHiddenFocus: sinon.spy(),
        addEventListener: (ev, cb) => {
          listeners[ev] = cb;
        },
        removeEventListener: sinon.spy(),
      };
      listeners = {};
    });
    it("should properly handle handoff with no text passed in", () => {
      feed.handoffSearchToAwesomebar({
        _target: { browser: { ownerGlobal: { gURLBar: fakeUrlBar } } },
        data: {},
        meta: { fromTarget: {} },
      });
      assert.calledOnce(fakeUrlBar.setHiddenFocus);
      assert.notCalled(fakeUrlBar.handoff);
      assert.notCalled(feed.store.dispatch);

      // Now type a character.
      listeners.keydown({ key: "f" });
      assert.calledOnce(fakeUrlBar.handoff);
      assert.calledOnce(fakeUrlBar.removeHiddenFocus);
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        meta: {
          from: "ActivityStream:Main",
          skipMain: true,
          to: "ActivityStream:Content",
          toTarget: {},
        },
        type: "DISABLE_SEARCH",
      });
    });
    it("should properly handle handoff with text data passed in", () => {
      feed.handoffSearchToAwesomebar({
        _target: { browser: { ownerGlobal: { gURLBar: fakeUrlBar } } },
        data: { text: "foo" },
        meta: { fromTarget: {} },
      });
      assert.calledOnce(fakeUrlBar.handoff);
      assert.calledWith(
        fakeUrlBar.handoff,
        "foo",
        global.Services.search.defaultEngine
      );
      assert.notCalled(fakeUrlBar.focus);
      assert.notCalled(fakeUrlBar.setHiddenFocus);

      // Now call blur listener.
      listeners.blur();
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        meta: {
          from: "ActivityStream:Main",
          skipMain: true,
          to: "ActivityStream:Content",
          toTarget: {},
        },
        type: "SHOW_SEARCH",
      });
    });
    it("should properly handle handoff with text data passed in, in private browsing mode", () => {
      global.PrivateBrowsingUtils.isBrowserPrivate = () => true;
      feed.handoffSearchToAwesomebar({
        _target: { browser: { ownerGlobal: { gURLBar: fakeUrlBar } } },
        data: { text: "foo" },
        meta: { fromTarget: {} },
      });
      assert.calledOnce(fakeUrlBar.handoff);
      assert.calledWith(
        fakeUrlBar.handoff,
        "foo",
        global.Services.search.defaultPrivateEngine
      );
      assert.notCalled(fakeUrlBar.focus);
      assert.notCalled(fakeUrlBar.setHiddenFocus);

      // Now call blur listener.
      listeners.blur();
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        meta: {
          from: "ActivityStream:Main",
          skipMain: true,
          to: "ActivityStream:Content",
          toTarget: {},
        },
        type: "SHOW_SEARCH",
      });
      global.PrivateBrowsingUtils.isBrowserPrivate = () => false;
    });
    it("should SHOW_SEARCH on ESC keydown", () => {
      feed.handoffSearchToAwesomebar({
        _target: { browser: { ownerGlobal: { gURLBar: fakeUrlBar } } },
        data: { text: "foo" },
        meta: { fromTarget: {} },
      });
      assert.calledOnce(fakeUrlBar.handoff);
      assert.calledWithExactly(
        fakeUrlBar.handoff,
        "foo",
        global.Services.search.defaultEngine
      );
      assert.notCalled(fakeUrlBar.focus);

      // Now call ESC keydown.
      listeners.keydown({ key: "Escape" });
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        meta: {
          from: "ActivityStream:Main",
          skipMain: true,
          to: "ActivityStream:Content",
          toTarget: {},
        },
        type: "SHOW_SEARCH",
      });
    });
  });

  describe("#observe", () => {
    it("should dispatch a PLACES_LINK_BLOCKED action with the url of the blocked link", () => {
      feed.observe(null, BLOCKED_EVENT, "foo123.com");
      assert.equal(
        feed.store.dispatch.firstCall.args[0].type,
        at.PLACES_LINK_BLOCKED
      );
      assert.deepEqual(feed.store.dispatch.firstCall.args[0].data, {
        url: "foo123.com",
      });
    });
    it("should not call dispatch if the topic is something other than BLOCKED_EVENT", () => {
      feed.observe(null, "someotherevent");
      assert.notCalled(feed.store.dispatch);
    });
  });

  describe("Custom dispatch", () => {
    it("should only dispatch 1 PLACES_LINKS_CHANGED action if many bookmark-added notifications happened at once", async () => {
      // Yes, onItemAdded has at least 8 arguments. See function definition for docs.
      const args = [
        {
          itemType: TYPE_BOOKMARK,
          source: SOURCES.DEFAULT,
          dateAdded: FAKE_BOOKMARK.dateAdded,
          guid: FAKE_BOOKMARK.bookmarkGuid,
          title: FAKE_BOOKMARK.bookmarkTitle,
          url: "https://www.foo.com",
          isTagging: false,
          type: "bookmark-added",
        },
      ];
      await feed.placesObserver.handlePlacesEvent(args);
      await feed.placesObserver.handlePlacesEvent(args);
      await feed.placesObserver.handlePlacesEvent(args);
      await feed.placesObserver.handlePlacesEvent(args);
      assert.calledOnce(
        feed.store.dispatch.withArgs(
          ac.OnlyToMain({ type: at.PLACES_LINKS_CHANGED })
        )
      );
    });
    it("should only dispatch 1 PLACES_LINKS_CHANGED action if many onItemRemoved notifications happened at once", async () => {
      const args = [
        {
          id: null,
          parentId: null,
          index: null,
          itemType: TYPE_BOOKMARK,
          url: "foo.com",
          guid: "123foo",
          parentGuid: "",
          source: SOURCES.DEFAULT,
          type: "bookmark-removed",
        },
      ];
      await feed.placesObserver.handlePlacesEvent(args);
      await feed.placesObserver.handlePlacesEvent(args);
      await feed.placesObserver.handlePlacesEvent(args);
      await feed.placesObserver.handlePlacesEvent(args);

      assert.calledOnce(
        feed.store.dispatch.withArgs(
          ac.OnlyToMain({ type: at.PLACES_LINKS_CHANGED })
        )
      );
    });
    it("should only dispatch 1 PLACES_LINKS_CHANGED action if any page-removed notifications happened at once", async () => {
      await feed.placesObserver.handlePlacesEvent([
        { type: "page-removed", url: "foo.com", isRemovedFromStore: true },
      ]);
      await feed.placesObserver.handlePlacesEvent([
        { type: "page-removed", url: "foo1.com", isRemovedFromStore: true },
      ]);
      await feed.placesObserver.handlePlacesEvent([
        { type: "page-removed", url: "foo2.com", isRemovedFromStore: true },
      ]);

      assert.calledOnce(
        feed.store.dispatch.withArgs(
          ac.OnlyToMain({ type: at.PLACES_LINKS_CHANGED })
        )
      );
    });
  });

  describe("PlacesObserver", () => {
    let dispatch;
    let observer;
    beforeEach(() => {
      dispatch = sandbox.spy();
      observer = new PlacesObserver(dispatch);
    });

    describe("#history-cleared", () => {
      it("should dispatch a PLACES_HISTORY_CLEARED action", async () => {
        const args = [{ type: "history-cleared" }];
        await observer.handlePlacesEvent(args);
        assert.calledWith(dispatch, { type: at.PLACES_HISTORY_CLEARED });
      });
    });

    describe("#page-removed", () => {
      it("should dispatch a PLACES_LINKS_DELETED action with the right url", async () => {
        const args = [
          {
            type: "page-removed",
            url: "foo.com",
            isRemovedFromStore: true,
          },
        ];
        await observer.handlePlacesEvent(args);
        assert.calledWith(dispatch, {
          type: at.PLACES_LINKS_DELETED,
          data: { urls: ["foo.com"] },
        });
      });
    });

    describe("#bookmark-added", () => {
      it("should dispatch a PLACES_BOOKMARK_ADDED action with the bookmark data - http", async () => {
        const args = [
          {
            itemType: TYPE_BOOKMARK,
            source: SOURCES.DEFAULT,
            dateAdded: FAKE_BOOKMARK.dateAdded,
            guid: FAKE_BOOKMARK.bookmarkGuid,
            title: FAKE_BOOKMARK.bookmarkTitle,
            url: "http://www.foo.com",
            isTagging: false,
            type: "bookmark-added",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.calledWith(dispatch.secondCall, {
          type: at.PLACES_BOOKMARK_ADDED,
          data: {
            bookmarkGuid: FAKE_BOOKMARK.bookmarkGuid,
            bookmarkTitle: FAKE_BOOKMARK.bookmarkTitle,
            dateAdded: FAKE_BOOKMARK.dateAdded * 1000,
            url: "http://www.foo.com",
          },
        });
      });
      it("should dispatch a PLACES_BOOKMARK_ADDED action with the bookmark data - https", async () => {
        const args = [
          {
            itemType: TYPE_BOOKMARK,
            source: SOURCES.DEFAULT,
            dateAdded: FAKE_BOOKMARK.dateAdded,
            guid: FAKE_BOOKMARK.bookmarkGuid,
            title: FAKE_BOOKMARK.bookmarkTitle,
            url: "https://www.foo.com",
            isTagging: false,
            type: "bookmark-added",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.calledWith(dispatch.secondCall, {
          type: at.PLACES_BOOKMARK_ADDED,
          data: {
            bookmarkGuid: FAKE_BOOKMARK.bookmarkGuid,
            bookmarkTitle: FAKE_BOOKMARK.bookmarkTitle,
            dateAdded: FAKE_BOOKMARK.dateAdded * 1000,
            url: "https://www.foo.com",
          },
        });
      });
      it("should not dispatch a PLACES_BOOKMARK_ADDED action - not http/https", async () => {
        const args = [
          {
            itemType: TYPE_BOOKMARK,
            source: SOURCES.DEFAULT,
            dateAdded: FAKE_BOOKMARK.dateAdded,
            guid: FAKE_BOOKMARK.bookmarkGuid,
            title: FAKE_BOOKMARK.bookmarkTitle,
            url: "foo.com",
            isTagging: false,
            type: "bookmark-added",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_ADDED action - has IMPORT source", async () => {
        const args = [
          {
            itemType: TYPE_BOOKMARK,
            source: SOURCES.IMPORT,
            dateAdded: FAKE_BOOKMARK.dateAdded,
            guid: FAKE_BOOKMARK.bookmarkGuid,
            title: FAKE_BOOKMARK.bookmarkTitle,
            url: "foo.com",
            isTagging: false,
            type: "bookmark-added",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_ADDED action - has RESTORE source", async () => {
        const args = [
          {
            itemType: TYPE_BOOKMARK,
            source: SOURCES.RESTORE,
            dateAdded: FAKE_BOOKMARK.dateAdded,
            guid: FAKE_BOOKMARK.bookmarkGuid,
            title: FAKE_BOOKMARK.bookmarkTitle,
            url: "foo.com",
            isTagging: false,
            type: "bookmark-added",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_ADDED action - has RESTORE_ON_STARTUP source", async () => {
        const args = [
          {
            itemType: TYPE_BOOKMARK,
            source: SOURCES.RESTORE_ON_STARTUP,
            dateAdded: FAKE_BOOKMARK.dateAdded,
            guid: FAKE_BOOKMARK.bookmarkGuid,
            title: FAKE_BOOKMARK.bookmarkTitle,
            url: "foo.com",
            isTagging: false,
            type: "bookmark-added",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_ADDED action - has SYNC source", async () => {
        const args = [
          {
            itemType: TYPE_BOOKMARK,
            source: SOURCES.SYNC,
            dateAdded: FAKE_BOOKMARK.dateAdded,
            guid: FAKE_BOOKMARK.bookmarkGuid,
            title: FAKE_BOOKMARK.bookmarkTitle,
            url: "foo.com",
            isTagging: false,
            type: "bookmark-added",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should ignore events that are not of TYPE_BOOKMARK", async () => {
        const args = [
          {
            itemType: "nottypebookmark",
            source: SOURCES.DEFAULT,
            dateAdded: FAKE_BOOKMARK.dateAdded,
            guid: FAKE_BOOKMARK.bookmarkGuid,
            title: FAKE_BOOKMARK.bookmarkTitle,
            url: "https://www.foo.com",
            isTagging: false,
            type: "bookmark-added",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
    });
    describe("#bookmark-removed", () => {
      it("should ignore events that are not of TYPE_BOOKMARK", async () => {
        const args = [
          {
            id: null,
            parentId: null,
            index: null,
            itemType: "nottypebookmark",
            url: null,
            guid: "123foo",
            parentGuid: "",
            source: SOURCES.DEFAULT,
            type: "bookmark-removed",
          },
        ];
        await observer.handlePlacesEvent(args);
        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARKS_REMOVED action - has SYNC source", async () => {
        const args = [
          {
            id: null,
            parentId: null,
            index: null,
            itemType: TYPE_BOOKMARK,
            url: "foo.com",
            guid: "123foo",
            parentGuid: "",
            source: SOURCES.SYNC,
            type: "bookmark-removed",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARKS_REMOVED action - has IMPORT source", async () => {
        const args = [
          {
            id: null,
            parentId: null,
            index: null,
            itemType: TYPE_BOOKMARK,
            url: "foo.com",
            guid: "123foo",
            parentGuid: "",
            source: SOURCES.IMPORT,
            type: "bookmark-removed",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARKS_REMOVED action - has RESTORE source", async () => {
        const args = [
          {
            id: null,
            parentId: null,
            index: null,
            itemType: TYPE_BOOKMARK,
            url: "foo.com",
            guid: "123foo",
            parentGuid: "",
            source: SOURCES.RESTORE,
            type: "bookmark-removed",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARKS_REMOVED action - has RESTORE_ON_STARTUP source", async () => {
        const args = [
          {
            id: null,
            parentId: null,
            index: null,
            itemType: TYPE_BOOKMARK,
            url: "foo.com",
            guid: "123foo",
            parentGuid: "",
            source: SOURCES.RESTORE_ON_STARTUP,
            type: "bookmark-removed",
          },
        ];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should dispatch a PLACES_BOOKMARKS_REMOVED action with the right URL and bookmarkGuid", async () => {
        const args = [
          {
            id: null,
            parentId: null,
            index: null,
            itemType: TYPE_BOOKMARK,
            url: "foo.com",
            guid: "123foo",
            parentGuid: "",
            source: SOURCES.DEFAULT,
            type: "bookmark-removed",
          },
        ];
        await observer.handlePlacesEvent(args);
        assert.calledWith(dispatch, {
          type: at.PLACES_BOOKMARKS_REMOVED,
          data: { urls: ["foo.com"] },
        });
      });
    });
  });

  describe("BookmarksObserver", () => {
    let dispatch;
    let observer;
    beforeEach(() => {
      dispatch = sandbox.spy();
      observer = new BookmarksObserver(dispatch);
    });
    it("should have a QueryInterface property", () => {
      assert.property(observer, "QueryInterface");
    });
    describe("Other empty methods (to keep code coverage happy)", () => {
      it("should have a various empty functions for xpconnect happiness", () => {
        observer.onItemChanged();
      });
    });
  });
});

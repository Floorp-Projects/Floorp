import {actionTypes as at} from "common/Actions.jsm";
import {GlobalOverrider} from "test/unit/utils";
import {PlacesFeed} from "lib/PlacesFeed.jsm";
const {HistoryObserver, BookmarksObserver} = PlacesFeed;

const FAKE_BOOKMARK = {bookmarkGuid: "xi31", bookmarkTitle: "Foo", dateAdded: 123214232, url: "foo.com"};
const TYPE_BOOKMARK = 0; // This is fake, for testing
const SOURCES = {
  DEFAULT: 0,
  IMPORT_REPLACE: 3
};

const BLOCKED_EVENT = "newtab-linkBlocked"; // The event dispatched in NewTabUtils when a link is blocked;

describe("PlacesFeed", () => {
  let globals;
  let sandbox;
  let feed;
  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    globals.set("NewTabUtils", {
      activityStreamProvider: {getBookmark() {}},
      activityStreamLinks: {
        addBookmark: sandbox.spy(),
        deleteBookmark: sandbox.spy(),
        deleteHistoryEntry: sandbox.spy(),
        blockURL: sandbox.spy()
      }
    });
    globals.set("PlacesUtils", {
      history: {
        addObserver: sandbox.spy(),
        removeObserver: sandbox.spy(),
        insert: sandbox.stub()
      },
      bookmarks: {
        TYPE_BOOKMARK,
        addObserver: sandbox.spy(),
        removeObserver: sandbox.spy(),
        SOURCES
      }
    });
    globals.set("Pocket", {savePage: sandbox.spy()});
    global.Components.classes["@mozilla.org/browser/nav-history-service;1"] = {
      getService() {
        return global.PlacesUtils.history;
      }
    };
    global.Components.classes["@mozilla.org/browser/nav-bookmarks-service;1"] = {
      getService() {
        return global.PlacesUtils.bookmarks;
      }
    };
    sandbox.spy(global.Services.obs, "addObserver");
    sandbox.spy(global.Services.obs, "removeObserver");
    sandbox.spy(global.Components.utils, "reportError");

    feed = new PlacesFeed();
    feed.store = {dispatch: sinon.spy()};
  });
  afterEach(() => globals.restore());

  it("should have a HistoryObserver that dispatches to the store", () => {
    assert.instanceOf(feed.historyObserver, HistoryObserver);
    const action = {type: "FOO"};

    feed.historyObserver.dispatch(action);

    assert.calledOnce(feed.store.dispatch);
    assert.equal(feed.store.dispatch.firstCall.args[0].type, action.type);
  });

  it("should have a BookmarksObserver that dispatch to the store", () => {
    assert.instanceOf(feed.bookmarksObserver, BookmarksObserver);
    const action = {type: "FOO"};

    feed.bookmarksObserver.dispatch(action);

    assert.calledOnce(feed.store.dispatch);
    assert.equal(feed.store.dispatch.firstCall.args[0].type, action.type);
  });
  describe("#onAction", () => {
    it("should add bookmark, history, blocked observers on INIT", () => {
      feed.onAction({type: at.INIT});

      assert.calledWith(global.PlacesUtils.history.addObserver, feed.historyObserver, true);
      assert.calledWith(global.PlacesUtils.bookmarks.addObserver, feed.bookmarksObserver, true);
      assert.calledWith(global.Services.obs.addObserver, feed, BLOCKED_EVENT);
    });
    it("should remove bookmark, history, blocked observers on UNINIT", () => {
      feed.onAction({type: at.UNINIT});

      assert.calledWith(global.PlacesUtils.history.removeObserver, feed.historyObserver);
      assert.calledWith(global.PlacesUtils.bookmarks.removeObserver, feed.bookmarksObserver);
      assert.calledWith(global.Services.obs.removeObserver, feed, BLOCKED_EVENT);
    });
    it("should block a url on BLOCK_URL", () => {
      feed.onAction({type: at.BLOCK_URL, data: "apple.com"});
      assert.calledWith(global.NewTabUtils.activityStreamLinks.blockURL, {url: "apple.com"});
    });
    it("should bookmark a url on BOOKMARK_URL", () => {
      const data = {url: "pear.com", title: "A pear"};
      const _target = {browser: {ownerGlobal() {}}};
      feed.onAction({type: at.BOOKMARK_URL, data, _target});
      assert.calledWith(global.NewTabUtils.activityStreamLinks.addBookmark, data, _target.browser);
    });
    it("should delete a bookmark on DELETE_BOOKMARK_BY_ID", () => {
      feed.onAction({type: at.DELETE_BOOKMARK_BY_ID, data: "g123kd"});
      assert.calledWith(global.NewTabUtils.activityStreamLinks.deleteBookmark, "g123kd");
    });
    it("should delete a history entry on DELETE_HISTORY_URL", () => {
      feed.onAction({type: at.DELETE_HISTORY_URL, data: {url: "guava.com", forceBlock: null}});
      assert.calledWith(global.NewTabUtils.activityStreamLinks.deleteHistoryEntry, "guava.com");
      assert.notCalled(global.NewTabUtils.activityStreamLinks.blockURL);
    });
    it("should delete a history entry on DELETE_HISTORY_URL and force a site to be blocked if specified", () => {
      feed.onAction({type: at.DELETE_HISTORY_URL, data: {url: "guava.com", forceBlock: "g123kd"}});
      assert.calledWith(global.NewTabUtils.activityStreamLinks.deleteHistoryEntry, "guava.com");
      assert.calledWith(global.NewTabUtils.activityStreamLinks.blockURL, {url: "guava.com"});
    });
    it("should call openNewWindow with the correct url on OPEN_NEW_WINDOW", () => {
      sinon.stub(feed, "openNewWindow");
      const openWindowAction = {type: at.OPEN_NEW_WINDOW, data: {url: "foo.com"}};
      feed.onAction(openWindowAction);
      assert.calledWith(feed.openNewWindow, openWindowAction);
    });
    it("should call openNewWindow with the correct url and privacy args on OPEN_PRIVATE_WINDOW", () => {
      sinon.stub(feed, "openNewWindow");
      const openWindowAction = {type: at.OPEN_PRIVATE_WINDOW, data: {url: "foo.com"}};
      feed.onAction(openWindowAction);
      assert.calledWith(feed.openNewWindow, openWindowAction, true);
    });
    it("should call openNewWindow with the correct url on OPEN_NEW_WINDOW", () => {
      const openWindowAction = {
        type: at.OPEN_NEW_WINDOW,
        data: {url: "foo.com"},
        _target: {browser: {ownerGlobal: {openLinkIn: () => {}}}}
      };
      sinon.stub(openWindowAction._target.browser.ownerGlobal, "openLinkIn");
      feed.onAction(openWindowAction);
      assert.calledOnce(openWindowAction._target.browser.ownerGlobal.openLinkIn);
    });
    it("should open link on OPEN_LINK", () => {
      sinon.stub(feed, "openNewWindow");
      const openLinkAction = {
        type: at.OPEN_LINK,
        data: {url: "foo.com", event: {where: "current"}},
        _target: {browser: {ownerGlobal: {openLinkIn: sinon.spy(), whereToOpenLink: e => e.where}}}
      };
      feed.onAction(openLinkAction);
      assert.calledWith(openLinkAction._target.browser.ownerGlobal.openLinkIn, openLinkAction.data.url, "current");
    });
    it("should open link with referrer on OPEN_LINK", () => {
      globals.set("Services", {io: {newURI: url => `URI:${url}`}});
      sinon.stub(feed, "openNewWindow");
      const openLinkAction = {
        type: at.OPEN_LINK,
        data: {url: "foo.com", referrer: "foo.com/ref", event: {where: "tab"}},
        _target: {browser: {ownerGlobal: {openLinkIn: sinon.spy(), whereToOpenLink: e => e.where}}}
      };
      feed.onAction(openLinkAction);
      assert.calledWith(openLinkAction._target.browser.ownerGlobal.openLinkIn, openLinkAction.data.url, "tab", {referrerURI: `URI:${openLinkAction.data.referrer}`});
    });
    it("should save to Pocket on SAVE_TO_POCKET", () => {
      feed.onAction({type: at.SAVE_TO_POCKET, data: {site: {url: "raspberry.com", title: "raspberry"}}, _target: {browser: {}}});
      assert.calledWith(global.Pocket.savePage, {}, "raspberry.com", "raspberry");
    });
  });

  describe("#observe", () => {
    it("should dispatch a PLACES_LINK_BLOCKED action with the url of the blocked link", () => {
      feed.observe(null, BLOCKED_EVENT, "foo123.com");
      assert.equal(feed.store.dispatch.firstCall.args[0].type, at.PLACES_LINK_BLOCKED);
      assert.deepEqual(feed.store.dispatch.firstCall.args[0].data, {url: "foo123.com"});
    });
    it("should not call dispatch if the topic is something other than BLOCKED_EVENT", () => {
      feed.observe(null, "someotherevent");
      assert.notCalled(feed.store.dispatch);
    });
  });

  describe("HistoryObserver", () => {
    let dispatch;
    let observer;
    beforeEach(() => {
      dispatch = sandbox.spy();
      observer = new HistoryObserver(dispatch);
    });
    it("should have a QueryInterface property", () => {
      assert.property(observer, "QueryInterface");
    });
    describe("#onDeleteURI", () => {
      it("should dispatch a PLACES_LINKS_DELETED action with the right url", async () => {
        await observer.onDeleteURI({spec: "foo.com"});

        assert.calledWith(dispatch, {type: at.PLACES_LINKS_DELETED, data: ["foo.com"]});
      });
      it("should dispatch a PLACES_LINKS_DELETED action with multiple urls", async () => {
        const promise = observer.onDeleteURI({spec: "bar.com"});
        observer.onDeleteURI({spec: "foo.com"});
        await promise;

        const result = dispatch.firstCall.args[0].data;
        assert.lengthOf(result, 2);
        assert.equal(result[0], "bar.com");
        assert.equal(result[1], "foo.com");
      });
    });
    describe("#onClearHistory", () => {
      it("should dispatch a PLACES_HISTORY_CLEARED action", () => {
        observer.onClearHistory();
        assert.calledWith(dispatch, {type: at.PLACES_HISTORY_CLEARED});
      });
    });
    describe("Other empty methods (to keep code coverage happy)", () => {
      it("should have a various empty functions for xpconnect happiness", () => {
        observer.onBeginUpdateBatch();
        observer.onEndUpdateBatch();
        observer.onVisits();
        observer.onTitleChanged();
        observer.onFrecencyChanged();
        observer.onManyFrecenciesChanged();
        observer.onPageChanged();
        observer.onDeleteVisits();
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
    describe("#onItemAdded", () => {
      beforeEach(() => {
      });
      it("should dispatch a PLACES_BOOKMARK_ADDED action with the bookmark data - http", async () => {
        // Yes, onItemAdded has at least 8 arguments. See function definition for docs.
        const args = [null, null, null, TYPE_BOOKMARK,
          {spec: FAKE_BOOKMARK.url, scheme: "http"}, FAKE_BOOKMARK.bookmarkTitle,
          FAKE_BOOKMARK.dateAdded,
          FAKE_BOOKMARK.bookmarkGuid,
          "",
          SOURCES.DEFAULT
        ];
        await observer.onItemAdded(...args);

        assert.calledWith(dispatch, {type: at.PLACES_BOOKMARK_ADDED, data: FAKE_BOOKMARK});
      });
      it("should dispatch a PLACES_BOOKMARK_ADDED action with the bookmark data - https", async () => {
        // Yes, onItemAdded has at least 8 arguments. See function definition for docs.
        const args = [null, null, null, TYPE_BOOKMARK,
          {spec: FAKE_BOOKMARK.url, scheme: "https"}, FAKE_BOOKMARK.bookmarkTitle,
          FAKE_BOOKMARK.dateAdded,
          FAKE_BOOKMARK.bookmarkGuid,
          "",
          SOURCES.DEFAULT
        ];
        await observer.onItemAdded(...args);

        assert.calledWith(dispatch, {type: at.PLACES_BOOKMARK_ADDED, data: FAKE_BOOKMARK});
      });
      it("should not dispatch a PLACES_BOOKMARK_ADDED action - not http/https", async () => {
        // Yes, onItemAdded has at least 8 arguments. See function definition for docs.
        const args = [null, null, null, TYPE_BOOKMARK,
          {spec: FAKE_BOOKMARK.url, scheme: "places"}, FAKE_BOOKMARK.bookmarkTitle,
          FAKE_BOOKMARK.dateAdded,
          FAKE_BOOKMARK.bookmarkGuid,
          "",
          SOURCES.DEFAULT
        ];
        await observer.onItemAdded(...args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_ADDED action - has IMPORT_REPLACE source", async () => {
        // Yes, onItemAdded has at least 8 arguments. See function definition for docs.
        const args = [null, null, null, TYPE_BOOKMARK,
          {spec: FAKE_BOOKMARK.url, scheme: "http"}, FAKE_BOOKMARK.bookmarkTitle,
          FAKE_BOOKMARK.dateAdded,
          FAKE_BOOKMARK.bookmarkGuid,
          "",
          SOURCES.IMPORT_REPLACE
        ];
        await observer.onItemAdded(...args);

        assert.notCalled(dispatch);
      });
      it("should ignore events that are not of TYPE_BOOKMARK", async () => {
        const args = [null, null, null, "nottypebookmark"];
        await observer.onItemAdded(...args);
        assert.notCalled(dispatch);
      });
    });
    describe("#onItemRemoved", () => {
      it("should ignore events that are not of TYPE_BOOKMARK", async () => {
        await observer.onItemRemoved(null, null, null, "nottypebookmark", null, "123foo");
        assert.notCalled(dispatch);
      });
      it("should dispatch a PLACES_BOOKMARK_REMOVED action with the right URL and bookmarkGuid", () => {
        observer.onItemRemoved(null, null, null, TYPE_BOOKMARK, {spec: "foo.com"}, "123foo");
        assert.calledWith(dispatch, {type: at.PLACES_BOOKMARK_REMOVED, data: {bookmarkGuid: "123foo", url: "foo.com"}});
      });
    });
    describe("#onItemChanged", () => {
      beforeEach(() => {
        sandbox.stub(global.NewTabUtils.activityStreamProvider, "getBookmark")
          .withArgs(FAKE_BOOKMARK.guid).returns(Promise.resolve(FAKE_BOOKMARK));
      });
      it("has an empty function to keep xpconnect happy", async () => {
        await observer.onItemChanged();
      });
      // Disabled in Issue 3203, see observer.onItemChanged for more information.
      it.skip("should dispatch a PLACES_BOOKMARK_CHANGED action with the bookmark data", async () => {
        const args = [null, "title", null, null, null, TYPE_BOOKMARK, null, FAKE_BOOKMARK.guid];
        await observer.onItemChanged(...args);

        assert.calledWith(dispatch, {type: at.PLACES_BOOKMARK_CHANGED, data: FAKE_BOOKMARK});
      });
      it.skip("should catch errors gracefully", async () => {
        const e = new Error("test error");
        global.NewTabUtils.activityStreamProvider.getBookmark.restore();
        sandbox.stub(global.NewTabUtils.activityStreamProvider, "getBookmark")
          .returns(Promise.reject(e));

        const args = [null, "title", null, null, null, TYPE_BOOKMARK, null, FAKE_BOOKMARK.guid];
        await observer.onItemChanged(...args);

        assert.calledWith(global.Components.utils.reportError, e);
      });
      it.skip("should ignore events that are not of TYPE_BOOKMARK", async () => {
        await observer.onItemChanged(null, "title", null, null, null, "nottypebookmark");
        assert.notCalled(dispatch);
      });
      it.skip("should ignore events that are not changes to uri/title", async () => {
        await observer.onItemChanged(null, "tags", null, null, null, TYPE_BOOKMARK);
        assert.notCalled(dispatch);
      });
    });
    describe("Other empty methods (to keep code coverage happy)", () => {
      it("should have a various empty functions for xpconnect happiness", () => {
        observer.onBeginUpdateBatch();
        observer.onEndUpdateBatch();
        observer.onItemVisited();
        observer.onItemMoved();
      });
    });
  });
});

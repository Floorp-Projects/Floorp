const {PlacesFeed} = require("lib/PlacesFeed.jsm");
const {HistoryObserver, BookmarksObserver} = PlacesFeed;
const {GlobalOverrider} = require("test/unit/utils");
const {actionTypes: at} = require("common/Actions.jsm");

const FAKE_BOOKMARK = {bookmarkGuid: "xi31", bookmarkTitle: "Foo", lastModified: 123214232, url: "foo.com"};
const TYPE_BOOKMARK = 0; // This is fake, for testing

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
      history: {addObserver: sandbox.spy(), removeObserver: sandbox.spy()},
      bookmarks: {TYPE_BOOKMARK, addObserver: sandbox.spy(), removeObserver: sandbox.spy()}
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
      feed.onAction({type: at.BOOKMARK_URL, data: "pear.com"});
      assert.calledWith(global.NewTabUtils.activityStreamLinks.addBookmark, "pear.com");
    });
    it("should delete a bookmark on DELETE_BOOKMARK_BY_ID", () => {
      feed.onAction({type: at.DELETE_BOOKMARK_BY_ID, data: "g123kd"});
      assert.calledWith(global.NewTabUtils.activityStreamLinks.deleteBookmark, "g123kd");
    });
    it("should delete a history entry on DELETE_HISTORY_URL", () => {
      feed.onAction({type: at.DELETE_HISTORY_URL, data: "guava.com"});
      assert.calledWith(global.NewTabUtils.activityStreamLinks.deleteHistoryEntry, "guava.com");
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
      it("should dispatch a PLACES_LINK_DELETED action with the right url", () => {
        observer.onDeleteURI({spec: "foo.com"});
        assert.calledWith(dispatch, {type: at.PLACES_LINK_DELETED, data: {url: "foo.com"}});
      });
    });
    describe("#onClearHistory", () => {
      it("should dispatch a PLACES_HISTORY_CLEARED action", () => {
        observer.onClearHistory();
        assert.calledWith(dispatch, {type: at.PLACES_HISTORY_CLEARED});
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
        // Make sure getBookmark returns our fake bookmark if it is called with the expected guid
        sandbox.stub(global.NewTabUtils.activityStreamProvider, "getBookmark")
          .withArgs(FAKE_BOOKMARK.guid).returns(Promise.resolve(FAKE_BOOKMARK));
      });
      it("should dispatch a PLACES_BOOKMARK_ADDED action with the bookmark data", async () => {
        // Yes, onItemAdded has at least 8 arguments. See function definition for docs.
        const args = [null, null, null, TYPE_BOOKMARK, null, null, null, FAKE_BOOKMARK.guid];
        await observer.onItemAdded(...args);

        assert.calledWith(dispatch, {type: at.PLACES_BOOKMARK_ADDED, data: FAKE_BOOKMARK});
      });
      it("should catch errors gracefully", async () => {
        const e = new Error("test error");
        global.NewTabUtils.activityStreamProvider.getBookmark.restore();
        sandbox.stub(global.NewTabUtils.activityStreamProvider, "getBookmark")
          .returns(Promise.reject(e));

        const args = [null, null, null, TYPE_BOOKMARK, null, null, null, FAKE_BOOKMARK.guid];
        await observer.onItemAdded(...args);

        assert.calledWith(global.Components.utils.reportError, e);
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
      it("should dispatch a PLACES_BOOKMARK_CHANGED action with the bookmark data", async () => {
        const args = [null, "title", null, null, null, TYPE_BOOKMARK, null, FAKE_BOOKMARK.guid];
        await observer.onItemChanged(...args);

        assert.calledWith(dispatch, {type: at.PLACES_BOOKMARK_CHANGED, data: FAKE_BOOKMARK});
      });
      it("should catch errors gracefully", async () => {
        const e = new Error("test error");
        global.NewTabUtils.activityStreamProvider.getBookmark.restore();
        sandbox.stub(global.NewTabUtils.activityStreamProvider, "getBookmark")
          .returns(Promise.reject(e));

        const args = [null, "title", null, null, null, TYPE_BOOKMARK, null, FAKE_BOOKMARK.guid];
        await observer.onItemChanged(...args);

        assert.calledWith(global.Components.utils.reportError, e);
      });
      it("should ignore events that are not of TYPE_BOOKMARK", async () => {
        await observer.onItemChanged(null, "title", null, null, null, "nottypebookmark");
        assert.notCalled(dispatch);
      });
      it("should ignore events that are not changes to uri/title", async () => {
        await observer.onItemChanged(null, "tags", null, null, null, TYPE_BOOKMARK);
        assert.notCalled(dispatch);
      });
    });
  });
});

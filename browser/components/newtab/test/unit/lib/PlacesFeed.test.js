import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {GlobalOverrider} from "test/unit/utils";
import {PlacesFeed} from "lib/PlacesFeed.jsm";
const {HistoryObserver, BookmarksObserver, PlacesObserver} = PlacesFeed;

const FAKE_BOOKMARK = {bookmarkGuid: "xi31", bookmarkTitle: "Foo", dateAdded: 123214232, url: "foo.com"};
const TYPE_BOOKMARK = 0; // This is fake, for testing
const SOURCES = {
  DEFAULT: 0,
  SYNC: 1,
  IMPORT: 2,
  RESTORE: 5,
  RESTORE_ON_STARTUP: 6,
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
        blockURL: sandbox.spy(),
        addPocketEntry: sandbox.spy(() => Promise.resolve()),
        deletePocketEntry: sandbox.spy(() => Promise.resolve()),
        archivePocketEntry: sandbox.spy(() => Promise.resolve()),
      },
    });
    sandbox.stub(global.PlacesUtils.bookmarks, "TYPE_BOOKMARK").value(TYPE_BOOKMARK);
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

    global.Cc["@mozilla.org/timer;1"] = {
      createInstance() {
        return {
          initWithCallback: sinon.stub().callsFake(callback => callback()),
          cancel: sinon.spy(),
        };
      },
    };
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

  it("should have a PlacesObserver that dispatches to the store", () => {
    assert.instanceOf(feed.placesObserver, PlacesObserver);
    const action = {type: "FOO"};

    feed.placesObserver.dispatch(action);

    assert.calledOnce(feed.store.dispatch);
    assert.equal(feed.store.dispatch.firstCall.args[0].type, action.type);
  });
  describe("#onAction", () => {
    it("should add bookmark, history, places, blocked observers on INIT", () => {
      feed.onAction({type: at.INIT});

      assert.calledWith(global.PlacesUtils.history.addObserver, feed.historyObserver, true);
      assert.calledWith(global.PlacesUtils.bookmarks.addObserver, feed.bookmarksObserver, true);
      assert.calledWith(global.PlacesUtils.observers.addListener, ["bookmark-added"], feed.placesObserver.handlePlacesEvent);
      assert.calledWith(global.Services.obs.addObserver, feed, BLOCKED_EVENT);
    });
    it("should remove bookmark, history, places, blocked observers, and timers on UNINIT", () => {
      feed.placesChangedTimer = global.Cc["@mozilla.org/timer;1"].createInstance();
      let spy = feed.placesChangedTimer.cancel;
      feed.onAction({type: at.UNINIT});

      assert.calledWith(global.PlacesUtils.history.removeObserver, feed.historyObserver);
      assert.calledWith(global.PlacesUtils.bookmarks.removeObserver, feed.bookmarksObserver);
      assert.calledWith(global.PlacesUtils.observers.removeListener, ["bookmark-added"], feed.placesObserver.handlePlacesEvent);
      assert.calledWith(global.Services.obs.removeObserver, feed, BLOCKED_EVENT);
      assert.equal(feed.placesChangedTimer, null);
      assert.calledOnce(spy);
    });
    it("should block a url on BLOCK_URL", () => {
      feed.onAction({type: at.BLOCK_URL, data: {url: "apple.com", pocket_id: 1234}});
      assert.calledWith(global.NewTabUtils.activityStreamLinks.blockURL, {url: "apple.com", pocket_id: 1234});
    });
    it("should bookmark a url on BOOKMARK_URL", () => {
      const data = {url: "pear.com", title: "A pear"};
      const _target = {browser: {ownerGlobal() {}}};
      feed.onAction({type: at.BOOKMARK_URL, data, _target});
      assert.calledWith(global.NewTabUtils.activityStreamLinks.addBookmark, data, _target.browser.ownerGlobal);
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
      assert.calledWith(global.NewTabUtils.activityStreamLinks.blockURL, {url: "guava.com", pocket_id: undefined});
    });
    it("should call openLinkIn with the correct url and where on OPEN_NEW_WINDOW", () => {
      const openLinkIn = sinon.stub();
      const openWindowAction = {
        type: at.OPEN_NEW_WINDOW,
        data: {url: "foo.com"},
        _target: {browser: {ownerGlobal: {openLinkIn}}},
      };

      feed.onAction(openWindowAction);

      assert.calledOnce(openLinkIn);
      const [url, where, params] = openLinkIn.firstCall.args;
      assert.equal(url, "foo.com");
      assert.equal(where, "window");
      assert.propertyVal(params, "private", false);
    });
    it("should call openLinkIn with the correct url, where and privacy args on OPEN_PRIVATE_WINDOW", () => {
      const openLinkIn = sinon.stub();
      const openWindowAction = {
        type: at.OPEN_PRIVATE_WINDOW,
        data: {url: "foo.com"},
        _target: {browser: {ownerGlobal: {openLinkIn}}},
      };

      feed.onAction(openWindowAction);

      assert.calledOnce(openLinkIn);
      const [url, where, params] = openLinkIn.firstCall.args;
      assert.equal(url, "foo.com");
      assert.equal(where, "window");
      assert.propertyVal(params, "private", true);
    });
    it("should open link on OPEN_LINK", () => {
      const openLinkIn = sinon.stub();
      const openLinkAction = {
        type: at.OPEN_LINK,
        data: {url: "foo.com"},
        _target: {browser: {ownerGlobal: {openLinkIn, whereToOpenLink: e => "current"}}},
      };

      feed.onAction(openLinkAction);

      assert.calledOnce(openLinkIn);
      const [url, where, params] = openLinkIn.firstCall.args;
      assert.equal(url, "foo.com");
      assert.equal(where, "current");
      assert.propertyVal(params, "private", false);
      assert.propertyVal(params, "triggeringPrincipal", undefined);
    });
    it("should open link with referrer on OPEN_LINK", () => {
      const openLinkIn = sinon.stub();
      const openLinkAction = {
        type: at.OPEN_LINK,
        data: {url: "foo.com", referrer: "foo.com/ref"},
        _target: {browser: {ownerGlobal: {openLinkIn, whereToOpenLink: e => "tab"}}},
      };

      feed.onAction(openLinkAction);

      const [, , params] = openLinkIn.firstCall.args;
      assert.nestedPropertyVal(params, "referrerInfo.referrerPolicy", 5);
      assert.nestedPropertyVal(
        params,
        "referrerInfo.originalReferrer.spec",
        "foo.com/ref"
      );
    });
    it("should mark link with typed bonus as typed before opening OPEN_LINK", () => {
      const callOrder = [];
      sinon.stub(global.PlacesUtils.history, "markPageAsTyped").callsFake(() => {
        callOrder.push("markPageAsTyped");
      });
      const openLinkIn = sinon.stub().callsFake(() => {
        callOrder.push("openLinkIn");
      });
      const openLinkAction = {
        type: at.OPEN_LINK,
        data: {
          typedBonus: true,
          url: "foo.com",
        },
        _target: {browser: {ownerGlobal: {openLinkIn, whereToOpenLink: e => "tab"}}},
      };

      feed.onAction(openLinkAction);

      assert.sameOrderedMembers(callOrder, ["markPageAsTyped", "openLinkIn"]);
    });
    it("should open the pocket link if it's a pocket story on OPEN_LINK", () => {
      const openLinkIn = sinon.stub();
      const openLinkAction = {
        type: at.OPEN_LINK,
        data: {url: "foo.com", open_url: "getpocket.com/foo", type: "pocket"},
        _target: {browser: {ownerGlobal: {openLinkIn, whereToOpenLink: e => "current"}}},
      };

      feed.onAction(openLinkAction);

      assert.calledOnce(openLinkIn);
      const [url, where, params] = openLinkIn.firstCall.args;
      assert.equal(url, "getpocket.com/foo");
      assert.equal(where, "current");
      assert.propertyVal(params, "private", false);
      assert.propertyVal(params, "triggeringPrincipal", undefined);
    });
    it("should call fillSearchTopSiteTerm on FILL_SEARCH_TERM", () => {
      sinon.stub(feed, "fillSearchTopSiteTerm");

      feed.onAction({type: at.FILL_SEARCH_TERM});

      assert.calledOnce(feed.fillSearchTopSiteTerm);
    });
    it("should set the URL bar value to the label value", () => {
      const locationBar = {search: sandbox.stub()};
      const action = {
        type: at.FILL_SEARCH_TERM,
        data: {label: "@Foo"},
        _target: {browser: {ownerGlobal: {gURLBar: locationBar}}},
      };

      feed.fillSearchTopSiteTerm(action);

      assert.calledOnce(locationBar.search);
      assert.calledWithExactly(locationBar.search, "@Foo ");
    });
    it("should call saveToPocket on SAVE_TO_POCKET", () => {
      const action = {
        type: at.SAVE_TO_POCKET,
        data: {site: {url: "raspberry.com", title: "raspberry"}},
        _target: {browser: {}},
      };
      sinon.stub(feed, "saveToPocket");
      feed.onAction(action);
      assert.calledWithExactly(feed.saveToPocket, action.data.site, action._target.browser);
    });
    it("should call NewTabUtils.activityStreamLinks.addPocketEntry if we are saving a pocket story", async () => {
      const action = {
        data: {site: {url: "raspberry.com", title: "raspberry"}},
        _target: {browser: {}},
      };
      await feed.saveToPocket(action.data.site, action._target.browser);
      assert.calledOnce(global.NewTabUtils.activityStreamLinks.addPocketEntry);
      assert.calledWithExactly(global.NewTabUtils.activityStreamLinks.addPocketEntry, action.data.site.url, action.data.site.title, action._target.browser);
    });
    it("should reject the promise if NewTabUtils.activityStreamLinks.addPocketEntry rejects", async () => {
      const e = new Error("Error");
      const action = {
        data: {site: {url: "raspberry.com", title: "raspberry"}},
        _target: {browser: {}},
      };
      global.NewTabUtils.activityStreamLinks.addPocketEntry = sandbox.stub().rejects(e);
      await feed.saveToPocket(action.data.site, action._target.browser);
      assert.calledWith(global.Cu.reportError, e);
    });
    it("should broadcast to content if we successfully added a link to Pocket", async () => {
      // test in the form that the API returns data based on: https://getpocket.com/developer/docs/v3/add
      global.NewTabUtils.activityStreamLinks.addPocketEntry = sandbox.stub().resolves({item: {open_url: "pocket.com/itemID", item_id: 1234}});
      const action = {
        data: {site: {url: "raspberry.com", title: "raspberry"}},
        _target: {browser: {}},
      };
      await feed.saveToPocket(action.data.site, action._target.browser);
      assert.equal(feed.store.dispatch.firstCall.args[0].type, at.PLACES_SAVED_TO_POCKET);
      assert.deepEqual(feed.store.dispatch.firstCall.args[0].data, {
        url: "raspberry.com",
        title: "raspberry",
        pocket_id: 1234,
        open_url: "pocket.com/itemID",
      });
    });
    it("should only broadcast if we got some data back from addPocketEntry", async () => {
      global.NewTabUtils.activityStreamLinks.addPocketEntry = sandbox.stub().resolves(null);
      const action = {
        data: {site: {url: "raspberry.com", title: "raspberry"}},
        _target: {browser: {}},
      };
      await feed.saveToPocket(action.data.site, action._target.browser);
      assert.notCalled(feed.store.dispatch);
    });
    it("should call deleteFromPocket on DELETE_FROM_POCKET", () => {
      sandbox.stub(feed, "deleteFromPocket");
      feed.onAction({type: at.DELETE_FROM_POCKET, data: {pocket_id: 12345}});

      assert.calledOnce(feed.deleteFromPocket);
      assert.calledWithExactly(feed.deleteFromPocket, 12345);
    });
    it("should catch if deletePocketEntry throws", async () => {
      const e = new Error("Error");
      global.NewTabUtils.activityStreamLinks.deletePocketEntry = sandbox.stub().rejects(e);
      await feed.deleteFromPocket(12345);

      assert.calledWith(global.Cu.reportError, e);
    });
    it("should call NewTabUtils.deletePocketEntry and dispatch POCKET_LINK_DELETED_OR_ARCHIVED when deleting from Pocket", async () => {
      await feed.deleteFromPocket(12345);

      assert.calledOnce(global.NewTabUtils.activityStreamLinks.deletePocketEntry);
      assert.calledWith(global.NewTabUtils.activityStreamLinks.deletePocketEntry, 12345);

      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {type: at.POCKET_LINK_DELETED_OR_ARCHIVED});
    });
    it("should call archiveFromPocket on ARCHIVE_FROM_POCKET", async () => {
      sandbox.stub(feed, "archiveFromPocket");
      await feed.onAction({type: at.ARCHIVE_FROM_POCKET, data: {pocket_id: 12345}});

      assert.calledOnce(feed.archiveFromPocket);
      assert.calledWithExactly(feed.archiveFromPocket, 12345);
    });
    it("should catch if archiveFromPocket throws", async () => {
      const e = new Error("Error");
      global.NewTabUtils.activityStreamLinks.archivePocketEntry = sandbox.stub().rejects(e);
      await feed.archiveFromPocket(12345);

      assert.calledWith(global.Cu.reportError, e);
    });
    it("should call NewTabUtils.archivePocketEntry and dispatch POCKET_LINK_DELETED_OR_ARCHIVED when archiving from Pocket", async () => {
      await feed.archiveFromPocket(12345);

      assert.calledOnce(global.NewTabUtils.activityStreamLinks.archivePocketEntry);
      assert.calledWith(global.NewTabUtils.activityStreamLinks.archivePocketEntry, 12345);

      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {type: at.POCKET_LINK_DELETED_OR_ARCHIVED});
    });
    it("should call handoffSearchToAwesomebar on HANDOFF_SEARCH_TO_AWESOMEBAR", () => {
      const action = {
        type: at.HANDOFF_SEARCH_TO_AWESOMEBAR,
        data: {text: "f"},
        meta: {fromTarget: {}},
        _target: {browser: {ownerGlobal: {gURLBar: {focus: () => {}}}}},
      };
      sinon.stub(feed, "handoffSearchToAwesomebar");
      feed.onAction(action);
      assert.calledWith(feed.handoffSearchToAwesomebar, action);
    });
  });

  describe("handoffSearchToAwesomebar", () => {
    let fakeUrlBar;
    let listeners;

    beforeEach(() => {
      fakeUrlBar = {
        focus: sinon.spy(),
        search: sinon.spy(),
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
        _target: {browser: {ownerGlobal: {gURLBar: fakeUrlBar}}},
        data: {},
        meta: {fromTarget: {}},
      });
      assert.calledOnce(fakeUrlBar.setHiddenFocus);
      assert.notCalled(fakeUrlBar.search);
      assert.notCalled(feed.store.dispatch);

      // Now type a character.
      listeners.keydown({key: "f"});
      assert.calledOnce(fakeUrlBar.search);
      assert.calledOnce(fakeUrlBar.removeHiddenFocus);
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, {
        meta: {
          from: "ActivityStream:Main",
          skipMain: true,
          to: "ActivityStream:Content",
          toTarget: {},
        },
        type: "HIDE_SEARCH",
      });
    });
    it("should properly handle handoff with text data passed in", () => {
      feed.handoffSearchToAwesomebar({
        _target: {browser: {ownerGlobal: {gURLBar: fakeUrlBar}}},
        data: {text: "foo"},
        meta: {fromTarget: {}},
      });
      assert.calledOnce(fakeUrlBar.search);
      assert.calledWith(fakeUrlBar.search, "@google foo");
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
    it("should SHOW_SEARCH on ESC keydown", () => {
      feed.handoffSearchToAwesomebar({
        _target: {browser: {ownerGlobal: {gURLBar: fakeUrlBar}}},
        data: {text: "foo"},
        meta: {fromTarget: {}},
      });
      assert.calledOnce(fakeUrlBar.search);
      assert.calledWith(fakeUrlBar.search, "@google foo");
      assert.notCalled(fakeUrlBar.focus);

      // Now call ESC keydown.
      listeners.keydown({key: "Escape"});
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
    it("should properly handle no defined search alias", () => {
      global.Services.search.defaultEngine.wrappedJSObject.__internalAliases = [];
      feed.handoffSearchToAwesomebar({
        _target: {browser: {ownerGlobal: {gURLBar: fakeUrlBar}}},
        data: {text: "foo"},
        meta: {fromTarget: {}},
      });
      assert.calledOnce(fakeUrlBar.search);
      assert.calledWith(fakeUrlBar.search, "foo");
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
      it("should dispatch a PLACES_LINK_DELETED action with the right url", async () => {
        await observer.onDeleteURI({spec: "foo.com"});

        assert.calledWith(dispatch, {type: at.PLACES_LINK_DELETED, data: {url: "foo.com"}});
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
        observer.onTitleChanged();
        observer.onFrecencyChanged();
        observer.onManyFrecenciesChanged();
        observer.onPageChanged();
        observer.onDeleteVisits();
      });
    });
  });

  describe("Custom dispatch", () => {
    it("should only dispatch 1 PLACES_LINKS_CHANGED action if many bookmark-added notifications happened at once", async () => {
      // Yes, onItemAdded has at least 8 arguments. See function definition for docs.
      const args = [{
        itemType: TYPE_BOOKMARK,
        source:  SOURCES.DEFAULT,
        dateAdded: FAKE_BOOKMARK.dateAdded,
        guid: FAKE_BOOKMARK.bookmarkGuid,
        title: FAKE_BOOKMARK.bookmarkTitle,
        url: "https://www.foo.com",
        isTagging: false,
      }];
      await feed.placesObserver.handlePlacesEvent(args);
      await feed.placesObserver.handlePlacesEvent(args);
      await feed.placesObserver.handlePlacesEvent(args);
      await feed.placesObserver.handlePlacesEvent(args);
      assert.calledOnce(feed.store.dispatch.withArgs(ac.OnlyToMain({type: at.PLACES_LINKS_CHANGED})));
    });
    it("should only dispatch 1 PLACES_LINKS_CHANGED action if many onItemRemoved notifications happened at once", async () => {
      const args = [null, null, null, TYPE_BOOKMARK, {spec: "foo.com"}, "123foo", "", SOURCES.DEFAULT];
      await feed.bookmarksObserver.onItemRemoved(...args);
      await feed.bookmarksObserver.onItemRemoved(...args);
      await feed.bookmarksObserver.onItemRemoved(...args);
      await feed.bookmarksObserver.onItemRemoved(...args);

      assert.calledOnce(feed.store.dispatch.withArgs(ac.OnlyToMain({type: at.PLACES_LINKS_CHANGED})));
    });
    it("should only dispatch 1 PLACES_LINKS_CHANGED action if any onDeleteURI notifications happened at once", async () => {
      await feed.historyObserver.onDeleteURI({spec: "foo.com"});
      await feed.historyObserver.onDeleteURI({spec: "foo1.com"});
      await feed.historyObserver.onDeleteURI({spec: "foo2.com"});

      assert.calledOnce(feed.store.dispatch.withArgs(ac.OnlyToMain({type: at.PLACES_LINKS_CHANGED})));
    });
  });

  describe("PlacesObserver", () => {
    describe("#bookmark-added", () => {
      let dispatch;
      let observer;
      beforeEach(() => {
        dispatch = sandbox.spy();
        observer = new PlacesObserver(dispatch);
      });
      it("should dispatch a PLACES_BOOKMARK_ADDED action with the bookmark data - http", async () => {
        const args = [{
          itemType: TYPE_BOOKMARK,
          source:  SOURCES.DEFAULT,
          dateAdded: FAKE_BOOKMARK.dateAdded,
          guid: FAKE_BOOKMARK.bookmarkGuid,
          title: FAKE_BOOKMARK.bookmarkTitle,
          url: "http://www.foo.com",
          isTagging: false,
        }];
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
        const args = [{
          itemType: TYPE_BOOKMARK,
          source:  SOURCES.DEFAULT,
          dateAdded: FAKE_BOOKMARK.dateAdded,
          guid: FAKE_BOOKMARK.bookmarkGuid,
          title: FAKE_BOOKMARK.bookmarkTitle,
          url: "https://www.foo.com",
          isTagging: false,
        }];
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
        const args = [{
          itemType: TYPE_BOOKMARK,
          source:  SOURCES.DEFAULT,
          dateAdded: FAKE_BOOKMARK.dateAdded,
          guid: FAKE_BOOKMARK.bookmarkGuid,
          title: FAKE_BOOKMARK.bookmarkTitle,
          url: "foo.com",
          isTagging: false,
        }];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_ADDED action - has IMPORT source", async () => {
        const args = [{
          itemType: TYPE_BOOKMARK,
          source:  SOURCES.IMPORT,
          dateAdded: FAKE_BOOKMARK.dateAdded,
          guid: FAKE_BOOKMARK.bookmarkGuid,
          title: FAKE_BOOKMARK.bookmarkTitle,
          url: "foo.com",
          isTagging: false,
        }];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_ADDED action - has RESTORE source", async () => {
        const args = [{
          itemType: TYPE_BOOKMARK,
          source:  SOURCES.RESTORE,
          dateAdded: FAKE_BOOKMARK.dateAdded,
          guid: FAKE_BOOKMARK.bookmarkGuid,
          title: FAKE_BOOKMARK.bookmarkTitle,
          url: "foo.com",
          isTagging: false,
        }];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_ADDED action - has RESTORE_ON_STARTUP source", async () => {
        const args = [{
          itemType: TYPE_BOOKMARK,
          source:  SOURCES.RESTORE_ON_STARTUP,
          dateAdded: FAKE_BOOKMARK.dateAdded,
          guid: FAKE_BOOKMARK.bookmarkGuid,
          title: FAKE_BOOKMARK.bookmarkTitle,
          url: "foo.com",
          isTagging: false,
        }];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_ADDED action - has SYNC source", async () => {
        const args = [{
          itemType: TYPE_BOOKMARK,
          source:  SOURCES.SYNC,
          dateAdded: FAKE_BOOKMARK.dateAdded,
          guid: FAKE_BOOKMARK.bookmarkGuid,
          title: FAKE_BOOKMARK.bookmarkTitle,
          url: "foo.com",
          isTagging: false,
        }];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
      });
      it("should ignore events that are not of TYPE_BOOKMARK", async () => {
        const args = [{
          itemType: "nottypebookmark",
          source:  SOURCES.DEFAULT,
          dateAdded: FAKE_BOOKMARK.dateAdded,
          guid: FAKE_BOOKMARK.bookmarkGuid,
          title: FAKE_BOOKMARK.bookmarkTitle,
          url: "https://www.foo.com",
          isTagging: false,
        }];
        await observer.handlePlacesEvent(args);

        assert.notCalled(dispatch);
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
    describe("#onItemRemoved", () => {
      it("should ignore events that are not of TYPE_BOOKMARK", async () => {
        await observer.onItemRemoved(null, null, null, "nottypebookmark", null, "123foo", "", SOURCES.DEFAULT);
        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_REMOVED action - has SYNC source", async () => {
        const args = [null, null, null, TYPE_BOOKMARK, {spec: "foo.com"}, "123foo", "", SOURCES.SYNC];
        await observer.onItemRemoved(...args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_REMOVED action - has IMPORT source", async () => {
        const args = [null, null, null, TYPE_BOOKMARK, {spec: "foo.com"}, "123foo", "", SOURCES.IMPORT];
        await observer.onItemRemoved(...args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_REMOVED action - has RESTORE source", async () => {
        const args = [null, null, null, TYPE_BOOKMARK, {spec: "foo.com"}, "123foo", "", SOURCES.RESTORE];
        await observer.onItemRemoved(...args);

        assert.notCalled(dispatch);
      });
      it("should not dispatch a PLACES_BOOKMARK_REMOVED action - has RESTORE_ON_STARTUP source", async () => {
        const args = [null, null, null, TYPE_BOOKMARK, {spec: "foo.com"}, "123foo", "", SOURCES.RESTORE_ON_STARTUP];
        await observer.onItemRemoved(...args);

        assert.notCalled(dispatch);
      });
      it("should dispatch a PLACES_BOOKMARK_REMOVED action with the right URL and bookmarkGuid", () => {
        observer.onItemRemoved(null, null, null, TYPE_BOOKMARK, {spec: "foo.com"}, "123foo", "", SOURCES.DEFAULT);
        assert.calledWith(dispatch, {type: at.PLACES_BOOKMARK_REMOVED, data: {bookmarkGuid: "123foo", url: "foo.com"}});
      });
    });
    describe("Other empty methods (to keep code coverage happy)", () => {
      it("should have a various empty functions for xpconnect happiness", () => {
        observer.onBeginUpdateBatch();
        observer.onEndUpdateBatch();
        observer.onItemVisited();
        observer.onItemMoved();
        observer.onItemChanged();
      });
    });
  });
});

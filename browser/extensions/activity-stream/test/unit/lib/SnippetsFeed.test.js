import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {GlobalOverrider} from "test/unit/utils";
import {SnippetsFeed} from "lib/SnippetsFeed.jsm";

const WEEK_IN_MS = 7 * 24 * 60 * 60 * 1000;
const searchData = {searchEngineIdentifier: "google", engines: ["searchEngine-google", "searchEngine-bing"]};
const signUpUrl = "https://accounts.firefox.com/signup?service=sync&context=fx_desktop_v3&entrypoint=snippets";
const FAKE_ADDONS = {
  foo: {
    name: "Foo",
    version: "0.1.0",
    type: "foo",
    isSystem: false,
    isWebExtension: true,
    userDisabled: false,
    installDate: 1231921
  },
  bar: {
    name: "Bar",
    version: "0.2.0",
    type: "bar",
    isSystem: false,
    isWebExtension: false,
    userDisabled: false,
    installDate: 1231921
  }
};

let overrider = new GlobalOverrider();

describe("SnippetsFeed", () => {
  let sandbox;
  let clock;
  let fakeDB;
  beforeEach(() => {
    clock = sinon.useFakeTimers();
    sandbox = sinon.sandbox.create();
    fakeDB = {
      objectStore: sandbox.stub().returns({
        get: sandbox.stub().returns(Promise.resolve()),
        set: sandbox.stub().returns(Promise.resolve())
      })
    };
    overrider.set({
      ProfileAge: class ProfileAge {
        constructor() {
          this.created = Promise.resolve(0);
          this.reset = Promise.resolve(WEEK_IN_MS);
        }
      },
      FxAccounts: {config: {promiseSignUpURI: sandbox.stub().returns(Promise.resolve(signUpUrl))}},
      NewTabUtils: {activityStreamProvider: {getTotalBookmarksCount: () => Promise.resolve(42)}},
      ActivityStreamStorage: class ActivityStreamStorage {
        constructor() {
          this.init = sandbox.stub.callsFake(Promise.resolve());
        }
        init() {
          return Promise.resolve();
        }
      },
      IndexedDB: {open: () => Promise.resolve(fakeDB)}
    });
  });
  afterEach(() => {
    clock.restore();
    overrider.restore();
    sandbox.restore();
  });
  it("should dispatch a SNIPPETS_DATA action with the right data on INIT", async () => {
    const url = "foo.com/%STARTPAGE_VERSION%";
    sandbox.stub(global.Services.prefs, "getStringPref").returns(url);
    sandbox.stub(global.Services.prefs, "getBoolPref")
      .withArgs("datareporting.healthreport.uploadEnabled")
      .returns(true)
      .withArgs("browser.onboarding.notification.finished")
      .returns(false);
    sandbox.stub(global.Services.prefs, "prefHasUserValue")
      .withArgs("services.sync.username")
      .returns(true);
    sandbox.stub(global.Services.prefs, "getIntPref")
      .withArgs("devtools.selfxss.count")
      .returns(5);
    sandbox.stub(global.AddonManager, "getActiveAddons")
      .resolves({
        addons: [
          Object.assign({id: "foo"}, FAKE_ADDONS.foo),
          Object.assign({id: "bar"}, FAKE_ADDONS.bar)
        ],
        fullData: true
      });

    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};
    sandbox.stub(feed._storage, "get")
      .withArgs("previousSessionEnd")
      .resolves(42)
      .withArgs("blockList")
      .resolves([1]);

    clock.tick(WEEK_IN_MS * 2);

    await feed.init();

    assert.calledOnce(feed.store.dispatch);

    const [action] = feed.store.dispatch.firstCall.args;
    assert.propertyVal(action, "type", at.SNIPPETS_DATA);
    assert.isObject(action.data);
    assert.propertyVal(action.data, "snippetsURL", "foo.com/5");
    assert.propertyVal(action.data, "version", 5);
    assert.propertyVal(action.data, "profileCreatedWeeksAgo", 2);
    assert.propertyVal(action.data, "profileResetWeeksAgo", 1);
    assert.propertyVal(action.data, "telemetryEnabled", true);
    assert.propertyVal(action.data, "onboardingFinished", false);
    assert.propertyVal(action.data, "fxaccount", true);
    assert.property(action.data, "selectedSearchEngine");
    assert.deepEqual(action.data.selectedSearchEngine, searchData);
    assert.propertyVal(action.data, "defaultBrowser", true);
    assert.propertyVal(action.data, "isDevtoolsUser", true);
    assert.deepEqual(action.data.addonInfo, FAKE_ADDONS);
    assert.deepEqual(action.data.blockList, [1]);
    assert.propertyVal(action.data, "previousSessionEnd", 42);
  });
  it("should call .init on an INIT action", () => {
    const feed = new SnippetsFeed();
    sandbox.stub(feed, "init");
    feed.store = {dispatch: sandbox.stub()};

    feed.onAction({type: at.INIT});
    assert.calledOnce(feed.init);
  });
  it("should call .uninit on an UNINIT action", () => {
    const feed = new SnippetsFeed();
    sandbox.stub(feed, "uninit");
    sandbox.stub(feed._storage, "set");
    feed.store = {dispatch: sandbox.stub()};

    feed.onAction({type: at.UNINIT});
    assert.calledOnce(feed.uninit);
  });
  it("should broadcast a SNIPPETS_RESET on uninit", () => {
    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};
    sandbox.stub(feed._storage, "set");

    feed.uninit();

    assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({type: at.SNIPPETS_RESET}));
  });
  it("should update the blocklist on SNIPPETS_BLOCKLIST_UPDATED", async () => {
    const feed = new SnippetsFeed();
    const saveBlockList = sandbox.stub(feed._storage, "set");
    sandbox.stub(feed._storage, "get").returns(["bar"]);
    feed.store = {dispatch: sandbox.stub()};

    await feed._saveBlockedSnippet("foo");

    assert.calledOnce(saveBlockList);
    assert.equal(saveBlockList.args[0][0], "blockList");
    assert.deepEqual(saveBlockList.args[0][1], ["bar", "foo"]);
  });
  it("should broadcast a SNIPPET_BLOCKED when a SNIPPETS_BLOCKLIST_UPDATED is received", () => {
    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};

    feed.onAction({type: at.SNIPPETS_BLOCKLIST_UPDATED, data: "foo"});

    assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({type: at.SNIPPET_BLOCKED, data: "foo"}));
  });
  it("should call _clearBlockList on SNIPPETS_BLOCKLIST_CLEARED", () => {
    const feed = new SnippetsFeed();
    const stub = sandbox.stub(feed, "_clearBlockList");

    feed.onAction({type: at.SNIPPETS_BLOCKLIST_CLEARED});

    assert.calledOnce(stub);
  });
  it("should set blockList to [] on SNIPPETS_BLOCKLIST_CLEARED", async () => {
    const feed = new SnippetsFeed();
    const stub = sandbox.stub(feed._storage, "set");

    await feed._clearBlockList();

    assert.calledOnce(stub);
    assert.calledWithExactly(stub, "blockList", []);
  });
  it("should dispatch an update event when the Search observer is called", async () => {
    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};
    sandbox.stub(feed, "getSelectedSearchEngine")
      .returns(Promise.resolve(searchData));

    await feed.observe(null, "browser-search-engine-modified");

    assert.calledOnce(feed.store.dispatch);
    const [action] = feed.store.dispatch.firstCall.args;
    assert.equal(action.type, at.SNIPPETS_DATA);
    assert.deepEqual(action.data, {selectedSearchEngine: searchData});
  });
  it("should call showFirefoxAccounts", () => {
    const feed = new SnippetsFeed();
    const browser = {};
    sandbox.spy(feed, "showFirefoxAccounts");
    feed.onAction({type: at.SHOW_FIREFOX_ACCOUNTS, _target: {browser}});
    assert.calledWith(feed.showFirefoxAccounts, browser);
  });
  it("should open Firefox Accounts", async () => {
    const feed = new SnippetsFeed();
    const browser = {loadURI: sinon.spy()};
    await feed.showFirefoxAccounts(browser);
    assert.calledWith(browser.loadURI, signUpUrl);
  });
  it("should call .getTotalBookmarksCount when TOTAL_BOOKMARKS_REQUEST is received", async () => {
    const feed = new SnippetsFeed();
    const browser = {};
    sandbox.spy(feed, "getTotalBookmarksCount");
    feed.onAction({type: at.TOTAL_BOOKMARKS_REQUEST, _target: {browser}});
    assert.calledWith(feed.getTotalBookmarksCount, browser);
  });
  it("should dispatch a TOTAL_BOOKMARKS_RESPONSE action when .getTotalBookmarksCount is called", async () => {
    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};
    const browser = {};
    const action = {type: at.TOTAL_BOOKMARKS_RESPONSE, data: 42};

    await feed.getTotalBookmarksCount(browser);

    assert.calledWith(feed.store.dispatch, ac.OnlyToOneContent(action, browser));
  });
  it("should still dispatch if .getTotalBookmarksCount results in an error", async () => {
    sandbox.stub(global.NewTabUtils.activityStreamProvider, "getTotalBookmarksCount")
      .throws(new Error("test error"));
    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};
    const browser = {};
    const action = {type: at.TOTAL_BOOKMARKS_RESPONSE, data: null};

    await feed.getTotalBookmarksCount(browser);

    assert.calledWith(feed.store.dispatch, ac.OnlyToOneContent(action, browser));
  });
  it("should return true for isDevtoolsUser is devtools.selfxss.count is 5", async () => {
    sandbox.stub(global.Services.prefs, "getIntPref")
      .withArgs("devtools.selfxss.count")
      .returns(5);
    const feed = new SnippetsFeed();
    const result = feed.isDevtoolsUser();
    assert.isTrue(result);
  });
  it("should return false for isDevtoolsUser is devtools.selfxss.count is less than 5", async () => {
    sandbox.stub(global.Services.prefs, "getIntPref")
      .withArgs("devtools.selfxss.count")
      .returns(4);
    const feed = new SnippetsFeed();
    const result = feed.isDevtoolsUser();
    assert.isFalse(result);
  });
  it("should set _previousSessionEnd on uninit", async () => {
    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};
    feed._previousSessionEnd = null;
    const stub = sandbox.stub(feed._storage, "set");

    feed.uninit();

    assert.calledOnce(stub);
    assert.calledWithExactly(stub, "previousSessionEnd", Date.now());
  });
});

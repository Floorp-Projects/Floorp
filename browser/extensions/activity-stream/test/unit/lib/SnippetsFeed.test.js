import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {GlobalOverrider} from "test/unit/utils";
import {SnippetsFeed} from "lib/SnippetsFeed.jsm";

const WEEK_IN_MS = 7 * 24 * 60 * 60 * 1000;
const searchData = {searchEngineIdentifier: "google", engines: ["searchEngine-google", "searchEngine-bing"]};
const signUpUrl = "https://accounts.firefox.com/signup?service=sync&context=fx_desktop_v3&entrypoint=snippets";

let overrider = new GlobalOverrider();

describe("SnippetsFeed", () => {
  let sandbox;
  let clock;
  beforeEach(() => {
    clock = sinon.useFakeTimers();
    sandbox = sinon.sandbox.create();
    overrider.set({
      ProfileAge: class ProfileAge {
        constructor() {
          this.created = Promise.resolve(0);
          this.reset = Promise.resolve(WEEK_IN_MS);
        }
      },
      fxAccounts: {promiseAccountsSignUpURI: sandbox.stub().returns(Promise.resolve(signUpUrl))}
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

    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};

    clock.tick(WEEK_IN_MS * 2);

    await feed.init();

    assert.calledOnce(feed.store.dispatch);

    const action = feed.store.dispatch.firstCall.args[0];
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
    feed.store = {dispatch: sandbox.stub()};

    feed.onAction({type: at.UNINIT});
    assert.calledOnce(feed.uninit);
  });
  it("should broadcast a SNIPPETS_RESET on uninit", () => {
    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};

    feed.uninit();

    assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({type: at.SNIPPETS_RESET}));
  });
  it("should broadcast a SNIPPET_BLOCKED when a SNIPPETS_BLOCKLIST_UPDATED is received", () => {
    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};
    const blockList = ["foo", "bar", "baz"];

    feed.onAction({type: at.SNIPPETS_BLOCKLIST_UPDATED, data: blockList});

    assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({type: at.SNIPPET_BLOCKED, data: blockList}));
  });
  it("should dispatch an update event when the Search observer is called", async () => {
    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};
    sandbox.stub(feed, "getSelectedSearchEngine")
      .returns(Promise.resolve(searchData));

    await feed.observe(null, "browser-search-engine-modified");

    assert.calledOnce(feed.store.dispatch);
    const action = feed.store.dispatch.firstCall.args[0];
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
});

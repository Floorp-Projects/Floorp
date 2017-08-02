const {SnippetsFeed} = require("lib/SnippetsFeed.jsm");
const {actionTypes: at} = require("common/Actions.jsm");
const {GlobalOverrider} = require("test/unit/utils");
const {createStore, combineReducers} = require("redux");
const {reducers} = require("common/Reducers.jsm");

const WEEK_IN_MS = 7 * 24 * 60 * 60 * 1000;

let overrider = new GlobalOverrider();

describe("SnippetsFeed", () => {
  let sandbox;
  let store;
  let clock;
  beforeEach(() => {
    clock = sinon.useFakeTimers();
    overrider.set({
      ProfileAge: class ProfileAge {
        constructor() {
          this.created = Promise.resolve(0);
          this.reset = Promise.resolve(WEEK_IN_MS);
        }
      }
    });
    sandbox = sinon.sandbox.create();
    store = createStore(combineReducers(reducers));
    sinon.spy(store, "dispatch");
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
      .withArgs("browser.onboarding.notification.finished")
      .returns(false);
    sandbox.stub(global.Services.prefs, "prefHasUserValue")
      .withArgs("services.sync.username")
      .returns(true);
    sandbox.stub(global.Services.telemetry, "canRecordBase").value(false);

    const feed = new SnippetsFeed();
    feed.store = store;
    clock.tick(WEEK_IN_MS * 2);

    await feed.init();

    const state = store.getState().Snippets;

    assert.propertyVal(state, "snippetsURL", "foo.com/5");
    assert.propertyVal(state, "version", 5);
    assert.propertyVal(state, "profileCreatedWeeksAgo", 2);
    assert.propertyVal(state, "profileResetWeeksAgo", 1);
    assert.propertyVal(state, "telemetryEnabled", false);
    assert.propertyVal(state, "onboardingFinished", false);
    assert.propertyVal(state, "fxaccount", true);
  });
  it("should update telemetryEnabled on each new tab", () => {
    sandbox.stub(global.Services.telemetry, "canRecordBase").value(false);
    const feed = new SnippetsFeed();
    feed.store = store;

    feed.onAction({type: at.NEW_TAB_INIT});

    const state = store.getState().Snippets;
    assert.propertyVal(state, "telemetryEnabled", false);
  });
  it("should call .init on an INIT aciton", () => {
    const feed = new SnippetsFeed();
    sandbox.stub(feed, "init");
    feed.store = store;

    feed.onAction({type: at.INIT});
    assert.calledOnce(feed.init);
  });
  it("should call .init when a FEED_INIT happens for feeds.snippets", () => {
    const feed = new SnippetsFeed();
    sandbox.stub(feed, "init");
    feed.store = store;

    feed.onAction({type: at.FEED_INIT, data: "feeds.snippets"});

    assert.calledOnce(feed.init);
  });
  it("should dispatch a SNIPPETS_RESET on uninit", () => {
    const feed = new SnippetsFeed();
    feed.store = store;

    feed.uninit();

    assert.calledWith(feed.store.dispatch, {type: at.SNIPPETS_RESET});
  });
});

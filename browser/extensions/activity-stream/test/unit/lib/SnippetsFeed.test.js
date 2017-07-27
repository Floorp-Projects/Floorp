const {SnippetsFeed} = require("lib/SnippetsFeed.jsm");
const {actionTypes: at} = require("common/Actions.jsm");
const {GlobalOverrider} = require("test/unit/utils");

const WEEK_IN_MS = 7 * 24 * 60 * 60 * 1000;

let overrider = new GlobalOverrider();

describe("SnippetsFeed", () => {
  let sandbox;
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
  });
  afterEach(() => {
    clock.restore();
    overrider.restore();
    sandbox.restore();
  });
  it("should dispatch a SNIPPETS_DATA action with the right data on INIT", async () => {
    const url = "foo.com/%STARTPAGE_VERSION%";
    sandbox.stub(global.Services.prefs, "getStringPref").returns(url);
    sandbox.stub(global.Services.prefs, "getBoolPref").returns(true);
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
  });
  it("should call .init on an INIT aciton", () => {
    const feed = new SnippetsFeed();
    sandbox.stub(feed, "init");
    feed.store = {dispatch: sandbox.stub()};

    feed.onAction({type: at.INIT});
    assert.calledOnce(feed.init);
  });
  it("should call .init when a FEED_INIT happens for feeds.snippets", () => {
    const feed = new SnippetsFeed();
    sandbox.stub(feed, "init");
    feed.store = {dispatch: sandbox.stub()};

    feed.onAction({type: at.FEED_INIT, data: "feeds.snippets"});

    assert.calledOnce(feed.init);
  });
  it("should dispatch a SNIPPETS_RESET on uninit", () => {
    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};

    feed.uninit();

    assert.calledWith(feed.store.dispatch, {type: at.SNIPPETS_RESET});
  });
});

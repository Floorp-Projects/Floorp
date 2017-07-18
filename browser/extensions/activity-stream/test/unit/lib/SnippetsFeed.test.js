const {SnippetsFeed} = require("lib/SnippetsFeed.jsm");
const {actionTypes: at, actionCreators: ac} = require("common/Actions.jsm");

describe("SnippetsFeed", () => {
  let sandbox;
  beforeEach(() => {
    sandbox = sinon.sandbox.create();
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should dispatch the right version and snippetsURL on INIT", () => {
    const url = "foo.com/%STARTPAGE_VERSION%";
    sandbox.stub(global.Services.prefs, "getStringPref").returns(url);
    const feed = new SnippetsFeed();
    feed.store = {dispatch: sandbox.stub()};

    feed.onAction({type: at.INIT});

    assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({
      type: at.SNIPPETS_DATA,
      data: {
        snippetsURL: "foo.com/4",
        version: 4
      }
    }));
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
  describe("_onUrlChange", () => {
    it("should dispatch a new snippetsURL", () => {
      const url = "boo.com/%STARTPAGE_VERSION%";
      sandbox.stub(global.Services.prefs, "getStringPref").returns(url);
      const feed = new SnippetsFeed();
      feed.store = {dispatch: sandbox.stub()};

      feed._onUrlChange();

      assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({
        type: at.SNIPPETS_DATA,
        data: {snippetsURL: "boo.com/4"}
      }));
    });
  });
});

const {PrefsFeed} = require("lib/PrefsFeed.jsm");
const {actionTypes: at, actionCreators: ac} = require("common/Actions.jsm");

const FAKE_PREFS = new Map([["foo", {value: 1}], ["bar", {value: 2}]]);

describe("PrefsFeed", () => {
  let feed;
  beforeEach(() => {
    feed = new PrefsFeed(FAKE_PREFS);
    feed.store = {dispatch: sinon.spy()};
    feed._prefs = {
      get: sinon.spy(item => FAKE_PREFS.get(item).value),
      set: sinon.spy(),
      observe: sinon.spy(),
      observeBranch: sinon.spy(),
      ignore: sinon.spy(),
      ignoreBranch: sinon.spy()
    };
  });
  it("should set a pref when a SET_PREF action is received", () => {
    feed.onAction(ac.SetPref("foo", 2));
    assert.calledWith(feed._prefs.set, "foo", 2);
  });
  it("should dispatch PREFS_INITIAL_VALUES on init", () => {
    feed.onAction({type: at.INIT});
    assert.calledOnce(feed.store.dispatch);
    assert.equal(feed.store.dispatch.firstCall.args[0].type, at.PREFS_INITIAL_VALUES);
    assert.deepEqual(feed.store.dispatch.firstCall.args[0].data, {foo: 1, bar: 2});
  });
  it("should add one branch observer on init", () => {
    feed.onAction({type: at.INIT});
    assert.calledOnce(feed._prefs.observeBranch);
    assert.calledWith(feed._prefs.observeBranch, feed);
  });
  it("should remove the branch observer on uninit", () => {
    feed.onAction({type: at.UNINIT});
    assert.calledOnce(feed._prefs.ignoreBranch);
    assert.calledWith(feed._prefs.ignoreBranch, feed);
  });
  it("should send a PREF_CHANGED action when onPrefChanged is called", () => {
    feed.onPrefChanged("foo", 2);
    assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({type: at.PREF_CHANGED, data: {name: "foo", value: 2}}));
  });
});

const {PrefsFeed} = require("lib/PrefsFeed.jsm");
const {actionTypes: at, actionCreators: ac} = require("common/Actions.jsm");

const FAKE_PREFS = [{name: "foo", value: 1}, {name: "bar", value: 2}];

describe("PrefsFeed", () => {
  let feed;
  beforeEach(() => {
    feed = new PrefsFeed(FAKE_PREFS.map(p => p.name));
    feed.store = {dispatch: sinon.spy()};
    feed._prefs = {
      get: sinon.spy(item => FAKE_PREFS.filter(p => p.name === item)[0].value),
      set: sinon.spy(),
      observe: sinon.spy(),
      ignore: sinon.spy()
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
  it("should add one observer per pref on init", () => {
    feed.onAction({type: at.INIT});
    FAKE_PREFS.forEach(pref => {
      assert.calledWith(feed._prefs.observe, pref.name);
      assert.isTrue(feed._observers.has(pref.name));
    });
  });
  it("should call onPrefChanged when an observer is called", () => {
    sinon.stub(feed, "onPrefChanged");
    feed.onAction({type: at.INIT});
    const handlerForFoo = feed._observers.get("foo");

    handlerForFoo(true);

    assert.calledWith(feed.onPrefChanged, "foo", true);
  });
  it("should remove all observers on uninit", () => {
    feed.onAction({type: at.UNINIT});
    FAKE_PREFS.forEach(pref => {
      assert.calledWith(feed._prefs.ignore, pref.name);
    });
  });
  it("should send a PREF_CHANGED action when onPrefChanged is called", () => {
    feed.onPrefChanged("foo", 2);
    assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({type: at.PREF_CHANGED, data: {name: "foo", value: 2}}));
  });
});

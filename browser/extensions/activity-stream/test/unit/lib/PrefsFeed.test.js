import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {PrefsFeed} from "lib/PrefsFeed.jsm";
import {PrerenderData} from "common/PrerenderData.jsm";
const {initialPrefs} = PrerenderData;

const PRERENDER_PREF_NAME = "prerender";
const ONBOARDING_FINISHED_PREF = "browser.onboarding.notification.finished";

describe("PrefsFeed", () => {
  let feed;
  let FAKE_PREFS;
  beforeEach(() => {
    FAKE_PREFS = new Map([["foo", 1], ["bar", 2]]);
    feed = new PrefsFeed(FAKE_PREFS);
    feed.store = {dispatch: sinon.spy()};
    feed._prefs = {
      get: sinon.spy(item => FAKE_PREFS.get(item)),
      set: sinon.spy((name, value) => FAKE_PREFS.set(name, value)),
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
  describe("INIT prerendering", () => {
    it("should set a prerender pref on init", () => {
      feed.onAction({type: at.INIT});
      assert.calledWith(feed._prefs.set, PRERENDER_PREF_NAME);
    });
    it("should set prerender pref to true if prefs match initial values", () => {
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));
      feed.onAction({type: at.INIT});
      assert.calledWith(feed._prefs.set, PRERENDER_PREF_NAME, true);
    });
    it("should set prerender pref to false if a pref does not match its initial value", () => {
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));
      FAKE_PREFS.set("showSearch", false);
      feed.onAction({type: at.INIT});
      assert.calledWith(feed._prefs.set, PRERENDER_PREF_NAME, false);
    });
  });
  describe("Onboarding", () => {
    let sandbox;
    let defaultBranch;
    beforeEach(() => {
      sandbox = sinon.sandbox.create();
      defaultBranch = {setBoolPref: sandbox.stub()};
      sandbox.stub(global.Services.prefs, "getDefaultBranch").returns(defaultBranch);
    });
    afterEach(() => {
      sandbox.restore();
    });
    it("should set ONBOARDING_FINISHED_PREF to true if prefs.feeds.snippets if false", () => {
      FAKE_PREFS.set("feeds.snippets", false);
      feed.onAction({type: at.INIT});
      assert.calledWith(defaultBranch.setBoolPref, ONBOARDING_FINISHED_PREF, true);
    });
    it("should not set ONBOARDING_FINISHED_PREF if prefs.feeds.snippets is true", () => {
      FAKE_PREFS.set("feeds.snippets", true);
      feed.onAction({type: at.INIT});
      assert.notCalled(defaultBranch.setBoolPref);
    });
    it("should set ONBOARDING_FINISHED_PREF to true if the feeds.snippets pref changes to false", () => {
      feed.onPrefChanged("feeds.snippets", false);
      assert.calledWith(defaultBranch.setBoolPref, ONBOARDING_FINISHED_PREF, true);
    });
    it("should set ONBOARDING_FINISHED_PREF to false if the feeds.snippets pref changes to true", () => {
      feed.onPrefChanged("feeds.snippets", true);
      assert.calledWith(defaultBranch.setBoolPref, ONBOARDING_FINISHED_PREF, false);
    });
    it("should not set ONBOARDING_FINISHED_PREF if an unrelated pref changes", () => {
      feed.onPrefChanged("foo", true);
      assert.notCalled(defaultBranch.setBoolPref);
    });
    it("should set ONBOARDING_FINISHED_PREF to true if a DISABLE_ONBOARDING action was received", () => {
      feed.onAction({type: at.DISABLE_ONBOARDING});
      assert.calledWith(defaultBranch.setBoolPref, ONBOARDING_FINISHED_PREF, true);
    });
  });
  describe("onPrefChanged prerendering", () => {
    it("should not change the prerender pref if the pref is not included in invalidatingPrefs", () => {
      feed.onPrefChanged("foo123", true);
      assert.notCalled(feed._prefs.set);
    });
    it("should set the prerender pref to false if a pref in invalidatingPrefs is changed from its original value", () => {
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));

      feed._prefs.set("showSearch", false);
      feed.onPrefChanged("showSearch", false);
      assert.calledWith(feed._prefs.set, PRERENDER_PREF_NAME, false);
    });
    it("should set the prerender pref back to true if the invalidatingPrefs are changed back to their original values", () => {
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));
      FAKE_PREFS.set("showSearch", false);

      feed._prefs.set("showSearch", true);
      feed.onPrefChanged("showSearch", true);
      assert.calledWith(feed._prefs.set, PRERENDER_PREF_NAME, true);
    });
  });
});

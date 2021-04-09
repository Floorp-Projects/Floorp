import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { GlobalOverrider } from "test/unit/utils";
import { PrefsFeed } from "lib/PrefsFeed.jsm";

let overrider = new GlobalOverrider();

describe("PrefsFeed", () => {
  let feed;
  let FAKE_PREFS;
  let sandbox;
  let ServicesStub;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    FAKE_PREFS = new Map([
      ["foo", 1],
      ["bar", 2],
      ["baz", { value: 1, skipBroadcast: true }],
    ]);
    feed = new PrefsFeed(FAKE_PREFS);
    const storage = {
      getAll: sandbox.stub().resolves(),
      set: sandbox.stub().resolves(),
    };
    ServicesStub = {
      prefs: {
        clearUserPref: sinon.spy(),
        getStringPref: sinon.spy(),
        getIntPref: sinon.spy(),
        getBoolPref: sinon.spy(),
      },
      obs: {
        removeObserver: sinon.spy(),
        addObserver: sinon.spy(),
      },
    };
    feed.store = {
      dispatch: sinon.spy(),
      getState() {
        return this.state;
      },
      dbStorage: { getDbTable: sandbox.stub().returns(storage) },
    };
    // Setup for tests that don't call `init`
    feed._storage = storage;
    feed._prefs = {
      get: sinon.spy(item => FAKE_PREFS.get(item)),
      set: sinon.spy((name, value) => FAKE_PREFS.set(name, value)),
      observe: sinon.spy(),
      observeBranch: sinon.spy(),
      ignore: sinon.spy(),
      ignoreBranch: sinon.spy(),
      reset: sinon.stub(),
      _branchStr: "branch.str.",
    };
    overrider.set({
      PrivateBrowsingUtils: { enabled: true },
      Services: ServicesStub,
      NimbusFeatures: { newtab: new global.ExperimentFeature() },
    });
  });
  afterEach(() => {
    overrider.restore();
    sandbox.restore();
  });

  it("should set a pref when a SET_PREF action is received", () => {
    feed.onAction(ac.SetPref("foo", 2));
    assert.calledWith(feed._prefs.set, "foo", 2);
  });
  it("should call clearUserPref with action CLEAR_PREF", () => {
    feed.onAction({ type: at.CLEAR_PREF, data: { name: "pref.test" } });
    assert.calledWith(ServicesStub.prefs.clearUserPref, "branch.str.pref.test");
  });
  it("should dispatch PREFS_INITIAL_VALUES on init with pref values and .isPrivateBrowsingEnabled", () => {
    feed.onAction({ type: at.INIT });
    assert.calledOnce(feed.store.dispatch);
    assert.equal(
      feed.store.dispatch.firstCall.args[0].type,
      at.PREFS_INITIAL_VALUES
    );
    const [{ data }] = feed.store.dispatch.firstCall.args;
    assert.equal(data.foo, 1);
    assert.equal(data.bar, 2);
    assert.isTrue(data.isPrivateBrowsingEnabled);
  });
  it("should dispatch PREFS_INITIAL_VALUES with a .featureConfig", () => {
    sandbox.stub(global.NimbusFeatures.newtab, "getValue").returns({
      prefsButtonIcon: "icon-foo",
    });
    feed.onAction({ type: at.INIT });
    assert.equal(
      feed.store.dispatch.firstCall.args[0].type,
      at.PREFS_INITIAL_VALUES
    );
    const [{ data }] = feed.store.dispatch.firstCall.args;
    assert.deepEqual(data.featureConfig, { prefsButtonIcon: "icon-foo" });
  });
  it("should dispatch PREFS_INITIAL_VALUES with an empty object if no experiment is returned", () => {
    sandbox.stub(global.NimbusFeatures.newtab, "getValue").returns(null);
    feed.onAction({ type: at.INIT });
    assert.equal(
      feed.store.dispatch.firstCall.args[0].type,
      at.PREFS_INITIAL_VALUES
    );
    const [{ data }] = feed.store.dispatch.firstCall.args;
    assert.deepEqual(data.featureConfig, {});
  });
  it("should add one branch observer on init", () => {
    feed.onAction({ type: at.INIT });
    assert.calledOnce(feed._prefs.observeBranch);
    assert.calledWith(feed._prefs.observeBranch, feed);
  });
  it("should initialise the storage on init", () => {
    feed.init();

    assert.calledOnce(feed.store.dbStorage.getDbTable);
    assert.calledWithExactly(feed.store.dbStorage.getDbTable, "sectionPrefs");
  });
  it("should handle region on init", () => {
    feed.init();
    assert.equal(feed.geo, "US");
  });
  it("should add region observer on init", () => {
    sandbox.stub(global.Region, "home").get(() => "");
    feed.init();
    assert.equal(feed.geo, "");
    assert.calledWith(
      ServicesStub.obs.addObserver,
      feed,
      global.Region.REGION_TOPIC
    );
  });
  it("should remove the branch observer on uninit", () => {
    feed.onAction({ type: at.UNINIT });
    assert.calledOnce(feed._prefs.ignoreBranch);
    assert.calledWith(feed._prefs.ignoreBranch, feed);
  });
  it("should call removeObserver", () => {
    feed.geo = "";
    feed.uninit();
    assert.calledWith(
      ServicesStub.obs.removeObserver,
      feed,
      global.Region.REGION_TOPIC
    );
  });
  it("should send a PREF_CHANGED action when onPrefChanged is called", () => {
    feed.onPrefChanged("foo", 2);
    assert.calledWith(
      feed.store.dispatch,
      ac.BroadcastToContent({
        type: at.PREF_CHANGED,
        data: { name: "foo", value: 2 },
      })
    );
  });
  it("should send a PREF_CHANGED actions when onExperimentUpdated is called", () => {
    sandbox.stub(global.NimbusFeatures.newtab, "getValue").returns({
      prefsButtonIcon: "icon-new",
    });
    feed.onExperimentUpdated();
    assert.calledWith(
      feed.store.dispatch,
      ac.BroadcastToContent({
        type: at.PREF_CHANGED,
        data: {
          name: "featureConfig",
          value: {
            prefsButtonIcon: "icon-new",
          },
        },
      })
    );
  });

  it("should set storage pref on UPDATE_SECTION_PREFS", async () => {
    await feed.onAction({
      type: at.UPDATE_SECTION_PREFS,
      data: { id: "topsites", value: { collapsed: false } },
    });
    assert.calledWith(feed._storage.set, "topsites", { collapsed: false });
  });
  it("should set storage pref with section prefix on UPDATE_SECTION_PREFS", async () => {
    await feed.onAction({
      type: at.UPDATE_SECTION_PREFS,
      data: { id: "topstories", value: { collapsed: false } },
    });
    assert.calledWith(feed._storage.set, "feeds.section.topstories", {
      collapsed: false,
    });
  });
  it("should catch errors on UPDATE_SECTION_PREFS", async () => {
    feed._storage.set.throws(new Error("foo"));
    assert.doesNotThrow(async () => {
      await feed.onAction({
        type: at.UPDATE_SECTION_PREFS,
        data: { id: "topstories", value: { collapsed: false } },
      });
    });
  });
  it("should send OnlyToMain pref update if config for pref has skipBroadcast: true", async () => {
    feed.onPrefChanged("baz", { value: 2, skipBroadcast: true });
    assert.calledWith(
      feed.store.dispatch,
      ac.OnlyToMain({
        type: at.PREF_CHANGED,
        data: { name: "baz", value: { value: 2, skipBroadcast: true } },
      })
    );
  });
  describe("#observe", () => {
    it("should call dispatch from observe", () => {
      feed.observe(undefined, global.Region.REGION_TOPIC);
      assert.calledOnce(feed.store.dispatch);
    });
  });
});

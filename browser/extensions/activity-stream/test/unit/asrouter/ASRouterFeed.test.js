import {_ASRouter, ASRouter} from "lib/ASRouter.jsm";
import {
  EXPERIMENT_PREF,
  FAKE_LOCAL_PROVIDER,
  FakeRemotePageManager
} from "./constants";
import {ASRouterFeed} from "lib/ASRouterFeed.jsm";
import {actionTypes as at} from "common/Actions.jsm";

const ONBOARDING_FINISHED_PREF = "browser.onboarding.notification.finished";

describe("ASRouterFeed", () => {
  let Router;
  let feed;
  let prefs;
  let channel;
  let sandbox;
  let storage;
  beforeEach(() => {
    Router = new _ASRouter({providers: [FAKE_LOCAL_PROVIDER]});
    sandbox = sinon.sandbox.create();
    storage = {
      get: sandbox.stub().returns(Promise.resolve([])),
      set: sandbox.stub().returns(Promise.resolve())
    };
    feed = new ASRouterFeed({router: Router}, storage);
    sandbox.spy(global.Services.prefs, "setBoolPref");

    // Add prefs to feed.store
    prefs = {};
    channel = new FakeRemotePageManager();
    feed.store = {
      _messageChannel: {channel},
      getState: () => ({Prefs: {values: prefs}}),
      dbStorage: {getDbTable: sandbox.stub().returns({})}
    };
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should set .router to the ASRouter singleton if none is specified in options", () => {
    feed = new ASRouterFeed();
    assert.equal(feed.router, ASRouter);

    feed = new ASRouterFeed({});
    assert.equal(feed.router, ASRouter);
  });
  describe("#onAction: INIT", () => {
    it("should initialize the ASRouter if it is not initialized and override onboardin if the experiment pref is true", () => {
      // Router starts out not initialized
      sandbox.stub(feed, "enable");
      prefs[EXPERIMENT_PREF] = true;

      // call .onAction with INIT
      feed.onAction({type: at.INIT});

      assert.calledOnce(feed.enable);
    });
    it("should initialize the MessageCenterRouter and override onboarding", async () => {
      sandbox.stub(Router, "init").returns(Promise.resolve());

      await feed.enable();

      assert.calledWith(Router.init, channel);
      assert.calledOnce(feed.store.dbStorage.getDbTable);
      assert.calledWithExactly(feed.store.dbStorage.getDbTable, "snippets");
      assert.calledWith(global.Services.prefs.setBoolPref, ONBOARDING_FINISHED_PREF, true);
    });
    it("should not re-initialize the ASRouter if it is already initialized", async () => {
      // Router starts initialized
      await Router.init(new FakeRemotePageManager(), storage);
      sinon.stub(Router, "init");
      prefs[EXPERIMENT_PREF] = true;

      // call .onAction with INIT
      feed.onAction({type: at.INIT});

      assert.notCalled(Router.init);
    });
  });
  describe("#onAction: PREF_CHANGE", () => {
    it("should return early if the pref changed does not enable/disable the router", async () => {
      // Router starts initialized
      await Router.init(new FakeRemotePageManager(), storage);
      sinon.stub(Router, "uninit");
      prefs[EXPERIMENT_PREF] = false;

      // call .onAction with INIT
      feed.onAction({type: at.PREF_CHANGED, data: {name: "someOtherPref"}});

      assert.notCalled(Router.uninit);
    });
    it("should uninitialize the ASRouter if it is already initialized and the experiment pref is false", async () => {
      // Router starts initialized
      await Router.init(new FakeRemotePageManager(), storage);
      sinon.stub(Router, "uninit");
      prefs[EXPERIMENT_PREF] = false;

      // call .onAction with INIT
      feed.onAction({type: at.PREF_CHANGED, data: {name: EXPERIMENT_PREF}});

      assert.calledOnce(Router.uninit);
    });
  });
  describe("#onAction: UNINIT", () => {
    it("should uninitialize the ASRouter and restore onboarding", async () => {
      await Router.init(new FakeRemotePageManager(), storage);
      sinon.stub(Router, "uninit");

      feed.onAction({type: at.UNINIT});

      assert.calledOnce(Router.uninit);
      assert.calledWith(global.Services.prefs.setBoolPref, ONBOARDING_FINISHED_PREF, false);
    });
  });
});

import {_ASRouter, ASRouter} from "lib/ASRouter.jsm";
import {FAKE_LOCAL_PROVIDER, FakeRemotePageManager} from "./constants";
import {ASRouterFeed} from "lib/ASRouterFeed.jsm";
import {actionTypes as at} from "common/Actions.jsm";
import {GlobalOverrider} from "test/unit/utils";

describe("ASRouterFeed", () => {
  let Router;
  let feed;
  let channel;
  let sandbox;
  let storage;
  let globals;
  let FakeBookmarkPanelHub;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    globals = new GlobalOverrider();
    FakeBookmarkPanelHub = {
      init: sandbox.stub(),
      uninit: sandbox.stub(),
    };
    globals.set("BookmarkPanelHub", FakeBookmarkPanelHub);

    Router = new _ASRouter({providers: [FAKE_LOCAL_PROVIDER]});
    storage = {
      get: sandbox.stub().returns(Promise.resolve([])),
      set: sandbox.stub().returns(Promise.resolve()),
    };
    feed = new ASRouterFeed({router: Router}, storage);
    channel = new FakeRemotePageManager();
    feed.store = {
      _messageChannel: {channel},
      getState: () => ({}),
      dbStorage: {getDbTable: sandbox.stub().returns({})},
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
    it("should initialize the ASRouter if it is not initialized", () => {
      sandbox.stub(feed, "enable");

      feed.onAction({type: at.INIT});

      assert.calledOnce(feed.enable);
    });
    it("should initialize ASRouter", async () => {
      sandbox.stub(Router, "init").returns(Promise.resolve());

      await feed.enable();

      assert.calledWith(Router.init, channel);
      assert.calledOnce(feed.store.dbStorage.getDbTable);
      assert.calledWithExactly(feed.store.dbStorage.getDbTable, "snippets");
    });
    it("should not re-initialize the ASRouter if it is already initialized", async () => {
      // Router starts initialized
      await Router.init(new FakeRemotePageManager(), storage, () => {});
      sinon.stub(Router, "init");

      // call .onAction with INIT
      feed.onAction({type: at.INIT});

      assert.notCalled(Router.init);
    });
  });
  describe("#onAction: UNINIT", () => {
    it("should uninitialize the ASRouter", async () => {
      await Router.init(new FakeRemotePageManager(), storage, () => {});
      sinon.stub(Router, "uninit");

      feed.onAction({type: at.UNINIT});

      assert.calledOnce(Router.uninit);
    });
  });
});

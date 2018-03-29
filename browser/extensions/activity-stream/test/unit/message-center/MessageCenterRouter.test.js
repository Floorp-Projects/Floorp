import {_MessageCenterRouter, MessageCenterRouter} from "lib/MessageCenterRouter.jsm";
import {CHILD_TO_PARENT_MESSAGE_NAME, EXPERIMENT_PREF, PARENT_TO_CHILD_MESSAGE_NAME} from "./constants";
import {actionTypes as at} from "common/Actions.jsm";
import {MessageCenterFeed} from "lib/MessageCenterFeed.jsm";

const ONBOARDING_FINISHED_PREF = "browser.onboarding.notification.finished";

// Stubs methods on RemotePageManager
class FakeRemotePageManager {
  constructor() {
    this.addMessageListener = sinon.stub();
    this.sendAsyncMessage = sinon.stub();
    this.removeMessageListener = sinon.stub();
  }
}

// Creates a message object that looks like messages returned by RemotePageManager listeners
function createRemoteMessage(action) {
  return {data: action, target: new FakeRemotePageManager()};
}

const FAKE_MESSAGES = [
  {id: "foo", template: "simple_template", content: {title: "Foo", body: "Foo123"}},
  {id: "bar", template: "fancy_template", content: {title: "Foo", body: "Foo123"}},
  {id: "baz", content: {title: "Foo", body: "Foo123"}}
];
const FAKE_MESSAGE_IDS = FAKE_MESSAGES.map(message => message.id);

describe("MessageCenterRouter", () => {
  let Router;
  let channel;
  beforeEach(() => {
    Router = new _MessageCenterRouter({messages: FAKE_MESSAGES});
    channel = new FakeRemotePageManager();
    Router.init(channel);
  });
  describe(".state", () => {
    it("should throw if an attempt to set .state was made", () => {
      assert.throws(() => {
        Router.state = {};
      });
    });
  });
  describe("#init", () => {
    it("should add a message listener to the RemotePageManager for incoming messages", () => {
      assert.calledWith(channel.addMessageListener, CHILD_TO_PARENT_MESSAGE_NAME);
      const [, listenerAdded] = channel.addMessageListener.firstCall.args;
      assert.isFunction(listenerAdded);
    });
  });
  describe("#uninit", () => {
    it("should remove the message listener on the RemotePageManager", () => {
      const [, listenerAdded] = channel.addMessageListener.firstCall.args;
      assert.isFunction(listenerAdded);

      Router.uninit();

      assert.calledWith(channel.removeMessageListener, CHILD_TO_PARENT_MESSAGE_NAME, listenerAdded);
    });
  });
  describe("#onMessage: CONNECT_UI_REQUEST", () => {
    it("should set state.currentId to a message id", async () => {
      await Router.onMessage(createRemoteMessage({type: "CONNECT_UI_REQUEST"}));

      assert.include(FAKE_MESSAGE_IDS, Router.state.currentId);
    });
    it("should send a message back to the to the target", async () => {
      const msg = createRemoteMessage({type: "CONNECT_UI_REQUEST"});
      await Router.onMessage(msg);

      const [currentMessage] = Router.state.messages.filter(message => message.id === Router.state.currentId);
      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "SET_MESSAGE", data: currentMessage});
    });
    it("should send a CLEAR_MESSAGE message and set state.currentId to null if no messages are available", async () => {
      await Router.setState({messages: []});
      const msg = createRemoteMessage({type: "CONNECT_UI_REQUEST"});
      await Router.onMessage(msg);

      assert.isNull(Router.state.currentId);
      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "CLEAR_MESSAGE"});
    });
  });
  describe("#onMessage: BLOCK_MESSAGE_BY_ID", () => {
    it("should add the id to the blockList, state.currentId to null if it is the blocked message id, and send a CLEAR_MESSAGE message", async () => {
      await Router.setState({currentId: "foo"});
      const msg = createRemoteMessage({type: "BLOCK_MESSAGE_BY_ID", data: {id: "foo"}});
      await Router.onMessage(msg);

      assert.isTrue(Router.state.blockList.foo);
      assert.isNull(Router.state.currentId);
      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "CLEAR_MESSAGE"});
    });
  });
  describe("#onMessage: UNBLOCK_MESSAGE_BY_ID", () => {
    it("should remove the id from the blockList", async () => {
      await Router.onMessage(createRemoteMessage({type: "BLOCK_MESSAGE_BY_ID", data: {id: "foo"}}));
      assert.isTrue(Router.state.blockList.foo);
      await Router.onMessage(createRemoteMessage({type: "UNBLOCK_MESSAGE_BY_ID", data: {id: "foo"}}));

      assert.isUndefined(Router.state.blockList.foo);
    });
  });

  describe("#onMessage: ADMIN_CONNECT_STATE", () => {
    it("should send a message containing the whole state", async () => {
      const msg = createRemoteMessage({type: "ADMIN_CONNECT_STATE"});
      await Router.onMessage(msg);

      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "ADMIN_SET_STATE", data: Router.state});
    });
  });
});

describe("MessageCenterFeed", () => {
  let Router;
  let feed;
  let prefs;
  let channel;
  let sandbox;
  beforeEach(() => {
    Router = new _MessageCenterRouter();
    feed = new MessageCenterFeed({router: Router});
    sandbox = sinon.sandbox.create();
    sandbox.spy(global.Services.prefs, "setBoolPref");

    // Add prefs to feed.store
    prefs = {};
    channel = new FakeRemotePageManager();
    feed.store = {_messageChannel: {channel}, getState: () => ({Prefs: {values: prefs}})};
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should set .router to the MessageCenterRouter singleton if none is specified in options", () => {
    feed = new MessageCenterFeed();
    assert.equal(feed.router, MessageCenterRouter);

    feed = new MessageCenterFeed({});
    assert.equal(feed.router, MessageCenterRouter);
  });
  describe("#onAction: INIT", () => {
    it("should initialize the MessageCenterRouter if it is not initialized and override onboardin if the experiment pref is true", () => {
      // Router starts out not initialized
      sinon.stub(Router, "init");
      prefs[EXPERIMENT_PREF] = true;

      // call .onAction with INIT
      feed.onAction({type: at.INIT});

      assert.calledWith(Router.init, channel);
      assert.calledWith(global.Services.prefs.setBoolPref, ONBOARDING_FINISHED_PREF, true);
    });
    it("should not re-initialize the MessageCenterRouter if it is already initialized", () => {
      // Router starts initialized
      Router.init(new FakeRemotePageManager());
      sinon.stub(Router, "init");
      prefs[EXPERIMENT_PREF] = true;

      // call .onAction with INIT
      feed.onAction({type: at.INIT});

      assert.notCalled(Router.init);
    });
  });
  describe("#onAction: PREF_CHANGE", () => {
    it("should uninitialize the MessageCenterRouter if it is already initialized and the experiment pref is false", () => {
      // Router starts initialized
      Router.init(new FakeRemotePageManager());
      sinon.stub(Router, "uninit");
      prefs[EXPERIMENT_PREF] = false;

      // call .onAction with INIT
      feed.onAction({type: at.PREF_CHANGED});

      assert.calledOnce(Router.uninit);
    });
  });
  describe("#onAction: UNINIT", () => {
    it("should uninitialize the MessageCenterRouter and restore onboarding", () => {
      Router.init(new FakeRemotePageManager());
      sinon.stub(Router, "uninit");

      feed.onAction({type: at.UNINIT});

      assert.calledOnce(Router.uninit);
      assert.calledWith(global.Services.prefs.setBoolPref, ONBOARDING_FINISHED_PREF, false);
    });
  });
});

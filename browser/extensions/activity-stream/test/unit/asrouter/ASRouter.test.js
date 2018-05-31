import {
  CHILD_TO_PARENT_MESSAGE_NAME,
  FAKE_LOCAL_MESSAGES,
  FAKE_LOCAL_PROVIDER,
  FAKE_REMOTE_MESSAGES,
  FAKE_REMOTE_PROVIDER,
  FakeRemotePageManager,
  PARENT_TO_CHILD_MESSAGE_NAME
} from "./constants";
import {_ASRouter} from "lib/ASRouter.jsm";

const FAKE_PROVIDERS = [FAKE_LOCAL_PROVIDER, FAKE_REMOTE_PROVIDER];
const ALL_MESSAGE_IDS = [...FAKE_LOCAL_MESSAGES, ...FAKE_REMOTE_MESSAGES].map(message => message.id);
const FAKE_BUNDLE = [FAKE_LOCAL_MESSAGES[1], FAKE_LOCAL_MESSAGES[2]];

// Creates a message object that looks like messages returned by
// RemotePageManager listeners
function fakeAsyncMessage(action) {
  return {data: action, target: new FakeRemotePageManager()};
}

describe("ASRouter", () => {
  let Router;
  let channel;
  let sandbox;
  let blockList;
  let fetchStub;
  let clock;

  function createFakeStorage() {
    return {
      get: sandbox.stub().returns(Promise.resolve(blockList)),
      set: sandbox.stub().returns(Promise.resolve())
    };
  }

  async function createRouterAndInit(providers = FAKE_PROVIDERS) {
    channel = new FakeRemotePageManager();
    Router = new _ASRouter({providers});
    await Router.init(channel, createFakeStorage());
  }

  beforeEach(async () => {
    blockList = [];
    sandbox = sinon.sandbox.create();
    clock = sandbox.useFakeTimers();
    fetchStub = sandbox.stub(global, "fetch")
      .withArgs("http://fake.com/endpoint")
      .resolves({ok: true, status: 200, json: () => Promise.resolve({messages: FAKE_REMOTE_MESSAGES})});
    await createRouterAndInit();
  });
  afterEach(() => {
    sandbox.restore();
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
    it("should set state.blockList to the block list in persistent storage", async () => {
      blockList = ["MESSAGE_ID"];

      Router = new _ASRouter({providers: FAKE_PROVIDERS});
      await Router.init(channel, createFakeStorage());

      assert.deepEqual(Router.state.blockList, ["MESSAGE_ID"]);
    });
    it("should await .loadMessagesFromAllProviders() and add messages from providers to state.messages", async () => {
      Router = new _ASRouter({providers: FAKE_PROVIDERS});

      const loadMessagesSpy = sandbox.spy(Router, "loadMessagesFromAllProviders");
      await Router.init(channel, createFakeStorage());

      assert.calledOnce(loadMessagesSpy);
      assert.isArray(Router.state.messages);
      assert.lengthOf(Router.state.messages, FAKE_LOCAL_MESSAGES.length + FAKE_REMOTE_MESSAGES.length);
    });
  });

  describe("#loadMessagesFromAllProviders", () => {
    function assertRouterContainsMessages(messages) {
      const messageIdsInRouter = Router.state.messages.map(m => m.id);
      for (const message of messages) {
        assert.include(messageIdsInRouter, message.id);
      }
    }

    it("should not trigger an update if not enough time has passed for a provider", async () => {
      await createRouterAndInit([
        {id: "remotey", type: "remote", url: "http://fake.com/endpoint", updateCycleInMs: 300}
      ]);

      const previousState = Router.state;

      // Since we've previously gotten messages during init and we haven't advanced our fake timer,
      // no updates should be triggered.
      await Router.loadMessagesFromAllProviders();
      assert.equal(Router.state, previousState);
    });
    it("should not trigger an update if we only have local providers", async () => {
      await createRouterAndInit([
        {id: "foo", type: "local", messages: FAKE_LOCAL_MESSAGES}
      ]);

      const previousState = Router.state;

      clock.tick(300);

      await Router.loadMessagesFromAllProviders();
      assert.equal(Router.state, previousState);
    });
    it("should update messages for a provider if enough time has passed, without removing messages for other providers", async () => {
      const NEW_MESSAGES = [{id: "new_123"}];
      await createRouterAndInit([
        {id: "remotey", type: "remote", url: "http://fake.com/endpoint", updateCycleInMs: 300},
        {id: "alocalprovider", type: "local", messages: FAKE_LOCAL_MESSAGES}
      ]);
      fetchStub
        .withArgs("http://fake.com/endpoint")
        .resolves({ok: true, status: 200, json: () => Promise.resolve({messages: NEW_MESSAGES})});

      clock.tick(301);
      await Router.loadMessagesFromAllProviders();

      // These are the new messages
      assertRouterContainsMessages(NEW_MESSAGES);
      // These are the local messages that should not have been deleted
      assertRouterContainsMessages(FAKE_LOCAL_MESSAGES);
    });
  });

  describe("blocking", () => {
    it("should not return a blocked message", async () => {
      // Block all messages except the first
      await Router.setState(() => ({blockList: ALL_MESSAGE_IDS.slice(1)}));
      const targetStub = {sendAsyncMessage: sandbox.stub()};

      await Router.sendNextMessage(targetStub, null);

      assert.calledOnce(targetStub.sendAsyncMessage);
      assert.equal(Router.state.lastMessageId, ALL_MESSAGE_IDS[0]);
    });
    it("should not return a message if all messages are blocked", async () => {
      await Router.setState(() => ({blockList: ALL_MESSAGE_IDS}));
      const targetStub = {sendAsyncMessage: sandbox.stub()};

      await Router.sendNextMessage(targetStub, null);

      assert.calledOnce(targetStub.sendAsyncMessage);
      assert.equal(Router.state.lastMessageId, null);
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
    it("should set state.lastMessageId to a message id", async () => {
      await Router.onMessage(fakeAsyncMessage({type: "CONNECT_UI_REQUEST"}));

      assert.include(ALL_MESSAGE_IDS, Router.state.lastMessageId);
    });
    it("should send a message back to the to the target", async () => {
      // force the only message to be a regular message so getRandomItemFromArray picks it
      await Router.setState({messages: [{id: "foo", template: "simple_template", content: {title: "Foo", body: "Foo123"}}]});
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST"});
      await Router.onMessage(msg);
      const [currentMessage] = Router.state.messages.filter(message => message.id === Router.state.lastMessageId);
      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "SET_MESSAGE", data: currentMessage});
    });
    it("should send a message back to the to the target if there is a bundle, too", async () => {
      // force the only message to be a bundled message so getRandomItemFromArray picks it
      await Router.setState({messages: [{id: "foo1", template: "simple_template", bundled: 2, content: {title: "Foo1", body: "Foo123-1"}}]});
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST"});
      await Router.onMessage(msg);
      const [currentMessage] = Router.state.messages.filter(message => message.id === Router.state.lastMessageId);
      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME);
      assert.equal(msg.target.sendAsyncMessage.firstCall.args[1].type, "SET_BUNDLED_MESSAGES");
      assert.equal(msg.target.sendAsyncMessage.firstCall.args[1].data.bundle[0].content, currentMessage.content);
    });
    it("should send a CLEAR_ALL message if no messages are available", async () => {
      await Router.setState({messages: []});
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST"});
      await Router.onMessage(msg);

      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "CLEAR_ALL"});
    });
  });

  describe("#onMessage: BLOCK_MESSAGE_BY_ID", () => {
    it("should add the id to the blockList and broadcast a CLEAR_MESSAGE message with the id", async () => {
      await Router.setState({lastMessageId: "foo"});
      const msg = fakeAsyncMessage({type: "BLOCK_MESSAGE_BY_ID", data: {id: "foo"}});
      await Router.onMessage(msg);

      assert.isTrue(Router.state.blockList.includes("foo"));
      assert.calledWith(channel.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "CLEAR_MESSAGE", data: {id: "foo"}});
    });
  });

  describe("#onMessage: BLOCK_BUNDLE", () => {
    it("should add all the ids in the bundle to the blockList and send a CLEAR_BUNDLE message", async () => {
      const bundleIds = [FAKE_BUNDLE[0].id, FAKE_BUNDLE[1].id];
      await Router.setState({lastMessageId: "foo"});
      const msg = fakeAsyncMessage({type: "BLOCK_BUNDLE", data: {bundle: FAKE_BUNDLE}});
      await Router.onMessage(msg);

      assert.isTrue(Router.state.blockList.includes(FAKE_BUNDLE[0].id));
      assert.isTrue(Router.state.blockList.includes(FAKE_BUNDLE[1].id));
      assert.calledWith(channel.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "CLEAR_BUNDLE"});
      assert.calledOnce(Router._storage.set);
      assert.calledWithExactly(Router._storage.set, "blockList", bundleIds);
    });
  });

  describe("#onMessage: UNBLOCK_MESSAGE_BY_ID", () => {
    it("should remove the id from the blockList", async () => {
      await Router.onMessage(fakeAsyncMessage({type: "BLOCK_MESSAGE_BY_ID", data: {id: "foo"}}));
      assert.isTrue(Router.state.blockList.includes("foo"));
      await Router.onMessage(fakeAsyncMessage({type: "UNBLOCK_MESSAGE_BY_ID", data: {id: "foo"}}));

      assert.isFalse(Router.state.blockList.includes("foo"));
    });
    it("should save the blockList", async () => {
      await Router.onMessage(fakeAsyncMessage({type: "UNBLOCK_MESSAGE_BY_ID", data: {id: "foo"}}));

      assert.calledOnce(Router._storage.set);
      assert.calledWithExactly(Router._storage.set, "blockList", []);
    });
  });

  describe("#onMessage: UNBLOCK_BUNDLE", () => {
    it("should remove all the ids in the bundle from the blockList", async () => {
      await Router.onMessage(fakeAsyncMessage({type: "BLOCK_BUNDLE", data: {bundle: FAKE_BUNDLE}}));
      assert.isTrue(Router.state.blockList.includes(FAKE_BUNDLE[0].id));
      assert.isTrue(Router.state.blockList.includes(FAKE_BUNDLE[1].id));
      await Router.onMessage(fakeAsyncMessage({type: "UNBLOCK_BUNDLE", data: {bundle: FAKE_BUNDLE}}));

      assert.isFalse(Router.state.blockList.includes(FAKE_BUNDLE[0].id));
      assert.isFalse(Router.state.blockList.includes(FAKE_BUNDLE[1].id));
    });
    it("should save the blockList", async () => {
      await Router.onMessage(fakeAsyncMessage({type: "UNBLOCK_BUNDLE", data: {bundle: FAKE_BUNDLE}}));

      assert.calledOnce(Router._storage.set);
      assert.calledWithExactly(Router._storage.set, "blockList", []);
    });
  });

  describe("#onMessage: ADMIN_CONNECT_STATE", () => {
    it("should send a message containing the whole state", async () => {
      const msg = fakeAsyncMessage({type: "ADMIN_CONNECT_STATE"});
      await Router.onMessage(msg);

      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "ADMIN_SET_STATE", data: Router.state});
    });
  });

  describe("#onMessage: CONNECT_UI_REQUEST GET_NEXT_MESSAGE", () => {
    it("should call sendNextMessage on CONNECT_UI_REQUEST", async () => {
      sandbox.stub(Router, "sendNextMessage").resolves();
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST"});

      await Router.onMessage(msg);

      assert.calledOnce(Router.sendNextMessage);
      assert.calledWithExactly(Router.sendNextMessage, sinon.match.instanceOf(FakeRemotePageManager));
    });
    it("should call sendNextMessage on GET_NEXT_MESSAGE", async () => {
      sandbox.stub(Router, "sendNextMessage").resolves();
      const msg = fakeAsyncMessage({type: "GET_NEXT_MESSAGE"});

      await Router.onMessage(msg);

      assert.calledOnce(Router.sendNextMessage);
      assert.calledWithExactly(Router.sendNextMessage, sinon.match.instanceOf(FakeRemotePageManager));
    });
    it("should call _getBundledMessages if we request a message that needs to be bundled", async () => {
      sandbox.stub(Router, "_getBundledMessages");
      // forcefully pick a message which needs to be bundled (the second message in FAKE_LOCAL_MESSAGES)
      const [, testMessage] = Router.state.messages;
      const msg = fakeAsyncMessage({type: "OVERRIDE_MESSAGE", data: {id: testMessage.id}});
      await Router.onMessage(msg);

      assert.calledOnce(Router._getBundledMessages);
    });
    it("should properly pick another message of the same template if it is bundled", async () => {
      Router.sendMessage = sinon.spy();
      // forcefully pick a message which needs to be bundled (the second message in FAKE_LOCAL_MESSAGES)
      const [, testMessage1, testMessage2] = Router.state.messages;
      const msg = fakeAsyncMessage({type: "OVERRIDE_MESSAGE", data: {id: testMessage1.id}});
      await Router.onMessage(msg);

      // Expected object should have some properties of the original message it picked (testMessage1)
      // plus the bundled content of the others that it picked of the same template (testMessage2)
      const expectedObj = {
        template: testMessage1.template,
        provider: testMessage1.provider,
        bundle: [{content: testMessage1.content, id: testMessage1.id}, {content: testMessage2.content, id: testMessage2.id}]
      };
      assert.calledWith(channel.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "SET_BUNDLED_MESSAGES", data: expectedObj});
    });
    it("should get the bundle and send the message if the message has a bundle", async () => {
      sandbox.stub(Router, "sendNextMessage").resolves();
      const msg = fakeAsyncMessage({type: "GET_NEXT_MESSAGE"});
      msg.bundled = 2; // force this message to want to be bundled
      await Router.onMessage(msg);
      assert.calledOnce(Router.sendNextMessage);
    });
  });

  describe("#onMessage: OVERRIDE_MESSAGE", () => {
    it("should broadcast a SET_MESSAGE message to all clients with a particular id", async () => {
      const [testMessage] = Router.state.messages;
      const msg = fakeAsyncMessage({type: "OVERRIDE_MESSAGE", data: {id: testMessage.id}});
      await Router.onMessage(msg);

      assert.calledWith(channel.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "SET_MESSAGE", data: testMessage});
    });
  });

  describe("#onMessage: Onboarding actions", () => {
    it("should call OpenBrowserWindow with a private window on OPEN_PRIVATE_BROWSER_WINDOW", async () => {
      let [testMessage] = Router.state.messages;
      const msg = fakeAsyncMessage({type: "OPEN_PRIVATE_BROWSER_WINDOW", data: testMessage});
      await Router.onMessage(msg);

      assert.calledWith(msg.target.browser.ownerGlobal.OpenBrowserWindow, {private: true});
    });
    it("should call openLinkIn with the correct params on OPEN_URL", async () => {
      sinon.spy(Router, "openLinkIn");
      let [testMessage] = Router.state.messages;
      testMessage.button_action_params = "some/url.com";
      const msg = fakeAsyncMessage({type: "OPEN_URL", data: testMessage});
      await Router.onMessage(msg);

      assert.calledWith(Router.openLinkIn, testMessage.button_action_params, msg.target, {isPrivate: false, where: "tabshifted"});
      assert.calledOnce(msg.target.browser.ownerGlobal.openLinkIn);
    });
    it("should call openLinkIn with the correct params on OPEN_ABOUT_PAGE", async () => {
      sinon.spy(Router, "openLinkIn");
      let [testMessage] = Router.state.messages;
      testMessage.button_action_params = "something";
      const msg = fakeAsyncMessage({type: "OPEN_ABOUT_PAGE", data: testMessage});
      await Router.onMessage(msg);

      assert.calledWith(Router.openLinkIn, `about:${testMessage.button_action_params}`, msg.target, {isPrivate: false, trusted: true, where: "tab"});
      assert.calledOnce(msg.target.browser.ownerGlobal.openTrustedLinkIn);
    });
  });
});

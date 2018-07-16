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
  let getStringPrefStub;
  let addObserverStub;

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
    getStringPrefStub = sandbox.stub(global.Services.prefs, "getStringPref");
    getStringPrefStub.returns("http://fake.com/endpoint");
    addObserverStub = sandbox.stub(global.Services.prefs, "addObserver");

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
    it("should add an observer for each provider with a defined endpointPref", () => {
      assert.calledOnce(addObserverStub);
      assert.calledWith(addObserverStub, "remotePref");
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
    it("should call loadMessagesFromAllProviders on pref endpoint change", async () => {
      sandbox.spy(Router, "loadMessagesFromAllProviders");

      await Router.observe();

      assert.calledOnce(Router.loadMessagesFromAllProviders);
    });
    it("should update provider url on pref change", async () => {
      getStringPrefStub.withArgs("remotePref").returns("baz.com");
      const {length} = Router.state.providers;
      await Router.observe("", "", "remotePref");

      const provider = Router.state.providers.find(p => p.url === "baz.com");

      assert.lengthOf(Router.state.providers, length);
      assert.isDefined(provider);
    });
  });

  describe("#loadMessagesFromAllProviders", () => {
    function assertRouterContainsMessages(messages) {
      const messageIdsInRouter = Router.state.messages.map(m => m.id);
      for (const message of messages) {
        assert.include(messageIdsInRouter, message.id);
      }
    }

    it("should load provider endpoint based on pref", async () => {
      getStringPrefStub.reset();
      getStringPrefStub.returns("example.com");
      await createRouterAndInit();

      assert.calledOnce(getStringPrefStub);
      assert.calledWithExactly(getStringPrefStub, "remotePref", "");
      assert.isDefined(Router.state.providers.find(p => p.url === "example.com"));
    });
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

      await Router.sendNextMessage(targetStub);

      assert.calledOnce(targetStub.sendAsyncMessage);
      assert.equal(Router.state.lastMessageId, ALL_MESSAGE_IDS[0]);
    });
    it("should not return a message if all messages are blocked", async () => {
      await Router.setState(() => ({blockList: ALL_MESSAGE_IDS}));
      const targetStub = {sendAsyncMessage: sandbox.stub()};

      await Router.sendNextMessage(targetStub);

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
      await Router.setState({messages: [{id: "foo1", template: "simple_template", bundled: 1, content: {title: "Foo1", body: "Foo123-1"}}]});
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST"});
      await Router.onMessage(msg);
      const [currentMessage] = Router.state.messages.filter(message => message.id === Router.state.lastMessageId);
      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME);
      assert.equal(msg.target.sendAsyncMessage.firstCall.args[1].type, "SET_BUNDLED_MESSAGES");
      assert.equal(msg.target.sendAsyncMessage.firstCall.args[1].data.bundle[0].content, currentMessage.content);
    });
    it("should return a null bundle if we do not have enough messages to fill the bundle", async () => {
      // force the only message to be a bundled message that needs 2 messages in the bundle
      await Router.setState({messages: [{id: "foo1", template: "simple_template", bundled: 2, content: {title: "Foo1", body: "Foo123-1"}}]});
      const bundle = await Router._getBundledMessages(Router.state.messages[0]);
      assert.equal(bundle, null);
    });
    it("should send a CLEAR_ALL message if no bundle available", async () => {
      // force the only message to be a bundled message that needs 2 messages in the bundle
      await Router.setState({messages: [{id: "foo1", template: "simple_template", bundled: 2, content: {title: "Foo1", body: "Foo123-1"}}]});
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST"});
      await Router.onMessage(msg);
      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "CLEAR_ALL"});
    });
    it("should send a CLEAR_ALL message if no messages are available", async () => {
      await Router.setState({messages: []});
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST"});
      await Router.onMessage(msg);

      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "CLEAR_ALL"});
    });
    it("should add the endpoint provided on CONNECT_UI_REQUEST", async () => {
      const url = "https://snippets-admin.mozilla.org/foo";
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST", data: {endpoint: {url}}});
      await Router.onMessage(msg);

      assert.isDefined(Router.state.providers.find(p => p.url === url));
    });
    it("should add the endpoint provided on ADMIN_CONNECT_STATE", async () => {
      const url = "https://snippets-admin.mozilla.org/foo";
      const msg = fakeAsyncMessage({type: "ADMIN_CONNECT_STATE", data: {endpoint: {url}}});
      await Router.onMessage(msg);

      assert.isDefined(Router.state.providers.find(p => p.url === url));
    });
    it("should not add the same endpoint twice", async () => {
      const url = "https://snippets-admin.mozilla.org/foo";
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST", data: {endpoint: {url}}});
      await Router.onMessage(msg);
      await Router.onMessage(msg);

      assert.lengthOf(Router.state.providers.filter(p => p.url === url), 1);
    });
    it("should not add a url that is not from a whitelisted host", async () => {
      const url = "https://mozilla.org";
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST", data: {endpoint: {url}}});
      await Router.onMessage(msg);

      assert.lengthOf(Router.state.providers.filter(p => p.url === url), 0);
    });
    it("should reject bad urls", async () => {
      const url = "foo";
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST", data: {endpoint: {url}}});
      await Router.onMessage(msg);

      assert.lengthOf(Router.state.providers.filter(p => p.url === url), 0);
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
      assert.calledWithExactly(Router.sendNextMessage, sinon.match.instanceOf(FakeRemotePageManager), {type: "CONNECT_UI_REQUEST"});
    });
    it("should call sendNextMessage on GET_NEXT_MESSAGE", async () => {
      sandbox.stub(Router, "sendNextMessage").resolves();
      const msg = fakeAsyncMessage({type: "GET_NEXT_MESSAGE"});

      await Router.onMessage(msg);

      assert.calledOnce(Router.sendNextMessage);
      assert.calledWithExactly(Router.sendNextMessage, sinon.match.instanceOf(FakeRemotePageManager), {type: "GET_NEXT_MESSAGE"});
    });
    it("should return the preview message if that's available", async () => {
      const expectedObj = {provider: "preview"};
      Router.setState({messages: [expectedObj]});

      await Router.sendNextMessage(channel);

      assert.calledWith(channel.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "SET_MESSAGE", data: expectedObj});
    });
    it("should call _getBundledMessages if we request a message that needs to be bundled", async () => {
      sandbox.stub(Router, "_getBundledMessages").resolves();
      // forcefully pick a message which needs to be bundled (the second message in FAKE_LOCAL_MESSAGES)
      const [, testMessage] = Router.state.messages;
      const msg = fakeAsyncMessage({type: "OVERRIDE_MESSAGE", data: {id: testMessage.id}});
      await Router.onMessage(msg);

      assert.calledOnce(Router._getBundledMessages);
    });
    it("should properly pick another message of the same template if it is bundled; force = true", async () => {
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
      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "SET_BUNDLED_MESSAGES", data: expectedObj});
    });
    it("should properly pick another message of the same template if it is bundled; force = false", async () => {
      // forcefully pick a message which needs to be bundled (the second message in FAKE_LOCAL_MESSAGES)
      const [, testMessage1, testMessage2] = Router.state.messages;
      const msg = fakeAsyncMessage({type: "OVERRIDE_MESSAGE", data: {id: testMessage1.id}});
      await Router.setMessageById(testMessage1.id, msg.target, false);

      // Expected object should have some properties of the original message it picked (testMessage1)
      // plus the bundled content of the others that it picked of the same template (testMessage2)
      const expectedObj = {
        template: testMessage1.template,
        provider: testMessage1.provider,
        bundle: [{content: testMessage1.content, id: testMessage1.id}, {content: testMessage2.content, id: testMessage2.id}]
      };
      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "SET_BUNDLED_MESSAGES", data: expectedObj});
    });
    it("should get the bundle and send the message if the message has a bundle", async () => {
      sandbox.stub(Router, "sendNextMessage").resolves();
      const msg = fakeAsyncMessage({type: "GET_NEXT_MESSAGE"});
      msg.bundled = 2; // force this message to want to be bundled
      await Router.onMessage(msg);
      assert.calledOnce(Router.sendNextMessage);
    });
  });

  describe("#onMessage: TRIGGER", () => {
    it("should pass the trigger to ASRouterTargeting on TRIGGER message", async () => {
      sandbox.stub(Router, "_findMessage").resolves();
      const msg = fakeAsyncMessage({type: "TRIGGER", data: {trigger: "firstRun"}});
      await Router.onMessage(msg);

      assert.calledOnce(Router._findMessage);
      assert.deepEqual(Router._findMessage.firstCall.args[2], {trigger: "firstRun"});
    });
    it("consider the trigger when picking a message", async () => {
      let messages = [
        {id: "foo1", template: "simple_template", bundled: 1, trigger: "foo", content: {title: "Foo1", body: "Foo123-1"}}
      ];

      const {target, data} = fakeAsyncMessage({type: "TRIGGER", data: {trigger: "foo"}});
      let message = await Router._findMessage(messages, target, data.data);
      assert.equal(message, messages[0]);
    });
    it("should pick a message with the right targeting and trigger", async () => {
      let messages = [
        {id: "foo1", template: "simple_template", bundled: 2, trigger: "foo", content: {title: "Foo1", body: "Foo123-1"}},
        {id: "foo2", template: "simple_template", bundled: 2, trigger: "bar", content: {title: "Foo2", body: "Foo123-2"}},
        {id: "foo3", template: "simple_template", bundled: 2, trigger: "foo", content: {title: "Foo3", body: "Foo123-3"}}
      ];
      await Router.setState({messages});
      const {target, data} = fakeAsyncMessage({type: "TRIGGER", data: {trigger: "foo"}});
      let {bundle} = await Router._getBundledMessages(messages[0], target, data.data);
      assert.equal(bundle.length, 2);
      // it should have picked foo1 and foo3 only
      assert.isTrue(bundle.every(elem => elem.id === "foo1" || elem.id === "foo3"));
    });
  });

  describe("#onMessage: OVERRIDE_MESSAGE", () => {
    it("should broadcast a SET_MESSAGE message to all clients with a particular id", async () => {
      const [testMessage] = Router.state.messages;
      const msg = fakeAsyncMessage({type: "OVERRIDE_MESSAGE", data: {id: testMessage.id}});
      await Router.onMessage(msg);

      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "SET_MESSAGE", data: testMessage});
    });

    it("should broadcast CLEAR_ALL if provided id did not resolve to a message", async () => {
      const msg = fakeAsyncMessage({type: "OVERRIDE_MESSAGE", data: {id: -1}});
      await Router.onMessage(msg);

      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "CLEAR_ALL"});
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

  describe("valid preview endpoint", () => {
    it("should report an error if url protocol is not https", () => {
      sandbox.stub(Cu, "reportError");

      assert.equal(false, Router._validPreviewEndpoint("http://foo.com"));
      assert.calledTwice(Cu.reportError);
    });
  });
});

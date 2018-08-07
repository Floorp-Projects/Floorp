import {_ASRouter, MessageLoaderUtils} from "lib/ASRouter.jsm";
import {
  CHILD_TO_PARENT_MESSAGE_NAME,
  FAKE_LOCAL_MESSAGES,
  FAKE_LOCAL_PROVIDER,
  FAKE_LOCAL_PROVIDERS,
  FAKE_REMOTE_MESSAGES,
  FAKE_REMOTE_PROVIDER,
  FakeRemotePageManager,
  PARENT_TO_CHILD_MESSAGE_NAME
} from "./constants";
import {ASRouterTriggerListeners} from "lib/ASRouterTriggerListeners.jsm";

const MESSAGE_PROVIDER_PREF_NAME = "browser.newtabpage.activity-stream.asrouter.messageProviders";
const FAKE_PROVIDERS = [FAKE_LOCAL_PROVIDER, FAKE_REMOTE_PROVIDER];
const ALL_MESSAGE_IDS = [...FAKE_LOCAL_MESSAGES, ...FAKE_REMOTE_MESSAGES].map(message => message.id);
const FAKE_BUNDLE = [FAKE_LOCAL_MESSAGES[1], FAKE_LOCAL_MESSAGES[2]];
const ONE_DAY = 24 * 60 * 60 * 1000;

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
  let impressions;
  let fetchStub;
  let clock;
  let getStringPrefStub;
  let addObserverStub;
  let dispatchStub;

  function createFakeStorage() {
    const getStub = sandbox.stub();
    getStub.withArgs("blockList").returns(Promise.resolve(blockList));
    getStub.withArgs("impressions").returns(Promise.resolve(impressions));
    return {
      get: getStub,
      set: sandbox.stub().returns(Promise.resolve())
    };
  }

  function setMessageProviderPref(value, prefName = MESSAGE_PROVIDER_PREF_NAME) {
    getStringPrefStub
      .withArgs(prefName, "")
      .returns(JSON.stringify(value));
  }

  async function createRouterAndInit(providers = FAKE_PROVIDERS) {
    setMessageProviderPref(providers);
    channel = new FakeRemotePageManager();
    dispatchStub = sandbox.stub();
    Router = new _ASRouter(MESSAGE_PROVIDER_PREF_NAME, FAKE_LOCAL_PROVIDERS);
    await Router.init(channel, createFakeStorage(), dispatchStub);
  }

  beforeEach(async () => {
    blockList = [];
    impressions = {};
    sandbox = sinon.sandbox.create();
    clock = sandbox.useFakeTimers();
    fetchStub = sandbox.stub(global, "fetch")
      .withArgs("http://fake.com/endpoint")
      .resolves({ok: true, status: 200, json: () => Promise.resolve({messages: FAKE_REMOTE_MESSAGES})});
    getStringPrefStub = sandbox.stub(global.Services.prefs, "getStringPref");
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
    it("should add an observer for the messageProviderPref", () => {
      assert.calledOnce(addObserverStub);
      assert.calledWith(addObserverStub, MESSAGE_PROVIDER_PREF_NAME);
    });
    it("should set state.blockList to the block list in persistent storage", async () => {
      blockList = ["foo"];
      Router = new _ASRouter({providers: FAKE_PROVIDERS});
      await Router.init(channel, createFakeStorage(), dispatchStub);

      assert.deepEqual(Router.state.blockList, ["foo"]);
    });
    it("should set state.impressions to the impressions object in persistent storage", async () => {
      // Note that impressions are only kept if a message exists in router and has a .frequency property,
      // otherwise they will be cleaned up by .cleanupImpressions()
      const testMessage = {id: "foo", frequency: {lifetimeCap: 10}};
      impressions = {foo: [0, 1, 2]};

      Router = new _ASRouter({providers: [{id: "onboarding", type: "local", messages: [testMessage]}]});
      await Router.init(channel, createFakeStorage(), dispatchStub);

      assert.deepEqual(Router.state.impressions, impressions);
    });
    it("should await .loadMessagesFromAllProviders() and add messages from providers to state.messages", async () => {
      Router = new _ASRouter(MESSAGE_PROVIDER_PREF_NAME, FAKE_LOCAL_PROVIDERS);

      const loadMessagesSpy = sandbox.spy(Router, "loadMessagesFromAllProviders");
      await Router.init(channel, createFakeStorage(), dispatchStub);

      assert.calledOnce(loadMessagesSpy);
      assert.isArray(Router.state.messages);
      assert.lengthOf(Router.state.messages, FAKE_LOCAL_MESSAGES.length + FAKE_REMOTE_MESSAGES.length);
    });
    it("should call loadMessagesFromAllProviders on pref change", async () => {
      sandbox.spy(Router, "loadMessagesFromAllProviders");

      await Router.observe();

      assert.calledOnce(Router.loadMessagesFromAllProviders);
    });
    it("should update provider on pref change", async () => {
      const modifiedRemoteProvider = Object.assign({}, FAKE_REMOTE_PROVIDER, {url: "baz.com"});
      setMessageProviderPref([FAKE_LOCAL_PROVIDER, modifiedRemoteProvider]);

      const {length} = Router.state.providers;
      await Router.observe("", "", MESSAGE_PROVIDER_PREF_NAME);

      const provider = Router.state.providers.find(p => p.url === "baz.com");
      assert.lengthOf(Router.state.providers, length);
      assert.isDefined(provider);
    });
    it("should load additional whitelisted hosts", async () => {
      getStringPrefStub.returns("[\"whitelist.com\"]");
      await createRouterAndInit();

      assert.propertyVal(Router.WHITELIST_HOSTS, "whitelist.com", "preview");
      // Should still include the defaults
      assert.lengthOf(Object.keys(Router.WHITELIST_HOSTS), 3);
    });
    it("should fallback to defaults if pref parsing fails", async () => {
      getStringPrefStub.returns("err");
      await createRouterAndInit();

      assert.lengthOf(Object.keys(Router.WHITELIST_HOSTS), 2);
      assert.propertyVal(Router.WHITELIST_HOSTS, "snippets-admin.mozilla.org", "preview");
      assert.propertyVal(Router.WHITELIST_HOSTS, "activity-stream-icons.services.mozilla.com", "production");
    });
    it("should set this.dispatchToAS to the third parameter passed to .init()", async () => {
      assert.equal(Router.dispatchToAS, dispatchStub);
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
    it("should parse the triggers in the messages and register the trigger listeners", async () => {
      sandbox.spy(ASRouterTriggerListeners.get("openURL"), "init");

      /* eslint-disable object-curly-newline */ /* eslint-disable object-property-newline */
      await createRouterAndInit([
        {id: "foo", type: "local", messages: [
          {id: "foo", template: "simple_template", trigger: {id: "firstRun"}, content: {title: "Foo", body: "Foo123"}},
          {id: "bar1", template: "simple_template", trigger: {id: "openURL", params: ["www.mozilla.org", "www.mozilla.com"]}, content: {title: "Bar1", body: "Bar123"}},
          {id: "bar2", template: "simple_template", trigger: {id: "openURL", params: ["www.example.com"]}, content: {title: "Bar2", body: "Bar123"}}
        ]}
      ]);
      /* eslint-enable object-curly-newline */ /* eslint-enable object-property-newline */

      assert.calledTwice(ASRouterTriggerListeners.get("openURL").init);
      assert.calledWithExactly(ASRouterTriggerListeners.get("openURL").init,
        Router._triggerHandler, ["www.mozilla.org", "www.mozilla.com"]);
      assert.calledWithExactly(ASRouterTriggerListeners.get("openURL").init,
        Router._triggerHandler, ["www.example.com"]);
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
    it("should unregister the trigger listeners", () => {
      for (const listener of ASRouterTriggerListeners.values()) {
        sandbox.spy(listener, "uninit");
      }

      Router.uninit();

      for (const listener of ASRouterTriggerListeners.values()) {
        assert.calledOnce(listener.uninit);
      }
    });
    it("should set .dispatchToAS to null", () => {
      Router.uninit();
      assert.isNull(Router.dispatchToAS);
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
    it("should properly order the message's bundle if specified", async () => {
      // force the only messages to be a bundled messages so getRandomItemFromArray picks one of them
      const firstMessage = {id: "foo2", template: "simple_template", bundled: 2, order: 1, content: {title: "Foo2", body: "Foo123-2"}};
      const secondMessage = {id: "foo1", template: "simple_template", bundled: 2, order: 2, content: {title: "Foo1", body: "Foo123-1"}};
      await Router.setState({messages: [secondMessage, firstMessage]});
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST"});
      await Router.onMessage(msg);
      assert.calledWith(msg.target.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME);
      assert.equal(msg.target.sendAsyncMessage.firstCall.args[1].type, "SET_BUNDLED_MESSAGES");
      assert.equal(msg.target.sendAsyncMessage.firstCall.args[1].data.bundle[0].content, firstMessage.content);
      assert.equal(msg.target.sendAsyncMessage.firstCall.args[1].data.bundle[1].content, secondMessage.content);
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
    it("should make a request to the provided endpoint on CONNECT_UI_REQUEST", async () => {
      const url = "https://snippets-admin.mozilla.org/foo";
      const msg = fakeAsyncMessage({type: "CONNECT_UI_REQUEST", data: {endpoint: {url}}});
      await Router.onMessage(msg);

      assert.calledWith(global.fetch, url);
      assert.lengthOf(Router.state.providers.filter(p => p.url === url), 0);
    });
    it("should make a request to the provided endpoint on ADMIN_CONNECT_STATE and remove the endpoint", async () => {
      const url = "https://snippets-admin.mozilla.org/foo";
      const msg = fakeAsyncMessage({type: "ADMIN_CONNECT_STATE", data: {endpoint: {url}}});
      await Router.onMessage(msg);

      assert.calledWith(global.fetch, url);
      assert.lengthOf(Router.state.providers.filter(p => p.url === url), 0);
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
      assert.calledWithExactly(Router.sendNextMessage, sinon.match.instanceOf(FakeRemotePageManager), {});
    });
    it("should call sendNextMessage on GET_NEXT_MESSAGE", async () => {
      sandbox.stub(Router, "sendNextMessage").resolves();
      const msg = fakeAsyncMessage({type: "GET_NEXT_MESSAGE"});

      await Router.onMessage(msg);

      assert.calledOnce(Router.sendNextMessage);
      assert.calledWithExactly(Router.sendNextMessage, sinon.match.instanceOf(FakeRemotePageManager), {});
    });
    it("should return the preview message if that's available and remove it from Router.state", async () => {
      const expectedObj = {provider: "preview"};
      Router.setState({messages: [expectedObj]});

      await Router.sendNextMessage(channel);

      assert.calledWith(channel.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "SET_MESSAGE", data: expectedObj});
      assert.isUndefined(Router.state.messages.find(m => m.provider === "preview"));
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
        bundle: [{content: testMessage1.content, id: testMessage1.id, order: 1}, {content: testMessage2.content, id: testMessage2.id}]
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
        bundle: [{content: testMessage1.content, id: testMessage1.id, order: 1}, {content: testMessage2.content, id: testMessage2.id, order: 2}]
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
      const msg = fakeAsyncMessage({type: "TRIGGER", data: {trigger: {id: "firstRun"}}});
      await Router.onMessage(msg);

      assert.calledOnce(Router._findMessage);
      assert.deepEqual(Router._findMessage.firstCall.args[2], {id: "firstRun"});
    });
    it("consider the trigger when picking a message", async () => {
      let messages = [
        {id: "foo1", template: "simple_template", bundled: 1, trigger: {id: "foo"}, content: {title: "Foo1", body: "Foo123-1"}}
      ];

      const {target, data} = fakeAsyncMessage({type: "TRIGGER", data: {trigger: {id: "foo"}}});
      let message = await Router._findMessage(messages, target, data.data.trigger);
      assert.equal(message, messages[0]);
    });
    it("should pick a message with the right targeting and trigger", async () => {
      let messages = [
        {id: "foo1", template: "simple_template", bundled: 2, trigger: {id: "foo"}, content: {title: "Foo1", body: "Foo123-1"}},
        {id: "foo2", template: "simple_template", bundled: 2, trigger: {id: "bar"}, content: {title: "Foo2", body: "Foo123-2"}},
        {id: "foo3", template: "simple_template", bundled: 2, trigger: {id: "foo"}, content: {title: "Foo3", body: "Foo123-3"}}
      ];
      await Router.setState({messages});
      const {target, data} = fakeAsyncMessage({type: "TRIGGER", data: {trigger: {id: "foo"}}});
      let {bundle} = await Router._getBundledMessages(messages[0], target, data.data.trigger);
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

  describe("#onMessage: INSTALL_ADDON_FROM_URL", () => {
    it("should call installAddonFromURL with correct arguments", async () => {
      sandbox.stub(MessageLoaderUtils, "installAddonFromURL").resolves(null);
      const msg = fakeAsyncMessage({type: "INSTALL_ADDON_FROM_URL", data: {url: "foo.com"}});

      await Router.onMessage(msg);

      assert.calledOnce(MessageLoaderUtils.installAddonFromURL);
      assert.calledWithExactly(MessageLoaderUtils.installAddonFromURL, msg.target.browser, "foo.com");
    });
  });

  describe("_triggerHandler", () => {
    it("should call #onMessage with the correct trigger", () => {
      sinon.spy(Router, "onMessage");
      const target = {};
      const trigger = {id: "FAKE_TRIGGER", param: "some fake param"};
      Router._triggerHandler(target, trigger);
      assert.calledOnce(Router.onMessage);
      assert.calledWithExactly(Router.onMessage, {target, data: {type: "TRIGGER", trigger}});
    });
  });

  describe("valid preview endpoint", () => {
    it("should report an error if url protocol is not https", () => {
      sandbox.stub(Cu, "reportError");

      assert.equal(false, Router._validPreviewEndpoint("http://foo.com"));
      assert.calledTwice(Cu.reportError);
    });
  });

  describe("impressions", () => {
    it("should add an impression and update _storage with the current time if the message frequency caps", async () => {
      clock.tick(42);
      const msg = fakeAsyncMessage({type: "IMPRESSION", data: {id: "foo", frequency: {lifetime: 5}}});
      await Router.onMessage(msg);

      assert.isArray(Router.state.impressions.foo);
      assert.deepEqual(Router.state.impressions.foo, [42]);
      assert.calledWith(Router._storage.set, "impressions", {foo: [42]});
    });
    it("should not add an impression if the message doesn't have frequency caps", async () => {
      // Note that storage.set is called during initialization, so it needs to be reset
      Router._storage.set.reset();
      clock.tick(42);
      const msg = fakeAsyncMessage({type: "IMPRESSION", data: {id: "foo"}});
      await Router.onMessage(msg);

      assert.notProperty(Router.state.impressions, "foo");
      assert.notCalled(Router._storage.set);
    });
    describe("getLongestPeriod", () => {
      it("should return the period if there is only one definition", () => {
        const message = {id: "foo", frequency: {custom: [{period: 200, cap: 2}]}};
        assert.equal(Router.getLongestPeriod(message), 200);
      });
      it("should return the longest period if there are more than one definitions", () => {
        const message = {id: "foo", frequency: {custom: [{period: 1000, cap: 3}, {period: ONE_DAY, cap: 5}, {period: 100, cap: 2}]}};
        assert.equal(Router.getLongestPeriod(message), ONE_DAY);
      });
      it("should return null if there are is no .frequency", () => {
        const message = {id: "foo"};
        assert.isNull(Router.getLongestPeriod(message));
      });
      it("should return null if there are is no .frequency.custom", () => {
        const message = {id: "foo", frequency: {lifetime: 10}};
        assert.isNull(Router.getLongestPeriod(message));
      });
    });
    describe("cleanup on init", () => {
      it("should clear impressions for messages which do not exist in state.messages", async () => {
        const messages = [{id: "foo", frequency: {lifetime: 10}}];
        impressions = {foo: [0], bar: [0, 1]};
        // Impressions for "bar" should be removed since that id does not exist in messages
        const result = {foo: [0]};

        await createRouterAndInit([{id: "onboarding", type: "local", messages}]);
        assert.calledWith(Router._storage.set, "impressions", result);
        assert.deepEqual(Router.state.impressions, result);
      });
      it("should clear impressions older than the period if no lifetime impression cap is included", async () => {
        const CURRENT_TIME = ONE_DAY * 2;
        clock.tick(CURRENT_TIME);
        const messages = [{id: "foo", frequency: {custom: [{period: ONE_DAY, cap: 5}]}}];
        impressions = {foo: [0, 1, CURRENT_TIME - 10]};
        // Only 0 and 1 are more than 24 hours before CURRENT_TIME
        const result = {foo: [CURRENT_TIME - 10]};

        await createRouterAndInit([{id: "onboarding", type: "local", messages}]);
        assert.calledWith(Router._storage.set, "impressions", result);
        assert.deepEqual(Router.state.impressions, result);
      });
      it("should clear impressions older than the longest period if no lifetime impression cap is included", async () => {
        const CURRENT_TIME = ONE_DAY * 2;
        clock.tick(CURRENT_TIME);
        const messages = [{id: "foo", frequency: {custom: [{period: ONE_DAY, cap: 5}, {period: 100, cap: 2}]}}];
        impressions = {foo: [0, 1, CURRENT_TIME - 10]};
        // Only 0 and 1 are more than 24 hours before CURRENT_TIME
        const result = {foo: [CURRENT_TIME - 10]};

        await createRouterAndInit([{id: "onboarding", type: "local", messages}]);
        assert.calledWith(Router._storage.set, "impressions", result);
        assert.deepEqual(Router.state.impressions, result);
      });
      it("should clear impressions if they are not properly formatted", async () => {
        const messages = [{id: "foo", frequency: {lifetime: 10}}];
        // this is impromperly formatted since impressions are supposed to be an array
        impressions = {foo: 0};
        const result = {};

        await createRouterAndInit([{id: "onboarding", type: "local", messages}]);
        assert.calledWith(Router._storage.set, "impressions", result);
        assert.deepEqual(Router.state.impressions, result);
      });
      it("should not clear impressions for messages which do exist in state.messages", async () => {
        const messages = [{id: "foo", frequency: {lifetime: 10}}, {id: "bar", frequency: {lifetime: 10}}];
        impressions = {foo: [0], bar: []};

        await createRouterAndInit([{id: "onboarding", type: "local", messages}]);
        assert.notCalled(Router._storage.set);
        assert.deepEqual(Router.state.impressions, impressions);
      });
    });
  });

  describe("handle targeting errors", () => {
    it("should dispatch an event when a targeting expression throws an error", async () => {
      sandbox.stub(global.FilterExpressions, "eval").returns(Promise.reject(new Error("fake error")));
      await Router.setState({messages: [{id: "foo", targeting: "foo2.[[("}]});
      const msg = fakeAsyncMessage({type: "GET_NEXT_MESSAGE"});
      await Router.onMessage(msg);

      assert.calledOnce(dispatchStub);
      const [action] = dispatchStub.firstCall.args;
      assert.equal(action.type, "AS_ROUTER_TELEMETRY_USER_EVENT");
      assert.equal(action.data.message_id, "foo");
    });
  });
});

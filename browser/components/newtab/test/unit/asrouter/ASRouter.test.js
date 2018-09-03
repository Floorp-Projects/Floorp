import {_ASRouter, MessageLoaderUtils} from "lib/ASRouter.jsm";
import {
  CHILD_TO_PARENT_MESSAGE_NAME,
  FAKE_LOCAL_MESSAGES,
  FAKE_LOCAL_PROVIDER,
  FAKE_LOCAL_PROVIDERS,
  FAKE_REMOTE_MESSAGES,
  FAKE_REMOTE_PROVIDER,
  FAKE_REMOTE_SETTINGS_PROVIDER,
  FakeRemotePageManager,
  PARENT_TO_CHILD_MESSAGE_NAME
} from "./constants";
import {ASRouterTriggerListeners} from "lib/ASRouterTriggerListeners.jsm";
import {CFRPageActions} from "lib/CFRPageActions.jsm";
import {GlobalOverrider} from "test/unit/utils";
import ProviderResponseSchema from "content-src/asrouter/schemas/provider-response.schema.json";

const MESSAGE_PROVIDER_PREF_NAME = "browser.newtabpage.activity-stream.asrouter.messageProviders";
const FAKE_PROVIDERS = [FAKE_LOCAL_PROVIDER, FAKE_REMOTE_PROVIDER, FAKE_REMOTE_SETTINGS_PROVIDER];
const ALL_MESSAGE_IDS = [...FAKE_LOCAL_MESSAGES, ...FAKE_REMOTE_MESSAGES].map(message => message.id);
const FAKE_BUNDLE = [FAKE_LOCAL_MESSAGES[1], FAKE_LOCAL_MESSAGES[2]];
const ONE_DAY = 24 * 60 * 60 * 1000;

// Creates a message object that looks like messages returned by
// RemotePageManager listeners
function fakeAsyncMessage(action) {
  return {data: action, target: new FakeRemotePageManager()};
}
// Create a message that looks like a user action
function fakeExecuteUserAction(action) {
  return fakeAsyncMessage({data: action, type: "USER_ACTION"});
}

describe("ASRouter", () => {
  let Router;
  let channel;
  let sandbox;
  let messageBlockList;
  let providerBlockList;
  let messageImpressions;
  let providerImpressions;
  let fetchStub;
  let clock;
  let getStringPrefStub;
  let addObserverStub;
  let dispatchStub;

  function createFakeStorage() {
    const getStub = sandbox.stub();
    getStub.withArgs("messageBlockList").returns(Promise.resolve(messageBlockList));
    getStub.withArgs("providerBlockList").returns(Promise.resolve(providerBlockList));
    getStub.withArgs("messageImpressions").returns(Promise.resolve(messageImpressions));
    getStub.withArgs("providerImpressions").returns(Promise.resolve(providerImpressions));
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
    messageBlockList = [];
    providerBlockList = [];
    messageImpressions = {};
    providerImpressions = {};
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
    it("should set state.messageBlockList to the block list in persistent storage", async () => {
      messageBlockList = ["foo"];
      Router = new _ASRouter({providers: FAKE_PROVIDERS});
      await Router.init(channel, createFakeStorage(), dispatchStub);

      assert.deepEqual(Router.state.messageBlockList, ["foo"]);
    });
    it("should set state.messageImpressions to the messageImpressions object in persistent storage", async () => {
      // Note that messageImpressions are only kept if a message exists in router and has a .frequency property,
      // otherwise they will be cleaned up by .cleanupImpressions()
      const testMessage = {id: "foo", frequency: {lifetimeCap: 10}};
      messageImpressions = {foo: [0, 1, 2]};

      Router = new _ASRouter({providers: [{id: "onboarding", type: "local", messages: [testMessage]}]});
      await Router.init(channel, createFakeStorage(), dispatchStub);

      assert.deepEqual(Router.state.messageImpressions, messageImpressions);
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
    it("should update the list of providers on pref change", async () => {
      const modifiedRemoteProvider = Object.assign({}, FAKE_REMOTE_PROVIDER, {url: "baz.com"});
      setMessageProviderPref([FAKE_LOCAL_PROVIDER, modifiedRemoteProvider, FAKE_REMOTE_SETTINGS_PROVIDER]);

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
        {id: "remotey", type: "remote", enabled: true, url: "http://fake.com/endpoint", updateCycleInMs: 300}
      ]);

      const previousState = Router.state;

      // Since we've previously gotten messages during init and we haven't advanced our fake timer,
      // no updates should be triggered.
      await Router.loadMessagesFromAllProviders();
      assert.equal(Router.state, previousState);
    });
    it("should not trigger an update if we only have local providers", async () => {
      await createRouterAndInit([
        {id: "foo", type: "local", enabled: true, messages: FAKE_LOCAL_MESSAGES}
      ]);

      const previousState = Router.state;

      clock.tick(300);

      await Router.loadMessagesFromAllProviders();
      assert.equal(Router.state, previousState);
    });
    it("should update messages for a provider if enough time has passed, without removing messages for other providers", async () => {
      const NEW_MESSAGES = [{id: "new_123"}];
      await createRouterAndInit([
        {id: "remotey", type: "remote", url: "http://fake.com/endpoint", enabled: true, updateCycleInMs: 300},
        {id: "alocalprovider", type: "local", enabled: true, messages: FAKE_LOCAL_MESSAGES}
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
        {id: "foo", type: "local", enabled: true, messages: [
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
    it("should gracefully handle RemoteSettings blowing up", async () => {
      sandbox.stub(MessageLoaderUtils, "_getRemoteSettingsMessages").rejects("fake error");
      await createRouterAndInit();
    });
  });

  describe("#_updateMessageProviders", () => {
    it("should correctly replace %STARTPAGE_VERSION% in remote provider urls", () => {
      // If this test fails, you need to update the constant STARTPAGE_VERSION in
      // ASRouter.jsm to match the `version` property of provider-response-schema.json
      const expectedStartpageVersion = ProviderResponseSchema.version;
      const provider = {id: "foo", enabled: true, type: "remote", url: "https://www.mozilla.org/%STARTPAGE_VERSION%/"};
      setMessageProviderPref([provider]);
      Router._updateMessageProviders();
      assert.equal(Router.state.providers[0].url, `https://www.mozilla.org/${expectedStartpageVersion}/`);
    });
    it("should replace other params in remote provider urls by calling Services.urlFormater.formatURL", () => {
      const url = "https://www.example.com/";
      const replacedUrl = "https://www.foo.bar/";
      const stub = sandbox.stub(global.Services.urlFormatter, "formatURL")
        .withArgs(url)
        .returns(replacedUrl);
      const provider = {id: "foo", enabled: true, type: "remote", url};
      setMessageProviderPref([provider]);
      Router._updateMessageProviders();
      assert.calledOnce(stub);
      assert.calledWithExactly(stub, url);
      assert.equal(Router.state.providers[0].url, replacedUrl);
    });
    it("should only add the providers that are enabled", () => {
      const providers = [
        {id: "foo", enabled: false, type: "remote", url: "https://www.foo.com/"},
        {id: "bar", enabled: true, type: "remote", url: "https://www.bar.com/"}
      ];
      setMessageProviderPref(providers);
      Router._updateMessageProviders();
      assert.equal(Router.state.providers.length, 1);
      assert.equal(Router.state.providers[0].id, providers[1].id);
    });
  });

  describe("blocking", () => {
    it("should not return a blocked message", async () => {
      // Block all messages except the first
      await Router.setState(() => ({messageBlockList: ALL_MESSAGE_IDS.slice(1)}));
      const targetStub = {sendAsyncMessage: sandbox.stub()};

      await Router.sendNextMessage(targetStub);

      assert.calledOnce(targetStub.sendAsyncMessage);
      assert.equal(Router.state.lastMessageId, ALL_MESSAGE_IDS[0]);
    });
    it("should not return a message from a blocked provider", async () => {
      // There are only two providers; block the FAKE_LOCAL_PROVIDER, leaving
      // only FAKE_REMOTE_PROVIDER unblocked, which provides only one message
      await Router.setState(() => ({providerBlockList: [FAKE_LOCAL_PROVIDER.id]}));
      const targetStub = {sendAsyncMessage: sandbox.stub()};

      await Router.sendNextMessage(targetStub);

      assert.calledOnce(targetStub.sendAsyncMessage);
      assert.equal(Router.state.lastMessageId, FAKE_REMOTE_MESSAGES[0].id);
    });
    it("should not return a message if all messages are blocked", async () => {
      await Router.setState(() => ({messageBlockList: ALL_MESSAGE_IDS}));
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
    it("should add the id to the messageBlockList and broadcast a CLEAR_MESSAGE message with the id", async () => {
      await Router.setState({lastMessageId: "foo"});
      const msg = fakeAsyncMessage({type: "BLOCK_MESSAGE_BY_ID", data: {id: "foo"}});
      await Router.onMessage(msg);

      assert.isTrue(Router.state.messageBlockList.includes("foo"));
      assert.calledWith(channel.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "CLEAR_MESSAGE", data: {id: "foo"}});
    });
  });

  describe("#onMessage: BLOCK_PROVIDER_BY_ID", () => {
    it("should add the provider id to the providerBlockList and broadcast a CLEAR_PROVIDER with the provider id", async () => {
      const msg = fakeAsyncMessage({type: "BLOCK_PROVIDER_BY_ID", data: {id: "bar"}});
      await Router.onMessage(msg);

      assert.isTrue(Router.state.providerBlockList.includes("bar"));
      assert.calledWith(channel.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "CLEAR_PROVIDER", data: {id: "bar"}});
    });
  });

  describe("#onMessage: BLOCK_BUNDLE", () => {
    it("should add all the ids in the bundle to the messageBlockList and send a CLEAR_BUNDLE message", async () => {
      const bundleIds = [FAKE_BUNDLE[0].id, FAKE_BUNDLE[1].id];
      await Router.setState({lastMessageId: "foo"});
      const msg = fakeAsyncMessage({type: "BLOCK_BUNDLE", data: {bundle: FAKE_BUNDLE}});
      await Router.onMessage(msg);

      assert.isTrue(Router.state.messageBlockList.includes(FAKE_BUNDLE[0].id));
      assert.isTrue(Router.state.messageBlockList.includes(FAKE_BUNDLE[1].id));
      assert.calledWith(channel.sendAsyncMessage, PARENT_TO_CHILD_MESSAGE_NAME, {type: "CLEAR_BUNDLE"});
      assert.calledWithExactly(Router._storage.set, "messageBlockList", bundleIds);
    });
  });

  describe("#onMessage: UNBLOCK_MESSAGE_BY_ID", () => {
    it("should remove the id from the messageBlockList", async () => {
      await Router.onMessage(fakeAsyncMessage({type: "BLOCK_MESSAGE_BY_ID", data: {id: "foo"}}));
      assert.isTrue(Router.state.messageBlockList.includes("foo"));
      await Router.onMessage(fakeAsyncMessage({type: "UNBLOCK_MESSAGE_BY_ID", data: {id: "foo"}}));

      assert.isFalse(Router.state.messageBlockList.includes("foo"));
    });
    it("should save the messageBlockList", async () => {
      await Router.onMessage(fakeAsyncMessage({type: "UNBLOCK_MESSAGE_BY_ID", data: {id: "foo"}}));

      assert.calledWithExactly(Router._storage.set, "messageBlockList", []);
    });
  });

  describe("#onMessage: UNBLOCK_PROVIDER_BY_ID", () => {
    it("should remove the id from the providerBlockList", async () => {
      await Router.onMessage(fakeAsyncMessage({type: "BLOCK_PROVIDER_BY_ID", data: {id: "foo"}}));
      assert.isTrue(Router.state.providerBlockList.includes("foo"));
      await Router.onMessage(fakeAsyncMessage({type: "UNBLOCK_PROVIDER_BY_ID", data: {id: "foo"}}));

      assert.isFalse(Router.state.providerBlockList.includes("foo"));
    });
    it("should save the providerBlockList", async () => {
      await Router.onMessage(fakeAsyncMessage({type: "UNBLOCK_PROVIDER_BY_ID", data: {id: "foo"}}));

      assert.calledWithExactly(Router._storage.set, "providerBlockList", []);
    });
  });

  describe("#onMessage: UNBLOCK_BUNDLE", () => {
    it("should remove all the ids in the bundle from the messageBlockList", async () => {
      await Router.onMessage(fakeAsyncMessage({type: "BLOCK_BUNDLE", data: {bundle: FAKE_BUNDLE}}));
      assert.isTrue(Router.state.messageBlockList.includes(FAKE_BUNDLE[0].id));
      assert.isTrue(Router.state.messageBlockList.includes(FAKE_BUNDLE[1].id));
      await Router.onMessage(fakeAsyncMessage({type: "UNBLOCK_BUNDLE", data: {bundle: FAKE_BUNDLE}}));

      assert.isFalse(Router.state.messageBlockList.includes(FAKE_BUNDLE[0].id));
      assert.isFalse(Router.state.messageBlockList.includes(FAKE_BUNDLE[1].id));
    });
    it("should save the messageBlockList", async () => {
      await Router.onMessage(fakeAsyncMessage({type: "UNBLOCK_BUNDLE", data: {bundle: FAKE_BUNDLE}}));

      assert.calledWithExactly(Router._storage.set, "messageBlockList", []);
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
      assert.deepEqual(Router._findMessage.firstCall.args[1], {id: "firstRun"});
    });
    it("consider the trigger when picking a message", async () => {
      const messages = [
        {id: "foo1", template: "simple_template", bundled: 1, trigger: {id: "foo"}, content: {title: "Foo1", body: "Foo123-1"}}
      ];

      const {data} = fakeAsyncMessage({type: "TRIGGER", data: {trigger: {id: "foo"}}});
      const message = await Router._findMessage(messages, data.data.trigger);
      assert.equal(message, messages[0]);
    });
    it("should pick a message with the right targeting and trigger", async () => {
      let messages = [
        {id: "foo1", template: "simple_template", bundled: 2, trigger: {id: "foo"}, content: {title: "Foo1", body: "Foo123-1"}},
        {id: "foo2", template: "simple_template", bundled: 2, trigger: {id: "bar"}, content: {title: "Foo2", body: "Foo123-2"}},
        {id: "foo3", template: "simple_template", bundled: 2, trigger: {id: "foo"}, content: {title: "Foo3", body: "Foo123-3"}}
      ];
      await Router.setState({messages});
      const {target} = fakeAsyncMessage({type: "TRIGGER", data: {trigger: {id: "foo"}}});
      let {bundle} = await Router._getBundledMessages(messages[0], target, {id: "foo"});
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

    it("should call CFRPageActions.forceRecommendation if the template is cfr_action and force is true", async () => {
      sandbox.stub(CFRPageActions, "forceRecommendation");
      const testMessage = {id: "foo", template: "cfr_doorhanger"};
      await Router.setState({messages: [testMessage]});
      const msg = fakeAsyncMessage({type: "OVERRIDE_MESSAGE", data: {id: testMessage.id}});
      await Router.onMessage(msg);

      assert.notCalled(msg.target.sendAsyncMessage);
      assert.calledOnce(CFRPageActions.forceRecommendation);
    });

    it("should call CFRPageActions.addRecommendation if the template is cfr_action and force is false", async () => {
      sandbox.stub(CFRPageActions, "addRecommendation");
      const testMessage = {id: "foo", template: "cfr_doorhanger"};
      await Router.setState({messages: [testMessage]});
      await Router._sendMessageToTarget(testMessage, {}, {}, false);

      assert.calledOnce(CFRPageActions.addRecommendation);
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
      const msg = fakeExecuteUserAction({type: "OPEN_PRIVATE_BROWSER_WINDOW", data: testMessage});
      await Router.onMessage(msg);

      assert.calledWith(msg.target.browser.ownerGlobal.OpenBrowserWindow, {private: true});
    });
    it("should call openLinkIn with the correct params on OPEN_URL", async () => {
      let [testMessage] = Router.state.messages;
      testMessage.button_action = {type: "OPEN_URL", data: {url: "some/url.com"}};
      const msg = fakeExecuteUserAction(testMessage.button_action);
      await Router.onMessage(msg);

      assert.calledOnce(msg.target.browser.ownerGlobal.openLinkIn);
      assert.calledWith(msg.target.browser.ownerGlobal.openLinkIn,
        "some/url.com", "tabshifted", {"private": false, "triggeringPrincipal": undefined});
    });
    it("should call openLinkIn with the correct params on OPEN_ABOUT_PAGE", async () => {
      let [testMessage] = Router.state.messages;
      testMessage.button_action = {type: "OPEN_ABOUT_PAGE", data: {page: "something"}};
      const msg = fakeExecuteUserAction(testMessage.button_action);
      await Router.onMessage(msg);

      assert.calledOnce(msg.target.browser.ownerGlobal.openTrustedLinkIn);
      assert.calledWith(msg.target.browser.ownerGlobal.openTrustedLinkIn, "about:something", "tab");
    });
  });

  describe("#onMessage: INSTALL_ADDON_FROM_URL", () => {
    it("should call installAddonFromURL with correct arguments", async () => {
      sandbox.stub(MessageLoaderUtils, "installAddonFromURL").resolves(null);
      const msg = fakeExecuteUserAction({type: "INSTALL_ADDON_FROM_URL", data: {url: "foo.com"}});

      await Router.onMessage(msg);

      assert.calledOnce(MessageLoaderUtils.installAddonFromURL);
      assert.calledWithExactly(MessageLoaderUtils.installAddonFromURL, msg.target.browser, "foo.com");
    });
  });

  describe("#dispatch(action, target)", () => {
    it("should an action and target to onMessage", async () => {
      // use the IMPRESSION action to make sure actions are actually getting processed
      sandbox.stub(Router, "addImpression");
      sandbox.spy(Router, "onMessage");
      const target = {};
      const action = {type: "IMPRESSION"};

      Router.dispatch(action, target);

      assert.calledWith(Router.onMessage, {data: action, target});
      assert.calledOnce(Router.addImpression);
    });
  });

  describe("#onMessage: DOORHANGER_TELEMETRY", () => {
    it("should dispatch an AS_ROUTER_TELEMETRY_USER_EVENT on DOORHANGER_TELEMETRY message", async () => {
      const msg = fakeAsyncMessage({type: "DOORHANGER_TELEMETRY", data: {message_id: "foo"}});
      await Router.onMessage(msg);

      assert.calledOnce(dispatchStub);
      const [action] = dispatchStub.firstCall.args;
      assert.equal(action.type, "AS_ROUTER_TELEMETRY_USER_EVENT");
      assert.equal(action.data.message_id, "foo");
    });
  });

  describe("_triggerHandler", () => {
    it("should call #onMessage with the correct trigger", () => {
      sinon.spy(Router, "onMessage");
      const target = {};
      const trigger = {id: "FAKE_TRIGGER", param: "some fake param"};
      Router._triggerHandler(target, trigger);
      assert.calledOnce(Router.onMessage);
      assert.calledWithExactly(Router.onMessage, {target, data: {type: "TRIGGER", data: {trigger}}});
    });
  });

  describe("#UITour", () => {
    let globals;
    let showMenuStub;
    beforeEach(() => {
      globals = new GlobalOverrider();
      showMenuStub = sandbox.stub();
      globals.set("UITour", {showMenu: showMenuStub});
    });
    it("should call UITour.showMenu with the correct params on OPEN_APPLICATIONS_MENU", async () => {
      const msg = fakeExecuteUserAction({type: "OPEN_APPLICATIONS_MENU", data: {target: "appMenu"}});
      await Router.onMessage(msg);

      assert.calledOnce(showMenuStub);
      assert.calledWith(showMenuStub, msg.target.browser.ownerGlobal, "appMenu");
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
    async function addProviderWithFrequency(id, frequency) {
      await Router.setState(state => {
        const newProvider = {id, frequency};
        const providers = [...state.providers, newProvider];
        return {providers};
      });
    }

    describe("#addImpression", () => {
      it("should add a message impression and update _storage with the current time if the message has frequency caps", async () => {
        clock.tick(42);
        const msg = fakeAsyncMessage({type: "IMPRESSION", data: {id: "foo", provider: FAKE_LOCAL_PROVIDER.id, frequency: {lifetime: 5}}});
        await Router.onMessage(msg);
        assert.isArray(Router.state.messageImpressions.foo);
        assert.deepEqual(Router.state.messageImpressions.foo, [42]);
        assert.calledWith(Router._storage.set, "messageImpressions", {foo: [42]});
      });
      it("should not add a message impression if the message doesn't have frequency caps", async () => {
        // Note that storage.set is called during initialization, so it needs to be reset
        Router._storage.set.reset();
        clock.tick(42);
        const msg = fakeAsyncMessage({type: "IMPRESSION", data: {id: "foo"}});
        await Router.onMessage(msg);
        assert.notProperty(Router.state.messageImpressions, "foo");
        assert.notCalled(Router._storage.set);
      });
      it("should add a provider impression and update _storage with the current time if the message's provider has frequency caps", async () => {
        clock.tick(42);
        await addProviderWithFrequency("foo", {lifetime: 5});
        const msg = fakeAsyncMessage({type: "IMPRESSION", data: {id: "bar", provider: "foo"}});
        await Router.onMessage(msg);
        assert.isArray(Router.state.providerImpressions.foo);
        assert.deepEqual(Router.state.providerImpressions.foo, [42]);
        assert.calledWith(Router._storage.set, "providerImpressions", {foo: [42]});
      });
      it("should not add a provider impression if the message's provider doesn't have frequency caps", async () => {
        // Note that storage.set is called during initialization, so it needs to be reset
        Router._storage.set.reset();
        clock.tick(42);
        // Add "foo" provider with no frequency
        await addProviderWithFrequency("foo", null);
        const msg = fakeAsyncMessage({type: "IMPRESSION", data: {id: "bar", provider: "foo"}});
        await Router.onMessage(msg);
        assert.notProperty(Router.state.providerImpressions, "foo");
        assert.notCalled(Router._storage.set);
      });
    });

    describe("#isBelowFrequencyCaps", () => {
      it("should call #_isBelowItemFrequencyCap for the message and for the provider with the correct impressions and arguments", async () => {
        sinon.spy(Router, "_isBelowItemFrequencyCap");

        const MAX_MESSAGE_LIFETIME_CAP = 100; // Defined in ASRouter
        const fooMessageImpressions = [0, 1];
        const barProviderImpressions = [0, 1, 2];

        const message = {id: "foo", provider: "bar", frequency: {lifetime: 3}};
        const provider = {id: "bar", frequency: {lifetime: 5}};

        await Router.setState(state => {
          // Add provider
          const providers = [...state.providers, provider];
          // Add fooMessageImpressions
          const messageImpressions = Object.assign({}, state.messageImpressions); // eslint-disable-line no-shadow
          messageImpressions.foo = fooMessageImpressions;
          // Add barProviderImpressions
          const providerImpressions = Object.assign({}, state.providerImpressions); // eslint-disable-line no-shadow
          providerImpressions.bar = barProviderImpressions;
          return {providers, messageImpressions, providerImpressions};
        });

        await Router.isBelowFrequencyCaps(message);

        assert.calledTwice(Router._isBelowItemFrequencyCap);
        assert.calledWithExactly(Router._isBelowItemFrequencyCap, message, fooMessageImpressions, MAX_MESSAGE_LIFETIME_CAP);
        assert.calledWithExactly(Router._isBelowItemFrequencyCap, provider, barProviderImpressions);
      });
    });

    describe("#_isBelowItemFrequencyCap", () => {
      it("should return false if the # of impressions exceeds the maxLifetimeCap", () => {
        const item = {id: "foo", frequency: {lifetime: 5}};
        const impressions = [0, 1];
        const maxLifetimeCap = 1;
        const result = Router._isBelowItemFrequencyCap(item, impressions, maxLifetimeCap);
        assert.isFalse(result);
      });

      describe("lifetime frequency caps", () => {
        it("should return true if .frequency is not defined on the item", () => {
          const item = {id: "foo"};
          const impressions = [0, 1];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isTrue(result);
        });
        it("should return true if there are no impressions", () => {
          const item = {id: "foo", frequency: {lifetime: 10, custom: [{period: ONE_DAY, cap: 2}]}};
          const impressions = [];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isTrue(result);
        });
        it("should return true if the # of impressions is less than .frequency.lifetime of the item", () => {
          const item = {id: "foo", frequency: {lifetime: 3}};
          const impressions = [0, 1];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isTrue(result);
        });
        it("should return false if the # of impressions is equal to .frequency.lifetime of the item", async () => {
          const item = {id: "foo", frequency: {lifetime: 3}};
          const impressions = [0, 1, 2];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isFalse(result);
        });
        it("should return false if the # of impressions is greater than .frequency.lifetime of the item", async () => {
          const item = {id: "foo", frequency: {lifetime: 3}};
          const impressions = [0, 1, 2, 3];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isFalse(result);
        });
      });

      describe("custom frequency caps", () => {
        it("should return true if impressions in the time period < the cap and total impressions < the lifetime cap", () => {
          clock.tick(ONE_DAY + 10);
          const item = {id: "foo", frequency: {custom: [{period: ONE_DAY, cap: 2}], lifetime: 3}};
          const impressions = [0, ONE_DAY + 1];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isTrue(result);
        });
        it("should return false if impressions in the time period > the cap and total impressions < the lifetime cap", () => {
          clock.tick(200);
          const item = {id: "msg1", frequency: {custom: [{period: 100, cap: 2}], lifetime: 3}};
          const impressions = [0, 160, 161];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isFalse(result);
        });
        it("should return false if impressions in one of the time periods > the cap and total impressions < the lifetime cap", () => {
          clock.tick(ONE_DAY + 200);
          const itemTrue = {id: "msg2", frequency: {custom: [{period: 100, cap: 2}]}};
          const itemFalse = {id: "msg1", frequency: {custom: [{period: 100, cap: 2}, {period: ONE_DAY, cap: 3}]}};
          const impressions = [0, ONE_DAY + 160, ONE_DAY - 100, ONE_DAY - 200];
          assert.isTrue(Router._isBelowItemFrequencyCap(itemTrue, impressions));
          assert.isFalse(Router._isBelowItemFrequencyCap(itemFalse, impressions));
        });
        it("should return false if impressions in the time period < the cap and total impressions > the lifetime cap", () => {
          clock.tick(ONE_DAY + 10);
          const item = {id: "msg1", frequency: {custom: [{period: ONE_DAY, cap: 2}], lifetime: 3}};
          const impressions = [0, 1, 2, 3, ONE_DAY + 1];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isFalse(result);
        });
        it("should return true if daily impressions < the daily cap and there is no lifetime cap", () => {
          clock.tick(ONE_DAY + 10);
          const item = {id: "msg1", frequency: {custom: [{period: ONE_DAY, cap: 2}]}};
          const impressions = [0, 1, 2, 3, ONE_DAY + 1];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isTrue(result);
        });
        it("should return false if daily impressions > the daily cap and there is no lifetime cap", () => {
          clock.tick(ONE_DAY + 10);
          const item = {id: "msg1", frequency: {custom: [{period: ONE_DAY, cap: 2}]}};
          const impressions = [0, 1, 2, 3, ONE_DAY + 1, ONE_DAY + 2, ONE_DAY + 3];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isFalse(result);
        });
        it("should allow the 'daily' alias for period", () => {
          clock.tick(ONE_DAY + 10);
          const item = {id: "msg1", frequency: {custom: [{period: "daily", cap: 2}]}};
          assert.isFalse(Router._isBelowItemFrequencyCap(item, [0, 1, 2, 3, ONE_DAY + 1, ONE_DAY + 2, ONE_DAY + 3]));
          assert.isTrue(Router._isBelowItemFrequencyCap(item, [0, 1, 2, 3, ONE_DAY + 1]));
        });
      });
    });

    describe("#getLongestPeriod", () => {
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
      it("should clear messageImpressions for messages which do not exist in state.messages", async () => {
        const messages = [{id: "foo", frequency: {lifetime: 10}}];
        messageImpressions = {foo: [0], bar: [0, 1]};
        // Impressions for "bar" should be removed since that id does not exist in messages
        const result = {foo: [0]};

        await createRouterAndInit([{id: "onboarding", type: "local", messages, enabled: true}]);
        assert.calledWith(Router._storage.set, "messageImpressions", result);
        assert.deepEqual(Router.state.messageImpressions, result);
      });
      it("should clear messageImpressions older than the period if no lifetime impression cap is included", async () => {
        const CURRENT_TIME = ONE_DAY * 2;
        clock.tick(CURRENT_TIME);
        const messages = [{id: "foo", frequency: {custom: [{period: ONE_DAY, cap: 5}]}}];
        messageImpressions = {foo: [0, 1, CURRENT_TIME - 10]};
        // Only 0 and 1 are more than 24 hours before CURRENT_TIME
        const result = {foo: [CURRENT_TIME - 10]};

        await createRouterAndInit([{id: "onboarding", type: "local", messages, enabled: true}]);
        assert.calledWith(Router._storage.set, "messageImpressions", result);
        assert.deepEqual(Router.state.messageImpressions, result);
      });
      it("should clear messageImpressions older than the longest period if no lifetime impression cap is included", async () => {
        const CURRENT_TIME = ONE_DAY * 2;
        clock.tick(CURRENT_TIME);
        const messages = [{id: "foo", frequency: {custom: [{period: ONE_DAY, cap: 5}, {period: 100, cap: 2}]}}];
        messageImpressions = {foo: [0, 1, CURRENT_TIME - 10]};
        // Only 0 and 1 are more than 24 hours before CURRENT_TIME
        const result = {foo: [CURRENT_TIME - 10]};

        await createRouterAndInit([{id: "onboarding", type: "local", messages, enabled: true}]);
        assert.calledWith(Router._storage.set, "messageImpressions", result);
        assert.deepEqual(Router.state.messageImpressions, result);
      });
      it("should clear messageImpressions if they are not properly formatted", async () => {
        const messages = [{id: "foo", frequency: {lifetime: 10}}];
        // this is impromperly formatted since messageImpressions are supposed to be an array
        messageImpressions = {foo: 0};
        const result = {};

        await createRouterAndInit([{id: "onboarding", type: "local", messages, enabled: true}]);
        assert.calledWith(Router._storage.set, "messageImpressions", result);
        assert.deepEqual(Router.state.messageImpressions, result);
      });
      it("should not clear messageImpressions for messages which do exist in state.messages", async () => {
        const messages = [{id: "foo", frequency: {lifetime: 10}}, {id: "bar", frequency: {lifetime: 10}}];
        messageImpressions = {foo: [0], bar: []};

        await createRouterAndInit([{id: "onboarding", type: "local", messages, enabled: true}]);
        assert.notCalled(Router._storage.set);
        assert.deepEqual(Router.state.messageImpressions, messageImpressions);
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

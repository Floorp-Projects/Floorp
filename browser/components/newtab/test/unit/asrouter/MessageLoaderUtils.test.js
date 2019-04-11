import {GlobalOverrider} from "test/unit/utils";
import {MessageLoaderUtils} from "lib/ASRouter.jsm";
const {STARTPAGE_VERSION} = MessageLoaderUtils;

const FAKE_OPTIONS = {
  storage: {
    set() {
      return Promise.resolve();
    },
    get() { return Promise.resolve(); },
  },
  dispatchToAS: () => {},
};
const FAKE_RESPONSE_HEADERS = {get() {}};

describe("MessageLoaderUtils", () => {
  let fetchStub;
  let clock;
  let sandbox;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    clock = sinon.useFakeTimers();
    fetchStub = sinon.stub(global, "fetch");
  });
  afterEach(() => {
    sandbox.restore();
    clock.restore();
    fetchStub.restore();
  });

  describe("#loadMessagesForProvider", () => {
    it("should return messages for a local provider with hardcoded messages", async () => {
      const sourceMessage = {id: "foo"};
      const provider = {id: "provider123", type: "local", messages: [sourceMessage]};

      const result = await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);

      assert.isArray(result.messages);
      // Does the message have the right properties?
      const [message] = result.messages;
      assert.propertyVal(message, "id", "foo");
      assert.propertyVal(message, "provider", "provider123");
    });
    it("should filter out local messages listed in the `exclude` field", async () => {
      const sourceMessage = {id: "foo"};
      const provider = {id: "provider123", type: "local", messages: [sourceMessage], exclude: ["foo"]};

      const result = await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);

      assert.lengthOf(result.messages, 0);
    });
    it("should return messages for remote provider", async () => {
      const sourceMessage = {id: "foo"};
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve({messages: [sourceMessage]}), headers: FAKE_RESPONSE_HEADERS});
      const provider = {id: "provider123", type: "remote", url: "https://foo.com"};

      const result = await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);
      assert.isArray(result.messages);
      // Does the message have the right properties?
      const [message] = result.messages;
      assert.propertyVal(message, "id", "foo");
      assert.propertyVal(message, "provider", "provider123");
      assert.propertyVal(message, "provider_url", "https://foo.com");
    });
    describe("remote provider HTTP codes", () => {
      const testMessage = {id: "foo"};
      const provider = {id: "provider123", type: "remote", url: "https://foo.com", updateCycleInMs: 300};
      const respJson = {messages: [testMessage]};

      function assertReturnsCorrectMessages(actual) {
        assert.isArray(actual.messages);
        // Does the message have the right properties?
        const [message] = actual.messages;
        assert.propertyVal(message, "id", testMessage.id);
        assert.propertyVal(message, "provider", provider.id);
        assert.propertyVal(message, "provider_url", provider.url);
      }

      it("should return messages for 200 response", async () => {
        fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(respJson), headers: FAKE_RESPONSE_HEADERS});
        assertReturnsCorrectMessages(await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS));
      });

      it("should return messages for a 302 response with json", async () => {
        fetchStub.resolves({ok: true, status: 302, json: () => Promise.resolve(respJson), headers: FAKE_RESPONSE_HEADERS});
        assertReturnsCorrectMessages(await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS));
      });

      it("should return an empty array for a 204 response", async () => {
        fetchStub.resolves({ok: true, status: 204, json: () => "", headers: FAKE_RESPONSE_HEADERS});
        const result = await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);
        assert.deepEqual(result.messages, []);
      });

      it("should return an empty array for a 500 response", async () => {
        fetchStub.resolves({ok: false, status: 500, json: () => "", headers: FAKE_RESPONSE_HEADERS});
        const result = await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);
        assert.deepEqual(result.messages, []);
      });

      it("should return cached messages for a 304 response", async () => {
        clock.tick(302);
        const messages = [{id: "message-1"}, {id: "message-2"}];
        const fakeStorage = {
          set() {
            return Promise.resolve();
          },
          get() {
            return Promise.resolve({
              [provider.id]: {
                version: STARTPAGE_VERSION,
                url: provider.url,
                messages,
                etag: "etag0987654321",
                lastFetched: 1,
              },
            });
          },
        };
        fetchStub.resolves({ok: true, status: 304, json: () => "", headers: FAKE_RESPONSE_HEADERS});
        const result = await MessageLoaderUtils.loadMessagesForProvider(provider, {...FAKE_OPTIONS, storage: fakeStorage});
        assert.equal(result.messages.length, messages.length);
        messages.forEach(message => {
          assert.ok(result.messages.find(m => m.id === message.id));
        });
      });

      it("should return an empty array if json doesn't parse properly", async () => {
        fetchStub.resolves({ok: false, status: 200, json: () => "", headers: FAKE_RESPONSE_HEADERS});
        const result = await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);
        assert.deepEqual(result.messages, []);
      });

      it("should report response parsing errors with MessageLoaderUtils.reportError", async () => {
        const err = {};
        sandbox.spy(MessageLoaderUtils, "reportError");
        fetchStub.resolves({ok: true, status: 200, json: sandbox.stub().rejects(err), headers: FAKE_RESPONSE_HEADERS});
        await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);

        assert.calledOnce(MessageLoaderUtils.reportError);
        // Report that json parsing failed
        assert.calledWith(MessageLoaderUtils.reportError, err);
      });

      it("should report missing `messages` with MessageLoaderUtils.reportError", async () => {
        sandbox.spy(MessageLoaderUtils, "reportError");
        fetchStub.resolves({ok: true, status: 200, json: sandbox.stub().resolves({}), headers: FAKE_RESPONSE_HEADERS});
        await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);

        assert.calledOnce(MessageLoaderUtils.reportError);
        // Report no messages returned
        assert.calledWith(MessageLoaderUtils.reportError, "No messages returned from https://foo.com.");
      });

      it("should report bad status responses with MessageLoaderUtils.reportError", async () => {
        sandbox.spy(MessageLoaderUtils, "reportError");
        fetchStub.resolves({ok: false, status: 500, json: sandbox.stub().resolves({}), headers: FAKE_RESPONSE_HEADERS});
        await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);

        assert.calledOnce(MessageLoaderUtils.reportError);
        // Report no messages returned
        assert.calledWith(MessageLoaderUtils.reportError, "Invalid response status 500 from https://foo.com.");
      });

      it("should return an empty array if the request rejects", async () => {
        fetchStub.rejects(new Error("something went wrong"));
        const result = await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);
        assert.deepEqual(result.messages, []);
      });
    });
    describe("remote provider caching", () => {
      const provider = {id: "provider123", type: "remote", url: "https://foo.com", updateCycleInMs: 300};

      it("should return cached results if they aren't expired", async () => {
        clock.tick(1);
        const messages = [{id: "message-1"}, {id: "message-2"}];
        const fakeStorage = {
          set() { return Promise.resolve(); },
          get() {
            return Promise.resolve({
              [provider.id]: {
                version: STARTPAGE_VERSION,
                url: provider.url,
                messages,
                etag: "etag0987654321",
                lastFetched: Date.now(),
              },
            });
          },
        };
        const result = await MessageLoaderUtils.loadMessagesForProvider(provider, {...FAKE_OPTIONS, storage: fakeStorage});
        assert.equal(result.messages.length, messages.length);
        messages.forEach(message => {
          assert.ok(result.messages.find(m => m.id === message.id));
        });
      });

      it("should return fetch results if the cache messages are expired", async () => {
        clock.tick(302);
        const testMessage = {id: "foo"};
        const respJson = {messages: [testMessage]};
        const fakeStorage = {
          set() { return Promise.resolve(); },
          get() {
            return Promise.resolve({
              [provider.id]: {
                version: STARTPAGE_VERSION,
                url: provider.url,
                messages: [{id: "message-1"}, {id: "message-2"}],
                etag: "etag0987654321",
                lastFetched: 1,
              },
            });
          },
        };
        fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(respJson), headers: FAKE_RESPONSE_HEADERS});
        const result = await MessageLoaderUtils.loadMessagesForProvider(provider, {...FAKE_OPTIONS, storage: fakeStorage});
        assert.equal(result.messages.length, 1);
        assert.equal(result.messages[0].id, testMessage.id);
      });
    });
    it("should return an empty array for a remote provider with a blank URL without attempting a request", async () => {
      const provider = {id: "provider123", type: "remote", url: ""};

      const result = await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);

      assert.notCalled(fetchStub);
      assert.deepEqual(result.messages, []);
    });
    it("should return .lastUpdated with the time at which the messages were fetched", async () => {
      const sourceMessage = {id: "foo"};
      const provider = {
        id: "provider123",
        type: "remote",
        url: "foo.com",
      };

      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => new Promise(resolve => {
          clock.tick(42);
          resolve({messages: [sourceMessage]});
        }),
        headers: FAKE_RESPONSE_HEADERS,
      });

      const result = await MessageLoaderUtils.loadMessagesForProvider(provider, FAKE_OPTIONS);

      assert.propertyVal(result, "lastUpdated", 42);
    });
  });

  describe("#shouldProviderUpdate", () => {
    it("should return true if the provider does not had a .lastUpdated property", () => {
      assert.isTrue(MessageLoaderUtils.shouldProviderUpdate({id: "foo"}));
    });
    it("should return false if the provider does not had a .updateCycleInMs property and has a .lastUpdated", () => {
      clock.tick(1);
      assert.isFalse(MessageLoaderUtils.shouldProviderUpdate({id: "foo", lastUpdated: 0}));
    });
    it("should return true if the time since .lastUpdated is greater than .updateCycleInMs", () => {
      clock.tick(301);
      assert.isTrue(MessageLoaderUtils.shouldProviderUpdate({id: "foo", lastUpdated: 0, updateCycleInMs: 300}));
    });
    it("should return false if the time since .lastUpdated is less than .updateCycleInMs", () => {
      clock.tick(299);
      assert.isFalse(MessageLoaderUtils.shouldProviderUpdate({id: "foo", lastUpdated: 0,  updateCycleInMs: 300}));
    });
  });

  describe("#_loadAddonIconInURLBar", () => {
    let notificationContainerEl;
    let browser;
    let getContainerStub;
    beforeEach(() => {
      notificationContainerEl = {style: {}};
      browser = {ownerDocument: {getElementById() { return {}; }}};
      getContainerStub = sandbox.stub(browser.ownerDocument, "getElementById");
    });
    it("should return for empty args", () => {
      MessageLoaderUtils._loadAddonIconInURLBar();
      assert.notCalled(getContainerStub);
    });
    it("should return if notification popup box not found", () => {
      getContainerStub.returns(null);
      MessageLoaderUtils._loadAddonIconInURLBar(browser);
      assert.calledOnce(getContainerStub);
    });
    it("should unhide notification popup box with display style as none", () => {
      getContainerStub.returns(notificationContainerEl);
      notificationContainerEl.style.display = "none";
      MessageLoaderUtils._loadAddonIconInURLBar(browser);
      assert.calledWith(browser.ownerDocument.getElementById, "notification-popup-box");
      assert.equal(notificationContainerEl.style.display, "block");
    });
    it("should unhide notification popup box with display style empty", () => {
      getContainerStub.returns(notificationContainerEl);
      notificationContainerEl.style.display = "";
      MessageLoaderUtils._loadAddonIconInURLBar(browser);
      assert.calledWith(browser.ownerDocument.getElementById, "notification-popup-box");
      assert.equal(notificationContainerEl.style.display, "block");
    });
  });

  describe("#installAddonFromURL", () => {
    let globals;
    let getInstallStub;
    let installAddonStub;
    beforeEach(() => {
      globals = new GlobalOverrider();
      getInstallStub = sandbox.stub();
      installAddonStub = sandbox.stub();
      sandbox.stub(MessageLoaderUtils, "_loadAddonIconInURLBar").returns(null);
      globals.set("AddonManager", {
        getInstallForURL: getInstallStub,
        installAddonFromWebpage: installAddonStub,
      });
    });
    afterEach(() => {
      globals.restore();
    });
    it("should call the Addons API when passed a valid URL", async () => {
      getInstallStub.resolves(null);
      installAddonStub.resolves(null);

      await MessageLoaderUtils.installAddonFromURL({}, "foo.com");

      assert.calledOnce(getInstallStub);
      assert.calledOnce(installAddonStub);

      // Verify that the expected installation source has been passed to the getInstallForURL
      // method (See Bug 1496167 for a rationale).
      assert.calledWithExactly(getInstallStub, "foo.com", {telemetryInfo: {source: "amo"}});
    });
    it("should not call the Addons API on invalid URLs", async () => {
      sandbox.stub(global.Services.scriptSecurityManager, "getSystemPrincipal").throws();

      await MessageLoaderUtils.installAddonFromURL({}, "https://foo.com");

      assert.notCalled(getInstallStub);
      assert.notCalled(installAddonStub);
    });
  });

  describe("#cleanupCache", () => {
    it("should remove data for providers no longer active", async () => {
      const fakeStorage = {
        get: sinon.stub().returns(Promise.resolve({
          "id-1": {},
          "id-2": {},
          "id-3": {},
        })),
        set: sinon.stub().returns(Promise.resolve()),
      };
      const fakeProviders = [{id: "id-1", type: "remote"}, {id: "id-3", type: "remote"}];

      await MessageLoaderUtils.cleanupCache(fakeProviders, fakeStorage);

      assert.calledOnce(fakeStorage.set);
      assert.calledWith(fakeStorage.set, MessageLoaderUtils.REMOTE_LOADER_CACHE_KEY, {"id-1": {}, "id-3": {}});
    });
  });
});

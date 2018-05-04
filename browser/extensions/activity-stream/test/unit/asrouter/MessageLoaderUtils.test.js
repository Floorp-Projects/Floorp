import {MessageLoaderUtils} from "lib/ASRouter.jsm";

describe("MessageLoaderUtils", () => {
  let fetchStub;
  let clock;

  beforeEach(() => {
    clock = sinon.useFakeTimers();
    fetchStub = sinon.stub(global, "fetch");
  });
  afterEach(() => {
    clock.restore();
    fetchStub.restore();
  });

  describe("#loadMessagesForProvider", () => {
    it("should return messages for a local provider with hardcoded messages", async () => {
      const sourceMessage = {id: "foo"};
      const provider = {id: "provider123", type: "local", messages: [sourceMessage]};

      const result = await MessageLoaderUtils.loadMessagesForProvider(provider);

      assert.isArray(result.messages);
      // Does the message have the right properties?
      const [message] = result.messages;
      assert.propertyVal(message, "id", "foo");
      assert.propertyVal(message, "provider", "provider123");
    });
    it("should return messages for remote provider", async () => {
      const sourceMessage = {id: "foo"};
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve({messages: [sourceMessage]})});
      const provider = {id: "provider123", type: "remote", url: "https://foo.com"};

      const result = await MessageLoaderUtils.loadMessagesForProvider(provider);
      assert.isArray(result.messages);
      // Does the message have the right properties?
      const [message] = result.messages;
      assert.propertyVal(message, "id", "foo");
      assert.propertyVal(message, "provider", "provider123");
      assert.propertyVal(message, "provider_url", "https://foo.com");
    });
    it("should return an empty array if the request results in an error", async () => {
      const provider = {id: "provider123", type: "remote", url: "https://foo.com"};
      fetchStub.rejects(new Error("something went wrong"));

      const result = await MessageLoaderUtils.loadMessagesForProvider(provider);

      assert.deepEqual(result.messages, []);
    });
    it("should return an empty array for a remote provider with a blank URL without attempting a request", async () => {
      const provider = {id: "provider123", type: "remote", url: ""};

      const result = await MessageLoaderUtils.loadMessagesForProvider(provider);

      assert.notCalled(fetchStub);
      assert.deepEqual(result.messages, []);
    });
    it("should return .lastUpdated with the time at which the messages were fetched", async () => {
      const sourceMessage = {id: "foo"};
      const provider = {
        id: "provider123",
        type: "remote",
        url: "foo.com"
      };

      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => new Promise(resolve => {
          clock.tick(42);
          resolve({messages: [sourceMessage]});
        })
      });

      const result = await MessageLoaderUtils.loadMessagesForProvider(provider);

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
});

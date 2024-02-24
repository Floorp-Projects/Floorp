import { ASRouterUtils } from "../../content-src/asrouter-utils.mjs";

describe("ASRouterUtils", () => {
  let sandbox = null;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    globalThis.ASRouterMessage = sandbox.stub().resolves({});
  });
  afterEach(() => {
    sandbox.restore();
  });
  describe("sendMessage", () => {
    it("default", () => {
      ASRouterUtils.sendMessage({ foo: "bar" });
      assert.calledOnce(globalThis.ASRouterMessage);
      assert.calledWith(globalThis.ASRouterMessage, { foo: "bar" });
    });
    it("throws if ASRouterMessage is not defined", () => {
      globalThis.ASRouterMessage = null;
      assert.throws(() => ASRouterUtils.sendMessage({ foo: "bar" }));
    });
    it("can accept the legacy NEWTAB_MESSAGE_REQUEST message without throwing", async () => {
      assert.doesNotThrow(async () => {
        let result = await ASRouterUtils.sendMessage({
          type: "NEWTAB_MESSAGE_REQUEST",
          data: {},
        });
        sandbox.assert.deepEqual(result, {});
      });
    });
  });
  describe("blockById", () => {
    it("default", () => {
      ASRouterUtils.blockById(1, { foo: "bar" });
      assert.calledWith(
        globalThis.ASRouterMessage,
        sinon.match({ data: { foo: "bar", id: 1 } })
      );
    });
  });
  describe("modifyMessageJson", () => {
    it("default", () => {
      ASRouterUtils.modifyMessageJson({ foo: "bar" });
      assert.calledWith(
        globalThis.ASRouterMessage,
        sinon.match({ data: { content: { foo: "bar" } } })
      );
    });
  });
  describe("executeAction", () => {
    it("default", () => {
      ASRouterUtils.executeAction({ foo: "bar" });
      assert.calledWith(
        globalThis.ASRouterMessage,
        sinon.match({ data: { foo: "bar" } })
      );
    });
  });
  describe("unblockById", () => {
    it("default", () => {
      ASRouterUtils.unblockById(2);
      assert.calledWith(
        globalThis.ASRouterMessage,
        sinon.match({ data: { id: 2 } })
      );
    });
  });
  describe("blockBundle", () => {
    it("default", () => {
      ASRouterUtils.blockBundle(2);
      assert.calledWith(
        globalThis.ASRouterMessage,
        sinon.match({ data: { bundle: 2 } })
      );
    });
  });
  describe("unblockBundle", () => {
    it("default", () => {
      ASRouterUtils.unblockBundle(2);
      assert.calledWith(
        globalThis.ASRouterMessage,
        sinon.match({ data: { bundle: 2 } })
      );
    });
  });
  describe("overrideMessage", () => {
    it("default", () => {
      ASRouterUtils.overrideMessage(12);
      assert.calledWith(
        globalThis.ASRouterMessage,
        sinon.match({ data: { id: 12 } })
      );
    });
  });
  describe("editState", () => {
    it("default", () => {
      ASRouterUtils.editState("foo", "bar");
      assert.calledWith(
        globalThis.ASRouterMessage,
        sinon.match({ data: { foo: "bar" } })
      );
    });
  });
  describe("sendTelemetry", () => {
    it("default", () => {
      ASRouterUtils.sendTelemetry({ foo: "bar" });
      assert.calledOnce(globalThis.ASRouterMessage);
    });
  });
});

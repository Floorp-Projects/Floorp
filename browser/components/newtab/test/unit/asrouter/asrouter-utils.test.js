import { ASRouterUtils } from "content-src/asrouter/asrouter-utils";
import { GlobalOverrider } from "test/unit/utils";

describe("ASRouterUtils", () => {
  let globals = null;
  let overrider = null;
  let sandbox = null;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    globals = {
      ASRouterMessage: sandbox.stub().resolves({}),
    };
    overrider = new GlobalOverrider();
    overrider.set(globals);
  });
  afterEach(() => {
    sandbox.restore();
    overrider.restore();
  });
  describe("sendMessage", () => {
    it("default", () => {
      ASRouterUtils.sendMessage({ foo: "bar" });
      assert.calledOnce(globals.ASRouterMessage);
      assert.calledWith(globals.ASRouterMessage, { foo: "bar" });
    });
    it("throws if ASRouterMessage is not defined", () => {
      overrider.set("ASRouterMessage", undefined);
      assert.throws(() => ASRouterUtils.sendMessage({ foo: "bar" }));
    });
  });
  describe("blockById", () => {
    it("default", () => {
      ASRouterUtils.blockById(1, { foo: "bar" });
      assert.calledWith(
        globals.ASRouterMessage,
        sinon.match({ data: { foo: "bar", id: 1 } })
      );
    });
  });
  describe("modifyMessageJson", () => {
    it("default", () => {
      ASRouterUtils.modifyMessageJson({ foo: "bar" });
      assert.calledWith(
        globals.ASRouterMessage,
        sinon.match({ data: { content: { foo: "bar" } } })
      );
    });
  });
  describe("executeAction", () => {
    it("default", () => {
      ASRouterUtils.executeAction({ foo: "bar" });
      assert.calledWith(
        globals.ASRouterMessage,
        sinon.match({ data: { foo: "bar" } })
      );
    });
  });
  describe("unblockById", () => {
    it("default", () => {
      ASRouterUtils.unblockById(2);
      assert.calledWith(
        globals.ASRouterMessage,
        sinon.match({ data: { id: 2 } })
      );
    });
  });
  describe("blockBundle", () => {
    it("default", () => {
      ASRouterUtils.blockBundle(2);
      assert.calledWith(
        globals.ASRouterMessage,
        sinon.match({ data: { bundle: 2 } })
      );
    });
  });
  describe("unblockBundle", () => {
    it("default", () => {
      ASRouterUtils.unblockBundle(2);
      assert.calledWith(
        globals.ASRouterMessage,
        sinon.match({ data: { bundle: 2 } })
      );
    });
  });
  describe("overrideMessage", () => {
    it("default", () => {
      ASRouterUtils.overrideMessage(12);
      assert.calledWith(
        globals.ASRouterMessage,
        sinon.match({ data: { id: 12 } })
      );
    });
  });
  describe("editState", () => {
    it("default", () => {
      ASRouterUtils.editState("foo", "bar");
      assert.calledWith(
        globals.ASRouterMessage,
        sinon.match({ data: { foo: "bar" } })
      );
    });
  });
  describe("sendTelemetry", () => {
    it("default", () => {
      ASRouterUtils.sendTelemetry({ foo: "bar" });
      assert.calledOnce(globals.ASRouterMessage);
    });
  });
});

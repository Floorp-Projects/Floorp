import { ASRouterTelemetry } from "lib/ASRouterTelemetry.jsm";

describe("ASRouterTelemetry", () => {
  describe("constructor", () => {
    it("does not throw", () => {
      const t = new ASRouterTelemetry();
      assert.isDefined(t);
    });
  });
  describe("sendTelemetry", () => {
    it("does not throw", () => {
      const t = new ASRouterTelemetry();
      t.sendTelemetry({});
      assert.isDefined(t);
    });
  });
});

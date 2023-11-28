import { AWScreenUtils } from "modules/AWScreenUtils.jsm";
import { GlobalOverrider } from "newtab/test/unit/utils";
import { ASRouter } from "newtab/lib/ASRouter.jsm";

describe("AWScreenUtils", () => {
  let sandbox;
  let globals;

  beforeEach(() => {
    globals = new GlobalOverrider();
    globals.set({
      ASRouter,
      ASRouterTargeting: {
        Environment: {},
      },
    });

    sandbox = sinon.createSandbox();
  });
  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });
  describe("removeScreens", () => {
    it("should run callback function once for each array element", async () => {
      const callback = sandbox.stub().resolves(false);
      const arr = ["foo", "bar"];
      await AWScreenUtils.removeScreens(arr, callback);
      assert.calledTwice(callback);
    });
    it("should remove screen when passed function evaluates true", async () => {
      const callback = sandbox.stub().resolves(true);
      const arr = ["foo", "bar"];
      await AWScreenUtils.removeScreens(arr, callback);
      assert.deepEqual(arr, []);
    });
  });
  describe("evaluateScreenTargeting", () => {
    it("should return the eval result if the eval succeeds", async () => {
      const evalStub = sandbox.stub(ASRouter, "evaluateExpression").resolves({
        evaluationStatus: {
          success: true,
          result: false,
        },
      });
      const result = await AWScreenUtils.evaluateScreenTargeting(
        "test expression"
      );
      assert.calledOnce(evalStub);
      assert.equal(result, false);
    });
    it("should return true if the targeting eval fails", async () => {
      const evalStub = sandbox.stub(ASRouter, "evaluateExpression").resolves({
        evaluationStatus: {
          success: false,
          result: false,
        },
      });
      const result = await AWScreenUtils.evaluateScreenTargeting(
        "test expression"
      );
      assert.calledOnce(evalStub);
      assert.equal(result, true);
    });
  });
  describe("evaluateTargetingAndRemoveScreens", () => {
    it("should manipulate an array of screens", async () => {
      const screens = [
        {
          id: "first",
          targeting: true,
        },
        {
          id: "second",
          targeting: false,
        },
      ];

      const expectedScreens = [
        {
          id: "first",
          targeting: true,
        },
      ];
      sandbox.stub(ASRouter, "evaluateExpression").callsFake(targeting => {
        return {
          evaluationStatus: {
            success: true,
            result: targeting.expression,
          },
        };
      });
      const evaluatedStrings =
        await AWScreenUtils.evaluateTargetingAndRemoveScreens(screens);
      assert.deepEqual(evaluatedStrings, expectedScreens);
    });
    it("should not remove screens with no targeting", async () => {
      const screens = [
        {
          id: "first",
        },
        {
          id: "second",
          targeting: false,
        },
      ];

      const expectedScreens = [
        {
          id: "first",
        },
      ];
      sandbox
        .stub(AWScreenUtils, "evaluateScreenTargeting")
        .callsFake(targeting => {
          if (targeting === undefined) {
            return true;
          }
          return targeting;
        });
      const evaluatedStrings =
        await AWScreenUtils.evaluateTargetingAndRemoveScreens(screens);
      assert.deepEqual(evaluatedStrings, expectedScreens);
    });
  });

  describe("addScreenImpression", () => {
    it("Should call addScreenImpression with provided screen ID", () => {
      const addScreenImpressionStub = sandbox.stub(
        ASRouter,
        "addScreenImpression"
      );
      const testScreen = { id: "test" };
      AWScreenUtils.addScreenImpression(testScreen);

      assert.calledOnce(addScreenImpressionStub);
      assert.equal(addScreenImpressionStub.firstCall.args[0].id, testScreen.id);
    });
  });
});

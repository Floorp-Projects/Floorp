import {

  actionCreators as ac,
  actionTypes as at,
  actionUtils as au,
  BACKGROUND_PROCESS,
  CONTENT_MESSAGE_TYPE,
  globalImportContext,
  MAIN_MESSAGE_TYPE,
  PRELOAD_MESSAGE_TYPE,
  UI_CODE
} from "common/Actions.jsm";

describe("Actions", () => {
  it("should set globalImportContext to UI_CODE", () => {
    assert.equal(globalImportContext, UI_CODE);
  });
});

describe("ActionTypes", () => {
  it("should be in alpha order", () => {
    assert.equal(Object.keys(at).join(", "), Object.keys(at).sort().join(", "));
  });
});

describe("ActionCreators", () => {
  describe("_RouteMessage", () => {
    it("should throw if options are not passed as the second param", () => {
      assert.throws(() => {
        au._RouteMessage({type: "FOO"});
      });
    });
    it("should set all defined options on the .meta property of the new action", () => {
      assert.deepEqual(
        au._RouteMessage({type: "FOO", meta: {hello: "world"}}, {from: "foo", to: "bar"}),
        {type: "FOO", meta: {hello: "world", from: "foo", to: "bar"}}
      );
    });
    it("should remove any undefined options related to message routing", () => {
      const action = au._RouteMessage({type: "FOO", meta: {fromTarget: "bar"}}, {from: "foo", to: "bar"});
      assert.isUndefined(action.meta.fromTarget);
    });
  });
  describe("SendToMain", () => {
    it("should create the right action", () => {
      const action = {type: "FOO", data: "BAR"};
      const newAction = ac.SendToMain(action);
      assert.deepEqual(newAction, {
        type: "FOO",
        data: "BAR",
        meta: {from: CONTENT_MESSAGE_TYPE, to: MAIN_MESSAGE_TYPE}
      });
    });
    it("should add the fromTarget if it was supplied", () => {
      const action = {type: "FOO", data: "BAR"};
      const newAction = ac.SendToMain(action, "port123");
      assert.equal(newAction.meta.fromTarget, "port123");
    });
    describe("isSendToMain", () => {
      it("should return true if action is SendToMain", () => {
        const newAction = ac.SendToMain({type: "FOO"});
        assert.isTrue(au.isSendToMain(newAction));
      });
      it("should return false if action is not SendToMain", () => {
        assert.isFalse(au.isSendToMain({type: "FOO"}));
      });
    });
  });
  describe("SendToContent", () => {
    it("should create the right action", () => {
      const action = {type: "FOO", data: "BAR"};
      const targetId = "abc123";
      const newAction = ac.SendToContent(action, targetId);
      assert.deepEqual(newAction, {
        type: "FOO",
        data: "BAR",
        meta: {from: MAIN_MESSAGE_TYPE, to: CONTENT_MESSAGE_TYPE, toTarget: targetId}
      });
    });
    it("should throw if no targetId is provided", () => {
      assert.throws(() => {
        ac.SendToContent({type: "FOO"});
      });
    });
    describe("isSendToContent", () => {
      it("should return true if action is SendToContent", () => {
        const newAction = ac.SendToContent({type: "FOO"}, "foo123");
        assert.isTrue(au.isSendToContent(newAction));
      });
      it("should return false if action is not SendToMain", () => {
        assert.isFalse(au.isSendToContent({type: "FOO"}));
        assert.isFalse(au.isSendToContent(ac.BroadcastToContent({type: "FOO"})));
      });
    });
    describe("isFromMain", () => {
      it("should return true if action is SendToContent", () => {
        const newAction = ac.SendToContent({type: "FOO"}, "foo123");
        assert.isTrue(au.isFromMain(newAction));
      });
      it("should return true if action is BroadcastToContent", () => {
        const newAction = ac.BroadcastToContent({type: "FOO"});
        assert.isTrue(au.isFromMain(newAction));
      });
      it("should return false if action is SendToMain", () => {
        const newAction = ac.SendToMain({type: "FOO"});
        assert.isFalse(au.isFromMain(newAction));
      });
    });
  });
  describe("BroadcastToContent", () => {
    it("should create the right action", () => {
      const action = {type: "FOO", data: "BAR"};
      const newAction = ac.BroadcastToContent(action);
      assert.deepEqual(newAction, {
        type: "FOO",
        data: "BAR",
        meta: {from: MAIN_MESSAGE_TYPE, to: CONTENT_MESSAGE_TYPE}
      });
    });
    describe("isBroadcastToContent", () => {
      it("should return true if action is BroadcastToContent", () => {
        assert.isTrue(au.isBroadcastToContent(ac.BroadcastToContent({type: "FOO"})));
      });
      it("should return false if action is not BroadcastToContent", () => {
        assert.isFalse(au.isBroadcastToContent({type: "FOO"}));
        assert.isFalse(au.isBroadcastToContent(ac.SendToContent({type: "FOO"}, "foo123")));
      });
    });
  });
  describe("SendToPreloaded", () => {
    it("should create the right action", () => {
      const action = {type: "FOO", data: "BAR"};
      const newAction = ac.SendToPreloaded(action);
      assert.deepEqual(newAction, {
        type: "FOO",
        data: "BAR",
        meta: {from: MAIN_MESSAGE_TYPE, to: PRELOAD_MESSAGE_TYPE}
      });
    });
  });
  describe("isSendToPreloaded", () => {
    it("should return true if action is SendToPreloaded", () => {
      assert.isTrue(au.isSendToPreloaded(ac.SendToPreloaded({type: "FOO"})));
    });
    it("should return false if action is not SendToPreloaded", () => {
      assert.isFalse(au.isSendToPreloaded({type: "FOO"}));
      assert.isFalse(au.isSendToPreloaded(ac.BroadcastToContent({type: "FOO"})));
    });
  });
  describe("UserEvent", () => {
    it("should include the given data", () => {
      const data = {action: "foo"};
      assert.equal(ac.UserEvent(data).data, data);
    });
    it("should wrap with SendToMain", () => {
      const action = ac.UserEvent({action: "foo"});
      assert.isTrue(au.isSendToMain(action), "isSendToMain");
    });
  });
  describe("UndesiredEvent", () => {
    it("should include the given data", () => {
      const data = {action: "foo"};
      assert.equal(ac.UndesiredEvent(data).data, data);
    });
    it("should wrap with SendToMain if in UI code", () => {
      assert.isTrue(au.isSendToMain(ac.UndesiredEvent({action: "foo"})), "isSendToMain");
    });
    it("should not wrap with SendToMain if not in UI code", () => {
      const action = ac.UndesiredEvent({action: "foo"}, BACKGROUND_PROCESS);
      assert.isFalse(au.isSendToMain(action), "isSendToMain");
    });
  });
  describe("PerfEvent", () => {
    it("should include the right data", () => {
      const data = {action: "foo"};
      assert.equal(ac.UndesiredEvent(data).data, data);
    });
    it("should wrap with SendToMain if in UI code", () => {
      assert.isTrue(au.isSendToMain(ac.PerfEvent({action: "foo"})), "isSendToMain");
    });
    it("should not wrap with SendToMain if not in UI code", () => {
      const action = ac.PerfEvent({action: "foo"}, BACKGROUND_PROCESS);
      assert.isFalse(au.isSendToMain(action), "isSendToMain");
    });
  });
  describe("ImpressionStats", () => {
    it("should include the right data", () => {
      const data = {action: "foo"};
      assert.equal(ac.ImpressionStats(data).data, data);
    });
    it("should wrap with SendToMain if in UI code", () => {
      assert.isTrue(au.isSendToMain(ac.ImpressionStats({action: "foo"})), "isSendToMain");
    });
    it("should not wrap with SendToMain if not in UI code", () => {
      const action = ac.ImpressionStats({action: "foo"}, BACKGROUND_PROCESS);
      assert.isFalse(au.isSendToMain(action), "isSendToMain");
    });
  });
});

describe("ActionUtils", () => {
  describe("getPortIdOfSender", () => {
    it("should return the PortID from a SendToMain action", () => {
      const portID = "foo123";
      const result = au.getPortIdOfSender(ac.SendToMain({type: "FOO"}, portID));
      assert.equal(result, portID);
    });
  });
});

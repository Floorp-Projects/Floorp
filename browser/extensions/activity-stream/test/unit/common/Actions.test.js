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
  describe("AlsoToMain", () => {
    it("should create the right action", () => {
      const action = {type: "FOO", data: "BAR"};
      const newAction = ac.AlsoToMain(action);
      assert.deepEqual(newAction, {
        type: "FOO",
        data: "BAR",
        meta: {from: CONTENT_MESSAGE_TYPE, to: MAIN_MESSAGE_TYPE}
      });
    });
    it("should add the fromTarget if it was supplied", () => {
      const action = {type: "FOO", data: "BAR"};
      const newAction = ac.AlsoToMain(action, "port123");
      assert.equal(newAction.meta.fromTarget, "port123");
    });
    describe("isSendToMain", () => {
      it("should return true if action is AlsoToMain", () => {
        const newAction = ac.AlsoToMain({type: "FOO"});
        assert.isTrue(au.isSendToMain(newAction));
      });
      it("should return false if action is not AlsoToMain", () => {
        assert.isFalse(au.isSendToMain({type: "FOO"}));
      });
    });
  });
  describe("AlsoToOneContent", () => {
    it("should create the right action", () => {
      const action = {type: "FOO", data: "BAR"};
      const targetId = "abc123";
      const newAction = ac.AlsoToOneContent(action, targetId);
      assert.deepEqual(newAction, {
        type: "FOO",
        data: "BAR",
        meta: {from: MAIN_MESSAGE_TYPE, to: CONTENT_MESSAGE_TYPE, toTarget: targetId}
      });
    });
    it("should throw if no targetId is provided", () => {
      assert.throws(() => {
        ac.AlsoToOneContent({type: "FOO"});
      });
    });
    describe("isSendToOneContent", () => {
      it("should return true if action is AlsoToOneContent", () => {
        const newAction = ac.AlsoToOneContent({type: "FOO"}, "foo123");
        assert.isTrue(au.isSendToOneContent(newAction));
      });
      it("should return false if action is not AlsoToMain", () => {
        assert.isFalse(au.isSendToOneContent({type: "FOO"}));
        assert.isFalse(au.isSendToOneContent(ac.BroadcastToContent({type: "FOO"})));
      });
    });
    describe("isFromMain", () => {
      it("should return true if action is AlsoToOneContent", () => {
        const newAction = ac.AlsoToOneContent({type: "FOO"}, "foo123");
        assert.isTrue(au.isFromMain(newAction));
      });
      it("should return true if action is BroadcastToContent", () => {
        const newAction = ac.BroadcastToContent({type: "FOO"});
        assert.isTrue(au.isFromMain(newAction));
      });
      it("should return false if action is AlsoToMain", () => {
        const newAction = ac.AlsoToMain({type: "FOO"});
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
        assert.isFalse(au.isBroadcastToContent(ac.AlsoToOneContent({type: "FOO"}, "foo123")));
      });
    });
  });
  describe("AlsoToPreloaded", () => {
    it("should create the right action", () => {
      const action = {type: "FOO", data: "BAR"};
      const newAction = ac.AlsoToPreloaded(action);
      assert.deepEqual(newAction, {
        type: "FOO",
        data: "BAR",
        meta: {from: MAIN_MESSAGE_TYPE, to: PRELOAD_MESSAGE_TYPE}
      });
    });
  });
  describe("isSendToPreloaded", () => {
    it("should return true if action is AlsoToPreloaded", () => {
      assert.isTrue(au.isSendToPreloaded(ac.AlsoToPreloaded({type: "FOO"})));
    });
    it("should return false if action is not AlsoToPreloaded", () => {
      assert.isFalse(au.isSendToPreloaded({type: "FOO"}));
      assert.isFalse(au.isSendToPreloaded(ac.BroadcastToContent({type: "FOO"})));
    });
  });
  describe("UserEvent", () => {
    it("should include the given data", () => {
      const data = {action: "foo"};
      assert.equal(ac.UserEvent(data).data, data);
    });
    it("should wrap with AlsoToMain", () => {
      const action = ac.UserEvent({action: "foo"});
      assert.isTrue(au.isSendToMain(action), "isSendToMain");
    });
  });
  describe("ASRouterUserEvent", () => {
    it("should include the given data", () => {
      const data = {action: "foo"};
      assert.equal(ac.ASRouterUserEvent(data).data, data);
    });
    it("should wrap with AlsoToMain", () => {
      const action = ac.ASRouterUserEvent({action: "foo"});
      assert.isTrue(au.isSendToMain(action), "isSendToMain");
    });
  });
  describe("UndesiredEvent", () => {
    it("should include the given data", () => {
      const data = {action: "foo"};
      assert.equal(ac.UndesiredEvent(data).data, data);
    });
    it("should wrap with AlsoToMain if in UI code", () => {
      assert.isTrue(au.isSendToMain(ac.UndesiredEvent({action: "foo"})), "isSendToMain");
    });
    it("should not wrap with AlsoToMain if not in UI code", () => {
      const action = ac.UndesiredEvent({action: "foo"}, BACKGROUND_PROCESS);
      assert.isFalse(au.isSendToMain(action), "isSendToMain");
    });
  });
  describe("PerfEvent", () => {
    it("should include the right data", () => {
      const data = {action: "foo"};
      assert.equal(ac.UndesiredEvent(data).data, data);
    });
    it("should wrap with AlsoToMain if in UI code", () => {
      assert.isTrue(au.isSendToMain(ac.PerfEvent({action: "foo"})), "isSendToMain");
    });
    it("should not wrap with AlsoToMain if not in UI code", () => {
      const action = ac.PerfEvent({action: "foo"}, BACKGROUND_PROCESS);
      assert.isFalse(au.isSendToMain(action), "isSendToMain");
    });
  });
  describe("ImpressionStats", () => {
    it("should include the right data", () => {
      const data = {action: "foo"};
      assert.equal(ac.ImpressionStats(data).data, data);
    });
    it("should wrap with AlsoToMain if in UI code", () => {
      assert.isTrue(au.isSendToMain(ac.ImpressionStats({action: "foo"})), "isSendToMain");
    });
    it("should not wrap with AlsoToMain if not in UI code", () => {
      const action = ac.ImpressionStats({action: "foo"}, BACKGROUND_PROCESS);
      assert.isFalse(au.isSendToMain(action), "isSendToMain");
    });
  });
  describe("WebExtEvent", () => {
    it("should set the provided type", () => {
      const action = ac.WebExtEvent(at.WEBEXT_CLICK, {source: "MyExtension", url: "foo.com"});
      assert.equal(action.type, at.WEBEXT_CLICK);
    });
    it("should set the provided data", () => {
      const data = {source: "MyExtension", url: "foo.com"};
      const action = ac.WebExtEvent(at.WEBEXT_CLICK, data);
      assert.equal(action.data, data);
    });
    it("should throw if the 'source' property is missing", () => {
      assert.throws(() => {
        ac.WebExtEvent(at.WEBEXT_CLICK, {});
      });
    });
  });
});

describe("ActionUtils", () => {
  describe("getPortIdOfSender", () => {
    it("should return the PortID from a AlsoToMain action", () => {
      const portID = "foo123";
      const result = au.getPortIdOfSender(ac.AlsoToMain({type: "FOO"}, portID));
      assert.equal(result, portID);
    });
  });
});

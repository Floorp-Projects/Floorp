const {
  actionCreators: ac,
  actionUtils: au,
  MAIN_MESSAGE_TYPE,
  CONTENT_MESSAGE_TYPE
} = require("common/Actions.jsm");

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
});

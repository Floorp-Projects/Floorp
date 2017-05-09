const DetectUserSessionStart = require("content-src/lib/detect-user-session-start");
const {actionTypes: at} = require("common/Actions.jsm");

describe("detectUserSessionStart", () => {
  describe("#sendEventOrAddListener", () => {
    it("should call ._sendEvent immediately if the document is visible", () => {
      const mockDocument = {visibilityState: "visible"};
      const instance = new DetectUserSessionStart({document: mockDocument});
      sinon.stub(instance, "_sendEvent");

      instance.sendEventOrAddListener();

      assert.calledOnce(instance._sendEvent);
    });
    it("should add an event listener on visibility changes the document is not visible", () => {
      const mockDocument = {visibilityState: "hidden", addEventListener: sinon.spy()};
      const instance = new DetectUserSessionStart({document: mockDocument});
      sinon.stub(instance, "_sendEvent");

      instance.sendEventOrAddListener();

      assert.notCalled(instance._sendEvent);
      assert.calledWith(mockDocument.addEventListener, "visibilitychange", instance._onVisibilityChange);
    });
  });
  describe("#_sendEvent", () => {
    it("should send an async message with the NEW_TAB_VISIBLE event", () => {
      const sendAsyncMessage = sinon.spy();
      const instance = new DetectUserSessionStart({sendAsyncMessage});

      instance._sendEvent();

      assert.calledWith(sendAsyncMessage, "ActivityStream:ContentToMain", {type: at.NEW_TAB_VISIBLE});
    });
  });
  describe("_onVisibilityChange", () => {
    it("should not send an event if visiblity is not visible", () => {
      const instance = new DetectUserSessionStart({document: {visibilityState: "hidden"}});
      sinon.stub(instance, "_sendEvent");

      instance._onVisibilityChange();

      assert.notCalled(instance._sendEvent);
    });
    it("should send an event and remove the event listener if visibility is visible", () => {
      const mockDocument = {visibilityState: "visible", removeEventListener: sinon.spy()};
      const instance = new DetectUserSessionStart({document: mockDocument});
      sinon.stub(instance, "_sendEvent");

      instance._onVisibilityChange();

      assert.calledOnce(instance._sendEvent);
      assert.calledWith(mockDocument.removeEventListener, "visibilitychange", instance._onVisibilityChange);
    });
  });
});

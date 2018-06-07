import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {DetectUserSessionStart} from "content-src/lib/detect-user-session-start";

describe("detectUserSessionStart", () => {
  let store;
  class PerfService {
    getMostRecentAbsMarkStartByName() { return 1234; }
    mark() {}
  }

  beforeEach(() => {
    store = {dispatch: () => {}};
  });
  describe("#sendEventOrAddListener", () => {
    it("should call ._sendEvent immediately if the document is visible", () => {
      const mockDocument = {visibilityState: "visible"};
      const instance = new DetectUserSessionStart(store, {document: mockDocument});
      sinon.stub(instance, "_sendEvent");

      instance.sendEventOrAddListener();

      assert.calledOnce(instance._sendEvent);
    });
    it("should add an event listener on visibility changes the document is not visible", () => {
      const mockDocument = {visibilityState: "hidden", addEventListener: sinon.spy()};
      const instance = new DetectUserSessionStart(store, {document: mockDocument});
      sinon.stub(instance, "_sendEvent");

      instance.sendEventOrAddListener();

      assert.notCalled(instance._sendEvent);
      assert.calledWith(mockDocument.addEventListener, "visibilitychange", instance._onVisibilityChange);
    });
  });
  describe("#_sendEvent", () => {
    it("should dispatch an action with the SAVE_SESSION_PERF_DATA", () => {
      const dispatch = sinon.spy(store, "dispatch");
      const instance = new DetectUserSessionStart(store);

      instance._sendEvent();

      assert.calledWith(dispatch, ac.AlsoToMain({
        type: at.SAVE_SESSION_PERF_DATA,
        data: {visibility_event_rcvd_ts: sinon.match.number}
      }));
    });

    it("shouldn't send a message if getMostRecentAbsMarkStartByName throws", () => {
      let perfService = new PerfService();
      sinon.stub(perfService, "getMostRecentAbsMarkStartByName").throws();
      const dispatch = sinon.spy(store, "dispatch");
      const instance = new DetectUserSessionStart(store, {perfService});

      instance._sendEvent();

      assert.notCalled(dispatch);
    });

    it('should call perfService.mark("visibility_event_rcvd_ts")', () => {
      let perfService = new PerfService();
      sinon.stub(perfService, "mark");
      const instance = new DetectUserSessionStart(store, {perfService});

      instance._sendEvent();

      assert.calledWith(perfService.mark, "visibility_event_rcvd_ts");
    });
  });

  describe("_onVisibilityChange", () => {
    it("should not send an event if visiblity is not visible", () => {
      const instance = new DetectUserSessionStart(store, {document: {visibilityState: "hidden"}});
      sinon.stub(instance, "_sendEvent");

      instance._onVisibilityChange();

      assert.notCalled(instance._sendEvent);
    });
    it("should send an event and remove the event listener if visibility is visible", () => {
      const mockDocument = {visibilityState: "visible", removeEventListener: sinon.spy()};
      const instance = new DetectUserSessionStart(store, {document: mockDocument});
      sinon.stub(instance, "_sendEvent");

      instance._onVisibilityChange();

      assert.calledOnce(instance._sendEvent);
      assert.calledWith(mockDocument.removeEventListener, "visibilitychange", instance._onVisibilityChange);
    });
  });
});

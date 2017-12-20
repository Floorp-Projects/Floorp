import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {NewTabInit} from "lib/NewTabInit.jsm";

describe("NewTabInit", () => {
  let instance;
  let store;
  let STATE;
  const requestFromTab = portID => instance.onAction(ac.SendToMain(
    {type: at.NEW_TAB_STATE_REQUEST}, portID));
  beforeEach(() => {
    STATE = {};
    store = {getState: sinon.stub().returns(STATE), dispatch: sinon.stub()};
    instance = new NewTabInit();
    instance.store = store;
  });
  it("should reply with a copy of the state immediately", () => {
    requestFromTab(123);

    const resp = ac.SendToContent({type: at.NEW_TAB_INITIAL_STATE, data: STATE}, 123);
    assert.calledWith(store.dispatch, resp);
  });
  describe("early / simulated new tabs", () => {
    const simulateTabInit = portID => instance.onAction({
      type: at.NEW_TAB_INIT,
      data: {portID, simulated: true}
    });
    beforeEach(() => {
      simulateTabInit("foo");
    });
    it("should dispatch if not replied yet", () => {
      requestFromTab("foo");

      assert.calledWith(store.dispatch, ac.SendToContent({type: at.NEW_TAB_INITIAL_STATE, data: STATE}, "foo"));
    });
    it("should dispatch once for multiple requests", () => {
      requestFromTab("foo");
      requestFromTab("foo");
      requestFromTab("foo");

      assert.calledOnce(store.dispatch);
    });
    describe("multiple tabs", () => {
      beforeEach(() => {
        simulateTabInit("bar");
      });
      it("should dispatch once to each tab", () => {
        requestFromTab("foo");
        requestFromTab("bar");
        assert.calledTwice(store.dispatch);
        requestFromTab("foo");
        requestFromTab("bar");

        assert.calledTwice(store.dispatch);
      });
      it("should clean up when tabs close", () => {
        assert.propertyVal(instance._repliedEarlyTabs, "size", 2);
        instance.onAction(ac.SendToMain({type: at.NEW_TAB_UNLOAD}, "foo"));
        assert.propertyVal(instance._repliedEarlyTabs, "size", 1);
        instance.onAction(ac.SendToMain({type: at.NEW_TAB_UNLOAD}, "foo"));
        assert.propertyVal(instance._repliedEarlyTabs, "size", 1);
        instance.onAction(ac.SendToMain({type: at.NEW_TAB_UNLOAD}, "bar"));
        assert.propertyVal(instance._repliedEarlyTabs, "size", 0);
      });
    });
  });
});

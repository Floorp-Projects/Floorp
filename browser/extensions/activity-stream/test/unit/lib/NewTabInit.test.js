const {NewTabInit} = require("lib/NewTabInit.jsm");
const {actionTypes: at, actionCreators: ac} = require("common/Actions.jsm");

describe("NewTabInit", () => {
  let instance;
  let store;
  let STATE;
  beforeEach(() => {
    STATE = {};
    store = {getState: sinon.stub().returns(STATE), dispatch: sinon.stub()};
    instance = new NewTabInit();
    instance.store = store;
  });
  it("should reply with a copy of the state immediately if localization is ready", () => {
    STATE.App = {strings: {}};

    instance.onAction(ac.SendToMain({type: at.NEW_TAB_STATE_REQUEST}, 123));

    const resp = ac.SendToContent({type: at.NEW_TAB_INITIAL_STATE, data: STATE}, 123);
    assert.calledWith(store.dispatch, resp);
  });
  it("should not reply immediately if localization is not ready", () => {
    STATE.App = {strings: null};

    instance.onAction(ac.SendToMain({type: at.NEW_TAB_STATE_REQUEST}, 123));

    assert.notCalled(store.dispatch);
  });
  it("should dispatch responses for queued targets when LOCALE_UPDATED is received", () => {
    STATE.App = {strings: null};

    // Send requests before strings are ready
    instance.onAction(ac.SendToMain({type: at.NEW_TAB_STATE_REQUEST}, "foo"));
    instance.onAction(ac.SendToMain({type: at.NEW_TAB_STATE_REQUEST}, "bar"));
    instance.onAction(ac.SendToMain({type: at.NEW_TAB_STATE_REQUEST}, "baz"));
    assert.notCalled(store.dispatch);

    // Update strings
    STATE.App = {strings: {}};
    instance.onAction({type: at.LOCALE_UPDATED});

    assert.calledThrice(store.dispatch);
    const action = {type: at.NEW_TAB_INITIAL_STATE, data: STATE};
    assert.calledWith(store.dispatch, ac.SendToContent(action, "foo"));
    assert.calledWith(store.dispatch, ac.SendToContent(action, "bar"));
    assert.calledWith(store.dispatch, ac.SendToContent(action, "baz"));
  });
  it("should clear targets from the queue once they have been sent", () => {
    STATE.App = {strings: null};
    instance.onAction(ac.SendToMain({type: at.NEW_TAB_STATE_REQUEST}, "foo"));
    instance.onAction(ac.SendToMain({type: at.NEW_TAB_STATE_REQUEST}, "bar"));
    instance.onAction(ac.SendToMain({type: at.NEW_TAB_STATE_REQUEST}, "baz"));

    STATE.App = {strings: {}};
    instance.onAction({type: at.LOCALE_UPDATED});
    assert.calledThrice(store.dispatch);

    store.dispatch.reset();
    instance.onAction({type: at.LOCALE_UPDATED});
    assert.notCalled(store.dispatch);
  });
});

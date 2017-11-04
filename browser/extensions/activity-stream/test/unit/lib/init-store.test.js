const initStore = require("content-src/lib/init-store");
const {MERGE_STORE_ACTION, rehydrationMiddleware} = initStore;
const {GlobalOverrider, addNumberReducer} = require("test/unit/utils");
const {actionCreators: ac, actionTypes: at} = require("common/Actions.jsm");

describe("initStore", () => {
  let globals;
  let store;
  beforeEach(() => {
    globals = new GlobalOverrider();
    globals.set("sendAsyncMessage", globals.sandbox.spy());
    globals.set("addMessageListener", globals.sandbox.spy());
    store = initStore({number: addNumberReducer});
  });
  afterEach(() => globals.restore());
  it("should create a store with the provided reducers", () => {
    assert.ok(store);
    assert.property(store.getState(), "number");
  });
  it("should add a listener that dispatches actions", () => {
    assert.calledWith(global.addMessageListener, initStore.INCOMING_MESSAGE_NAME);
    const listener = global.addMessageListener.firstCall.args[1];
    globals.sandbox.spy(store, "dispatch");
    const message = {name: initStore.INCOMING_MESSAGE_NAME, data: {type: "FOO"}};

    listener(message);

    assert.calledWith(store.dispatch, message.data);
  });
  it("should not throw if addMessageListener is not defined", () => {
    // Note: this is being set/restored by GlobalOverrider
    delete global.addMessageListener;

    assert.doesNotThrow(() => initStore({number: addNumberReducer}));
  });
  it("should initialize with an initial state if provided as the second argument", () => {
    store = initStore({number: addNumberReducer}, {number: 42});

    assert.equal(store.getState().number, 42);
  });
  it("should log errors from failed messages", () => {
    const callback = global.addMessageListener.firstCall.args[1];
    globals.sandbox.stub(global.console, "error");
    globals.sandbox.stub(store, "dispatch").throws(Error("failed"));

    const message = {name: initStore.INCOMING_MESSAGE_NAME, data: {type: MERGE_STORE_ACTION}};
    callback(message);

    assert.calledOnce(global.console.error);
  });
  it("should replace the state if a MERGE_STORE_ACTION is dispatched", () => {
    store.dispatch({type: initStore.MERGE_STORE_ACTION, data: {number: 42}});
    assert.deepEqual(store.getState(), {number: 42});
  });
  it("should send out SendToMain actions", () => {
    const action = ac.SendToMain({type: "FOO"});
    store.dispatch(action);
    assert.calledWith(global.sendAsyncMessage, initStore.OUTGOING_MESSAGE_NAME, action);
  });
  it("should not send out other types of actions", () => {
    store.dispatch({type: "FOO"});
    assert.notCalled(global.sendAsyncMessage);
  });
  describe("rehydrationMiddleware", () => {
    it("should allow NEW_TAB_STATE_REQUEST to go through", () => {
      const action = ac.SendToMain({type: at.NEW_TAB_STATE_REQUEST});
      const next = sinon.spy();
      rehydrationMiddleware(store)(next)(action);
      assert.calledWith(next, action);
    });
    it("should dispatch an additional NEW_TAB_STATE_REQUEST if INIT was received after a request", () => {
      const requestAction = ac.SendToMain({type: at.NEW_TAB_STATE_REQUEST});
      const next = sinon.spy();

      rehydrationMiddleware(store)(next)(requestAction);

      next.reset();
      rehydrationMiddleware(store)(next)({type: at.INIT});
      assert.calledWith(next, requestAction);
    });
    it("should allow MERGE_STORE_ACTION to go through", () => {
      const action = {type: MERGE_STORE_ACTION};
      const next = sinon.spy();
      rehydrationMiddleware(store)(next)(action);
      assert.calledWith(next, action);
    });
    it("should not allow actions from main to go through before MERGE_STORE_ACTION was received", () => {
      const next = sinon.spy();

      rehydrationMiddleware(store)(next)(ac.BroadcastToContent({type: "FOO"}));
      rehydrationMiddleware(store)(next)(ac.SendToContent({type: "FOO"}, 123));

      assert.notCalled(next);
    });
    it("should allow all local actions to go through", () => {
      const action = {type: "FOO"};
      const next = sinon.spy();
      rehydrationMiddleware(store)(next)(action);
      assert.calledWith(next, action);
    });
    it("should allow actions from main to go through after MERGE_STORE_ACTION has been received", () => {
      const next = sinon.spy();
      rehydrationMiddleware(store)(next)({type: MERGE_STORE_ACTION});
      next.reset();

      const action = ac.SendToContent({type: "FOO"}, 123);
      rehydrationMiddleware(store)(next)(action);
      assert.calledWith(next, action);
    });
  });
});

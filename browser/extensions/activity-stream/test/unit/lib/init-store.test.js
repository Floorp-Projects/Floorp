import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {addNumberReducer, GlobalOverrider} from "test/unit/utils";
import {
  EARLY_QUEUED_ACTIONS,
  INCOMING_MESSAGE_NAME,
  initStore,
  MERGE_STORE_ACTION,
  OUTGOING_MESSAGE_NAME,
  queueEarlyMessageMiddleware,
  rehydrationMiddleware
} from "content-src/lib/init-store";

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
    assert.calledWith(global.addMessageListener, INCOMING_MESSAGE_NAME);
    const listener = global.addMessageListener.firstCall.args[1];
    globals.sandbox.spy(store, "dispatch");
    const message = {name: INCOMING_MESSAGE_NAME, data: {type: "FOO"}};

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

    const message = {name: INCOMING_MESSAGE_NAME, data: {type: MERGE_STORE_ACTION}};
    callback(message);

    assert.calledOnce(global.console.error);
  });
  it("should replace the state if a MERGE_STORE_ACTION is dispatched", () => {
    store.dispatch({type: MERGE_STORE_ACTION, data: {number: 42}});
    assert.deepEqual(store.getState(), {number: 42});
  });
  it("should send out SendToMain actions", () => {
    const action = ac.SendToMain({type: "FOO"});
    store.dispatch(action);
    assert.calledWith(global.sendAsyncMessage, OUTGOING_MESSAGE_NAME, action);
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
  describe("queueEarlyMessageMiddleware", () => {
    it("should allow all local actions to go through", () => {
      const action = {type: "FOO"};
      const next = sinon.spy();

      queueEarlyMessageMiddleware(store)(next)(action);

      assert.calledWith(next, action);
    });
    it("should allow action to main that does not belong to EARLY_QUEUED_ACTIONS to go through", () => {
      const action = ac.SendToMain({type: "FOO"});
      const next = sinon.spy();

      queueEarlyMessageMiddleware(store)(next)(action);

      assert.calledWith(next, action);
    });
    it(`should line up EARLY_QUEUED_ACTIONS only let them go through after it receives the action from main`, () => {
      EARLY_QUEUED_ACTIONS.forEach(actionType => {
        const testStore = initStore({number: addNumberReducer});
        const next = sinon.spy();
        const action = ac.SendToMain({type: actionType});
        const fromMainAction = ac.SendToContent({type: "FOO"}, 123);

        // Early actions should be added to the queue
        queueEarlyMessageMiddleware(testStore)(next)(action);
        queueEarlyMessageMiddleware(testStore)(next)(action);

        assert.notCalled(next);
        assert.equal(testStore._earlyActionQueue.length, 2);
        next.reset();

        // Receiving action from main would empty the queue
        queueEarlyMessageMiddleware(testStore)(next)(fromMainAction);

        assert.calledThrice(next);
        assert.equal(next.firstCall.args[0], fromMainAction);
        assert.equal(next.secondCall.args[0], action);
        assert.equal(next.thirdCall.args[0], action);
        assert.equal(testStore._earlyActionQueue.length, 0);
        next.reset();

        // New action should go through immediately
        queueEarlyMessageMiddleware(testStore)(next)(action);
        assert.calledOnce(next);
        assert.calledWith(next, action);
      });
    });
  });
});

import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { addNumberReducer, GlobalOverrider } from "test/unit/utils";
import {
  EARLY_QUEUED_ACTIONS,
  INCOMING_MESSAGE_NAME,
  initStore,
  MERGE_STORE_ACTION,
  OUTGOING_MESSAGE_NAME,
  queueEarlyMessageMiddleware,
  rehydrationMiddleware,
} from "content-src/lib/init-store";

describe("initStore", () => {
  let globals;
  let store;
  beforeEach(() => {
    globals = new GlobalOverrider();
    globals.set("RPMSendAsyncMessage", globals.sandbox.spy());
    globals.set("RPMAddMessageListener", globals.sandbox.spy());
    store = initStore({ number: addNumberReducer });
  });
  afterEach(() => globals.restore());
  it("should create a store with the provided reducers", () => {
    assert.ok(store);
    assert.property(store.getState(), "number");
  });
  it("should add a listener that dispatches actions", () => {
    assert.calledWith(global.RPMAddMessageListener, INCOMING_MESSAGE_NAME);
    const [, listener] = global.RPMAddMessageListener.firstCall.args;
    globals.sandbox.spy(store, "dispatch");
    const message = { name: INCOMING_MESSAGE_NAME, data: { type: "FOO" } };

    listener(message);

    assert.calledWith(store.dispatch, message.data);
  });
  it("should not throw if RPMAddMessageListener is not defined", () => {
    // Note: this is being set/restored by GlobalOverrider
    delete global.RPMAddMessageListener;

    assert.doesNotThrow(() => initStore({ number: addNumberReducer }));
  });
  it("should log errors from failed messages", () => {
    const [, callback] = global.RPMAddMessageListener.firstCall.args;
    globals.sandbox.stub(global.console, "error");
    globals.sandbox.stub(store, "dispatch").throws(Error("failed"));

    const message = {
      name: INCOMING_MESSAGE_NAME,
      data: { type: MERGE_STORE_ACTION },
    };
    callback(message);

    assert.calledOnce(global.console.error);
  });
  it("should replace the state if a MERGE_STORE_ACTION is dispatched", () => {
    store.dispatch({ type: MERGE_STORE_ACTION, data: { number: 42 } });
    assert.deepEqual(store.getState(), { number: 42 });
  });
  it("should call .send and update the local store if an AlsoToMain action is dispatched", () => {
    const subscriber = sinon.spy();
    const action = ac.AlsoToMain({ type: "FOO" });

    store.subscribe(subscriber);
    store.dispatch(action);

    assert.calledWith(
      global.RPMSendAsyncMessage,
      OUTGOING_MESSAGE_NAME,
      action
    );
    assert.calledOnce(subscriber);
  });
  it("should call .send but not update the local store if an OnlyToMain action is dispatched", () => {
    const subscriber = sinon.spy();
    const action = ac.OnlyToMain({ type: "FOO" });

    store.subscribe(subscriber);
    store.dispatch(action);

    assert.calledWith(
      global.RPMSendAsyncMessage,
      OUTGOING_MESSAGE_NAME,
      action
    );
    assert.notCalled(subscriber);
  });
  it("should not send out other types of actions", () => {
    store.dispatch({ type: "FOO" });
    assert.notCalled(global.RPMSendAsyncMessage);
  });
  describe("rehydrationMiddleware", () => {
    it("should allow NEW_TAB_STATE_REQUEST to go through", () => {
      const action = ac.AlsoToMain({ type: at.NEW_TAB_STATE_REQUEST });
      const next = sinon.spy();
      rehydrationMiddleware(store)(next)(action);
      assert.calledWith(next, action);
    });
    it("should dispatch an additional NEW_TAB_STATE_REQUEST if INIT was received after a request", () => {
      const requestAction = ac.AlsoToMain({ type: at.NEW_TAB_STATE_REQUEST });
      const next = sinon.spy();
      const dispatch = rehydrationMiddleware(store)(next);

      dispatch(requestAction);
      next.resetHistory();
      dispatch({ type: at.INIT });

      assert.calledWith(next, requestAction);
    });
    it("should allow MERGE_STORE_ACTION to go through", () => {
      const action = { type: MERGE_STORE_ACTION };
      const next = sinon.spy();
      rehydrationMiddleware(store)(next)(action);
      assert.calledWith(next, action);
    });
    it("should not allow actions from main to go through before MERGE_STORE_ACTION was received", () => {
      const next = sinon.spy();
      const dispatch = rehydrationMiddleware(store)(next);

      dispatch(ac.BroadcastToContent({ type: "FOO" }));
      dispatch(ac.AlsoToOneContent({ type: "FOO" }, 123));

      assert.notCalled(next);
    });
    it("should allow all local actions to go through", () => {
      const action = { type: "FOO" };
      const next = sinon.spy();
      rehydrationMiddleware(store)(next)(action);
      assert.calledWith(next, action);
    });
    it("should allow actions from main to go through after MERGE_STORE_ACTION has been received", () => {
      const next = sinon.spy();
      const dispatch = rehydrationMiddleware(store)(next);

      dispatch({ type: MERGE_STORE_ACTION });
      next.resetHistory();

      const action = ac.AlsoToOneContent({ type: "FOO" }, 123);
      dispatch(action);
      assert.calledWith(next, action);
    });
  });
  describe("queueEarlyMessageMiddleware", () => {
    it("should allow all local actions to go through", () => {
      const action = { type: "FOO" };
      const next = sinon.spy();

      queueEarlyMessageMiddleware(store)(next)(action);

      assert.calledWith(next, action);
    });
    it("should allow action to main that does not belong to EARLY_QUEUED_ACTIONS to go through", () => {
      const action = ac.AlsoToMain({ type: "FOO" });
      const next = sinon.spy();

      queueEarlyMessageMiddleware(store)(next)(action);

      assert.calledWith(next, action);
    });
    it(`should line up EARLY_QUEUED_ACTIONS only let them go through after it receives the action from main`, () => {
      EARLY_QUEUED_ACTIONS.forEach(actionType => {
        const testStore = initStore({ number: addNumberReducer });
        const next = sinon.spy();
        const dispatch = queueEarlyMessageMiddleware(testStore)(next);
        const action = ac.AlsoToMain({ type: actionType });
        const fromMainAction = ac.AlsoToOneContent({ type: "FOO" }, 123);

        // Early actions should be added to the queue
        dispatch(action);
        dispatch(action);

        assert.notCalled(next);
        assert.equal(testStore.getState.earlyActionQueue.length, 2);
        next.resetHistory();

        // Receiving action from main would empty the queue
        dispatch(fromMainAction);

        assert.calledThrice(next);
        assert.equal(next.firstCall.args[0], fromMainAction);
        assert.equal(next.secondCall.args[0], action);
        assert.equal(next.thirdCall.args[0], action);
        assert.equal(testStore.getState.earlyActionQueue.length, 0);
        next.resetHistory();

        // New action should go through immediately
        dispatch(action);
        assert.calledOnce(next);
        assert.calledWith(next, action);
      });
    });
  });
});

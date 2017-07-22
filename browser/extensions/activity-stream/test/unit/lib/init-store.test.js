const initStore = require("content-src/lib/init-store");
const {GlobalOverrider, addNumberReducer} = require("test/unit/utils");
const {actionCreators: ac} = require("common/Actions.jsm");

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
  it("should add a listener for incoming actions", () => {
    assert.calledWith(global.addMessageListener, initStore.INCOMING_MESSAGE_NAME);
    const callback = global.addMessageListener.firstCall.args[1];
    globals.sandbox.spy(store, "dispatch");
    const message = {name: initStore.INCOMING_MESSAGE_NAME, data: {type: "FOO"}};
    callback(message);
    assert.calledWith(store.dispatch, message.data);
  });
  it("should log errors from failed messages", () => {
    const callback = global.addMessageListener.firstCall.args[1];
    globals.sandbox.stub(global.console, "error");
    globals.sandbox.stub(store, "dispatch").throws(Error("failed"));

    const message = {name: initStore.INCOMING_MESSAGE_NAME, data: {type: "FOO"}};
    callback(message);

    assert.calledOnce(global.console.error);
  });
  it("should replace the state if a MERGE_STORE_ACTION is dispatched", () => {
    store.dispatch({type: initStore.MERGE_STORE_ACTION, data: {number: 42}});
    assert.deepEqual(store.getState(), {number: 42});
  });
  it("should send out SendToMain ations", () => {
    const action = ac.SendToMain({type: "FOO"});
    store.dispatch(action);
    assert.calledWith(global.sendAsyncMessage, initStore.OUTGOING_MESSAGE_NAME, action);
  });
  it("should not send out other types of ations", () => {
    store.dispatch({type: "FOO"});
    assert.notCalled(global.sendAsyncMessage);
  });
});

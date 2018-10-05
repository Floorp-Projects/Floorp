import {combineReducers, createStore} from "redux";
import {actionTypes as at} from "common/Actions.jsm";
import {enableASRouterContent} from "content-src/lib/asroutercontent.js";
import {reducers} from "common/Reducers.jsm";

describe("asrouter", () => {
  let sandbox;
  let store;
  let asrouterContent;
  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    store = createStore(combineReducers(reducers));
    sandbox.spy(store, "subscribe");
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should initialize asrouter once if ASRouter.initialized is true", () => {
    ({asrouterContent} = enableASRouterContent(store, {
      init: sandbox.stub(),
      initialized: false,
    }));
    store.dispatch({type: at.AS_ROUTER_INITIALIZED, data: {}});
    asrouterContent.initialized = true;

    // Dispatch another irrelevant event to make sure we don't initialize twice.
    store.dispatch({type: at.PREF_CHANGED, data: {name: "foo", value: "bar"}});

    assert.calledOnce(asrouterContent.init);
  });
  it("should do nothing if ASRouter is not initialized", () => {
    const addStub = sandbox.stub(global.document.body.classList, "add");
    ({asrouterContent} = enableASRouterContent(store, {
      init: sandbox.stub(),
      initialized: false,
    }));
    store.dispatch({type: at.INIT});
    assert.notCalled(addStub);
  });
});

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
  it("should initialize asrouter once if asrouterExperimentEnabled is true", () => {
    ({asrouterContent} = enableASRouterContent(store, {
      init: sandbox.stub(),
      uninit: sandbox.stub(),
      initialized: false
    }));
    store.dispatch({type: at.PREF_CHANGED, data: {name: "asrouterExperimentEnabled", value: true}});

    assert.calledOnce(asrouterContent.init);
  });
  it("should uninitialize asrouter if asrouterExperimentEnabled pref is turned off", () => {
    ({asrouterContent} = enableASRouterContent(store, {
      init: sandbox.stub(),
      uninit: sandbox.stub(),
      initialized: true
    }));
    store.dispatch({type: at.PREF_CHANGED, data: {name: "asrouterExperimentEnabled", value: true}});

    store.dispatch({type: at.PREF_CHANGED, data: {name: "asrouterExperimentEnabled", value: false}});
    assert.calledOnce(asrouterContent.uninit);
  });
});

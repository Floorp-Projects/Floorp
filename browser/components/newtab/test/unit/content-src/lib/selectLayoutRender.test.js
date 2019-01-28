import {combineReducers, createStore} from "redux";
import {actionTypes as at} from "common/Actions.jsm";
import {GlobalOverrider} from "test/unit/utils";
import {reducers} from "common/Reducers.jsm";
import {selectLayoutRender} from "content-src/lib/selectLayoutRender";

const FAKE_LAYOUT = [{width: 3, components: [{type: "foo", feed: {url: "foo.com"}}]}];
const FAKE_FEEDS = {"foo.com": {data: {recommendations: ["foo", "bar"]}}};

describe("selectLayoutRender", () => {
  let store;
  let globals;

  beforeEach(() => {
    globals = new GlobalOverrider();
    store = createStore(combineReducers(reducers));
  });

  afterEach(() => {
    globals.restore();
  });

  it("should return an empty array given initial state", () => {
    const result = selectLayoutRender(store.getState());
    assert.deepEqual(result, []);
  });

  it("should add .data property from feeds to each compontent in .layout", () => {
    store.dispatch({type: at.DISCOVERY_STREAM_LAYOUT_UPDATE, data: {layout: FAKE_LAYOUT}});
    store.dispatch({type: at.DISCOVERY_STREAM_FEEDS_UPDATE, data: FAKE_FEEDS});

    const result = selectLayoutRender(store.getState());

    assert.lengthOf(result, 1);
    assert.propertyVal(result[0], "width", 3);
    assert.deepEqual(result[0].components[0], {type: "foo", feed: {url: "foo.com"}, data: {recommendations: ["foo", "bar"]}});
  });

  it("should return layout property without data if feed isn't available", () => {
    store.dispatch({type: at.DISCOVERY_STREAM_LAYOUT_UPDATE, data: {layout: FAKE_LAYOUT}});
    store.dispatch({type: at.DISCOVERY_STREAM_FEEDS_UPDATE, data: {}});

    const result = selectLayoutRender(store.getState());

    assert.lengthOf(result, 1);
    assert.propertyVal(result[0], "width", 3);
    assert.deepEqual(result[0].components[0], FAKE_LAYOUT[0].components[0]);
  });

  it("should return spoc result for rolls below the probability", () => {
    const fakeSpocConfig = {positions: [{index: 0}, {index: 1}], probability: 0.5};
    const fakeLayout = [{width: 3, components: [{type: "foo", feed: {url: "foo.com"}, spocs: fakeSpocConfig}]}];
    const fakeSpocsData = {lastUpdated: 0, spocs: {spocs: ["fooSpoc", "barSpoc"]}};

    store.dispatch({type: at.DISCOVERY_STREAM_LAYOUT_UPDATE, data: {layout: fakeLayout}});
    store.dispatch({type: at.DISCOVERY_STREAM_FEEDS_UPDATE, data: FAKE_FEEDS});
    store.dispatch({type: at.DISCOVERY_STREAM_SPOCS_UPDATE, data: fakeSpocsData});
    globals.sandbox.stub(global.Math, "random").returns(0.1);

    const result = selectLayoutRender(store.getState());

    assert.lengthOf(result, 1);
    assert.deepEqual(result[0].components[0].data.recommendations[0], "fooSpoc");
    assert.deepEqual(result[0].components[0].data.recommendations[1], "barSpoc");
    assert.deepEqual(result[0].components[0].data.recommendations[2], "foo");
    assert.deepEqual(result[0].components[0].data.recommendations[3], "bar");
  });

  it("should not return spoc result for rolls above the probability", () => {
    const fakeSpocConfig = {positions: [{index: 0}, {index: 1}], probability: 0.5};
    const fakeLayout = [{width: 3, components: [{type: "foo", feed: {url: "foo.com"}, spocs: fakeSpocConfig}]}];
    const fakeSpocsData = {lastUpdated: 0, spocs: {spocs: ["fooSpoc", "barSpoc"]}};

    store.dispatch({type: at.DISCOVERY_STREAM_LAYOUT_UPDATE, data: {layout: fakeLayout}});
    store.dispatch({type: at.DISCOVERY_STREAM_FEEDS_UPDATE, data: FAKE_FEEDS});
    store.dispatch({type: at.DISCOVERY_STREAM_SPOCS_UPDATE, data: fakeSpocsData});
    globals.sandbox.stub(global.Math, "random").returns(0.6);

    const result = selectLayoutRender(store.getState());

    assert.lengthOf(result, 1);
    assert.deepEqual(result[0].components[0].data.recommendations[0], "foo");
    assert.deepEqual(result[0].components[0].data.recommendations[1], "bar");
  });
});

import { combineReducers, createStore } from "redux";
import { actionTypes as at } from "common/Actions.jsm";
import { GlobalOverrider } from "test/unit/utils";
import { reducers } from "common/Reducers.jsm";
import { selectLayoutRender } from "content-src/lib/selectLayoutRender";
const FAKE_LAYOUT = [
  {
    width: 3,
    components: [
      { type: "foo", feed: { url: "foo.com" }, properties: { items: 2 } },
    ],
  },
];
const FAKE_FEEDS = {
  "foo.com": { data: { recommendations: [{ id: "foo" }, { id: "bar" }] } },
};

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
    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
      prefs: {},
      rollCache: [],
    });
    assert.deepEqual(layoutRender, []);
  });

  it("should add .data property from feeds to each compontent in .layout", () => {
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: FAKE_LAYOUT },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: FAKE_FEEDS["foo.com"], url: "foo.com" },
    });
    store.dispatch({ type: at.DISCOVERY_STREAM_FEEDS_UPDATE });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.lengthOf(layoutRender, 1);
    assert.propertyVal(layoutRender[0], "width", 3);
    assert.deepEqual(layoutRender[0].components[0], {
      type: "foo",
      feed: { url: "foo.com" },
      properties: { items: 2 },
      data: {
        recommendations: [
          { id: "foo", pos: 0 },
          { id: "bar", pos: 1 },
        ],
      },
    });
  });

  it("should return layout with placeholder data if feed doesn't have data", () => {
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: FAKE_LAYOUT },
    });
    store.dispatch({ type: at.DISCOVERY_STREAM_FEEDS_UPDATE });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.lengthOf(layoutRender, 1);
    assert.propertyVal(layoutRender[0], "width", 3);
    assert.deepEqual(layoutRender[0].components[0].data.recommendations, [
      { placeholder: true },
      { placeholder: true },
    ]);
  });

  it("should return layout with empty spocs data if feed isn't defined but spocs is", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [{ type: "foo", spocs: { positions: [{ index: 2 }] } }],
      },
    ];
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });
    store.dispatch({ type: at.DISCOVERY_STREAM_FEEDS_UPDATE });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.lengthOf(layoutRender, 1);
    assert.propertyVal(layoutRender[0], "width", 3);
    assert.deepEqual(layoutRender[0].components[0].data.spocs, []);
  });

  it("should return layout with spocs data if feed isn't defined but spocs is", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [{ type: "foo", spocs: { positions: [{ index: 0 }] } }],
      },
    ];
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });
    store.dispatch({ type: at.DISCOVERY_STREAM_FEEDS_UPDATE });
    store.dispatch({
      type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
      data: {
        lastUpdated: 0,
        spocs: {
          spocs: {
            items: [{ id: 1 }, { id: 2 }, { id: 3 }],
          },
        },
      },
    });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.lengthOf(layoutRender, 1);
    assert.propertyVal(layoutRender[0], "width", 3);
    assert.deepEqual(layoutRender[0].components[0].data.spocs, [
      { id: 1, pos: 0 },
      { id: 2, pos: 1 },
      { id: 3, pos: 2 },
    ]);
  });

  it("should return layout with no spocs data if feed and spocs are unavailable", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [{ type: "foo", spocs: { positions: [{ index: 0 }] } }],
      },
    ];
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });
    store.dispatch({ type: at.DISCOVERY_STREAM_FEEDS_UPDATE });
    store.dispatch({
      type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
      data: {
        lastUpdated: 0,
        spocs: {
          spocs: {
            items: [],
          },
        },
      },
    });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.lengthOf(layoutRender, 1);
    assert.propertyVal(layoutRender[0], "width", 3);
    assert.equal(layoutRender[0].components[0].data.spocs.length, 0);
  });

  it("should return feed data offset by layout set prop", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [
          { type: "foo", properties: { offset: 1 }, feed: { url: "foo.com" } },
        ],
      },
    ];
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: FAKE_FEEDS["foo.com"], url: "foo.com" },
    });
    store.dispatch({ type: at.DISCOVERY_STREAM_FEEDS_UPDATE });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.deepEqual(layoutRender[0].components[0].data, {
      recommendations: [{ id: "bar" }],
    });
  });

  it("should return spoc result when there are more positions than spocs", () => {
    const fakeSpocConfig = {
      positions: [{ index: 0 }, { index: 1 }, { index: 2 }],
    };
    const fakeLayout = [
      {
        width: 3,
        components: [
          { type: "foo", feed: { url: "foo.com" }, spocs: fakeSpocConfig },
        ],
      },
    ];
    const fakeSpocsData = {
      lastUpdated: 0,
      spocs: { spocs: { items: ["fooSpoc", "barSpoc"] } },
    };

    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: FAKE_FEEDS["foo.com"], url: "foo.com" },
    });
    store.dispatch({ type: at.DISCOVERY_STREAM_FEEDS_UPDATE });
    store.dispatch({
      type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
      data: fakeSpocsData,
    });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.lengthOf(layoutRender, 1);
    assert.deepEqual(
      layoutRender[0].components[0].data.recommendations[0],
      "fooSpoc"
    );
    assert.deepEqual(
      layoutRender[0].components[0].data.recommendations[1],
      "barSpoc"
    );
    assert.deepEqual(layoutRender[0].components[0].data.recommendations[2], {
      id: "foo",
    });
    assert.deepEqual(layoutRender[0].components[0].data.recommendations[3], {
      id: "bar",
    });
  });

  it("should return a layout with feeds of items length with positions", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [
          { type: "foo", properties: { items: 3 }, feed: { url: "foo.com" } },
        ],
      },
    ];
    const fakeRecommendations = [
      { name: "item1" },
      { name: "item2" },
      { name: "item3" },
      { name: "item4" },
    ];
    const fakeFeeds = {
      "foo.com": { data: { recommendations: fakeRecommendations } },
    };
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: fakeFeeds["foo.com"], url: "foo.com" },
    });
    store.dispatch({ type: at.DISCOVERY_STREAM_FEEDS_UPDATE });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    const { recommendations } = layoutRender[0].components[0].data;
    assert.equal(recommendations.length, 4);
    assert.equal(recommendations[0].pos, 0);
    assert.equal(recommendations[1].pos, 1);
    assert.equal(recommendations[2].pos, 2);
    assert.equal(recommendations[3].pos, undefined);
  });
  it("should stop rendering feeds if we hit one that's not ready", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [
          { type: "foo1" },
          { type: "foo2", properties: { items: 3 }, feed: { url: "foo2.com" } },
          { type: "foo3", properties: { items: 3 }, feed: { url: "foo3.com" } },
          { type: "foo4", properties: { items: 3 }, feed: { url: "foo4.com" } },
          { type: "foo5" },
        ],
      },
    ];
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: { data: { recommendations: [] } }, url: "foo2.com" },
    });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.equal(layoutRender[0].components[0].type, "foo1");
    assert.equal(layoutRender[0].components[1].type, "foo2");
    assert.isTrue(
      layoutRender[0].components[2].data.recommendations[0].placeholder
    );
    assert.lengthOf(layoutRender[0].components, 3);
    assert.isUndefined(layoutRender[0].components[3]);
  });
  it("should render everything if everything is ready", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [
          { type: "foo1" },
          { type: "foo2", properties: { items: 3 }, feed: { url: "foo2.com" } },
          { type: "foo3", properties: { items: 3 }, feed: { url: "foo3.com" } },
          { type: "foo4", properties: { items: 3 }, feed: { url: "foo4.com" } },
          { type: "foo5" },
        ],
      },
    ];
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: { data: { recommendations: [] } }, url: "foo2.com" },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: { data: { recommendations: [] } }, url: "foo3.com" },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: { data: { recommendations: [] } }, url: "foo4.com" },
    });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.equal(layoutRender[0].components[0].type, "foo1");
    assert.equal(layoutRender[0].components[1].type, "foo2");
    assert.equal(layoutRender[0].components[2].type, "foo3");
    assert.equal(layoutRender[0].components[3].type, "foo4");
    assert.equal(layoutRender[0].components[4].type, "foo5");
  });
  it("should stop rendering feeds if we hit a not ready spoc", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [
          { type: "foo1" },
          { type: "foo2", properties: { items: 3 }, feed: { url: "foo2.com" } },
          {
            type: "foo3",
            properties: { items: 3 },
            feed: { url: "foo3.com" },
            spocs: { positions: [{ index: 0 }] },
          },
          { type: "foo4", properties: { items: 3 }, feed: { url: "foo4.com" } },
          { type: "foo5" },
        ],
      },
    ];
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: { data: { recommendations: [] } }, url: "foo2.com" },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: { data: { recommendations: [] } }, url: "foo3.com" },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: { data: { recommendations: [] } }, url: "foo4.com" },
    });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.equal(layoutRender[0].components[0].type, "foo1");
    assert.equal(layoutRender[0].components[1].type, "foo2");
    assert.deepEqual(layoutRender[0].components[2].data.recommendations, [
      { placeholder: true },
      { placeholder: true },
      { placeholder: true },
    ]);
  });
  it("should not render a spoc if there are no available spocs", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [
          { type: "foo1" },
          { type: "foo2", properties: { items: 3 }, feed: { url: "foo2.com" } },
          {
            type: "foo3",
            properties: { items: 3 },
            feed: { url: "foo3.com" },
            spocs: { positions: [{ index: 0 }] },
          },
          { type: "foo4", properties: { items: 3 }, feed: { url: "foo4.com" } },
          { type: "foo5" },
        ],
      },
    ];
    const fakeSpocsData = { lastUpdated: 0, spocs: { spocs: [] } };
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: { data: { recommendations: [] } }, url: "foo2.com" },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: {
        feed: { data: { recommendations: [{ name: "rec" }] } },
        url: "foo3.com",
      },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: { feed: { data: { recommendations: [] } }, url: "foo4.com" },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
      data: fakeSpocsData,
    });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.deepEqual(layoutRender[0].components[2].data.recommendations[0], {
      name: "rec",
      pos: 0,
    });
  });
  it("should not render a row if no components exist after filter in that row", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [{ type: "TopSites" }],
      },
      {
        width: 3,
        components: [{ type: "Message" }],
      },
    ];
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
      prefs: { "feeds.topsites": true },
    });

    assert.equal(layoutRender[0].components[0].type, "TopSites");
    assert.equal(layoutRender[1], undefined);
  });
  it("should not render a component if filtered", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [{ type: "Message" }, { type: "TopSites" }],
      },
    ];
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });

    const { layoutRender } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
      prefs: { "feeds.topsites": true },
    });

    assert.equal(layoutRender[0].components[0].type, "TopSites");
    assert.equal(layoutRender[0].components[1], undefined);
  });
  it("should skip rendering a spoc in position if that spoc is blocked for that session", () => {
    const fakeLayout = [
      {
        width: 3,
        components: [
          {
            type: "foo1",
            properties: { items: 3 },
            feed: { url: "foo1.com" },
            spocs: { positions: [{ index: 0 }] },
          },
        ],
      },
    ];
    const fakeSpocsData = {
      lastUpdated: 0,
      spocs: {
        spocs: { items: [{ name: "spoc", url: "https://foo.com" }] },
      },
    };
    store.dispatch({
      type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
      data: { layout: fakeLayout },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_FEED_UPDATE,
      data: {
        feed: { data: { recommendations: [{ name: "rec" }] } },
        url: "foo1.com",
      },
    });
    store.dispatch({
      type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
      data: fakeSpocsData,
    });

    const { layoutRender: layout1 } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    store.dispatch({
      type: at.DISCOVERY_STREAM_SPOC_BLOCKED,
      data: { url: "https://foo.com" },
    });

    const { layoutRender: layout2 } = selectLayoutRender({
      state: store.getState().DiscoveryStream,
    });

    assert.deepEqual(layout1[0].components[0].data.recommendations[0], {
      name: "spoc",
      url: "https://foo.com",
      pos: 0,
    });
    assert.deepEqual(layout2[0].components[0].data.recommendations[0], {
      name: "rec",
      pos: 0,
    });
  });
});

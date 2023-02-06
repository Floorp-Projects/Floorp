import { combineReducers, createStore } from "redux";
import {
  INITIAL_STATE,
  reducers,
  TOP_SITES_DEFAULT_ROWS,
} from "common/Reducers.sys.mjs";
import { mount } from "enzyme";
import { TopSites as OldTopSites } from "content-src/components/TopSites/TopSites";
import { Provider } from "react-redux";
import React from "react";
import {
  TopSites as TopSitesContainer,
  _TopSites as TopSites,
} from "content-src/components/DiscoveryStreamComponents/TopSites/TopSites";

describe("Discovery Stream <TopSites>", () => {
  let wrapper;
  let store;
  let defaultTopSiteRows;
  let defaultTopSites;

  beforeEach(() => {
    defaultTopSiteRows = [
      { label: "facebook" },
      { label: "amazon" },
      { label: "google" },
      { label: "apple" },
    ];
    defaultTopSites = {
      rows: defaultTopSiteRows,
    };
    INITIAL_STATE.Prefs.values.topSitesRows = TOP_SITES_DEFAULT_ROWS;
    store = createStore(combineReducers(reducers), INITIAL_STATE);
    wrapper = mount(
      <Provider store={store}>
        <TopSitesContainer TopSites={defaultTopSites} />
      </Provider>
    );
  });

  afterEach(() => {
    wrapper.unmount();
  });

  it("should return a wrapper around old TopSites", () => {
    const oldTopSites = wrapper.find(OldTopSites);
    const dsTopSitesWrapper = wrapper.find(".ds-top-sites");

    assert.ok(wrapper.exists());
    assert.lengthOf(oldTopSites, 1);
    assert.lengthOf(dsTopSitesWrapper, 1);
  });

  describe("TopSites header", () => {
    it("should have header title undefined by default", () => {
      const oldTopSites = wrapper.find(OldTopSites);
      assert.isUndefined(oldTopSites.props().title);
    });

    it("should set header title on old TopSites", () => {
      let DEFAULT_PROPS = {
        header: { title: "test" },
      };
      wrapper = mount(
        <Provider store={store}>
          <TopSitesContainer {...DEFAULT_PROPS} />
        </Provider>
      );
      const oldTopSites = wrapper.find(OldTopSites);
      assert.equal(oldTopSites.props().title, "test");
    });
  });

  describe("insertSpocContent", () => {
    let insertSpocContent;
    const topSiteSpoc = {
      url: "foo",
      sponsor: "bar",
      raw_image_src: "foobar",
      flight_id: "1234",
      id: "5678",
      shim: { impression: "1011" },
    };
    const data = { spocs: [topSiteSpoc] };
    const resultSpocFirst = {
      customScreenshotURL:
        "https://img-getpocket.cdn.mozilla.net/40x40/filters:format(jpeg):quality(60):no_upscale():strip_exif()/foobar",
      type: "SPOC",
      label: "bar",
      title: "bar",
      url: "foo",
      flightId: "1234",
      id: "5678",
      guid: "5678",
      shim: {
        impression: "1011",
      },
      pos: 0,
    };
    const resultSpocForth = {
      customScreenshotURL:
        "https://img-getpocket.cdn.mozilla.net/40x40/filters:format(jpeg):quality(60):no_upscale():strip_exif()/foobar",
      type: "SPOC",
      label: "bar",
      title: "bar",
      url: "foo",
      flightId: "1234",
      id: "5678",
      guid: "5678",
      shim: {
        impression: "1011",
      },
      pos: 4,
    };
    const pinnedSite = {
      label: "pinnedSite",
      isPinned: true,
    };

    beforeEach(() => {
      const instance = wrapper.find(TopSites).instance();
      insertSpocContent = instance.insertSpocContent.bind(instance);
    });

    it("Should return null if no data or no TopSites", () => {
      assert.isNull(insertSpocContent(defaultTopSites, {}, 1));
      assert.isNull(insertSpocContent({}, data, 1));
    });

    it("Should return null if an organic SPOC topsite exists", () => {
      const topSitesWithOrganicSpoc = {
        rows: [...defaultTopSiteRows, topSiteSpoc],
      };

      assert.isNull(insertSpocContent(topSitesWithOrganicSpoc, data, 1));
    });

    it("Should return next spoc if the first SPOC is an existing organic top site", () => {
      const topSitesWithOrganicSpoc = {
        rows: [...defaultTopSiteRows, topSiteSpoc],
      };
      const extraSpocData = {
        spocs: [
          topSiteSpoc,
          {
            url: "foo2",
            sponsor: "bar2",
            raw_image_src: "foobar2",
            flight_id: "1234",
            id: "5678",
            shim: { impression: "1011" },
          },
        ],
      };

      const result = insertSpocContent(
        topSitesWithOrganicSpoc,
        extraSpocData,
        5
      );

      const availableSpoc = {
        customScreenshotURL:
          "https://img-getpocket.cdn.mozilla.net/40x40/filters:format(jpeg):quality(60):no_upscale():strip_exif()/foobar2",
        type: "SPOC",
        label: "bar2",
        title: "bar2",
        url: "foo2",
        flightId: "1234",
        id: "5678",
        guid: "5678",
        shim: {
          impression: "1011",
        },
        pos: 5,
      };
      const expectedResult = {
        rows: [...topSitesWithOrganicSpoc.rows, availableSpoc],
      };

      assert.deepEqual(result, expectedResult);
    });

    it("should add spoc to the 4th position", () => {
      const result = insertSpocContent(defaultTopSites, data, 4);

      const expectedResult = {
        rows: [...defaultTopSiteRows, resultSpocForth],
      };
      assert.deepEqual(result, expectedResult);
    });

    it("should add to first position", () => {
      const result = insertSpocContent(defaultTopSites, data, 0);
      assert.deepEqual(result, {
        rows: [resultSpocFirst, ...defaultTopSiteRows.slice(1)],
      });
    });

    it("should add to first position even if there are pins", () => {
      const topSiteRowsWithPins = [
        pinnedSite,
        pinnedSite,
        ...defaultTopSiteRows,
      ];

      const result = insertSpocContent({ rows: topSiteRowsWithPins }, data, 0);

      assert.deepEqual(result, {
        rows: [
          resultSpocFirst,
          pinnedSite,
          pinnedSite,
          ...defaultTopSiteRows.slice(1),
        ],
      });
    });
  });
});

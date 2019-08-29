import { combineReducers, createStore } from "redux";
import {
  INITIAL_STATE,
  reducers,
  TOP_SITES_DEFAULT_ROWS,
} from "common/Reducers.jsm";
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
  const defaultTopSiteRows = [
    { label: "facebook" },
    { label: "amazon" },
    { label: "google" },
    { label: "apple" },
  ];
  const defaultTopSites = {
    rows: defaultTopSiteRows,
  };

  beforeEach(() => {
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
      image_src: "foobar",
      campaign_id: "1234",
      id: "5678",
      shim: { impression: "1011" },
    };
    const data = { spocs: [topSiteSpoc] };
    const resultSpocLeft = {
      customScreenshotURL: "foobar",
      type: "SPOC",
      label: "bar",
      title: "bar",
      url: "foo",
      campaignId: "1234",
      id: "5678",
      guid: "5678",
      shim: {
        impression: "1011",
      },
      pos: 0,
    };
    const resultSpocRight = {
      customScreenshotURL: "foobar",
      type: "SPOC",
      label: "bar",
      title: "bar",
      url: "foo",
      campaignId: "1234",
      id: "5678",
      guid: "5678",
      shim: {
        impression: "1011",
      },
      pos: 7,
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
      assert.isNull(insertSpocContent(defaultTopSites, {}, "right"));
      assert.isNull(insertSpocContent({}, data, "right"));
    });

    it("Should return null if an organic SPOC topsite exists", () => {
      const topSitesWithOrganicSpoc = {
        rows: [...defaultTopSiteRows, topSiteSpoc],
      };

      assert.isNull(insertSpocContent(topSitesWithOrganicSpoc, data, "right"));
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
            image_src: "foobar2",
            campaign_id: "1234",
            id: "5678",
            shim: { impression: "1011" },
          },
        ],
      };

      const result = insertSpocContent(
        topSitesWithOrganicSpoc,
        extraSpocData,
        "right"
      );

      const availableSpoc = {
        customScreenshotURL: "foobar2",
        type: "SPOC",
        label: "bar2",
        title: "bar2",
        url: "foo2",
        campaignId: "1234",
        id: "5678",
        guid: "5678",
        shim: {
          impression: "1011",
        },
        pos: 7,
      };
      const expectedResult = {
        rows: [...topSitesWithOrganicSpoc.rows, availableSpoc],
      };

      assert.deepEqual(result, expectedResult);
    });

    it("should add to end of row if the row is not full and alignment is right", () => {
      const result = insertSpocContent(defaultTopSites, data, "right");

      const expectedResult = {
        rows: [...defaultTopSiteRows, resultSpocRight],
      };
      assert.deepEqual(result, expectedResult);
    });

    it("should add to front of row if the row is not full and alignment is left", () => {
      const result = insertSpocContent(defaultTopSites, data, "left");
      assert.deepEqual(result, {
        rows: [resultSpocLeft, ...defaultTopSiteRows],
      });
    });

    it("should add to first available in the front row if alignment is left and there are pins", () => {
      const topSiteRowsWithPins = [
        pinnedSite,
        pinnedSite,
        ...defaultTopSiteRows,
      ];

      const result = insertSpocContent(
        { rows: topSiteRowsWithPins },
        data,
        "left"
      );

      assert.deepEqual(result, {
        rows: [pinnedSite, pinnedSite, resultSpocLeft, ...defaultTopSiteRows],
      });
    });

    it("should add to first available in the next row if alignment is right and there are all pins in the front row", () => {
      const pinnedArray = new Array(8).fill(pinnedSite);
      const result = insertSpocContent({ rows: pinnedArray }, data, "right");

      assert.deepEqual(result, {
        rows: [...pinnedArray, resultSpocRight],
      });
    });

    it("should add to first available in the current row if alignment is right and there are some pins in the front row", () => {
      const pinnedArray = new Array(6).fill(pinnedSite);
      const topSite = { label: "foo" };

      const rowsWithPins = [topSite, topSite, ...pinnedArray];

      const result = insertSpocContent({ rows: rowsWithPins }, data, "right");

      assert.deepEqual(result, {
        rows: [topSite, resultSpocRight, ...pinnedArray, topSite],
      });
    });

    it("should preserve the indices of pinned items", () => {
      const topSite = { label: "foo" };
      const rowsWithPins = [pinnedSite, topSite, topSite, pinnedSite];

      const result = insertSpocContent({ rows: rowsWithPins }, data, "left");

      // Pinned items should retain in Index 0 and Index 3 like defined in rowsWithPins
      assert.deepEqual(result, {
        rows: [pinnedSite, resultSpocLeft, topSite, pinnedSite, topSite],
      });
    });
  });
});

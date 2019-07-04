import {combineReducers, createStore} from "redux";
import {INITIAL_STATE, reducers, TOP_SITES_DEFAULT_ROWS} from "common/Reducers.jsm";
import {mount} from "enzyme";
import {TopSites as OldTopSites} from "content-src/components/TopSites/TopSites";
import {Provider} from "react-redux";
import React from "react";
import {TopSites} from "content-src/components/DiscoveryStreamComponents/TopSites/TopSites";

describe("Discovery Stream <TopSites>", () => {
  let wrapper;
  let store;

  beforeEach(() => {
    INITIAL_STATE.Prefs.values.topSitesRows = TOP_SITES_DEFAULT_ROWS;
    store = createStore(combineReducers(reducers), INITIAL_STATE);
    wrapper = mount(<Provider store={store}><TopSites /></Provider>);
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
        header: {title: "test"},
      };
      wrapper = mount(<Provider store={store}><TopSites {...DEFAULT_PROPS} /></Provider>);
      const oldTopSites = wrapper.find(OldTopSites);
      assert.equal(oldTopSites.props().title, "test");
    });
  });
});

import {TopSites as OldTopSites} from "content-src/components/TopSites/TopSites";
import React from "react";
import {shallow} from "enzyme";
import {_TopSites as TopSites} from "content-src/components/DiscoveryStreamComponents/TopSites/TopSites";

describe("Discovery Stream <TopSites>", () => {
  it("should return a wrapper around old TopSites", () => {
    const wrapper = shallow(<TopSites />);

    const oldTopSites = wrapper.find(OldTopSites);
    const dsTopSitesWrapper = wrapper.find(".ds-top-sites");

    assert.ok(wrapper.exists());
    assert.lengthOf(oldTopSites, 1);
    assert.lengthOf(dsTopSitesWrapper, 1);
  });
});

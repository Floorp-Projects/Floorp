import {MoreRecommendations} from "content-src/components/MoreRecommendations/MoreRecommendations";
import React from "react";
import {shallowWithIntl} from "test/unit/utils";

describe("<MoreRecommendations>", () => {
  it("should render a MoreRecommendations element", () => {
    const wrapper = shallowWithIntl(<MoreRecommendations />);
    assert.ok(wrapper.exists());
  });
  it("should render a link when provided with read_more_endpoint prop", () => {
    const wrapper = shallowWithIntl(<MoreRecommendations read_more_endpoint="https://endpoint.com" />);

    const link = wrapper.find(".more-recommendations");
    assert.lengthOf(link, 1);
  });
  it("should not render a link when provided with read_more_endpoint prop", () => {
    const wrapper = shallowWithIntl(<MoreRecommendations read_more_endpoint="" />);

    const link = wrapper.find(".more-recommendations");
    assert.lengthOf(link, 0);
  });
});

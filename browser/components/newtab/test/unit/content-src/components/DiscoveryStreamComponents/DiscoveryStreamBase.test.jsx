import {_DiscoveryStreamBase as DiscoveryStreamBase} from "content-src/components/DiscoveryStreamBase/DiscoveryStreamBase";
import React from "react";
import {shallow} from "enzyme";

describe("<DiscoveryStreamBase>", () => {
  it("should render once we have feeds and spocs", () => {
    const discoveryStreamProps = {
      spocs: {loaded: true},
      feeds: {loaded: true},
      layout: [],
    };

    const wrapper = shallow(<DiscoveryStreamBase DiscoveryStream={discoveryStreamProps} />);

    assert.equal(wrapper.find(".ds-layout").length, 1);
  });
  it("should not render anything without spocs", () => {
    const discoveryStreamProps = {
      spocs: {loaded: false},
      feeds: {loaded: true},
      layout: [],
    };

    const wrapper = shallow(<DiscoveryStreamBase DiscoveryStream={discoveryStreamProps} />);

    assert.equal(wrapper.find(".ds-layout").length, 0);
  });
  it("should not render anything without feeds", () => {
    const discoveryStreamProps = {
      spocs: {loaded: true},
      feeds: {loaded: false},
      layout: [],
    };

    const wrapper = shallow(<DiscoveryStreamBase DiscoveryStream={discoveryStreamProps} />);

    assert.equal(wrapper.find(".ds-layout").length, 0);
  });
});

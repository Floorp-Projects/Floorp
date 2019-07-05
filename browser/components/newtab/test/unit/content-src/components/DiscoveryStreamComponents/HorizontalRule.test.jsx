import { HorizontalRule } from "content-src/components/DiscoveryStreamComponents/HorizontalRule/HorizontalRule";
import React from "react";
import { shallow } from "enzyme";

describe("<HorizontalRule>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallow(<HorizontalRule />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-hr").exists());
  });
});

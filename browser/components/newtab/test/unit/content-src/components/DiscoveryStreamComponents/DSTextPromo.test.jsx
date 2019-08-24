import { DSTextPromo } from "content-src/components/DiscoveryStreamComponents/DSTextPromo/DSTextPromo";
import React from "react";
import { shallow } from "enzyme";

describe("<DSTextPromo>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallow(<DSTextPromo />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-text-promo").exists());
  });

  it("should render a header", () => {
    wrapper.setProps({ header: "foo" });
    assert.ok(wrapper.find(".text").exists());
  });

  it("should render a subtitle", () => {
    wrapper.setProps({ subtitle: "foo" });
    assert.ok(wrapper.find(".subtitle").exists());
  });
});

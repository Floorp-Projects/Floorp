import React from "react";
import { SectionTitle } from "content-src/components/DiscoveryStreamComponents/SectionTitle/SectionTitle";
import { shallow } from "enzyme";

describe("<SectionTitle>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallow(<SectionTitle header={{}} />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-section-title").exists());
  });

  it("should render a subtitle", () => {
    wrapper.setProps({ header: { title: "Foo", subtitle: "Bar" } });

    assert.equal(wrapper.find(".subtitle").text(), "Bar");
  });
});

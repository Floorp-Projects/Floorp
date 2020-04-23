import { CardGrid } from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import { DSCard } from "content-src/components/DiscoveryStreamComponents/DSCard/DSCard";
import React from "react";
import { shallow } from "enzyme";

describe("<CardGrid>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallow(<CardGrid />);
  });

  it("should render an empty div", () => {
    assert.ok(wrapper.exists());
    assert.lengthOf(wrapper.children(), 0);
  });

  it("should render DSCards", () => {
    wrapper.setProps({ items: 2, data: { recommendations: [{}, {}] } });

    assert.lengthOf(wrapper.find(".ds-card-grid").children(), 2);
    assert.equal(
      wrapper
        .find(".ds-card-grid")
        .children()
        .at(0)
        .type(),
      DSCard
    );
  });

  it("should add hero classname to card grid", () => {
    wrapper.setProps({
      display_variant: "hero",
      data: { recommendations: [{}, {}] },
    });

    assert.ok(wrapper.find(".ds-card-grid-hero").exists());
  });
});

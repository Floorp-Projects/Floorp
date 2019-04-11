import {CardGrid} from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import {DSCard} from "content-src/components/DiscoveryStreamComponents/DSCard/DSCard";
import React from "react";
import {shallowWithIntl} from "test/unit/utils";

describe("<CardGrid>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallowWithIntl(<CardGrid />);
  });

  it("should render an empty div", () => {
    assert.ok(wrapper.exists());
    assert.lengthOf(wrapper.children(), 0);
  });

  it("should render DSCards", () => {
    wrapper.setProps({items: 2, data: {recommendations: [{}, {}]}});

    assert.lengthOf(wrapper.find(".ds-card-grid").children(), 2);
    assert.equal(wrapper.find(".ds-card-grid").children().at(0)
      .type(), DSCard);
  });

  it("should add divisible-by-4 to the grid", () => {
    wrapper.setProps({items: 4, data: {recommendations: [{}, {}]}});

    assert.ok(wrapper.find(".ds-card-grid-divisible-by-4").exists());
  });

  it("should add divisible-by-3 to the grid", () => {
    wrapper.setProps({items: 3, data: {recommendations: [{}, {}]}});

    assert.ok(wrapper.find(".ds-card-grid-divisible-by-3").exists());
  });
});

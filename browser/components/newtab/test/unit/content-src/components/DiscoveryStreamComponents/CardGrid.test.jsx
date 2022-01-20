import { CardGrid } from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import {
  DSCard,
  LastCardMessage,
} from "content-src/components/DiscoveryStreamComponents/DSCard/DSCard";
import { actionCreators as ac } from "common/Actions.jsm";
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

  it("should add compact classname to card grid", () => {
    wrapper.setProps({
      compact: true,
      data: { recommendations: [{}, {}] },
    });

    assert.ok(wrapper.find(".ds-card-grid-compact-variant").exists());
  });

  it("should add description classname to card grid", () => {
    wrapper.setProps({
      include_descriptions: true,
      data: { recommendations: [{}, {}] },
    });

    assert.ok(wrapper.find(".ds-card-grid-include-descriptions").exists());
  });

  it("should show last card and more loaded state", () => {
    const dispatch = sinon.stub();
    wrapper.setProps({
      dispatch,
      compact: true,
      loadMoreEnabled: true,
      lastCardMessageEnabled: true,
      loadMoreThreshold: 2,
      data: {
        recommendations: [{}, {}, {}],
      },
    });

    const loadMoreButton = wrapper.find(".ds-card-grid-load-more-button");
    assert.ok(loadMoreButton.exists());

    loadMoreButton.simulate("click", { preventDefault: () => {} });
    assert.calledOnce(dispatch);
    assert.calledWith(
      dispatch,
      ac.UserEvent({
        event: "CLICK",
        source: "DS_LOAD_MORE_BUTTON",
      })
    );

    const lastCard = wrapper.find(LastCardMessage);
    assert.ok(lastCard.exists());
  });

  it("should only show load more with more than threshold number of stories", () => {
    wrapper.setProps({
      loadMoreEnabled: true,
      loadMoreThreshold: 2,
      data: {
        recommendations: [{}, {}, {}],
      },
    });

    let loadMoreButton = wrapper.find(".ds-card-grid-load-more-button");
    assert.ok(loadMoreButton.exists());

    wrapper.setProps({
      loadMoreThreshold: 3,
    });

    loadMoreButton = wrapper.find(".ds-card-grid-load-more-button");
    assert.ok(!loadMoreButton.exists());
  });
});

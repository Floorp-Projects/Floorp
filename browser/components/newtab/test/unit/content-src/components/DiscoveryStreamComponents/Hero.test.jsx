import {DSCard} from "content-src/components/DiscoveryStreamComponents/DSCard/DSCard";
import {Hero} from "content-src/components/DiscoveryStreamComponents/Hero/Hero";
import {List} from "content-src/components/DiscoveryStreamComponents/List/List";
import React from "react";
import {shallow} from "enzyme";

describe("<Hero>", () => {
  let DEFAULT_PROPS;
  beforeEach(() => {
    DEFAULT_PROPS = {
      data: {
        recommendations: [
          {url: 1},
          {url: 2},
          {url: 3},
        ],
      },
    };
  });

  it("should render with nothing", () => {
    const wrapper = shallow(<Hero />);

    assert.lengthOf(wrapper.find("a"), 0);
  });

  it("should render a hero link with expected url", () => {
    const wrapper = shallow(<Hero {...DEFAULT_PROPS} />);

    assert.equal(wrapper.find("SafeAnchor").prop("url"), DEFAULT_PROPS.data.recommendations[0].url);
  });

  describe("subComponent: cards", () => {
    beforeEach(() => {
      DEFAULT_PROPS.subComponentType = "cards";
    });

    it("should render no cards for 1 hero item", () => {
      const wrapper = shallow(<Hero {...DEFAULT_PROPS} items={1} />);

      assert.lengthOf(wrapper.find(DSCard), 0);
    });

    it("should render 1 card with expected url for 2 hero items", () => {
      const wrapper = shallow(<Hero {...DEFAULT_PROPS} items={2} />);

      assert.equal(wrapper.find(DSCard).prop("url"), DEFAULT_PROPS.data.recommendations[1].url);
    });
  });

  describe("subComponent: list", () => {
    beforeEach(() => {
      DEFAULT_PROPS.subComponentType = "list";
    });

    it("should render list with no items for 1 hero item", () => {
      const wrapper = shallow(<Hero {...DEFAULT_PROPS} items={1} />);

      assert.equal(wrapper.find(List).prop("items"), 0);
    });

    it("should render list with 1 item for 2 hero items", () => {
      const wrapper = shallow(<Hero {...DEFAULT_PROPS} items={2} />);

      assert.equal(wrapper.find(List).prop("items"), 1);
    });
  });
});

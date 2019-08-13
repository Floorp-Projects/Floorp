import {
  DSCard,
  PlaceholderDSCard,
} from "content-src/components/DiscoveryStreamComponents/DSCard/DSCard";
import {
  DSContextFooter,
  StatusMessage,
} from "content-src/components/DiscoveryStreamComponents/DSContextFooter/DSContextFooter";
import { actionCreators as ac } from "common/Actions.jsm";
import { DSEmptyState } from "content-src/components/DiscoveryStreamComponents/DSEmptyState/DSEmptyState";
import { Hero } from "content-src/components/DiscoveryStreamComponents/Hero/Hero";
import { List } from "content-src/components/DiscoveryStreamComponents/List/List";
import React from "react";
import { shallow } from "enzyme";

describe("<Hero>", () => {
  let DEFAULT_PROPS;
  beforeEach(() => {
    DEFAULT_PROPS = {
      data: {
        recommendations: [{ url: 1 }, { url: 2 }, { url: 3 }],
      },
    };
  });

  it("should render with nothing", () => {
    const wrapper = shallow(<Hero />);

    assert.lengthOf(wrapper.find("a"), 0);
  });

  it("should return Empty State for no recommendations", () => {
    const heroProps = {
      data: { recommendations: [] },
      header: { title: "headerTitle" },
    };

    const wrapper = shallow(<Hero {...heroProps} />);
    const dsEmptyState = wrapper.find(DSEmptyState);
    const dsHeader = wrapper.find(".ds-header");
    const dsHero = wrapper.find(".ds-hero.empty");

    assert.ok(wrapper.exists());
    assert.lengthOf(dsEmptyState, 1);
    assert.lengthOf(dsHeader, 1);
    assert.lengthOf(dsHero, 1);
  });

  it("should render a hero link with expected url", () => {
    const wrapper = shallow(<Hero {...DEFAULT_PROPS} />);

    assert.equal(
      wrapper.find("SafeAnchor").prop("url"),
      DEFAULT_PROPS.data.recommendations[0].url
    );
  });

  it("should render badges for pocket, bookmark when not a spoc element ", () => {
    const heroProps = {
      data: { recommendations: [{ context_type: "bookmark" }] },
      header: { title: "headerTitle" },
    };

    const wrapper = shallow(<Hero {...heroProps} />);
    const contextFooter = wrapper.find(DSContextFooter).shallow();
    assert.lengthOf(contextFooter.find(StatusMessage), 1);
  });

  it("should render Sponsored Context for a spoc element", () => {
    const heroProps = {
      data: {
        recommendations: [
          { context_type: "bookmark", context: "Sponsored by Foo" },
        ],
      },
      header: { title: "headerTitle" },
    };
    const wrapper = shallow(<Hero {...heroProps} />);
    const contextFooter = wrapper.find(DSContextFooter).shallow();

    assert.lengthOf(contextFooter.find(StatusMessage), 0);
    assert.equal(
      contextFooter.find(".story-sponsored-label").text(),
      heroProps.data.recommendations[0].context
    );
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

      assert.equal(
        wrapper.find(DSCard).prop("url"),
        DEFAULT_PROPS.data.recommendations[1].url
      );
    });

    it("should return PlaceholderDSCard for recommendations less than items", () => {
      const wrapper = shallow(<Hero {...DEFAULT_PROPS} items={4} />);

      const dsCard = wrapper.find(DSCard);
      assert.lengthOf(dsCard, 2);

      const placeholderDSCard = wrapper.find(PlaceholderDSCard);
      assert.lengthOf(placeholderDSCard, 1);
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

  describe("onLinkClick", () => {
    let dispatch;
    let sandbox;
    let wrapper;
    const heroProps = {
      data: { recommendations: [{ url: 1, id: "foo-id", pos: 1 }] },
      type: "foo",
      items: 1,
    };

    beforeEach(() => {
      sandbox = sinon.createSandbox();
      dispatch = sandbox.stub();
      wrapper = shallow(<Hero dispatch={dispatch} {...heroProps} />);
    });

    afterEach(() => {
      sandbox.restore();
    });

    it("should call dispatch with the correct events", () => {
      wrapper.instance().onLinkClick();

      assert.calledTwice(dispatch);
      assert.calledWith(
        dispatch,
        ac.UserEvent({
          event: "CLICK",
          source: "FOO",
          action_position: 1,
        })
      );
      assert.calledWith(
        dispatch,
        ac.ImpressionStats({
          click: 0,
          source: "FOO",
          tiles: [{ id: "foo-id", pos: 1 }],
        })
      );
    });
  });
});

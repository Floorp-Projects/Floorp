import {_List as List, ListItem} from "content-src/components/DiscoveryStreamComponents/List/List";
import {GlobalOverrider} from "test/unit/utils";
import React from "react";
import {shallow} from "enzyme";

describe("<List> presentation component", () => {
  const ValidRecommendations = [{a: 1}, {a: 2}];
  const ValidListProps = {
    DiscoveryStream: {
      feeds: {
        fakeFeedUrl: {
          data: {
            recommendations: ValidRecommendations,
          },
        },
      },
    },
    feed: {
      url: "fakeFeedUrl",
    },
    header: {
      title: "fakeFeedTitle",
    },
  };

  it("should return null if feed.data is falsy", () => {
    const ListProps = {
      DiscoveryStream: {feeds: {a: "stuff"}},
      feed: {url: "a"},
    };

    const wrapper = shallow(<List {...ListProps} />);

    assert.isNull(wrapper.getElement());
  });

  it("should return something containing a <ul> if props are valid", () => {
    const wrapper = shallow(<List {...ValidListProps} />);

    const list = wrapper.find("ul");
    assert.ok(wrapper.exists());
    assert.lengthOf(list, 1);
  });

  it("should return the right number of ListItems if props are valid", () => {
    const wrapper = shallow(<List {...ValidListProps} />);

    const listItem = wrapper.find(ListItem);
    assert.lengthOf(listItem, ValidRecommendations.length);
  });
});

describe("<ListItem> presentation component", () => {
  const ValidListItemProps = {
    url: "FAKE_URL",
    title: "FAKE_TITLE",
    domain: "example.com",
    image_src: "FAKE_IMAGE_SRC",
  };

  let globals;

  beforeEach(() => {
    globals = new GlobalOverrider();
  });

  afterEach(() => {
    globals.sandbox.restore();
  });

  it("should contain 'a.ds-list-item-link' with the props.url set", () => {
    const wrapper = shallow(<ListItem {...ValidListItemProps} />);

    const anchors = wrapper.find(
      `a.ds-list-item-link[href="${ValidListItemProps.url}"]`);
    assert.lengthOf(anchors, 1);
  });

  it("should include an background image of props.image_src", () => {
    const wrapper = shallow(<ListItem {...ValidListItemProps} />);

    const imageStyle = wrapper.find(".ds-list-image").prop("style");
    assert.propertyVal(imageStyle, "backgroundImage", `url(${ValidListItemProps.image_src})`);
  });
});

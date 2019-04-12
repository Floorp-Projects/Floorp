import {_List as List, ListItem, PlaceholderListItem} from "content-src/components/DiscoveryStreamComponents/List/List";
import {DSEmptyState} from "content-src/components/DiscoveryStreamComponents/DSEmptyState/DSEmptyState";
import {DSLinkMenu} from "content-src/components/DiscoveryStreamComponents/DSLinkMenu/DSLinkMenu";
import {GlobalOverrider} from "test/unit/utils";
import React from "react";
import {shallow} from "enzyme";

describe("<List> presentation component", () => {
  const ValidRecommendations = [{url: 1}, {url: 2}, {context: "test spoc", url: 3}];
  const ValidListProps = {
    data: {
      recommendations: ValidRecommendations,
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
      data: {feeds: {a: "stuff"}},
    };

    const wrapper = shallow(<List {...ListProps} />);
    assert.isNull(wrapper.getElement());
  });

  it("should return Empty State for no recommendations", () => {
    const ListProps = {
      data: {recommendations: []},
      header: {title: "headerTitle"},
    };

    const wrapper = shallow(<List {...ListProps} />);
    const dsEmptyState = wrapper.find(DSEmptyState);
    const dsHeader = wrapper.find(".ds-header");
    const dsList = wrapper.find(".ds-list.empty");

    assert.ok(wrapper.exists());
    assert.lengthOf(dsEmptyState, 1);
    assert.lengthOf(dsHeader, 1);
    assert.lengthOf(dsList, 1);
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

  it("should return fewer ListItems for fewer items", () => {
    const wrapper = shallow(<List {...ValidListProps} items={1} />);

    const listItem = wrapper.find(ListItem);
    assert.lengthOf(listItem, 1);
  });

  it("should return PlaceHolderListItem for recommendations less than items", () => {
    const wrapper = shallow(<List {...ValidListProps} items={4} />);

    const listItem = wrapper.find(ListItem);
    assert.lengthOf(listItem, 3);

    const placeholderListItem = wrapper.find(PlaceholderListItem);
    assert.lengthOf(placeholderListItem, 1);
  });

  it("should return fewer ListItems for starting point", () => {
    const wrapper = shallow(<List {...ValidListProps} recStartingPoint={1} />);

    const listItem = wrapper.find(ListItem);
    assert.lengthOf(listItem, ValidRecommendations.length - 1);
  });

  it("should return expected ListItems when offset", () => {
    const wrapper = shallow(<List {...ValidListProps} items={2} recStartingPoint={1} />);

    const listItemUrls = wrapper.find(ListItem).map(i => i.prop("url"));
    assert.sameOrderedMembers(listItemUrls, [ValidRecommendations[1].url, ValidRecommendations[2].url]);
  });

  it("should return expected spoc ListItem", () => {
    const wrapper = shallow(<List {...ValidListProps} items={3} recStartingPoint={0} />);

    const listItemContext = wrapper.find(ListItem).map(i => i.prop("context"));
    assert.sameOrderedMembers(listItemContext, [undefined, undefined, ValidRecommendations[2].context]);
  });
});

describe("<ListItem> presentation component", () => {
  const ValidListItemProps = {
    url: "FAKE_URL",
    title: "FAKE_TITLE",
    domain: "example.com",
    image_src: "FAKE_IMAGE_SRC",
  };
  const ValidLSpocListItemProps = {
    url: "FAKE_URL",
    title: "FAKE_TITLE",
    domain: "example.com",
    image_src: "FAKE_IMAGE_SRC",
    context: "FAKE_CONTEXT",
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
      `SafeAnchor.ds-list-item-link[url="${ValidListItemProps.url}"]`);
    assert.lengthOf(anchors, 1);
  });

  it("should not contain 'span.ds-list-item-context' without props.context", () => {
    const wrapper = shallow(<ListItem {...ValidListItemProps} />);

    const contextEl = wrapper.find("span.ds-list-item-context");
    assert.lengthOf(contextEl, 0);
  });

  it("should contain 'span.ds-list-item-context' spoc element", () => {
    const wrapper = shallow(<ListItem {...ValidLSpocListItemProps} />);

    const contextEl = wrapper.find("span.ds-list-item-context");
    assert.lengthOf(contextEl, 1);
  });
});

describe("<PlaceholderListItem> component", () => {
  it("should have placeholder prop", () => {
    const wrapper = shallow(<PlaceholderListItem />);
    const listItem = wrapper.find(ListItem);
    assert.lengthOf(listItem, 1);

    const placeholder = wrapper.find(ListItem).prop("placeholder");
    assert.isTrue(placeholder);
  });

  it("should contain placeholder listitem", () => {
    const wrapper = shallow(<ListItem placeholder={true} />);
    const listItem = wrapper.find("li.ds-list-item.placeholder");
    assert.lengthOf(listItem, 1);
  });

  it("should not be clickable", () => {
    const wrapper = shallow(<ListItem placeholder={true} />);
    const anchor = wrapper.find("SafeAnchor.ds-list-item-link");
    assert.lengthOf(anchor, 1);

    const linkClick = anchor.prop("onLinkClick");
    assert.isUndefined(linkClick);
  });

  it("should not have context menu", () => {
    const wrapper = shallow(<ListItem placeholder={true} />);
    const linkMenu = wrapper.find(DSLinkMenu);
    assert.lengthOf(linkMenu, 0);
  });
});

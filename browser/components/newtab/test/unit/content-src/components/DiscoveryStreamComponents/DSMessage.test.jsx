import {DSMessage} from "content-src/components/DiscoveryStreamComponents/DSMessage/DSMessage";
import React from "react";
import {SafeAnchor} from "content-src/components/DiscoveryStreamComponents/SafeAnchor/SafeAnchor";
import {shallowWithIntl} from "test/unit/utils";

describe("<DSMessage>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallowWithIntl(<DSMessage />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-message").exists());
  });

  it("should render an icon", () => {
    wrapper.setProps({icon: "foo"});

    assert.ok(wrapper.find(".glyph").exists());
    assert.propertyVal(wrapper.find(".glyph").props().style, "backgroundImage", `url(foo)`);
  });

  it("should render a title", () => {
    wrapper.setProps({title: "foo"});

    assert.ok(wrapper.find(".title-text").exists());
    assert.equal(wrapper.find(".title-text").text(), "foo");
  });

  it("should render a SafeAnchor", () => {
    wrapper.setProps({link_text: "foo", link_url: "https://foo.com"});

    assert.equal(wrapper.find(".title").children().at(0)
      .type(), SafeAnchor);
  });
});

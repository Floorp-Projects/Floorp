import {Navigation, Topic} from "content-src/components/DiscoveryStreamComponents/Navigation/Navigation";
import React from "react";
import {SafeAnchor} from "content-src/components/DiscoveryStreamComponents/SafeAnchor/SafeAnchor";
import {shallowWithIntl} from "test/unit/utils";

describe("<Navigation>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallowWithIntl(<Navigation header={{}} />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
  });

  it("should render a title", () => {
    wrapper.setProps({header: {title: "Foo"}});

    assert.equal(wrapper.find(".ds-header").text(), "Foo");
  });

  it("should render 2 Topics", () => {
    wrapper.setProps({
      links: [
        {url: "https://foo.com", name: "foo"},
        {url: "https://bar.com", name: "bar"},
      ],
    });

    assert.lengthOf(wrapper.find("ul").children(), 2);
  });
});

describe("<Topic>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallowWithIntl(<Topic url="https://foo.com" name="foo" />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.equal(wrapper.type(), "li");
    assert.equal(wrapper.children().at(0).type(), SafeAnchor);
  });
});

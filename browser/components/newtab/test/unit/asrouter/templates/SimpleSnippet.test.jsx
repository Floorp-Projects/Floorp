import {mount} from "enzyme";
import React from "react";
import schema from "content-src/asrouter/templates/SimpleSnippet/SimpleSnippet.schema.json";
import {SimpleSnippet} from "content-src/asrouter/templates/SimpleSnippet/SimpleSnippet.jsx";

const DEFAULT_CONTENT = {text: "foo"};

/**
 * mountAndCheckProps - Mounts a SimpleSnippet with DEFAULT_CONTENT extended with any props
 *                      passed in the content param and validates props against the schema.
 * @param {obj} content Object containing custom message content (e.g. {text, icon, title})
 * @returns enzyme wrapper for SimpleSnippet
 */
function mountAndCheckProps(content = {}) {
  const props = {content: Object.assign({}, DEFAULT_CONTENT, content)};
  assert.jsonSchema(props.content, schema);
  return mount(<SimpleSnippet {...props} />);
}

describe("SimpleSnippet", () => {
  it("should render .text", () => {
    const wrapper = mountAndCheckProps({text: "bar"});
    assert.equal(wrapper.find(".body").text(), "bar");
  });
  it("should not render title element if no .title prop is supplied", () => {
    const wrapper = mountAndCheckProps();
    assert.lengthOf(wrapper.find(".title"), 0);
  });
  it("should render .title", () => {
    const wrapper = mountAndCheckProps({title: "Foo"});
    assert.equal(wrapper.find(".title").text(), "Foo");
  });
  it("should render .icon", () => {
    const wrapper = mountAndCheckProps({icon: "data:image/gif;base64,R0lGODl"});
    assert.equal(wrapper.find(".icon").prop("src"), "data:image/gif;base64,R0lGODl");
  });
  it("should render .button_url and .button_label", () => {
    const wrapper = mountAndCheckProps({button_label: "Click here", button_url: "http://foo.com"});
    const button = wrapper.find("a");
    assert.equal(button.text(), "Click here");
    assert.equal(button.prop("href"), "http://foo.com");
  });
  it("should render .button_url and .button_label", () => {
    const wrapper = mountAndCheckProps({button_label: "Click here", button_url: "http://foo.com"});
    const button = wrapper.find("a");
    assert.equal(button.text(), "Click here");
    assert.equal(button.prop("href"), "http://foo.com");
  });
  it("should render button with an ASRouterAnchor class if .button_type is anchor", () => {
    const wrapper = mountAndCheckProps({button_label: "Click here", button_url: "http://foo.com", button_type: "anchor"});
    const button = wrapper.find("a");
    assert.equal(button.prop("className"), "ASRouterAnchor");
  });
});

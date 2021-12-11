import { mount } from "enzyme";
import React from "react";
import schema from "content-src/asrouter/templates/SimpleSnippet/SimpleSnippet.schema.json";
import { SimpleSnippet } from "content-src/asrouter/templates/SimpleSnippet/SimpleSnippet.jsx";

const DEFAULT_CONTENT = { text: "foo" };

describe("SimpleSnippet", () => {
  let sandbox;
  let onBlockStub;
  let sendUserActionTelemetryStub;

  /**
   * mountAndCheckProps - Mounts a SimpleSnippet with DEFAULT_CONTENT extended with any props
   *                      passed in the content param and validates props against the schema.
   * @param {obj} content Object containing custom message content (e.g. {text, icon, title})
   * @returns enzyme wrapper for SimpleSnippet
   */
  function mountAndCheckProps(content = {}, provider = "test-provider") {
    const props = {
      content: Object.assign({}, DEFAULT_CONTENT, content),
      provider,
      onBlock: onBlockStub,
      sendUserActionTelemetry: sendUserActionTelemetryStub,
      onAction: sandbox.stub(),
    };
    assert.jsonSchema(props.content, schema);
    return mount(<SimpleSnippet {...props} />);
  }

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    onBlockStub = sandbox.stub();
    sendUserActionTelemetryStub = sandbox.stub();
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should have the correct defaults", () => {
    const wrapper = mountAndCheckProps();
    [["button", "title", "block_button_text"]].forEach(prop => {
      const props = wrapper.find(prop[0]).props();
      assert.propertyVal(props, prop[1], schema.properties[prop[2]].default);
    });
  });

  it("should render .text", () => {
    const wrapper = mountAndCheckProps({ text: "bar" });
    assert.equal(wrapper.find(".body").text(), "bar");
  });
  it("should not render title element if no .title prop is supplied", () => {
    const wrapper = mountAndCheckProps();
    assert.lengthOf(wrapper.find(".title"), 0);
  });
  it("should render .title", () => {
    const wrapper = mountAndCheckProps({ title: "Foo" });
    assert.equal(
      wrapper
        .find(".title")
        .text()
        .trim(),
      "Foo"
    );
  });
  it("should render a light theme variant .icon", () => {
    const wrapper = mountAndCheckProps({
      icon: "data:image/gif;base64,R0lGODl",
    });
    assert.equal(
      wrapper.find(".icon-light-theme").prop("src"),
      "data:image/gif;base64,R0lGODl"
    );
  });
  it("should render a dark theme variant .icon", () => {
    const wrapper = mountAndCheckProps({
      icon_dark_theme: "data:image/gif;base64,R0lGODl",
    });
    assert.equal(
      wrapper.find(".icon-dark-theme").prop("src"),
      "data:image/gif;base64,R0lGODl"
    );
  });
  it("should render a light theme variant .icon as fallback", () => {
    const wrapper = mountAndCheckProps({
      icon_dark_theme: "",
      icon: "data:image/gif;base64,R0lGODp",
    });
    assert.equal(
      wrapper.find(".icon-dark-theme").prop("src"),
      "data:image/gif;base64,R0lGODp"
    );
  });
  it("should render .button_label and default className", () => {
    const wrapper = mountAndCheckProps({
      button_label: "Click here",
      button_action: "OPEN_APPLICATIONS_MENU",
      button_action_args: "appMenu",
    });

    const button = wrapper.find("button.ASRouterButton");
    button.simulate("click");

    assert.equal(button.text(), "Click here");
    assert.equal(button.prop("className"), "ASRouterButton secondary");
    assert.calledOnce(wrapper.props().onAction);
    assert.calledWithExactly(wrapper.props().onAction, {
      type: "OPEN_APPLICATIONS_MENU",
      data: { args: "appMenu" },
    });
  });
  it("should not wrap the main content if a section header is not present", () => {
    const wrapper = mountAndCheckProps({ text: "bar" });
    assert.lengthOf(wrapper.find(".innerContentWrapper"), 0);
  });
  it("should wrap the main content if a section header is present", () => {
    const wrapper = mountAndCheckProps({
      section_title_icon: "data:image/gif;base64,R0lGODl",
      section_title_text: "Messages from Mozilla",
    });

    assert.lengthOf(wrapper.find(".innerContentWrapper"), 1);
  });
  it("should render a section header if text and icon (light-theme) are specified", () => {
    const wrapper = mountAndCheckProps({
      section_title_icon: "data:image/gif;base64,R0lGODl",
      section_title_text: "Messages from Mozilla",
    });

    assert.equal(
      wrapper.find(".section-title .icon-light-theme").prop("style")
        .backgroundImage,
      'url("data:image/gif;base64,R0lGODl")'
    );
    assert.equal(
      wrapper
        .find(".section-title-text")
        .text()
        .trim(),
      "Messages from Mozilla"
    );
    // ensure there is no <a> when a section_title_url is not specified
    assert.lengthOf(wrapper.find(".section-title a"), 0);
  });
  it("should render a section header if text and icon (light-theme) are specified", () => {
    const wrapper = mountAndCheckProps({
      section_title_icon: "data:image/gif;base64,R0lGODl",
      section_title_icon_dark_theme: "data:image/gif;base64,R0lGODl",
      section_title_text: "Messages from Mozilla",
    });

    assert.equal(
      wrapper.find(".section-title .icon-dark-theme").prop("style")
        .backgroundImage,
      'url("data:image/gif;base64,R0lGODl")'
    );
    assert.equal(
      wrapper
        .find(".section-title-text")
        .text()
        .trim(),
      "Messages from Mozilla"
    );
    // ensure there is no <a> when a section_title_url is not specified
    assert.lengthOf(wrapper.find(".section-title a"), 0);
  });
  it("should render a section header wrapped in an <a> tag if a url is provided", () => {
    const wrapper = mountAndCheckProps({
      section_title_icon: "data:image/gif;base64,R0lGODl",
      section_title_text: "Messages from Mozilla",
      section_title_url: "https://www.mozilla.org",
    });

    assert.equal(
      wrapper.find(".section-title a").prop("href"),
      "https://www.mozilla.org"
    );
  });
  it("should send an OPEN_URL action when button_url is defined and button is clicked", () => {
    const wrapper = mountAndCheckProps({
      button_label: "Button",
      button_url: "https://mozilla.org",
    });

    const button = wrapper.find("button.ASRouterButton");
    button.simulate("click");

    assert.calledOnce(wrapper.props().onAction);
    assert.calledWithExactly(wrapper.props().onAction, {
      type: "OPEN_URL",
      data: { args: "https://mozilla.org" },
    });
  });
  it("should send an OPEN_ABOUT_PAGE action with entrypoint when the button is clicked", () => {
    const wrapper = mountAndCheckProps({
      button_label: "Button",
      button_action: "OPEN_ABOUT_PAGE",
      button_entrypoint_value: "snippet",
      button_entrypoint_name: "entryPoint",
      button_action_args: "logins",
    });

    const button = wrapper.find("button.ASRouterButton");
    button.simulate("click");

    assert.calledOnce(wrapper.props().onAction);
    assert.calledWithExactly(wrapper.props().onAction, {
      type: "OPEN_ABOUT_PAGE",
      data: { args: "logins", entrypoint: "entryPoint=snippet" },
    });
  });
  it("should send an OPEN_PREFERENCE_PAGE action with entrypoint when the button is clicked", () => {
    const wrapper = mountAndCheckProps({
      button_label: "Button",
      button_action: "OPEN_PREFERENCE_PAGE",
      button_entrypoint_value: "entry=snippet",
      button_action_args: "home",
    });

    const button = wrapper.find("button.ASRouterButton");
    button.simulate("click");

    assert.calledOnce(wrapper.props().onAction);
    assert.calledWithExactly(wrapper.props().onAction, {
      type: "OPEN_PREFERENCE_PAGE",
      data: { args: "home", entrypoint: "entry=snippet" },
    });
  });
  it("should call props.onBlock and sendUserActionTelemetry when CTA button is clicked", () => {
    const wrapper = mountAndCheckProps({ text: "bar" });

    wrapper.instance().onButtonClick();

    assert.calledOnce(onBlockStub);
    assert.calledOnce(sendUserActionTelemetryStub);
  });

  it("should not call props.onBlock if do_not_autoblock is true", () => {
    const wrapper = mountAndCheckProps({ text: "bar", do_not_autoblock: true });

    wrapper.instance().onButtonClick();

    assert.notCalled(onBlockStub);
  });

  it("should not call sendUserActionTelemetry for preview message when CTA button is clicked", () => {
    const wrapper = mountAndCheckProps({ text: "bar" }, "preview");

    wrapper.instance().onButtonClick();

    assert.calledOnce(onBlockStub);
    assert.notCalled(sendUserActionTelemetryStub);
  });
});

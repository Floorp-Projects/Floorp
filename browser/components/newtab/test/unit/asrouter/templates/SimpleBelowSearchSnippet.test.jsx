import {mount} from "enzyme";
import React from "react";
import schema from "content-src/asrouter/templates/SimpleBelowSearchSnippet/SimpleBelowSearchSnippet.schema.json";
import {SimpleBelowSearchSnippet} from "content-src/asrouter/templates/SimpleBelowSearchSnippet/SimpleBelowSearchSnippet.jsx";

const DEFAULT_CONTENT = {text: "foo"};

describe("SimpleBelowSearchSnippet", () => {
  let sandbox;
  let sendUserActionTelemetryStub;

  /**
   * mountAndCheckProps - Mounts a SimpleBelowSearchSnippet with DEFAULT_CONTENT extended with any props
   *                      passed in the content param and validates props against the schema.
   * @param {obj} content Object containing custom message content (e.g. {text, icon})
   * @returns enzyme wrapper for SimpleSnippet
   */
  function mountAndCheckProps(content = {}, provider = "test-provider") {
    const props = {
      content: {...DEFAULT_CONTENT, ...content},
      provider,
      sendUserActionTelemetry: sendUserActionTelemetryStub,
      onAction: sandbox.stub(),
    };
    assert.jsonSchema(props.content, schema);
    return mount(<SimpleBelowSearchSnippet {...props} />);
  }

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    sendUserActionTelemetryStub = sandbox.stub();
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render .text", () => {
    const wrapper = mountAndCheckProps({text: "bar"});
    assert.equal(wrapper.find(".body").text(), "bar");
  });

  it("should render .icon (light theme)", () => {
    const wrapper = mountAndCheckProps({icon: "data:image/gif;base64,R0lGODl"});
    assert.equal(wrapper.find(".icon-light-theme").prop("src"), "data:image/gif;base64,R0lGODl");
  });

  it("should render .icon (dark theme)", () => {
    const wrapper = mountAndCheckProps({icon_dark_theme: "data:image/gif;base64,R0lGODl"});
    assert.equal(wrapper.find(".icon-dark-theme").prop("src"), "data:image/gif;base64,R0lGODl");
  });
});

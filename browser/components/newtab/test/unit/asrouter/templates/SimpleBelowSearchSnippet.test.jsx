import { mount } from "enzyme";
import React from "react";
import { FluentBundle, FluentResource } from "@fluent/bundle";
import { LocalizationProvider, ReactLocalization } from "@fluent/react";
import schema from "content-src/asrouter/templates/SimpleBelowSearchSnippet/SimpleBelowSearchSnippet.schema.json";
import { SimpleBelowSearchSnippet } from "content-src/asrouter/templates/SimpleBelowSearchSnippet/SimpleBelowSearchSnippet.jsx";

const DEFAULT_CONTENT = { text: "foo" };

describe("SimpleBelowSearchSnippet", () => {
  let sandbox;
  let sendUserActionTelemetryStub;

  function mockL10nWrapper(content) {
    const bundle = new FluentBundle("en-US");
    for (const [id, value] of Object.entries(content)) {
      if (typeof value === "string") {
        bundle.addResource(new FluentResource(`${id} = ${value}`));
      }
    }
    const l10n = new ReactLocalization([bundle]);
    return {
      wrappingComponent: LocalizationProvider,
      wrappingComponentProps: { l10n },
    };
  }

  /**
   * mountAndCheckProps - Mounts a SimpleBelowSearchSnippet with DEFAULT_CONTENT extended with any props
   *                      passed in the content param and validates props against the schema.
   * @param {obj} content Object containing custom message content (e.g. {text, icon})
   * @returns enzyme wrapper for SimpleSnippet
   */
  function mountAndCheckProps(content = {}, provider = "test-provider") {
    const props = {
      content: { ...DEFAULT_CONTENT, ...content },
      provider,
      sendUserActionTelemetry: sendUserActionTelemetryStub,
      onAction: sandbox.stub(),
    };
    assert.jsonSchema(props.content, schema);
    return mount(
      <SimpleBelowSearchSnippet {...props} />,
      mockL10nWrapper(props.content)
    );
  }

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    sendUserActionTelemetryStub = sandbox.stub();
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render .text", () => {
    const wrapper = mountAndCheckProps({ text: "bar" });
    assert.equal(wrapper.find(".body").text(), "bar");
  });

  it("should render .icon (light theme)", () => {
    const wrapper = mountAndCheckProps({
      icon: "data:image/gif;base64,R0lGODl",
    });
    assert.equal(
      wrapper.find(".icon-light-theme").prop("src"),
      "data:image/gif;base64,R0lGODl"
    );
  });

  it("should render .icon (dark theme)", () => {
    const wrapper = mountAndCheckProps({
      icon_dark_theme: "data:image/gif;base64,R0lGODl",
    });
    assert.equal(
      wrapper.find(".icon-dark-theme").prop("src"),
      "data:image/gif;base64,R0lGODl"
    );
  });
});

import { FXASignupSnippet } from "content-src/asrouter/templates/FXASignupSnippet/FXASignupSnippet";
import { mount } from "enzyme";
import React from "react";
import schema from "content-src/asrouter/templates/FXASignupSnippet/FXASignupSnippet.schema.json";
import { SnippetsTestMessageProvider } from "lib/SnippetsTestMessageProvider.jsm";

const DEFAULT_CONTENT = SnippetsTestMessageProvider.getMessages().find(
  msg => msg.template === "fxa_signup_snippet"
).content;

describe("FXASignupSnippet", () => {
  let sandbox;

  function mountAndCheckProps(content = {}) {
    const props = {
      id: "foo123",
      content: Object.assign(
        { utm_campaign: "foo", utm_term: "bar" },
        DEFAULT_CONTENT,
        content
      ),
      onBlock() {},
      onDismiss: sandbox.stub(),
      sendUserActionTelemetry: sandbox.stub(),
      onAction: sandbox.stub(),
    };
    const comp = mount(<FXASignupSnippet {...props} />);
    // Check schema with the final props the component receives (including defaults)
    assert.jsonSchema(comp.children().get(0).props.content, schema);
    return comp;
  }

  beforeEach(() => {
    sandbox = sinon.createSandbox();
  });
  afterEach(() => {
    sandbox.restore();
  });

  it("should have the correct defaults", () => {
    const defaults = {
      id: "foo123",
      onBlock() {},
      content: {},
      onDismiss: sandbox.stub(),
      sendUserActionTelemetry: sandbox.stub(),
      onAction: sandbox.stub(),
    };
    const wrapper = mount(<FXASignupSnippet {...defaults} />);
    // SendToDeviceSnippet is a wrapper around SubmitFormSnippet
    const { props } = wrapper.children().get(0);

    const defaultProperties = Object.keys(schema.properties).filter(
      prop => schema.properties[prop].default
    );
    assert.lengthOf(defaultProperties, 4);
    defaultProperties.forEach(prop =>
      assert.propertyVal(props.content, prop, schema.properties[prop].default)
    );

    const defaultHiddenProperties = Object.keys(
      schema.properties.hidden_inputs.properties
    ).filter(prop => schema.properties.hidden_inputs.properties[prop].default);
    assert.lengthOf(defaultHiddenProperties, 0);
  });

  it("should have a form_action", () => {
    const wrapper = mountAndCheckProps();

    assert.propertyVal(
      wrapper.children().get(0).props,
      "form_action",
      "https://accounts.firefox.com/"
    );
  });

  it("should navigate to scene2", () => {
    const wrapper = mountAndCheckProps({});

    wrapper.find(".ASRouterButton").simulate("click");

    assert.lengthOf(wrapper.find(".mainInput"), 1);
  });
});

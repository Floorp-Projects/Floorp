import {FXASignupSnippet} from "content-src/asrouter/templates/FXASignupSnippet/FXASignupSnippet";
import {mount} from "enzyme";
import React from "react";
import schema from "content-src/asrouter/templates/SubmitFormSnippet/SubmitFormSnippet.schema.json";
import {SnippetsTestMessageProvider} from "lib/SnippetsTestMessageProvider.jsm";

const DEFAULT_CONTENT = SnippetsTestMessageProvider.getMessages().find(msg => msg.template === "fxa_signup_snippet").content;

describe("FXASignupSnippet", () => {
  let sandbox;

  function mountAndCheckProps(content = {}) {
    const props = {
      id: "foo123",
      content: Object.assign({}, DEFAULT_CONTENT, content),
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
    sandbox = sinon.sandbox.create();
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
    const {props} = wrapper.children().get(0);

    assert.propertyVal(props.content, "form_action", "https://accounts.firefox.com/");
    assert.propertyVal(props.content, "scene1_button_label", "Learn More");
    assert.propertyVal(props.content, "scene2_button_label", "Sign Me Up");
    assert.propertyVal(props.content, "scene2_email_placeholder_text", "Your Email Here");
    assert.propertyVal(props.content.hidden_inputs, "action", "email");
    assert.propertyVal(props.content.hidden_inputs, "context", "fx_desktop_v3");
    assert.propertyVal(props.content.hidden_inputs, "entrypoint", "snippets");
    assert.propertyVal(props.content.hidden_inputs, "service", "sync");
    assert.propertyVal(props.content.hidden_inputs, "utm_source", "snippet");
  });

  it("should navigate to scene2", () => {
    const wrapper = mountAndCheckProps({});

    wrapper.find(".ASRouterButton").simulate("click");

    assert.lengthOf(wrapper.find(".mainInput"), 1);
  });
});

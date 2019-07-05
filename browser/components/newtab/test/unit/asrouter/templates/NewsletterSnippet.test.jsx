import { mount } from "enzyme";
import { NewsletterSnippet } from "content-src/asrouter/templates/NewsletterSnippet/NewsletterSnippet";
import React from "react";
import schema from "content-src/asrouter/templates/NewsletterSnippet/NewsletterSnippet.schema.json";
import { SnippetsTestMessageProvider } from "lib/SnippetsTestMessageProvider.jsm";

const DEFAULT_CONTENT = SnippetsTestMessageProvider.getMessages().find(
  msg => msg.template === "newsletter_snippet"
).content;

describe("NewsletterSnippet", () => {
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
    const comp = mount(<NewsletterSnippet {...props} />);
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

  describe("schema test", () => {
    it("should validate the schema and defaults", () => {
      const wrapper = mountAndCheckProps();
      wrapper.find(".ASRouterButton").simulate("click");
      assert.equal(wrapper.find(".mainInput").instance().type, "email");
    });

    it("should have all of the default fields", () => {
      const defaults = {
        id: "foo123",
        content: {},
        onBlock() {},
        onDismiss: sandbox.stub(),
        sendUserActionTelemetry: sandbox.stub(),
        onAction: sandbox.stub(),
      };
      const wrapper = mount(<NewsletterSnippet {...defaults} />);
      // SendToDeviceSnippet is a wrapper around SubmitFormSnippet
      const { props } = wrapper.children().get(0);

      // the `locale` properties gets used as part of hidden_fields so we
      // check for it separately
      const properties = { ...schema.properties };
      const { locale } = properties;
      delete properties.locale;

      const defaultProperties = Object.keys(properties).filter(
        prop => properties[prop].default
      );
      assert.lengthOf(defaultProperties, 5);
      defaultProperties.forEach(prop =>
        assert.propertyVal(props.content, prop, properties[prop].default)
      );

      const defaultHiddenProperties = Object.keys(
        schema.properties.hidden_inputs.properties
      ).filter(
        prop => schema.properties.hidden_inputs.properties[prop].default
      );
      assert.lengthOf(defaultHiddenProperties, 1);
      defaultHiddenProperties.forEach(prop =>
        assert.propertyVal(
          props.content.hidden_inputs,
          prop,
          schema.properties.hidden_inputs.properties[prop].default
        )
      );
      assert.propertyVal(props.content.hidden_inputs, "lang", locale.default);
    });
  });
});

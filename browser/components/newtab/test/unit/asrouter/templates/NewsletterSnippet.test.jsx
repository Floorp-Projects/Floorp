import { mount } from "enzyme";
import { NewsletterSnippet } from "content-src/asrouter/templates/NewsletterSnippet/NewsletterSnippet";
import React from "react";
import { FluentBundle, FluentResource } from "@fluent/bundle";
import { LocalizationProvider, ReactLocalization } from "@fluent/react";
import schema from "content-src/asrouter/templates/NewsletterSnippet/NewsletterSnippet.schema.json";
import { SnippetsTestMessageProvider } from "lib/SnippetsTestMessageProvider.sys.mjs";

describe("NewsletterSnippet", () => {
  let sandbox;
  let DEFAULT_CONTENT;

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

  function mountAndCheckProps(content = {}) {
    const props = {
      id: "foo123",
      content: Object.assign({}, DEFAULT_CONTENT, content),
      onBlock() {},
      onDismiss: sandbox.stub(),
      sendUserActionTelemetry: sandbox.stub(),
      onAction: sandbox.stub(),
    };
    const comp = mount(
      <NewsletterSnippet {...props} />,
      mockL10nWrapper(props.content)
    );
    // Check schema with the final props the component receives (including defaults)
    assert.jsonSchema(comp.children().get(0).props.content, schema);
    return comp;
  }

  beforeEach(async () => {
    sandbox = sinon.createSandbox();
    DEFAULT_CONTENT = (await SnippetsTestMessageProvider.getMessages()).find(
      msg => msg.template === "newsletter_snippet"
    ).content;
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
      const wrapper = mount(
        <NewsletterSnippet {...defaults} />,
        mockL10nWrapper(DEFAULT_CONTENT)
      );
      // NewsletterSnippet is a wrapper around SubmitFormSnippet
      const { props } = wrapper.children().get(0);

      // the `locale` properties gets used as part of hidden_fields so we
      // check for it separately
      const properties = { ...schema.properties };
      const { locale } = properties;
      delete properties.locale;

      const defaultProperties = Object.keys(properties).filter(
        prop => properties[prop].default
      );
      assert.lengthOf(defaultProperties, 6);
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

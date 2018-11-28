import {mount} from "enzyme";
import {NewsletterSnippet} from "content-src/asrouter/templates/NewsletterSnippet/NewsletterSnippet";
import React from "react";
import schema from "content-src/asrouter/templates/NewsletterSnippet/NewsletterSnippet.schema.json";
import {SnippetsTestMessageProvider} from "lib/SnippetsTestMessageProvider.jsm";

const DEFAULT_CONTENT = SnippetsTestMessageProvider.getMessages().find(msg => msg.template === "newsletter_snippet").content;

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
  });
});

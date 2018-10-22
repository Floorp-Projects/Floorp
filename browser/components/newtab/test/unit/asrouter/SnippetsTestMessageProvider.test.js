import EOYSnippetSchema from "../../../content-src/asrouter/templates/EOYSnippet/EOYSnippet.schema.json";
import SimpleSnippetSchema from "../../../content-src/asrouter/templates/SimpleSnippet/SimpleSnippet.schema.json";
import {SnippetsTestMessageProvider} from "../../../lib/SnippetsTestMessageProvider.jsm";
import SubmitFormSnippetSchema from "../../../content-src/asrouter/templates/SubmitFormSnippet/SubmitFormSnippet.schema.json";

const schemas = {
  "simple_snippet": SimpleSnippetSchema,
  "newsletter_snippet": SubmitFormSnippetSchema,
  "fxa_signup_snippet": SubmitFormSnippetSchema,
  "send_to_device_snippet": SubmitFormSnippetSchema,
  "eoy_snippet": EOYSnippetSchema,
};

describe("SnippetsTestMessageProvider", () => {
  let messages = SnippetsTestMessageProvider.getMessages();

  it("should return an array of messages", () => {
    assert.isArray(messages);
  });

  it("should have a valid example of each schema", () => {
    Object.keys(schemas).forEach(templateName => {
      const example = messages.find(message => message.template === templateName);
      assert.ok(example, `has a ${templateName} example`);
    });
  });

  it("should have examples that are valid", () => {
    messages.forEach(example => {
      assert.jsonSchema(example.content, schemas[example.template], `${example.id} should be valid`);
    });
  });
});

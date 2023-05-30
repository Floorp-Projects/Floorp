import EOYSnippetSchema from "../../../content-src/asrouter/templates/EOYSnippet/EOYSnippet.schema.json";
import SimpleBelowSearchSnippetSchema from "../../../content-src/asrouter/templates/SimpleBelowSearchSnippet/SimpleBelowSearchSnippet.schema.json";
import SimpleSnippetSchema from "../../../content-src/asrouter/templates/SimpleSnippet/SimpleSnippet.schema.json";
import { SnippetsTestMessageProvider } from "../../../lib/SnippetsTestMessageProvider.sys.mjs";
import SubmitFormSnippetSchema from "../../../content-src/asrouter/templates/SubmitFormSnippet/SubmitFormSnippet.schema.json";
import SubmitFormScene2SnippetSchema from "../../../content-src/asrouter/templates/SubmitFormSnippet/SubmitFormScene2Snippet.schema.json";

const schemas = {
  simple_snippet: SimpleSnippetSchema,
  newsletter_snippet: SubmitFormSnippetSchema,
  fxa_signup_snippet: SubmitFormSnippetSchema,
  send_to_device_snippet: SubmitFormSnippetSchema,
  send_to_device_scene2_snippet: SubmitFormScene2SnippetSchema,
  eoy_snippet: EOYSnippetSchema,
  simple_below_search_snippet: SimpleBelowSearchSnippetSchema,
};

describe("SnippetsTestMessageProvider", async () => {
  let messages = await SnippetsTestMessageProvider.getMessages();

  it("should return an array of messages", () => {
    assert.isArray(messages);
  });

  it("should have a valid example of each schema", () => {
    Object.keys(schemas).forEach(templateName => {
      const example = messages.find(
        message => message.template === templateName
      );
      assert.ok(example, `has a ${templateName} example`);
    });
  });

  it("should have examples that are valid", () => {
    messages.forEach(example => {
      assert.jsonSchema(
        example.content,
        schemas[example.template],
        `${example.id} should be valid`
      );
    });
  });
});

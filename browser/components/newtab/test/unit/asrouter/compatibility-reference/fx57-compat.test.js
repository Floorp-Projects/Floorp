import EOYSnippetSchema from "content-src/asrouter/templates/EOYSnippet/EOYSnippet.schema.json";
import {expectedValues} from "./snippets-fx57";
import SimpleSnippetSchema from "content-src/asrouter/templates/SimpleSnippet/SimpleSnippet.schema.json";
import SubmitFormSchema from "content-src/asrouter/templates/SubmitFormSnippet/SubmitFormSnippet.schema.json";

export const SnippetsSchemas = {
  eoy_snippet: EOYSnippetSchema,
  simple_snippet: SimpleSnippetSchema,
  newsletter_snippet: SubmitFormSchema,
  fxa_signup_snippet: SubmitFormSchema,
  send_to_device_snippet: SubmitFormSchema,
};

describe("Firefox 57 compatibility test", () => {
  Object.keys(expectedValues).forEach(template => {
    describe(template, () => {
      const schema = SnippetsSchemas[template];
      it(`should have a schema for ${template}`, () => {
        assert.ok(schema);
      });
      it(`should validate with the schema for ${template}`, () => {
        assert.jsonSchema(expectedValues[template], schema);
      });
    });
  });
});

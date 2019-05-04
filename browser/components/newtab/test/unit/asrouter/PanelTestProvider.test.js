import {PanelTestProvider} from "lib/PanelTestProvider.jsm";
import schema from "content-src/asrouter/schemas/panel/cfr-fxa-bookmark.schema.json";
const messages = PanelTestProvider.getMessages();

describe("PanelTestProvider", () => {
  it("should have a message", () => {
    assert.lengthOf(messages, 1);
  });
  it("should be a valid message", () => {
    assert.jsonSchema(messages[0].content, schema);
  });
});

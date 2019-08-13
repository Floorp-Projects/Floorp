import { PanelTestProvider } from "lib/PanelTestProvider.jsm";
import schema from "content-src/asrouter/schemas/panel/cfr-fxa-bookmark.schema.json";
import update_schema from "content-src/asrouter/templates/OnboardingMessage/UpdateAction.schema.json";
const messages = PanelTestProvider.getMessages();

describe("PanelTestProvider", () => {
  it("should have a message", () => {
    // Careful: when changing this number make sure that new messages also go
    // through schema verifications.
    assert.lengthOf(messages, 5);
  });
  it("should be a valid message", () => {
    const fxaMessages = messages.filter(
      ({ template }) => template === "fxa_bookmark_panel"
    );
    for (let message of fxaMessages) {
      assert.jsonSchema(message.content, schema);
    }
  });
  it("should be a valid message", () => {
    const updateMessages = messages.filter(
      ({ template }) => template === "update_action"
    );
    for (let message of updateMessages) {
      assert.jsonSchema(message.content, update_schema);
    }
  });
});

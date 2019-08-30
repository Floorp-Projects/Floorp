import { PanelTestProvider } from "lib/PanelTestProvider.jsm";
import schema from "content-src/asrouter/schemas/panel/cfr-fxa-bookmark.schema.json";
import update_schema from "content-src/asrouter/templates/OnboardingMessage/UpdateAction.schema.json";
import whats_new_schema from "content-src/asrouter/templates/OnboardingMessage/WhatsNewMessage.schema.json";
const messages = PanelTestProvider.getMessages();

describe("PanelTestProvider", () => {
  it("should have a message", () => {
    // Careful: when changing this number make sure that new messages also go
    // through schema verifications.
    assert.lengthOf(messages, 7);
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
  it("should be a valid message", () => {
    const whatsNewMessages = messages.filter(
      ({ template }) => template === "whatsnew_panel_message"
    );
    for (let message of whatsNewMessages) {
      assert.jsonSchema(message.content, whats_new_schema);
      // Not part of `message.content` so it can't be enforced through schema
      assert.property(message, "order");
    }
  });
});

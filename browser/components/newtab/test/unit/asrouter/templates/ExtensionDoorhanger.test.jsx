import { CFRMessageProvider } from "lib/CFRMessageProvider.jsm";
import CFRDoorhangerSchema from "content-src/asrouter/templates/CFR/templates/ExtensionDoorhanger.schema.json";
import CFRChicletSchema from "content-src/asrouter/templates/CFR/templates/CFRUrlbarChiclet.schema.json";
import InfoBarSchema from "content-src/asrouter/templates/CFR/templates/InfoBar.schema.json";

const SCHEMAS = {
  cfr_urlbar_chiclet: CFRChicletSchema,
  cfr_doorhanger: CFRDoorhangerSchema,
  milestone_message: CFRDoorhangerSchema,
  infobar: InfoBarSchema,
};

const DEFAULT_CONTENT = {
  layout: "addon_recommendation",
  category: "dummyCategory",
  bucket_id: "some_bucket_id",
  notification_text: "Recommendation",
  heading_text: "Recommended Extension",
  info_icon: {
    label: { attributes: { tooltiptext: "Why am I seeing this" } },
    sumo_path: "extensionrecommendations",
  },
  addon: {
    id: "1234",
    title: "Addon name",
    icon: "https://mozilla.org/icon",
    author: "Author name",
    amo_url: "https://example.com",
  },
  text: "Description of addon",
  buttons: {
    primary: {
      label: {
        value: "Add Now",
        attributes: { accesskey: "A" },
      },
      action: {
        type: "INSTALL_ADDON_FROM_URL",
        data: { url: "https://example.com" },
      },
    },
    secondary: [
      {
        label: {
          value: "Not Now",
          attributes: { accesskey: "N" },
        },
        action: { type: "CANCEL" },
      },
    ],
  },
};

const L10N_CONTENT = {
  layout: "addon_recommendation",
  category: "dummyL10NCategory",
  bucket_id: "some_bucket_id",
  notification_text: { string_id: "notification_text_id" },
  heading_text: { string_id: "heading_text_id" },
  info_icon: {
    label: { string_id: "why_seeing_this" },
    sumo_path: "extensionrecommendations",
  },
  addon: {
    id: "1234",
    title: "Addon name",
    icon: "https://mozilla.org/icon",
    author: "Author name",
    amo_url: "https://example.com",
  },
  text: { string_id: "text_id" },
  buttons: {
    primary: {
      label: { string_id: "btn_ok_id" },
      action: {
        type: "INSTALL_ADDON_FROM_URL",
        data: { url: "https://example.com" },
      },
    },
    secondary: [
      {
        label: { string_id: "btn_cancel_id" },
        action: { type: "CANCEL" },
      },
    ],
  },
};

describe("ExtensionDoorhanger", () => {
  it("should validate DEFAULT_CONTENT", async () => {
    const messages = await CFRMessageProvider.getMessages();
    let doorhangerMessage = messages.find(m => m.id === "FACEBOOK_CONTAINER_3");
    assert.ok(doorhangerMessage, "Message found");
    assert.jsonSchema(
      { ...doorhangerMessage, content: DEFAULT_CONTENT },
      CFRDoorhangerSchema
    );
  });
  it("should validate L10N_CONTENT", async () => {
    const messages = await CFRMessageProvider.getMessages();
    let doorhangerMessage = messages.find(m => m.id === "FACEBOOK_CONTAINER_3");
    assert.ok(doorhangerMessage, "Message found");
    assert.jsonSchema(
      { ...doorhangerMessage, content: L10N_CONTENT },
      CFRDoorhangerSchema
    );
  });
  it("should validate all messages from CFRMessageProvider", async () => {
    const messages = await CFRMessageProvider.getMessages();
    messages.forEach(msg => assert.jsonSchema(msg, SCHEMAS[msg.template]));
  });
});

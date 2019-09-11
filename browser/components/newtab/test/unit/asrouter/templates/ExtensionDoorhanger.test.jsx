import { CFRMessageProvider } from "lib/CFRMessageProvider.jsm";
import schema from "content-src/asrouter/templates/CFR/templates/ExtensionDoorhanger.schema.json";

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
        data: { url: null },
      },
    },
    secondary: {
      label: {
        value: "Not Now",
        attributes: { accesskey: "N" },
      },
      action: { type: "CANCEL" },
    },
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
        data: { url: null },
      },
    },
    secondary: {
      label: { string_id: "btn_cancel_id" },
      action: { type: "CANCEL" },
    },
  },
};

describe("ExtensionDoorhanger", () => {
  it("should validate DEFAULT_CONTENT", () => {
    assert.jsonSchema(DEFAULT_CONTENT, schema);
  });
  it("should validate L10N_CONTENT", () => {
    assert.jsonSchema(L10N_CONTENT, schema);
  });
  it("should validate all messages from CFRMessageProvider", () => {
    const messages = CFRMessageProvider.getMessages();
    messages.forEach(msg => assert.jsonSchema(msg.content, schema));
  });
});

import { GlobalOverrider } from "test/unit/utils";
import { OnboardingMessageProvider } from "lib/OnboardingMessageProvider.jsm";
import schema from "content-src/asrouter/templates/OnboardingMessage/OnboardingMessage.schema.json";
import badgeSchema from "content-src/asrouter/templates/OnboardingMessage/ToolbarBadgeMessage.schema.json";
import whatsNewSchema from "content-src/asrouter/templates/OnboardingMessage/WhatsNewMessage.schema.json";

const DEFAULT_CONTENT = {
  title: "A title",
  text: "A description",
  icon: "icon",
  primary_button: {
    label: "some_button_label",
    action: {
      type: "SOME_TYPE",
      data: { args: "example.com" },
    },
  },
};

const L10N_CONTENT = {
  title: { string_id: "onboarding-private-browsing-title" },
  text: { string_id: "onboarding-private-browsing-text" },
  icon: "icon",
  primary_button: {
    label: { string_id: "onboarding-button-label-get-started" },
    action: { type: "SOME_TYPE" },
  },
};

describe("OnboardingMessage", () => {
  let globals;
  let sandbox;
  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    globals.set("FxAccountsConfig", {
      promiseConnectAccountURI: sandbox.stub().resolves("some/url"),
    });
    globals.set("AddonRepository", {
      getAddonsByIDs: ([content]) => [
        {
          name: content,
          sourceURI: { spec: "foo", scheme: "https" },
          icons: { 64: "icon" },
        },
      ],
    });
  });
  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });
  it("should validate DEFAULT_CONTENT", () => {
    assert.jsonSchema(DEFAULT_CONTENT, schema);
  });
  it("should validate L10N_CONTENT", () => {
    assert.jsonSchema(L10N_CONTENT, schema);
  });
  it("should validate all messages from OnboardingMessageProvider", async () => {
    const messages = await OnboardingMessageProvider.getUntranslatedMessages();
    // FXA_1 doesn't have content - so filter it out
    messages
      .filter(msg => msg.template in ["onboarding", "return_to_amo_overlay"])
      .forEach(msg => assert.jsonSchema(msg.content, schema));
  });
  it("should validate all badge template messages", async () => {
    const messages = await OnboardingMessageProvider.getUntranslatedMessages();

    messages
      .filter(msg => msg.template === "toolbar_badge")
      .forEach(msg => assert.jsonSchema(msg.content, badgeSchema));
  });
  it("should validate all What's New template messages", async () => {
    const messages = await OnboardingMessageProvider.getUntranslatedMessages();

    messages
      .filter(msg => msg.template === "whatsnew_panel_message")
      .forEach(msg => assert.jsonSchema(msg.content, whatsNewSchema));
  });
});

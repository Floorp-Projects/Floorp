import { GlobalOverrider } from "test/unit/utils";
import { OnboardingMessageProvider } from "lib/OnboardingMessageProvider.jsm";
import schema from "content-src/asrouter/templates/OnboardingMessage/OnboardingMessage.schema.json";
import badgeSchema from "content-src/asrouter/templates/OnboardingMessage/ToolbarBadgeMessage.schema.json";

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
    label: { string_id: "onboarding-button-label-try-now" },
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
      promiseEmailFirstURI: sandbox.stub().resolves("some/url"),
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
  it("should decode the content field (double decoding)", async () => {
    const fakeContent = "foo%2540bar.org";
    globals.set("AttributionCode", {
      getAttrDataAsync: sandbox
        .stub()
        .resolves({ content: fakeContent, source: "addons.mozilla.org" }),
    });

    const msgs = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(
      ({ id }) => id === "RETURN_TO_AMO_1"
    );
    const [
      translatedMessage,
    ] = await OnboardingMessageProvider.translateMessages(msgs);
    assert.propertyVal(
      translatedMessage.content.text.args,
      "addon-name",
      "foo@bar.org"
    );
  });
  it("should catch any decoding exceptions", async () => {
    const fakeContent = "foo%bar.org";
    globals.set("AttributionCode", {
      getAttrDataAsync: sandbox
        .stub()
        .resolves({ content: fakeContent, source: "addons.mozilla.org" }),
    });

    const msgs = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(
      ({ id }) => id === "RETURN_TO_AMO_1"
    );
    const [
      translatedMessage,
    ] = await OnboardingMessageProvider.translateMessages(msgs);
    assert.propertyVal(
      translatedMessage.content.text.args,
      "addon-name",
      fakeContent
    );
  });
  it("should ignore attribution from sources other than mozilla.org", async () => {
    const fakeContent = "foo%bar.org";
    globals.set("AttributionCode", {
      getAttrDataAsync: sandbox
        .stub()
        .resolves({ content: fakeContent, source: "addons.allizom.org" }),
    });

    const [
      returnToAMOMsg,
    ] = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(
      ({ id }) => id === "RETURN_TO_AMO_1"
    );
    assert.propertyVal(returnToAMOMsg.content.text.args, "addon-name", null);
  });
  it("should correctly add all addon information to the message after translation", async () => {
    const fakeContent = "foo%2540bar.org";
    globals.set("AttributionCode", {
      getAttrDataAsync: sandbox
        .stub()
        .resolves({ content: fakeContent, source: "addons.mozilla.org" }),
    });

    const msgs = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(
      ({ id }) => id === "RETURN_TO_AMO_1"
    );
    const [
      translatedMessage,
    ] = await OnboardingMessageProvider.translateMessages(msgs);
    assert.propertyVal(
      translatedMessage.content.text.args,
      "addon-name",
      "foo@bar.org"
    );
    assert.propertyVal(translatedMessage.content, "addon_icon", "icon");
    assert.propertyVal(
      translatedMessage.content.primary_button.action.data,
      "url",
      "foo"
    );
    assert.propertyVal(
      translatedMessage.content.primary_button.action.data,
      "telemetrySource",
      "rtamo"
    );
  });
  it("should skip return_to_amo_overlay if any addon fields are missing", async () => {
    const fakeContent = "foo%bar.org";
    globals.set("AttributionCode", {
      getAttrDataAsync: sandbox
        .stub()
        .resolves({ content: fakeContent, source: "addons.mozilla.org" }),
    });
    globals.set("AddonRepository", {
      getAddonsByIDs: ([content]) => [
        {
          name: content,
          sourceURI: { spec: "foo", scheme: "https" },
          icons: { 64: null },
        },
      ],
    });

    const msgs = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(
      ({ id }) => id === "RETURN_TO_AMO_1"
    );
    const translatedMessages = await OnboardingMessageProvider.translateMessages(
      msgs
    );
    assert.lengthOf(translatedMessages, 0);
  });
  it("should skip return_to_amo_overlay if any addon fields are missing", async () => {
    const fakeContent = "foo%bar.org";
    globals.set("AttributionCode", {
      getAttrDataAsync: sandbox
        .stub()
        .resolves({ content: fakeContent, source: "addons.mozilla.org" }),
    });
    globals.set("AddonRepository", {
      getAddonsByIDs: ([content]) => [
        {
          name: content,
          sourceURI: { spec: null, scheme: "https" },
          icons: { 64: "icon" },
        },
      ],
    });

    const msgs = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(
      ({ id }) => id === "RETURN_TO_AMO_1"
    );
    const translatedMessages = await OnboardingMessageProvider.translateMessages(
      msgs
    );
    assert.lengthOf(translatedMessages, 0);
  });
  it("should skip return_to_amo_overlay if any addon fields are missing", async () => {
    const fakeContent = "foo%bar.org";
    globals.set("AttributionCode", {
      getAttrDataAsync: sandbox
        .stub()
        .resolves({ content: fakeContent, source: "addons.mozilla.org" }),
    });
    globals.set("AddonRepository", {
      getAddonsByIDs: ([content]) => [
        {
          name: null,
          sourceURI: { spec: "foo", scheme: "https" },
          icons: { 64: "icon" },
        },
      ],
    });

    const msgs = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(
      ({ id }) => id === "RETURN_TO_AMO_1"
    );
    const translatedMessages = await OnboardingMessageProvider.translateMessages(
      msgs
    );
    assert.lengthOf(translatedMessages, 0);
  });
  it("should skip return_to_amo_overlay if addon scheme is not https", async () => {
    const fakeContent = "foo%bar.org";
    globals.set("AttributionCode", {
      getAttrDataAsync: sandbox
        .stub()
        .resolves({ content: fakeContent, source: "addons.mozilla.org" }),
    });
    globals.set("AddonRepository", {
      getAddonsByIDs: ([content]) => [
        {
          name: content,
          sourceURI: { spec: "foo", scheme: "http" },
          icons: { 64: "icon" },
        },
      ],
    });

    const msgs = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(
      ({ id }) => id === "RETURN_TO_AMO_1"
    );
    const translatedMessages = await OnboardingMessageProvider.translateMessages(
      msgs
    );
    assert.lengthOf(translatedMessages, 0);
  });
  it("should skip return_to_amo_overlay if getAddonInfo fails", async () => {
    globals.set("AttributionCode", {
      getAttrDataAsync: sandbox.stub().rejects(),
    });

    const msgs = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(
      ({ id }) => id === "RETURN_TO_AMO_1"
    );
    const translatedMessages = await OnboardingMessageProvider.translateMessages(
      msgs
    );
    assert.lengthOf(translatedMessages, 0);
  });
  it("should catch any exceptions fetching the addon information", async () => {
    const fakeContent = "foo%bar.org";
    globals.set("AttributionCode", {
      getAttrDataAsync: sandbox.stub().resolves({ content: fakeContent }),
    });
    globals.set("AddonRepository", {
      getAddonsByIDs: sandbox.stub().rejects(),
    });

    const msgs = await OnboardingMessageProvider.getUntranslatedMessages();
    const translatedMessages = await OnboardingMessageProvider.translateMessages(
      msgs
    );
    const returnToAMOMsgs = translatedMessages.filter(
      ({ id }) => id === "RETURN_TO_AMO_1"
    );
    assert.lengthOf(translatedMessages, msgs.length - 1);
    assert.lengthOf(returnToAMOMsgs, 0);
  });
});

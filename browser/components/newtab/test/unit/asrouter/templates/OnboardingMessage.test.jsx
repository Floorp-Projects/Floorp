import {GlobalOverrider} from "test/unit/utils";
import {OnboardingMessageProvider} from "lib/OnboardingMessageProvider.jsm";
import schema from "content-src/asrouter/templates/OnboardingMessage/OnboardingMessage.schema.json";

const DEFAULT_CONTENT = {
  "title": "A title",
  "text": "A description",
  "icon": "icon",
  "primary_button": {
    "label": "some_button_label",
    "action": {
      "type": "SOME_TYPE",
      "data": {"args": "example.com"},
    },
  },
};

const L10N_CONTENT = {
  "title": {string_id: "onboarding-private-browsing-title"},
  "text": {string_id: "onboarding-private-browsing-text"},
  "icon": "icon",
  "primary_button": {
    "label": {string_id: "onboarding-button-label-try-now"},
    "action": {type: "SOME_TYPE"},
  },
};

describe("OnboardingMessage", () => {
  let globals;
  let sandbox;
  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    globals.set("FxAccountsConfig", {promiseEmailFirstURI: sandbox.stub().resolves("some/url")});
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
    messages.filter(msg => msg.content).forEach(msg => assert.jsonSchema(msg.content, schema));
  });
  it("should decode the content field (double decoding)", async () => {
    const fakeContent = "foo%2540bar.org";
    globals.set("AttributionCode", {getAttrDataAsync: sandbox.stub().resolves({content: fakeContent, source: "addons.mozilla.org"})});
    globals.set("AddonRepository", {getAddonsByIDs: ([content]) => [{name: content}]});

    const [returnToAMOMsg] = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(({id}) => id === "RETURN_TO_AMO_1");
    assert.propertyVal(returnToAMOMsg.content.text.args, "addon-name", "foo@bar.org");
  });
  it("should catch any decoding exceptions", async () => {
    const fakeContent = "foo%bar.org";
    globals.set("AttributionCode", {getAttrDataAsync: sandbox.stub().resolves({content: fakeContent, source: "addons.mozilla.org"})});
    globals.set("AddonRepository", {getAddonsByIDs: ([content]) => [{name: content}]});

    const [returnToAMOMsg] = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(({id}) => id === "RETURN_TO_AMO_1");
    assert.propertyVal(returnToAMOMsg.content.text.args, "addon-name", fakeContent);
  });
  it("should ignore attribution from sources other than mozilla.org", async () => {
    const fakeContent = "foo%bar.org";
    globals.set("AttributionCode", {getAttrDataAsync: sandbox.stub().resolves({content: fakeContent, source: "addons.allizom.org"})});
    globals.set("AddonRepository", {getAddonsByIDs: ([content]) => [{name: content}]});

    const [returnToAMOMsg] = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(({id}) => id === "RETURN_TO_AMO_1");
    assert.propertyVal(returnToAMOMsg.content.text.args, "addon-name", null);
  });
});

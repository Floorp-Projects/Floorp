import { AboutWelcomeDefaults } from "aboutwelcome/lib/AboutWelcomeDefaults.jsm";
import { MultiStageProtonScreen } from "content-src/aboutwelcome/components/MultiStageProtonScreen";
import React from "react";
import { mount } from "enzyme";

describe("MultiStageAboutWelcomeProton module", () => {
  let sandbox;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
  });
  afterEach(() => sandbox.restore());

  describe("MultiStageAWProton component", () => {
    it("should render MultiStageProton Screen", () => {
      const SCREEN_PROPS = {
        content: {
          title: "test title",
          subtitle: "test subtitle",
        },
      };
      const wrapper = mount(<MultiStageProtonScreen {...SCREEN_PROPS} />);
      assert.ok(wrapper.exists());
    });

    it("should render section left on first screen", () => {
      const SCREEN_PROPS = {
        order: 0,
        content: {
          title: "test title",
          subtitle: "test subtitle",
        },
      };
      const wrapper = mount(<MultiStageProtonScreen {...SCREEN_PROPS} />);
      assert.ok(wrapper.exists());
      assert.equal(wrapper.find(".welcome-text h1").text(), "test title");
      assert.equal(wrapper.find(".section-left h1").text(), "test subtitle");
    });
  });

  describe("AboutWelcomeDefaults for proton", () => {
    const getData = () => AboutWelcomeDefaults.getDefaults({ isProton: true });
    async function prepConfig(config) {
      return AboutWelcomeDefaults.prepareContentForReact({
        ...(await getData()),
        isProton: true,
        ...config,
      });
    }
    beforeEach(() => {
      sandbox.stub(global.Services.prefs, "getBoolPref").returns(true);
    });
    it("should have 'pin' button by default", async () => {
      const data = await getData();

      assert.propertyVal(
        data.screens[0].content.primary_button.label,
        "string_id",
        "mr1-onboarding-pin-primary-button-label"
      );
    });
    it("should have 'pin' button if we need default and pin", async () => {
      const data = await prepConfig({ needDefault: true, needPin: true });

      assert.propertyVal(
        data.screens[0].content.primary_button.label,
        "string_id",
        "mr1-onboarding-pin-primary-button-label"
      );
      assert.propertyVal(data.screens[0], "id", "AW_PIN_FIREFOX");
      assert.propertyVal(data.screens[1], "id", "AW_SET_DEFAULT");
      assert.lengthOf(data.screens, 4);
    });
    it("should keep 'pin' and remove 'default' if already default", async () => {
      const data = await prepConfig({ needPin: true });

      assert.propertyVal(data.screens[0], "id", "AW_PIN_FIREFOX");
      assert.propertyVal(data.screens[1], "id", "AW_IMPORT_SETTINGS");
      assert.lengthOf(data.screens, 3);
    });
    it("should switch to 'default' if already pinned", async () => {
      const data = await prepConfig({ needDefault: true });

      assert.propertyVal(data.screens[0], "id", "AW_ONLY_DEFAULT");
      assert.propertyVal(data.screens[1], "id", "AW_IMPORT_SETTINGS");
      assert.lengthOf(data.screens, 3);
    });
    it("should switch to 'start' if already pinned and default", async () => {
      const data = await prepConfig();

      assert.propertyVal(data.screens[0], "id", "AW_GET_STARTED");
      assert.propertyVal(data.screens[1], "id", "AW_IMPORT_SETTINGS");
      assert.lengthOf(data.screens, 3);
    });
    it("should have a FxA button", async () => {
      const data = await prepConfig();

      assert.notProperty(data, "skipFxA");
      assert.property(data.screens[0].content, "secondary_button_top");
    });
    it("should remove the FxA button if pref disabled", async () => {
      global.Services.prefs.getBoolPref.returns(false);

      const data = await prepConfig();

      assert.property(data, "skipFxA", true);
      assert.notProperty(data.screens[0].content, "secondary_button_top");
    });
    it("should have an image caption", async () => {
      const data = await prepConfig();

      assert.property(data.screens[0].content, "help_text");
    });
    it("should remove the caption if deleteIfNotEn is true", async () => {
      sandbox.stub(global.Services.locale, "appLocaleAsBCP47").value("de");

      const data = await prepConfig({
        id: "DEFAULT_ABOUTWELCOME_PROTON",
        template: "multistage",
        transitions: true,
        background_url: `chrome://activity-stream/content/data/content/assets/proton-bkg.jpg`,
        screens: [
          {
            id: "AW_PIN_FIREFOX",
            order: 0,
            content: {
              help_text: {
                deleteIfNotEn: true,
                text: {
                  string_id: "mr1-onboarding-welcome-image-caption",
                },
              },
            },
          },
        ],
      });

      assert.notProperty(data.screens[0].content.help_text, "text");
    });
  });
  describe("AboutWelcomeDefaults prepareContentForReact", () => {
    it("should not set action without screens", async () => {
      const data = await AboutWelcomeDefaults.prepareContentForReact({
        ua: "test",
      });

      assert.propertyVal(data, "ua", "test");
      assert.notProperty(data, "screens");
    });
    it("should set action for import action", async () => {
      const TEST_CONTENT = {
        ua: "test",
        screens: [
          {
            id: "AW_IMPORT_SETTINGS",
            content: {
              primary_button: {
                action: {
                  type: "SHOW_MIGRATION_WIZARD",
                },
              },
            },
          },
        ],
      };
      const data = await AboutWelcomeDefaults.prepareContentForReact(
        TEST_CONTENT
      );
      assert.propertyVal(data, "ua", "test");
      assert.propertyVal(
        data.screens[0].content.primary_button.action.data,
        "source",
        "test"
      );
    });
    it("should not set action if the action type != SHOW_MIGRATION_WIZARD", async () => {
      const TEST_CONTENT = {
        ua: "test",
        screens: [
          {
            id: "AW_IMPORT_SETTINGS",
            content: {
              primary_button: {
                action: {
                  type: "SHOW_FIREFOX_ACCOUNTS",
                  data: {},
                },
              },
            },
          },
        ],
      };
      const data = await AboutWelcomeDefaults.prepareContentForReact(
        TEST_CONTENT
      );
      assert.propertyVal(data, "ua", "test");
      assert.notPropertyVal(
        data.screens[0].content.primary_button.action.data,
        "source",
        "test"
      );
    });
    it("should remove theme screens on win7", async () => {
      sandbox.stub(AppConstants, "isPlatformAndVersionAtMost").returns(true);

      const { screens } = await AboutWelcomeDefaults.prepareContentForReact({
        screens: [
          {
            order: 0,
            content: {
              tiles: { type: "theme" },
            },
          },
          { id: "hello", order: 1 },
          {
            order: 2,
            content: {
              tiles: { type: "theme" },
            },
          },
          { id: "world", order: 3 },
        ],
      });

      assert.deepEqual(screens, [
        { id: "hello", order: 0 },
        { id: "world", order: 1 },
      ]);
    });
  });
});

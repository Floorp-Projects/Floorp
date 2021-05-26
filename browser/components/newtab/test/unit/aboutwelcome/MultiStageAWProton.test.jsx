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
    it("should have 'default' button by default", async () => {
      const data = await getData();

      assert.propertyVal(
        data.screens[0].content.primary_button.label,
        "string_id",
        "mr1-onboarding-set-default-only-primary-button-label"
      );
    });
    it("should have 'primary' button if we need to pin", async () => {
      sandbox.stub(global.ShellService, "doesAppNeedPin").resolves(true);

      const data = await AboutWelcomeDefaults.prepareContentForReact({
        ...(await getData()),
        isProton: true,
      });

      assert.propertyVal(
        data.screens[0].content.primary_button.label,
        "string_id",
        "mr1-onboarding-set-default-pin-primary-button-label"
      );
    });
    it("should have a FxA button", async () => {
      sandbox.stub(global.Services.prefs, "getBoolPref").returns(true);

      const data = await AboutWelcomeDefaults.prepareContentForReact({
        ...(await getData()),
        isProton: true,
      });

      assert.notProperty(data, "skipFxA");
      assert.property(data.screens[0].content, "secondary_button_top");
    });
    it("should remove the FxA button if pref disabled", async () => {
      sandbox.stub(global.Services.prefs, "getBoolPref").returns(false);

      const data = await AboutWelcomeDefaults.prepareContentForReact({
        ...(await getData()),
        isProton: true,
      });

      assert.property(data, "skipFxA", true);
      assert.notProperty(data.screens[0].content, "secondary_button_top");
    });
    it("should have an image caption", async () => {
      const data = await AboutWelcomeDefaults.prepareContentForReact({
        ...(await getData()),
        isProton: true,
      });

      assert.property(data.screens[0].content, "help_text");
    });
    it("should remove the caption if deleteIfNotEn is true", async () => {
      sandbox.stub(global.Services.locale, "appLocaleAsBCP47").value("de");

      let defaults = {
        id: "DEFAULT_ABOUTWELCOME_PROTON",
        template: "multistage",
        transitions: true,
        background_url: `chrome://activity-stream/content/data/content/assets/proton-bkg.jpg`,
        screens: [
          {
            id: "AW_SET_DEFAULT",
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
      };

      const data = await AboutWelcomeDefaults.prepareContentForReact({
        ...defaults,
        isProton: true,
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

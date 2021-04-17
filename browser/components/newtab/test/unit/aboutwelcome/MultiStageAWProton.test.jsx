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

      const data = await getData();

      assert.propertyVal(
        data.screens[0].content.primary_button.label,
        "string_id",
        "mr1-onboarding-set-default-pin-primary-button-label"
      );
    });
    it("should keep caption for en-*", async () => {
      const data = await getData();

      assert.property(data.screens[0].content, "help_text");
    });
    it("should remove caption for not-en", async () => {
      sandbox.stub(global.Services.locale, "appLocaleAsBCP47").value("de");

      const data = await getData();

      assert.notProperty(data.screens[0].content, "help_text");
    });
  });
});

import { GlobalOverrider } from "test/unit/utils";
import {
  MultiStageAboutWelcome,
  SecondaryCTA,
  StepsIndicator,
  WelcomeScreen,
} from "content-src/aboutwelcome/components/MultiStageAboutWelcome";
import { MultiStageScreen } from "content-src/aboutwelcome/components/MultiStageScreen";
import { MultiStageProtonScreen } from "content-src/aboutwelcome/components/MultiStageProtonScreen";
import { Themes } from "content-src/aboutwelcome/components/Themes";
import React from "react";
import { shallow, mount } from "enzyme";
import { DEFAULT_WELCOME_CONTENT } from "aboutwelcome/lib/AboutWelcomeDefaults.jsm";
import { AboutWelcomeUtils } from "content-src/lib/aboutwelcome-utils";

describe("MultiStageAboutWelcome module", () => {
  let globals;
  let sandbox;

  const DEFAULT_PROPS = {
    screens: DEFAULT_WELCOME_CONTENT.screens,
    metricsFlowUri: "http://localhost/",
    message_id: "DEFAULT_ABOUTWELCOME",
    utm_term: "default",
  };

  beforeEach(async () => {
    globals = new GlobalOverrider();
    globals.set({
      AWGetDefaultSites: () => Promise.resolve([]),
      AWGetImportableSites: () => Promise.resolve("{}"),
      AWGetSelectedTheme: () => Promise.resolve("automatic"),
      AWSendEventTelemetry: () => {},
      AWWaitForRegionChange: () => Promise.resolve(),
      AWGetRegion: () => Promise.resolve(),
      AWIsDefaultBrowser: () => Promise.resolve("true"),
      AWWaitForMigrationClose: () => Promise.resolve(),
      AWSelectTheme: () => Promise.resolve(),
    });
    sandbox = sinon.createSandbox();
  });

  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });

  describe("MultiStageAboutWelcome functional component", () => {
    it("should render MultiStageAboutWelcome", () => {
      const wrapper = shallow(<MultiStageAboutWelcome {...DEFAULT_PROPS} />);

      assert.ok(wrapper.exists());
    });

    it("should send <MESSAGEID>_SITES impression telemetry ", async () => {
      const impressionSpy = sandbox.spy(
        AboutWelcomeUtils,
        "sendImpressionTelemetry"
      );
      globals.set({
        AWGetImportableSites: () =>
          Promise.resolve('["site-1","site-2","site-3","site-4","site-5"]'),
      });

      mount(<MultiStageAboutWelcome {...DEFAULT_PROPS} />);
      // Delay slightly to make sure we've finished executing any promise
      await new Promise(resolve => setTimeout(resolve, 0));

      assert.calledTwice(impressionSpy);
      assert.equal(
        impressionSpy.firstCall.args[0],
        `${DEFAULT_PROPS.message_id}_${DEFAULT_PROPS.screens[0].id}`
      );
      assert.equal(
        impressionSpy.secondCall.args[0],
        `${DEFAULT_PROPS.message_id}_SITES`
      );
      assert.equal(impressionSpy.secondCall.lastArg.display, "static");
      assert.equal(impressionSpy.secondCall.lastArg.importable, 5);
    });

    it("should pass activeTheme and initialTheme props to WelcomeScreen", async () => {
      let wrapper = mount(<MultiStageAboutWelcome {...DEFAULT_PROPS} />);
      // Spin the event loop to allow the useEffect hooks to execute,
      // any promises to resolve, and re-rendering to happen after the
      // promises have updated the state/props
      await new Promise(resolve => setTimeout(resolve, 0));
      // sync up enzyme's representation with the real DOM
      wrapper.update();

      let welcomeScreenWrapper = wrapper.find(WelcomeScreen);
      assert.strictEqual(welcomeScreenWrapper.prop("activeTheme"), "automatic");
      assert.strictEqual(
        welcomeScreenWrapper.prop("initialTheme"),
        "automatic"
      );
    });

    it("should render proton screens when design is set to proton", async () => {
      let wrapper = mount(
        <MultiStageAboutWelcome design="proton" {...DEFAULT_PROPS} />
      );
      await new Promise(resolve => setTimeout(resolve, 0));
      wrapper.update();

      let protonScreenWrapper = wrapper.find(MultiStageProtonScreen);
      assert.strictEqual(protonScreenWrapper.prop("design"), "proton");
    });

    it("should handle primary Action", () => {
      const stub = sinon.stub(AboutWelcomeUtils, "sendActionTelemetry");
      let wrapper = mount(<MultiStageAboutWelcome {...DEFAULT_PROPS} />);
      wrapper.update();

      let welcomeScreenWrapper = wrapper.find(WelcomeScreen);
      const btnPrimary = welcomeScreenWrapper.find(".primary");
      btnPrimary.simulate("click");
      assert.calledOnce(stub);
      assert.equal(
        stub.firstCall.args[0],
        welcomeScreenWrapper.props().messageId
      );
      assert.equal(stub.firstCall.args[1], "primary_button");
    });
  });

  describe("WelcomeScreen component", () => {
    describe("get started screen", () => {
      const startScreen = DEFAULT_WELCOME_CONTENT.screens.find(screen => {
        return screen.id === "AW_SET_DEFAULT";
      });

      const GET_STARTED_SCREEN_PROPS = {
        id: startScreen.id,
        totalNumberofScreens: 1,
        order: startScreen.order,
        content: startScreen.content,
        topSites: [],
        messageId: `${DEFAULT_PROPS.message_id}_${startScreen.id}`,
        UTMTerm: DEFAULT_PROPS.utm_term,
        flowParams: null,
      };

      it("should render GetStarted Screen", () => {
        const wrapper = shallow(
          <WelcomeScreen {...GET_STARTED_SCREEN_PROPS} />
        );
        assert.ok(wrapper.exists());
      });

      it("should render secondary.top button", () => {
        let SCREEN_PROPS = {
          content: {
            title: "Step",
            secondary_button_top: {
              text: "test",
              label: "test label",
            },
          },
          position: "top",
        };
        const wrapper = mount(<SecondaryCTA {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("div.secondary_button_top"));
      });

      it("should render steps indicator", () => {
        let SCREEN_PROPS = {
          totalNumberOfScreens: 1,
          order: 0,
        };
        <StepsIndicator {...SCREEN_PROPS} />;
        const wrapper = mount(<StepsIndicator {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("div.indicator"));
      });

      it("should have a primary, secondary and secondary.top button in the rendered input", () => {
        const wrapper = mount(<WelcomeScreen {...GET_STARTED_SCREEN_PROPS} />);
        assert.ok(wrapper.find(".primary"));
        assert.ok(wrapper.find(".secondary button[value='secondary_button']"));
        assert.ok(
          wrapper.find(".secondary button[value='secondary_button_top']")
        );
      });
    });

    describe("multistagescreen tiles", () => {
      let SCREEN_PROPS = {
        content: {
          title: "test title",
        },
        totalNumberOfScreens: 1,
        order: 0,
        id: "test",
        topSites: {
          data: [],
        },
      };
      it("should render multistage Screen", () => {
        const wrapper = mount(<MultiStageScreen {...SCREEN_PROPS} />);
        assert.ok(wrapper.exists());
      });
      it("no image displayed without source", () => {
        SCREEN_PROPS.content.tiles = {
          type: "image",
          media_type: "test-img",
        };
        const wrapper = mount(<MultiStageScreen {...SCREEN_PROPS} />);
        assert.isFalse(wrapper.find("div.test-img").exists());
      });
      it("should have image displayed with source", () => {
        SCREEN_PROPS.content.tiles = {
          type: "image",
          media_type: "test-img",
          source: {
            default: "",
          },
        };
        const wrapper = mount(<MultiStageScreen {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("div.test-img").exists());
      });
      it("should have video container displayed", () => {
        SCREEN_PROPS.content.tiles = {
          type: "video",
          media_type: "test-video",
          source: {
            default: "",
          },
        };
        const wrapper = mount(<MultiStageScreen {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("div.test-video").exists());
      });
      it("should have topsites section displayed", () => {
        SCREEN_PROPS.content.tiles = {
          type: "topsites",
        };
        const wrapper = mount(<MultiStageScreen {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("div.tiles-topsites-section").exists());
      });
      it("should have theme container displayed", () => {
        SCREEN_PROPS.content.tiles = {
          type: "theme",
          data: [],
        };
        const wrapper = mount(<MultiStageScreen {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("div.tiles-theme-container").exists());
      });
    });

    describe("theme screen", () => {
      const themeScreen = DEFAULT_WELCOME_CONTENT.screens.find(screen => {
        return screen.id === "AW_CHOOSE_THEME";
      });

      const THEME_SCREEN_PROPS = {
        id: themeScreen.id,
        totalNumberofScreens: 1,
        order: themeScreen.order,
        content: themeScreen.content,
        navigate: null,
        topSites: [],
        messageId: `${DEFAULT_PROPS.message_id}_${themeScreen.id}`,
        UTMTerm: DEFAULT_PROPS.utm_term,
        flowParams: null,
        activeTheme: "automatic",
      };

      it("should render WelcomeScreen", () => {
        const wrapper = shallow(<WelcomeScreen {...THEME_SCREEN_PROPS} />);

        assert.ok(wrapper.exists());
      });

      it("should select this.props.activeTheme in the rendered input", () => {
        const wrapper = shallow(<Themes {...THEME_SCREEN_PROPS} />);

        const selectedThemeInput = wrapper.find(".theme.selected input");
        assert.strictEqual(
          selectedThemeInput.prop("value"),
          THEME_SCREEN_PROPS.activeTheme
        );
      });

      it("should check this.props.activeTheme in the rendered input", () => {
        const wrapper = shallow(<Themes {...THEME_SCREEN_PROPS} />);

        const selectedThemeInput = wrapper.find(".theme input[checked=true]");
        assert.strictEqual(
          selectedThemeInput.prop("value"),
          THEME_SCREEN_PROPS.activeTheme
        );
      });
    });
    describe("import screen", () => {
      const IMPORT_SCREEN_PROPS = {
        content: {
          help_text: {
            text: "test help text",
            position: "default",
          },
        },
      };
      it("should render ImportScreen", () => {
        const wrapper = mount(<WelcomeScreen {...IMPORT_SCREEN_PROPS} />);
        assert.ok(wrapper.exists());
      });
      it("should have a help text in the rendered output", () => {
        const wrapper = mount(<WelcomeScreen {...IMPORT_SCREEN_PROPS} />);
        assert.equal(wrapper.find("p.helptext").text(), "test help text");
      });
      it("should not have a primary or secondary button", () => {
        const wrapper = mount(<WelcomeScreen {...IMPORT_SCREEN_PROPS} />);
        assert.isFalse(wrapper.find(".primary").exists());
        assert.isFalse(
          wrapper.find(".secondary button[value='secondary_button']").exists()
        );
        assert.isFalse(
          wrapper
            .find(".secondary button[value='secondary_button_top']")
            .exists()
        );
      });
    });
    describe("#handleAction", () => {
      let SCREEN_PROPS;
      let TEST_ACTION;
      beforeEach(() => {
        SCREEN_PROPS = {
          content: {
            primary_button: {
              action: {},
              label: "test button",
            },
          },
          navigate: sandbox.stub(),
          setActiveTheme: sandbox.stub(),
          UTMTerm: "you_tee_emm",
        };
        TEST_ACTION = SCREEN_PROPS.content.primary_button.action;
        sandbox.stub(AboutWelcomeUtils, "handleUserAction");
      });
      it("should handle navigate", () => {
        TEST_ACTION.navigate = true;
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");

        assert.calledOnce(SCREEN_PROPS.navigate);
      });
      it("should handle theme", () => {
        TEST_ACTION.theme = "test";
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");

        assert.calledWith(SCREEN_PROPS.setActiveTheme, "test");
      });
      it("should handle SHOW_FIREFOX_ACCOUNTS", () => {
        TEST_ACTION.type = "SHOW_FIREFOX_ACCOUNTS";
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");

        assert.calledWith(AboutWelcomeUtils.handleUserAction, {
          data: {
            extraParams: {
              utm_campaign: "firstrun",
              utm_medium: "referral",
              utm_source: "activity-stream",
              utm_term: "aboutwelcome-you_tee_emm-screen",
            },
          },
          type: "SHOW_FIREFOX_ACCOUNTS",
        });
      });
      it("should handle SHOW_MIGRATION_WIZARD", () => {
        TEST_ACTION.type = "SHOW_MIGRATION_WIZARD";
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");

        assert.calledWith(AboutWelcomeUtils.handleUserAction, {
          type: "SHOW_MIGRATION_WIZARD",
        });
      });
      it("should handle waitForDefault", () => {
        TEST_ACTION.waitForDefault = true;
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");

        assert.propertyVal(
          wrapper.state(),
          "alternateContent",
          "waiting_for_default"
        );
      });
    });
    describe("alternate content", () => {
      const SCREEN_PROPS = {
        content: {
          title: "Original",
          alternate: {
            title: "Alternate",
          },
        },
      };
      it("should show original title", () => {
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        assert.equal(wrapper.find(".welcome-text").text(), "Original");
      });
      it("should show alternate title", () => {
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.setState({ alternateContent: "alternate" });

        assert.equal(wrapper.find(".welcome-text").text(), "Alternate");
      });
    });
  });
});

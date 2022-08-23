import { GlobalOverrider } from "test/unit/utils";
import {
  MultiStageAboutWelcome,
  SecondaryCTA,
  StepsIndicator,
  WelcomeScreen,
} from "content-src/aboutwelcome/components/MultiStageAboutWelcome";
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
    startScreen: 0,
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
      AWWaitForMigrationClose: () => Promise.resolve(),
      AWSelectTheme: () => Promise.resolve(),
      AWFinish: () => Promise.resolve(),
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
        `${DEFAULT_PROPS.message_id}_0_${DEFAULT_PROPS.screens[0].id}`
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
      stub.restore();
    });

    it("should autoAdvance on last screen and send appropriate telemetry", () => {
      let clock = sinon.useFakeTimers();
      const screens = [
        {
          auto_advance: "primary_button",
          content: {
            title: "test title",
            subtitle: "test subtitle",
            primary_button: {
              label: "Test Button",
              action: {
                navigate: true,
              },
            },
          },
        },
      ];
      const AUTO_ADVANCE_PROPS = {
        screens,
        metricsFlowUri: "http://localhost/",
        message_id: "DEFAULT_ABOUTWELCOME",
        utm_term: "default",
        startScreen: 0,
      };
      const wrapper = mount(<MultiStageAboutWelcome {...AUTO_ADVANCE_PROPS} />);
      wrapper.update();
      const finishStub = sandbox.stub(global, "AWFinish");
      const telemetryStub = sinon.stub(
        AboutWelcomeUtils,
        "sendActionTelemetry"
      );

      assert.notCalled(finishStub);
      clock.tick(20001);
      assert.calledOnce(finishStub);
      assert.calledOnce(telemetryStub);
      assert.equal(telemetryStub.lastCall.args[2], "AUTO_ADVANCE");
      clock.restore();
      finishStub.restore();
      telemetryStub.restore();
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
        content: startScreen.content,
        topSites: [],
        messageId: `${DEFAULT_PROPS.message_id}_${startScreen.id}`,
        UTMTerm: DEFAULT_PROPS.utm_term,
        flowParams: null,
        startScreen: 0,
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

      it("should render the arrow icon in the secondary button", () => {
        let SCREEN_PROPS = {
          content: {
            title: "Step",
            secondary_button: {
              has_arrow_icon: true,
              label: "test label",
            },
          },
        };
        const wrapper = mount(<SecondaryCTA {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("button[class='arrow-icon']"));
      });

      it("should render steps indicator", () => {
        let SCREEN_PROPS = {
          totalNumberOfScreens: 1,
        };
        <StepsIndicator {...SCREEN_PROPS} />;
        const wrapper = mount(<StepsIndicator {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("div.indicator"));
      });

      it("should assign the total number of screens and current screen to the aria-valuemax and aria-valuenow labels", () => {
        const SCREEN_PROPS = {
          totalNumberOfScreens: 3,
          currentScreen: 1,
        };
        <StepsIndicator {...SCREEN_PROPS} />;
        const wrapper = mount(<StepsIndicator {...SCREEN_PROPS} />);
        assert.ok(
          wrapper.find(
            `div.steps[aria-valuemax=${SCREEN_PROPS.totalNumberOfScreens}][aria-valuenow=${SCREEN_PROPS.currentScreen}][aria-valuemin="1"]`
          )
        );
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

    describe("theme screen", () => {
      const THEME_SCREEN_PROPS = {
        id: "test-theme-screen",
        totalNumberofScreens: 1,
        content: {
          title: "test title",
          subtitle: "test subtitle",
          tiles: {
            type: "theme",
            action: {
              theme: "<event>",
            },
            data: [
              {
                theme: "automatic",
                label: "test-label",
                tooltip: "test-tooltip",
                description: "test-description",
              },
            ],
          },
          primary_button: {
            action: {},
            label: "test button",
          },
        },
        navigate: null,
        topSites: [],
        messageId: `${DEFAULT_PROPS.message_id}_"test-theme-screen"`,
        UTMTerm: DEFAULT_PROPS.utm_term,
        flowParams: null,
        activeTheme: "automatic",
      };

      it("should render WelcomeScreen", () => {
        const wrapper = shallow(<WelcomeScreen {...THEME_SCREEN_PROPS} />);

        assert.ok(wrapper.exists());
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
          title: "test title",
          subtitle: "test subtitle",
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
            title: "test title",
            subtitle: "test subtitle",
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
    });
  });
});

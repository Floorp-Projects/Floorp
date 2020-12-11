import { GlobalOverrider } from "test/unit/utils";
import {
  MultiStageAboutWelcome,
  WelcomeScreen,
} from "content-src/aboutwelcome/components/MultiStageAboutWelcome";
import React from "react";
import { shallow, mount } from "enzyme";
import {
  DEFAULT_WELCOME_CONTENT,
  AboutWelcomeUtils,
} from "content-src/lib/aboutwelcome-utils";

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
        return screen.id === "AW_GET_STARTED";
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

      it("should have primary and secondary button in the rendered input", () => {
        const wrapper = mount(<WelcomeScreen {...GET_STARTED_SCREEN_PROPS} />);
        assert.ok(wrapper.find(".primary"));
        assert.isTrue(wrapper.find("div.secondary-cta").hasClass("top"));
        assert.ok(wrapper.find(".secondary"));
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
        const wrapper = shallow(<WelcomeScreen {...THEME_SCREEN_PROPS} />);

        const selectedThemeInput = wrapper.find(".theme.selected input");
        assert.strictEqual(
          selectedThemeInput.prop("value"),
          THEME_SCREEN_PROPS.activeTheme
        );
      });

      it("should check this.props.activeTheme in the rendered input", () => {
        const wrapper = shallow(<WelcomeScreen {...THEME_SCREEN_PROPS} />);

        const selectedThemeInput = wrapper.find(".theme input[checked=true]");
        assert.strictEqual(
          selectedThemeInput.prop("value"),
          THEME_SCREEN_PROPS.activeTheme
        );
      });
    });
  });
});

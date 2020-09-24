import { act } from "react-dom/test-utils";
import { GlobalOverrider } from "test/unit/utils";
import {
  MultiStageAboutWelcome,
  WelcomeScreen,
} from "content-src/aboutwelcome/components/MultiStageAboutWelcome";
import React from "react";
import { shallow, mount } from "enzyme";
import { DEFAULT_WELCOME_CONTENT } from "content-src/lib/aboutwelcome-utils";

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
      AWGetDefaultSites: () => Promise.resolve({}),
      AWGetImportableSites: () => Promise.resolve("{}"),
      AWGetSelectedTheme: () => Promise.resolve("automatic"),
      AWSendEventTelemetry: () => {},
      AWWaitForRegionChange: () => Promise.resolve(),
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

    it("should pass activeTheme and initialTheme props to WelcomeScreen", async () => {
      let wrapper;

      await act(async () => {
        // need to use mount, as shallow doesn't currently support effects
        wrapper = mount(<MultiStageAboutWelcome {...DEFAULT_PROPS} />);

        // Spin the event loop to allow the useEffect hooks to execute,
        // any promises to resolve, and re-rendering to happen after the
        // promises have updated the state/props
        await new Promise(resolve => setTimeout(resolve, 0));

        // sync up enzyme's representation with the real DOM
        wrapper.update();
      });

      let welcomeScreenWrapper = wrapper.find(WelcomeScreen);
      assert.strictEqual(welcomeScreenWrapper.prop("activeTheme"), "automatic");
      assert.strictEqual(
        welcomeScreenWrapper.prop("initialTheme"),
        "automatic"
      );
    });
  });

  describe("WelcomeScreen component", () => {
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
        topSites: {},
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

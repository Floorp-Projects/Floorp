import React from "react";
import { shallow } from "enzyme";
import {
  Colorways,
  computeColorWay,
  VariationsCircle,
} from "content-src/aboutwelcome/components/Colorways";
import { WelcomeScreen } from "content-src/aboutwelcome/components/MultiStageAboutWelcome";

describe("Multistage AboutWelcome module", () => {
  let sandbox;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
  });
  afterEach(() => sandbox.restore());

  describe("Colorway component", () => {
    const COLORWAY_SCREEN_PROPS = {
      id: "test-colorway-screen",
      totalNumberofScreens: 1,
      order: 0,
      content: {
        subtitle: "test subtitle",
        tiles: {
          type: "colorway",
          action: {
            theme: "<event>",
          },
          defaultVariationId: "soft",
          systemDefaultVariationId: "automatic",
          systemVariations: [
            {
              id: "automatic",
              label: "test-auto",
            },
            {
              id: "light",
              label: "test-light",
            },
          ],
          variations: [
            {
              id: "soft",
              label: "Soft",
            },
          ],
          colorways: [
            {
              id: "default",
              label: "Default",
            },
            {
              id: "abstract",
              label: "Abstract",
            },
          ],
        },
        primary_button: {
          action: {},
          label: "test button",
        },
      },
      topSites: [],
      messageId: "test-colorway-screen",
      activeTheme: "automatic",
    };

    it("should render WelcomeScreen", () => {
      const wrapper = shallow(<WelcomeScreen {...COLORWAY_SCREEN_PROPS} />);

      assert.ok(wrapper.exists());
    });

    it("should use default when activeTheme is not set", () => {
      const wrapper = shallow(<Colorways {...COLORWAY_SCREEN_PROPS} />);
      wrapper.setProps({ activeTheme: null });

      const colorwaysOptionIcons = wrapper.find(
        ".tiles-theme-section .theme .icon"
      );
      assert.strictEqual(colorwaysOptionIcons.length, 2);

      // Default automatic theme is selected by default
      assert.strictEqual(
        colorwaysOptionIcons
          .first()
          .prop("className")
          .includes("selected"),
        true
      );

      assert.strictEqual(
        colorwaysOptionIcons
          .first()
          .prop("className")
          .includes("default"),
        true
      );
    });

    it("should render coloways options", () => {
      const wrapper = shallow(<Colorways {...COLORWAY_SCREEN_PROPS} />);

      const colorwaysOptions = wrapper.find(
        ".tiles-theme-section .theme input[name='theme']"
      );

      const colorwaysOptionIcons = wrapper.find(
        ".tiles-theme-section .theme .icon"
      );
      assert.strictEqual(colorwaysOptions.length, 2);
      assert.strictEqual(colorwaysOptionIcons.length, 2);

      // First colorway option
      // Default theme radio option is selected by default
      assert.strictEqual(
        colorwaysOptionIcons
          .first()
          .prop("className")
          .includes("selected"),
        true
      );

      //Colorway should be using id property
      assert.strictEqual(
        colorwaysOptions.first().prop("data-colorway"),
        "default"
      );
      // Value  of Default theme radio option should be using systemDefaultVariationId
      assert.strictEqual(colorwaysOptions.first().prop("value"), "automatic");

      // Second colorway option
      assert.strictEqual(
        colorwaysOptionIcons
          .last()
          .prop("className")
          .includes("selected"),
        false
      );

      //Colorway should be using id property
      assert.strictEqual(
        colorwaysOptions.last().prop("data-colorway"),
        "abstract"
      );
      // Value  of non-Default theme radio option should be using
      // 'colorwayId-defaultVariationId'
      assert.strictEqual(
        colorwaysOptions.last().prop("value"),
        "abstract-soft"
      );
    });

    it("should render variations options", () => {
      const wrapper = shallow(<Colorways {...COLORWAY_SCREEN_PROPS} />);

      let variationsWrapper = wrapper.find(VariationsCircle);
      assert.ok(variationsWrapper.exists());

      let props = variationsWrapper.props();

      // Variation Circle should display Default theme color variations by default
      assert.strictEqual(props.colorway, "default");
      assert.strictEqual(props.colorwayText, "Default");
      assert.strictEqual(props.activeTheme, "automatic");
      assert.strictEqual(props.variations.length, 2);
      assert.strictEqual(props.variations[0].id, "automatic");
      assert.strictEqual(props.variations[1].id, "light");
    });

    it("VariationsCircle should display active colorway", () => {
      let TEST_VARIATIONS_PROPS = {
        variations: [
          {
            id: "soft",
            label: "Soft",
          },
        ],
        colorway: "abstract",
        colorwayText: "Abstract",
        activeTheme: "abstract-soft",
        setVariation: sandbox.stub(),
      };
      const variationsWrapper = shallow(
        <VariationsCircle {...TEST_VARIATIONS_PROPS} />
      );
      assert.ok(variationsWrapper.exists());
      const variationContainer = variationsWrapper.find(".colorway-variations");
      assert.equal(
        variationContainer.props().className.includes("abstract"),
        true
      );
      // Localized tag text attribute is set to colorwayText
      assert.strictEqual(
        variationsWrapper.props().children[0].props.text,
        "Abstract"
      );
    });

    it("should computeColorWayId for default active theme", () => {
      let TEST_COLORWAY_PROPS = {
        ...COLORWAY_SCREEN_PROPS,
      };

      const colorwayId = computeColorWay(
        TEST_COLORWAY_PROPS.activeTheme,
        TEST_COLORWAY_PROPS.content.tiles.systemVariations
      );
      assert.strictEqual(colorwayId, "default");
    });

    it("should computeColorWayId for non-default active theme", () => {
      let TEST_COLORWAY_PROPS = {
        ...COLORWAY_SCREEN_PROPS,
        activeTheme: "abstract-soft",
      };

      const colorwayId = computeColorWay(
        TEST_COLORWAY_PROPS.activeTheme,
        TEST_COLORWAY_PROPS.content.tiles.systemVariations
      );
      assert.strictEqual(colorwayId, "abstract");
    });
  });
});

import React from "react";
import { shallow } from "enzyme";
import {
  Colorways,
  computeColorWay,
  ColorwayDescription,
  computeVariationIndex,
} from "content-src/aboutwelcome/components/MRColorways";
import { WelcomeScreen } from "content-src/aboutwelcome/components/MultiStageAboutWelcome";

describe("Multistage AboutWelcome module", () => {
  let sandbox;
  let COLORWAY_SCREEN_PROPS;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    COLORWAY_SCREEN_PROPS = {
      id: "test-colorway-screen",
      totalNumberofScreens: 1,
      content: {
        subtitle: "test subtitle",
        tiles: {
          type: "colorway",
          action: {
            theme: "<event>",
          },
          defaultVariationIndex: 0,
          systemVariations: ["automatic", "light"],
          variations: ["soft", "bold"],
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
      messageId: "test-mr-colorway-screen",
      activeTheme: "automatic",
    };
  });
  afterEach(() => {
    sandbox.restore();
  });

  describe("MRColorway component", () => {
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

    it("should use default when activeTheme is alpenglow", () => {
      const wrapper = shallow(<Colorways {...COLORWAY_SCREEN_PROPS} />);
      wrapper.setProps({ activeTheme: "alpenglow" });

      const colorwaysOptionIcons = wrapper.find(
        ".tiles-theme-section .theme .icon"
      );
      assert.strictEqual(colorwaysOptionIcons.length, 2);

      // Default automatic theme is selected when unsupported in colorway alpenglow theme is active
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

    it("should render colorways options", () => {
      const wrapper = shallow(<Colorways {...COLORWAY_SCREEN_PROPS} />);

      const colorwaysOptions = wrapper.find(
        ".tiles-theme-section .theme input[name='theme']"
      );

      const colorwaysOptionIcons = wrapper.find(
        ".tiles-theme-section .theme .icon"
      );

      const colorwaysLabels = wrapper.find(
        ".tiles-theme-section .theme span.sr-only"
      );

      assert.strictEqual(colorwaysOptions.length, 2);
      assert.strictEqual(colorwaysOptionIcons.length, 2);
      assert.strictEqual(colorwaysLabels.length, 2);

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

      //Colorway should be labelled for screen readers (parent label is for tooltip only, and does not describe the Colorway)
      assert.strictEqual(
        colorwaysOptions.last().prop("aria-labelledby"),
        "abstract-label"
      );
    });

    it("should handle colorway clicks", () => {
      sandbox.stub(React, "useEffect").callsFake((fn, vals) => {
        if (vals === undefined) {
          fn();
        } else if (vals[0] === "in") {
          fn();
        }
      });

      const handleAction = sandbox.stub();
      const wrapper = shallow(
        <Colorways handleAction={handleAction} {...COLORWAY_SCREEN_PROPS} />
      );
      const colorwaysOptions = wrapper.find(
        ".tiles-theme-section .theme input[name='theme']"
      );

      let props = wrapper.find(ColorwayDescription).props();
      assert.propertyVal(props.colorway, "label", "Default");

      const option = colorwaysOptions.last();
      assert.propertyVal(option.props(), "value", "abstract-soft");
      colorwaysOptions.last().simulate("click");
      assert.calledOnce(handleAction);
    });

    it("should render colorway description", () => {
      const wrapper = shallow(<Colorways {...COLORWAY_SCREEN_PROPS} />);

      let descriptionsWrapper = wrapper.find(ColorwayDescription);
      assert.ok(descriptionsWrapper.exists());

      let props = descriptionsWrapper.props();

      // Colorway  description should display Default theme desc by default
      assert.strictEqual(props.colorway.label, "Default");
    });

    it("ColorwayDescription should display active colorway desc", () => {
      let TEST_COLORWAY_PROPS = {
        colorway: {
          label: "Activist",
          description: "Test Activist",
        },
      };
      const descWrapper = shallow(
        <ColorwayDescription {...TEST_COLORWAY_PROPS} />
      );
      assert.ok(descWrapper.exists());
      const descText = descWrapper.find(".colorway-text");
      assert.equal(
        descText.props()["data-l10n-args"].includes("Activist"),
        true
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

    it("should computeVariationIndex for default active theme", () => {
      let TEST_COLORWAY_PROPS = {
        ...COLORWAY_SCREEN_PROPS,
      };

      const variationIndex = computeVariationIndex(
        TEST_COLORWAY_PROPS.activeTheme,
        TEST_COLORWAY_PROPS.content.tiles.systemVariations,
        TEST_COLORWAY_PROPS.content.tiles.variations,
        TEST_COLORWAY_PROPS.content.tiles.defaultVariationIndex
      );
      assert.strictEqual(
        variationIndex,
        TEST_COLORWAY_PROPS.content.tiles.defaultVariationIndex
      );
    });

    it("should computeVariationIndex for active theme", () => {
      let TEST_COLORWAY_PROPS = {
        ...COLORWAY_SCREEN_PROPS,
      };

      const variationIndex = computeVariationIndex(
        "light",
        TEST_COLORWAY_PROPS.content.tiles.systemVariations,
        TEST_COLORWAY_PROPS.content.tiles.variations,
        TEST_COLORWAY_PROPS.content.tiles.defaultVariationIndex
      );
      assert.strictEqual(variationIndex, 1);
    });

    it("should computeVariationIndex for colorway theme", () => {
      let TEST_COLORWAY_PROPS = {
        ...COLORWAY_SCREEN_PROPS,
      };

      const variationIndex = computeVariationIndex(
        "abstract-bold",
        TEST_COLORWAY_PROPS.content.tiles.systemVariations,
        TEST_COLORWAY_PROPS.content.tiles.variations,
        TEST_COLORWAY_PROPS.content.tiles.defaultVariationIndex
      );
      assert.strictEqual(variationIndex, 1);
    });

    describe("random colorways", () => {
      let test;
      beforeEach(() => {
        COLORWAY_SCREEN_PROPS.handleAction = sandbox.stub();
        sandbox.stub(window, "matchMedia");
        // eslint-disable-next-line max-nested-callbacks
        sandbox.stub(React, "useEffect").callsFake((fn, vals) => {
          if (vals?.length === 0) {
            fn();
          }
        });
        test = () => {
          shallow(<Colorways {...COLORWAY_SCREEN_PROPS} />);
          return COLORWAY_SCREEN_PROPS.handleAction.firstCall.firstArg
            .currentTarget;
        };
      });

      it("should select a random colorway", () => {
        const { value } = test();

        assert.strictEqual(value, "abstract-soft");
        assert.calledThrice(React.useEffect);
        assert.notCalled(window.matchMedia);
      });

      it("should select a random soft colorway when not dark", () => {
        window.matchMedia.returns({ matches: false });
        COLORWAY_SCREEN_PROPS.content.tiles.darkVariation = 1;

        const { value } = test();

        assert.strictEqual(value, "abstract-soft");
        assert.calledThrice(React.useEffect);
        assert.calledOnce(window.matchMedia);
      });

      it("should select a random bold colorway when dark", () => {
        window.matchMedia.returns({ matches: true });
        COLORWAY_SCREEN_PROPS.content.tiles.darkVariation = 1;

        const { value } = test();

        assert.strictEqual(value, "abstract-bold");
        assert.calledThrice(React.useEffect);
        assert.calledOnce(window.matchMedia);
      });
    });
  });
});

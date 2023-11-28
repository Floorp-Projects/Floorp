import { GlobalOverrider } from "newtab/test/unit/utils";
import {
  MultiStageAboutWelcome,
  SecondaryCTA,
  StepsIndicator,
  ProgressBar,
  WelcomeScreen,
} from "content-src/components/MultiStageAboutWelcome";
import { Themes } from "content-src/components/Themes";
import React from "react";
import { shallow, mount } from "enzyme";
import { AboutWelcomeDefaults } from "modules/AboutWelcomeDefaults.jsm";
import { AboutWelcomeUtils } from "content-src/lib/aboutwelcome-utils";

describe("MultiStageAboutWelcome module", () => {
  let globals;
  let sandbox;

  const DEFAULT_PROPS = {
    defaultScreens: AboutWelcomeDefaults.getDefaults().screens,
    metricsFlowUri: "http://localhost/",
    message_id: "DEFAULT_ABOUTWELCOME",
    utm_term: "default",
    startScreen: 0,
  };

  beforeEach(async () => {
    globals = new GlobalOverrider();
    globals.set({
      AWGetSelectedTheme: () => Promise.resolve("automatic"),
      AWSendEventTelemetry: () => {},
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
      const screens = [
        {
          content: {
            title: "test title",
            subtitle: "test subtitle",
            primary_button: {
              label: "Test button",
              action: {
                navigate: true,
              },
            },
          },
        },
      ];

      const PRIMARY_ACTION_PROPS = {
        defaultScreens: screens,
        metricsFlowUri: "http://localhost/",
        message_id: "DEFAULT_ABOUTWELCOME",
        utm_term: "default",
        startScreen: 0,
      };

      const stub = sinon.stub(AboutWelcomeUtils, "sendActionTelemetry");
      let wrapper = mount(<MultiStageAboutWelcome {...PRIMARY_ACTION_PROPS} />);
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
        defaultScreens: screens,
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

    it("should send telemetry ping on collectSelect", () => {
      const screens = [
        {
          id: "EASY_SETUP_TEST",
          content: {
            tiles: {
              type: "multiselect",
              data: [
                {
                  id: "checkbox-1",
                  defaultValue: true,
                },
              ],
            },
            primary_button: {
              label: "Test Button",
              action: {
                collectSelect: true,
              },
            },
          },
        },
      ];
      const EASY_SETUP_PROPS = {
        defaultScreens: screens,
        message_id: "DEFAULT_ABOUTWELCOME",
        startScreen: 0,
      };
      const stub = sinon.stub(AboutWelcomeUtils, "sendActionTelemetry");
      let wrapper = mount(<MultiStageAboutWelcome {...EASY_SETUP_PROPS} />);
      wrapper.update();

      let welcomeScreenWrapper = wrapper.find(WelcomeScreen);
      const btnPrimary = welcomeScreenWrapper.find(".primary");
      btnPrimary.simulate("click");
      assert.calledTwice(stub);
      assert.equal(
        stub.firstCall.args[0],
        welcomeScreenWrapper.props().messageId
      );
      assert.equal(stub.firstCall.args[1], "primary_button");
      assert.equal(
        stub.lastCall.args[0],
        welcomeScreenWrapper.props().messageId
      );
      assert.ok(stub.lastCall.args[1].includes("checkbox-1"));
      assert.equal(stub.lastCall.args[2], "SELECT_CHECKBOX");
      stub.restore();
    });
  });

  describe("WelcomeScreen component", () => {
    describe("easy setup screen", () => {
      const screen = AboutWelcomeDefaults.getDefaults().screens.find(
        s => s.id === "AW_EASY_SETUP_NEEDS_DEFAULT_AND_PIN"
      );
      let EASY_SETUP_SCREEN_PROPS;

      beforeEach(() => {
        EASY_SETUP_SCREEN_PROPS = {
          id: screen.id,
          content: screen.content,
          messageId: `${DEFAULT_PROPS.message_id}_${screen.id}`,
          UTMTerm: DEFAULT_PROPS.utm_term,
          flowParams: null,
          totalNumberOfScreens: 1,
          setActiveMultiSelect: sandbox.stub(),
        };
      });

      it("should render Easy Setup screen", () => {
        const wrapper = shallow(<WelcomeScreen {...EASY_SETUP_SCREEN_PROPS} />);
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
        assert.ok(wrapper.find("div.secondary-cta.top").exists());
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
        assert.ok(wrapper.find("button.arrow-icon").exists());
      });

      it("should render steps indicator", () => {
        let PROPS = { totalNumberOfScreens: 1 };
        const wrapper = mount(<StepsIndicator {...PROPS} />);
        assert.ok(wrapper.find("div.indicator").exists());
      });

      it("should assign the total number of screens and current screen to the aria-valuemax and aria-valuenow labels", () => {
        const EXTRA_PROPS = { totalNumberOfScreens: 3, order: 1 };
        const wrapper = mount(
          <WelcomeScreen {...EASY_SETUP_SCREEN_PROPS} {...EXTRA_PROPS} />
        );

        const steps = wrapper.find(`div.steps`);
        assert.ok(steps.exists());
        const { attributes } = steps.getDOMNode();
        assert.equal(
          parseInt(attributes.getNamedItem("aria-valuemax").value, 10),
          EXTRA_PROPS.totalNumberOfScreens
        );
        assert.equal(
          parseInt(attributes.getNamedItem("aria-valuenow").value, 10),
          EXTRA_PROPS.order + 1
        );
      });

      it("should render progress bar", () => {
        let SCREEN_PROPS = {
          step: 1,
          previousStep: 0,
          totalNumberOfScreens: 2,
        };
        const wrapper = mount(<ProgressBar {...SCREEN_PROPS} />);
        assert.ok(wrapper.find("div.indicator").exists());
        assert.propertyVal(
          wrapper.find("div.indicator").prop("style"),
          "--progress-bar-progress",
          "50%"
        );
      });

      it("should have a primary, secondary and secondary.top button in the rendered input", () => {
        const wrapper = mount(<WelcomeScreen {...EASY_SETUP_SCREEN_PROPS} />);
        assert.ok(wrapper.find(".primary").exists());
        assert.ok(
          wrapper
            .find(".secondary-cta button.secondary[value='secondary_button']")
            .exists()
        );
        assert.ok(
          wrapper
            .find(
              ".secondary-cta.top button.secondary[value='secondary_button_top']"
            )
            .exists()
        );
      });
    });

    describe("theme screen", () => {
      const THEME_SCREEN_PROPS = {
        id: "test-theme-screen",
        totalNumberOfScreens: 1,
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
        sandbox.stub(AboutWelcomeUtils, "handleUserAction").resolves();
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
      it("should handle dismiss", () => {
        SCREEN_PROPS.content.dismiss_button = {
          action: { dismiss: true },
        };
        const finishStub = sandbox.stub(global, "AWFinish");
        const wrapper = mount(<WelcomeScreen {...SCREEN_PROPS} />);

        wrapper.find(".dismiss-button").simulate("click");

        assert.calledOnce(finishStub);
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
              utm_term: "you_tee_emm-screen",
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
      it("should handle SHOW_MIGRATION_WIZARD INSIDE MULTI_ACTION", async () => {
        const migrationCloseStub = sandbox.stub(
          global,
          "AWWaitForMigrationClose"
        );
        const MULTI_ACTION_SCREEN_PROPS = {
          content: {
            title: "test title",
            subtitle: "test subtitle",
            primary_button: {
              action: {
                type: "MULTI_ACTION",
                navigate: true,
                data: {
                  actions: [
                    {
                      type: "PIN_FIREFOX_TO_TASKBAR",
                    },
                    {
                      type: "SET_DEFAULT_BROWSER",
                    },
                    {
                      type: "SHOW_MIGRATION_WIZARD",
                      data: {},
                    },
                  ],
                },
              },
              label: "test button",
            },
          },
          navigate: sandbox.stub(),
        };
        const wrapper = mount(<WelcomeScreen {...MULTI_ACTION_SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");
        assert.calledWith(AboutWelcomeUtils.handleUserAction, {
          type: "MULTI_ACTION",
          navigate: true,
          data: {
            actions: [
              {
                type: "PIN_FIREFOX_TO_TASKBAR",
              },
              {
                type: "SET_DEFAULT_BROWSER",
              },
              {
                type: "SHOW_MIGRATION_WIZARD",
                data: {},
              },
            ],
          },
        });
        // handleUserAction returns a Promise, so let's let the microtask queue
        // flush so that anything waiting for the handleUserAction Promise to
        // resolve can run.
        await new Promise(resolve => queueMicrotask(resolve));
        assert.calledOnce(migrationCloseStub);
      });

      it("should handle SHOW_MIGRATION_WIZARD INSIDE NESTED MULTI_ACTION", async () => {
        const migrationCloseStub = sandbox.stub(
          global,
          "AWWaitForMigrationClose"
        );
        const MULTI_ACTION_SCREEN_PROPS = {
          content: {
            title: "test title",
            subtitle: "test subtitle",
            primary_button: {
              action: {
                type: "MULTI_ACTION",
                navigate: true,
                data: {
                  actions: [
                    {
                      type: "PIN_FIREFOX_TO_TASKBAR",
                    },
                    {
                      type: "SET_DEFAULT_BROWSER",
                    },
                    {
                      type: "MULTI_ACTION",
                      data: {
                        actions: [
                          {
                            type: "SET_PREF",
                          },
                          {
                            type: "SHOW_MIGRATION_WIZARD",
                            data: {},
                          },
                        ],
                      },
                    },
                  ],
                },
              },
              label: "test button",
            },
          },
          navigate: sandbox.stub(),
        };
        const wrapper = mount(<WelcomeScreen {...MULTI_ACTION_SCREEN_PROPS} />);

        wrapper.find(".primary").simulate("click");
        assert.calledWith(AboutWelcomeUtils.handleUserAction, {
          type: "MULTI_ACTION",
          navigate: true,
          data: {
            actions: [
              {
                type: "PIN_FIREFOX_TO_TASKBAR",
              },
              {
                type: "SET_DEFAULT_BROWSER",
              },
              {
                type: "MULTI_ACTION",
                data: {
                  actions: [
                    {
                      type: "SET_PREF",
                    },
                    {
                      type: "SHOW_MIGRATION_WIZARD",
                      data: {},
                    },
                  ],
                },
              },
            ],
          },
        });
        // handleUserAction returns a Promise, so let's let the microtask queue
        // flush so that anything waiting for the handleUserAction Promise to
        // resolve can run.
        await new Promise(resolve => queueMicrotask(resolve));
        assert.calledOnce(migrationCloseStub);
      });
      it("should unset prefs from unchecked checkboxes", () => {
        const PREF_SCREEN_PROPS = {
          content: {
            title: "Checkboxes",
            tiles: {
              type: "multiselect",
              data: [
                {
                  id: "checkbox-1",
                  label: "checkbox 1",
                  checkedAction: {
                    type: "SET_PREF",
                    data: {
                      pref: {
                        name: "pref-a",
                        value: true,
                      },
                    },
                  },
                  uncheckedAction: {
                    type: "SET_PREF",
                    data: {
                      pref: {
                        name: "pref-a",
                      },
                    },
                  },
                },
                {
                  id: "checkbox-2",
                  label: "checkbox 2",
                  checkedAction: {
                    type: "MULTI_ACTION",
                    data: {
                      actions: [
                        {
                          type: "SET_PREF",
                          data: {
                            pref: {
                              name: "pref-b",
                              value: "pref-b",
                            },
                          },
                        },
                        {
                          type: "SET_PREF",
                          data: {
                            pref: {
                              name: "pref-c",
                              value: 3,
                            },
                          },
                        },
                      ],
                    },
                  },
                  uncheckedAction: {
                    type: "SET_PREF",
                    data: {
                      pref: { name: "pref-b" },
                    },
                  },
                },
              ],
            },
            primary_button: {
              label: "Set Prefs",
              action: {
                type: "MULTI_ACTION",
                collectSelect: true,
                isDynamic: true,
                navigate: true,
                data: {
                  actions: [],
                },
              },
            },
          },
          navigate: sandbox.stub(),
          setActiveMultiSelect: sandbox.stub(),
        };

        // No checkboxes checked. All prefs will be unset and pref-c will not be
        // reset.
        {
          const wrapper = mount(
            <WelcomeScreen {...PREF_SCREEN_PROPS} activeMultiSelect={[]} />
          );
          wrapper.find(".primary").simulate("click");
          assert.calledWith(AboutWelcomeUtils.handleUserAction, {
            type: "MULTI_ACTION",
            collectSelect: true,
            isDynamic: true,
            navigate: true,
            data: {
              actions: [
                { type: "SET_PREF", data: { pref: { name: "pref-a" } } },
                { type: "SET_PREF", data: { pref: { name: "pref-b" } } },
              ],
            },
          });

          AboutWelcomeUtils.handleUserAction.resetHistory();
        }

        // The first checkbox is checked. Only pref-a will be set and pref-c
        // will not be reset.
        {
          const wrapper = mount(
            <WelcomeScreen
              {...PREF_SCREEN_PROPS}
              activeMultiSelect={["checkbox-1"]}
            />
          );
          wrapper.find(".primary").simulate("click");
          assert.calledWith(AboutWelcomeUtils.handleUserAction, {
            type: "MULTI_ACTION",
            collectSelect: true,
            isDynamic: true,
            navigate: true,
            data: {
              actions: [
                {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: "pref-a",
                      value: true,
                    },
                  },
                },
                { type: "SET_PREF", data: { pref: { name: "pref-b" } } },
              ],
            },
          });

          AboutWelcomeUtils.handleUserAction.resetHistory();
        }

        // The second checkbox is checked. Prefs pref-b and pref-c will be set.
        {
          const wrapper = mount(
            <WelcomeScreen
              {...PREF_SCREEN_PROPS}
              activeMultiSelect={["checkbox-2"]}
            />
          );
          wrapper.find(".primary").simulate("click");
          assert.calledWith(AboutWelcomeUtils.handleUserAction, {
            type: "MULTI_ACTION",
            collectSelect: true,
            isDynamic: true,
            navigate: true,
            data: {
              actions: [
                { type: "SET_PREF", data: { pref: { name: "pref-a" } } },
                {
                  type: "MULTI_ACTION",
                  data: {
                    actions: [
                      {
                        type: "SET_PREF",
                        data: { pref: { name: "pref-b", value: "pref-b" } },
                      },
                      {
                        type: "SET_PREF",
                        data: { pref: { name: "pref-c", value: 3 } },
                      },
                    ],
                  },
                },
              ],
            },
          });

          AboutWelcomeUtils.handleUserAction.resetHistory();
        }

        // // Both checkboxes are checked. All prefs will be set.
        {
          const wrapper = mount(
            <WelcomeScreen
              {...PREF_SCREEN_PROPS}
              activeMultiSelect={["checkbox-1", "checkbox-2"]}
            />
          );
          wrapper.find(".primary").simulate("click");
          assert.calledWith(AboutWelcomeUtils.handleUserAction, {
            type: "MULTI_ACTION",
            collectSelect: true,
            isDynamic: true,
            navigate: true,
            data: {
              actions: [
                {
                  type: "SET_PREF",
                  data: { pref: { name: "pref-a", value: true } },
                },
                {
                  type: "MULTI_ACTION",
                  data: {
                    actions: [
                      {
                        type: "SET_PREF",
                        data: { pref: { name: "pref-b", value: "pref-b" } },
                      },
                      {
                        type: "SET_PREF",
                        data: { pref: { name: "pref-c", value: 3 } },
                      },
                    ],
                  },
                },
              ],
            },
          });

          AboutWelcomeUtils.handleUserAction.resetHistory();
        }
      });
    });
  });
});

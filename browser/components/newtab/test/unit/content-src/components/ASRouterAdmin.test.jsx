import {
  actionCreators as ac,
  actionTypes as at,
} from "common/Actions.sys.mjs";
import {
  ASRouterAdminInner,
  CollapseToggle,
  DiscoveryStreamAdmin,
  Personalization,
  ToggleStoryButton,
} from "content-src/components/ASRouterAdmin/ASRouterAdmin";
import { ASRouterUtils } from "content-src/asrouter/asrouter-utils";
import { GlobalOverrider } from "test/unit/utils";
import React from "react";
import { shallow } from "enzyme";

describe("ASRouterAdmin", () => {
  let globalOverrider;
  let sandbox;
  let wrapper;
  let globals;
  let FAKE_PROVIDER_PREF = [
    {
      enabled: true,
      id: "snippets_local_testing",
      localProvider: "SnippetsProvider",
      type: "local",
    },
  ];
  let FAKE_PROVIDER = [
    {
      enabled: true,
      id: "snippets_local_testing",
      localProvider: "SnippetsProvider",
      messages: [],
      type: "local",
    },
  ];
  beforeEach(() => {
    globalOverrider = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    sandbox.stub(ASRouterUtils, "getPreviewEndpoint").returns("foo");
    globals = {
      ASRouterMessage: sandbox.stub().resolves(),
      ASRouterAddParentListener: sandbox.stub(),
      ASRouterRemoveParentListener: sandbox.stub(),
    };
    globalOverrider.set(globals);
    wrapper = shallow(
      <ASRouterAdminInner collapsed={false} location={{ routes: [""] }} />
    );
  });
  afterEach(() => {
    sandbox.restore();
    globalOverrider.restore();
  });
  it("should render ASRouterAdmin component", () => {
    assert.ok(wrapper.exists());
  });
  it("should send ADMIN_CONNECT_STATE on mount", () => {
    assert.calledOnce(globals.ASRouterMessage);
    assert.calledWith(globals.ASRouterMessage, {
      type: "ADMIN_CONNECT_STATE",
      data: { endpoint: "foo" },
    });
  });
  it("should set a .collapsed class on the outer div if props.collapsed is true", () => {
    wrapper.setProps({ collapsed: true });
    assert.isTrue(wrapper.find(".asrouter-admin").hasClass("collapsed"));
  });
  it("should set a .expanded class on the outer div if props.collapsed is false", () => {
    wrapper.setProps({ collapsed: false });
    assert.isTrue(wrapper.find(".asrouter-admin").hasClass("expanded"));
    assert.isFalse(wrapper.find(".asrouter-admin").hasClass("collapsed"));
  });
  describe("#getSection", () => {
    it("should render a message provider section by default", () => {
      assert.equal(wrapper.find("h2").at(1).text(), "Messages");
    });
    it("should render a targeting section for targeting route", () => {
      wrapper = shallow(
        <ASRouterAdminInner location={{ routes: ["targeting"] }} />
      );
      assert.equal(wrapper.find("h2").at(0).text(), "Targeting Utilities");
    });
    it("should render a DS section for DS route", () => {
      wrapper = shallow(
        <ASRouterAdminInner
          location={{ routes: ["ds"] }}
          Sections={[]}
          Prefs={{}}
        />
      );
      assert.equal(wrapper.find("h2").at(0).text(), "Discovery Stream");
    });
    it("should render two error messages", () => {
      wrapper = shallow(
        <ASRouterAdminInner location={{ routes: ["errors"] }} Sections={[]} />
      );
      const firstError = {
        timestamp: Date.now() + 100,
        error: { message: "first" },
      };
      const secondError = {
        timestamp: Date.now(),
        error: { message: "second" },
      };
      wrapper.setState({
        providers: [{ id: "foo", errors: [firstError, secondError] }],
      });

      assert.equal(
        wrapper.find("tbody tr").at(0).find("td").at(0).text(),
        "foo"
      );
      assert.lengthOf(wrapper.find("tbody tr"), 2);
      assert.equal(
        wrapper.find("tbody tr").at(0).find("td").at(1).text(),
        secondError.error.message
      );
    });
  });
  describe("#render", () => {
    beforeEach(() => {
      wrapper.setState({
        providerPrefs: [],
        providers: [],
        userPrefs: {},
      });
    });
    describe("#renderProviders", () => {
      it("should render the provider", () => {
        wrapper.setState({
          providerPrefs: FAKE_PROVIDER_PREF,
          providers: FAKE_PROVIDER,
        });

        // Header + 1 item
        assert.lengthOf(wrapper.find(".message-item"), 2);
      });
    });
    describe("#renderMessages", () => {
      beforeEach(() => {
        sandbox.stub(ASRouterUtils, "blockById").resolves();
        sandbox.stub(ASRouterUtils, "unblockById").resolves();
        sandbox.stub(ASRouterUtils, "overrideMessage").resolves({ foo: "bar" });
        sandbox.stub(ASRouterUtils, "sendMessage").resolves();
        wrapper.setState({
          messageFilter: "all",
          messageBlockList: [],
          messageImpressions: { foo: 2 },
          groups: [{ id: "messageProvider", enabled: true }],
          providers: [{ id: "messageProvider", enabled: true }],
        });
      });
      it("should render a message when no filtering is applied", () => {
        wrapper.setState({
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
        wrapper.find(".message-item button.primary").simulate("click");
        assert.calledOnce(ASRouterUtils.blockById);
        assert.calledWith(ASRouterUtils.blockById, "foo");
      });
      it("should render a blocked message", () => {
        wrapper.setState({
          messages: [
            {
              id: "foo",
              groups: ["messageProvider"],
              provider: "messageProvider",
            },
          ],
          messageBlockList: ["foo"],
        });
        assert.lengthOf(wrapper.find(".message-item.blocked"), 1);
        wrapper.find(".message-item.blocked button").simulate("click");
        assert.calledOnce(ASRouterUtils.unblockById);
        assert.calledWith(ASRouterUtils.unblockById, "foo");
      });
      it("should render a message if provider matches filter", () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
      });
      it("should override with the selected message", async () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
        wrapper.find(".message-item button.show").simulate("click");
        assert.calledOnce(ASRouterUtils.overrideMessage);
        assert.calledWith(ASRouterUtils.overrideMessage, "foo");
        await ASRouterUtils.overrideMessage();
        assert.equal(wrapper.state().foo, "bar");
      });
      it("should hide message if provider filter changes", () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);

        wrapper.find("select").simulate("change", { target: { value: "bar" } });

        assert.lengthOf(wrapper.find(".message-id"), 0);
      });
      it("should not display Reset All button if provider filter value is set to all or test providers", () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".messages-reset"), 1);
        wrapper.find("select").simulate("change", { target: { value: "all" } });

        assert.lengthOf(wrapper.find(".messages-reset"), 0);

        wrapper
          .find("select")
          .simulate("change", { target: { value: "test_local_testing" } });
        assert.lengthOf(wrapper.find(".messages-reset"), 0);
      });
      it("should trigger disable and enable provider on Reset All button click", () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
          providerPrefs: [
            {
              id: "messageProvider",
            },
          ],
        });
        wrapper.find(".messages-reset").simulate("click");
        assert.calledTwice(ASRouterUtils.sendMessage);
        assert.calledWith(ASRouterUtils.sendMessage, {
          type: "DISABLE_PROVIDER",
          data: "messageProvider",
        });
        assert.calledWith(ASRouterUtils.sendMessage, {
          type: "ENABLE_PROVIDER",
          data: "messageProvider",
        });
      });
    });
  });
  describe("#DiscoveryStream", () => {
    let state = {};
    let dispatch;
    beforeEach(() => {
      dispatch = sandbox.stub();
      state = {
        config: {
          enabled: true,
        },
        layout: [],
        spocs: {
          frequency_caps: [],
        },
        feeds: {
          data: {},
        },
      };
      wrapper = shallow(
        <DiscoveryStreamAdmin
          dispatch={dispatch}
          otherPrefs={{}}
          state={{
            DiscoveryStream: state,
          }}
        />
      );
    });
    it("should render a DiscoveryStreamAdmin component", () => {
      assert.equal(wrapper.find("h3").at(0).text(), "Layout");
    });
    it("should render a spoc in DiscoveryStreamAdmin component", () => {
      state.spocs = {
        frequency_caps: [],
        data: {
          spocs: {
            items: [
              {
                id: 12345,
              },
            ],
          },
        },
      };
      wrapper = shallow(
        <DiscoveryStreamAdmin
          otherPrefs={{}}
          state={{ DiscoveryStream: state }}
        />
      );
      wrapper.instance().onStoryToggle({ id: 12345 });
      const messageSummary = wrapper.find(".message-summary").at(0);
      const pre = messageSummary.find("pre").at(0);
      const spocText = pre.text();
      assert.equal(spocText, '{\n  "id": 12345\n}');
    });
    it("should fire restorePrefDefaults with DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS", () => {
      wrapper.find("button").at(0).simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS,
        })
      );
    });
    it("should fire config change with DISCOVERY_STREAM_CONFIG_CHANGE", () => {
      wrapper.find("button").at(1).simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_CONFIG_CHANGE,
          data: { enabled: true },
        })
      );
    });
    it("should fire expireCache with DISCOVERY_STREAM_DEV_EXPIRE_CACHE", () => {
      wrapper.find("button").at(2).simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_DEV_EXPIRE_CACHE,
        })
      );
    });
    it("should fire systemTick with DISCOVERY_STREAM_DEV_SYSTEM_TICK", () => {
      wrapper.find("button").at(3).simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_DEV_SYSTEM_TICK,
        })
      );
    });
    it("should fire idleDaily with DISCOVERY_STREAM_DEV_IDLE_DAILY", () => {
      wrapper.find("button").at(4).simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_DEV_IDLE_DAILY,
        })
      );
    });
    it("should fire syncRemoteSettings with DISCOVERY_STREAM_DEV_SYNC_RS", () => {
      wrapper.find("button").at(5).simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_DEV_SYNC_RS,
        })
      );
    });
    it("should fire setConfigValue with DISCOVERY_STREAM_CONFIG_SET_VALUE", () => {
      const name = "name";
      const value = "value";
      wrapper.instance().setConfigValue(name, value);
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_CONFIG_SET_VALUE,
          data: { name, value },
        })
      );
    });
  });

  describe("#Personalization", () => {
    let dispatch;
    beforeEach(() => {
      dispatch = sandbox.stub();
      wrapper = shallow(
        <Personalization
          dispatch={dispatch}
          state={{
            Personalization: {
              lastUpdated: 1000,
              initialized: true,
            },
          }}
        />
      );
    });
    it("should render with pref checkbox, lastUpdated, and initialized", () => {
      assert.lengthOf(wrapper.find("TogglePrefCheckbox"), 1);
      assert.equal(
        wrapper.find("td").at(1).text(),
        "Personalization Last Updated"
      );
      assert.equal(
        wrapper.find("td").at(2).text(),
        new Date(1000).toLocaleString()
      );
      assert.equal(
        wrapper.find("td").at(3).text(),
        "Personalization Initialized"
      );
      assert.equal(wrapper.find("td").at(4).text(), "true");
    });
    it("should render with no data with no last updated", () => {
      wrapper = shallow(
        <Personalization
          dispatch={dispatch}
          state={{
            Personalization: {
              version: 2,
              lastUpdated: 0,
              initialized: true,
            },
          }}
        />
      );
      assert.equal(wrapper.find("td").at(2).text(), "(no data)");
    });
    it("should dispatch DISCOVERY_STREAM_PERSONALIZATION_TOGGLE", () => {
      wrapper.instance().togglePersonalization();
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_PERSONALIZATION_TOGGLE,
        })
      );
    });
  });

  describe("#ToggleStoryButton", () => {
    it("should fire onClick in toggle button", async () => {
      let result = "";
      function onClick(spoc) {
        result = spoc;
      }

      wrapper = shallow(<ToggleStoryButton story="spoc" onClick={onClick} />);
      wrapper.find("button").simulate("click");

      assert.equal(result, "spoc");
    });
  });
});

describe("CollapseToggle", () => {
  let wrapper;
  beforeEach(() => {
    wrapper = shallow(<CollapseToggle location={{ routes: [""] }} />);
  });

  describe("rendering inner content", () => {
    it("should not render ASRouterAdminInner for about:newtab (no hash)", () => {
      wrapper.setProps({ location: { hash: "", routes: [""] } });
      assert.lengthOf(wrapper.find(ASRouterAdminInner), 0);
    });

    it("should render ASRouterAdminInner for about:newtab#asrouter and subroutes", () => {
      wrapper.setProps({ location: { hash: "#asrouter", routes: [""] } });
      assert.lengthOf(wrapper.find(ASRouterAdminInner), 1);

      wrapper.setProps({ location: { hash: "#asrouter-foo", routes: [""] } });
      assert.lengthOf(wrapper.find(ASRouterAdminInner), 1);
    });

    it("should render ASRouterAdminInner for about:newtab#devtools and subroutes", () => {
      wrapper.setProps({ location: { hash: "#devtools", routes: [""] } });
      assert.lengthOf(wrapper.find(ASRouterAdminInner), 1);

      wrapper.setProps({ location: { hash: "#devtools-foo", routes: [""] } });
      assert.lengthOf(wrapper.find(ASRouterAdminInner), 1);
    });
  });
});

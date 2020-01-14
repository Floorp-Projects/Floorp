import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import {
  ASRouterAdminInner,
  CollapseToggle,
  DiscoveryStreamAdmin,
  Personalization,
  ToggleStoryButton,
} from "content-src/components/ASRouterAdmin/ASRouterAdmin";
import { GlobalOverrider } from "test/unit/utils";
import React from "react";
import { shallow } from "enzyme";

describe("ASRouterAdmin", () => {
  let globals;
  let sandbox;
  let sendMessageStub;
  let addListenerStub;
  let removeListenerStub;
  let wrapper;
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
    globals = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    sendMessageStub = sandbox.stub();
    addListenerStub = sandbox.stub();
    removeListenerStub = sandbox.stub();

    globals.set("RPMSendAsyncMessage", sendMessageStub);
    globals.set("RPMAddMessageListener", addListenerStub);
    globals.set("RPMRemoveMessageListener", removeListenerStub);

    wrapper = shallow(
      <ASRouterAdminInner collapsed={false} location={{ routes: [""] }} />
    );
  });
  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });
  it("should render ASRouterAdmin component", () => {
    assert.ok(wrapper.exists());
  });
  it("should send ADMIN_CONNECT_STATE on mount", () => {
    assert.calledOnce(sendMessageStub);
    assert.propertyVal(
      sendMessageStub.firstCall.args[1],
      "type",
      "ADMIN_CONNECT_STATE"
    );
  });
  it("should set a listener on mount", () => {
    assert.calledOnce(addListenerStub);
    assert.calledWithExactly(
      addListenerStub,
      sinon.match.string,
      wrapper.instance().onMessage
    );
  });
  it("should remove listener on unmount", () => {
    wrapper.unmount();
    assert.calledOnce(removeListenerStub);
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
      assert.equal(
        wrapper
          .find("h2")
          .at(2)
          .text(),
        "Messages"
      );
    });
    it("should render a targeting section for targeting route", () => {
      wrapper = shallow(
        <ASRouterAdminInner location={{ routes: ["targeting"] }} />
      );
      assert.equal(
        wrapper
          .find("h2")
          .at(0)
          .text(),
        "Targeting Utilities"
      );
    });
    it("should render a pocket section for pocket route", () => {
      wrapper = shallow(
        <ASRouterAdminInner location={{ routes: ["pocket"] }} Sections={[]} />
      );
      assert.equal(
        wrapper
          .find("h2")
          .at(0)
          .text(),
        "Pocket"
      );
    });
    it("should render a DS section for DS route", () => {
      wrapper = shallow(
        <ASRouterAdminInner
          location={{ routes: ["ds"] }}
          Sections={[]}
          Prefs={{}}
        />
      );
      assert.equal(
        wrapper
          .find("h2")
          .at(0)
          .text(),
        "Discovery Stream"
      );
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
        wrapper
          .find("tbody tr")
          .at(0)
          .find("td")
          .at(0)
          .text(),
        "foo"
      );
      assert.lengthOf(wrapper.find("tbody tr"), 2);
      assert.equal(
        wrapper
          .find("tbody tr")
          .at(0)
          .find("td")
          .at(1)
          .text(),
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
        // first call is ADMIN_CONNECT_STATE
        assert.propertyVal(
          sendMessageStub.secondCall.args[1],
          "type",
          "BLOCK_MESSAGE_BY_ID"
        );
        assert.propertyVal(
          sendMessageStub.secondCall.args[1].data,
          "id",
          "foo"
        );
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
        // first call is ADMIN_CONNECT_STATE
        assert.propertyVal(
          sendMessageStub.secondCall.args[1],
          "type",
          "UNBLOCK_MESSAGE_BY_ID"
        );
        assert.propertyVal(
          sendMessageStub.secondCall.args[1].data,
          "id",
          "foo"
        );
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
      it("should override with the selected message", () => {
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
        wrapper.find(".message-item button:not(.primary)").simulate("click");
        // first call is ADMIN_CONNECT_STATE
        assert.propertyVal(
          sendMessageStub.secondCall.args[1],
          "type",
          "OVERRIDE_MESSAGE"
        );
        assert.propertyVal(
          sendMessageStub.secondCall.args[1].data,
          "id",
          "foo"
        );
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
          layout_endpoint: "",
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
      assert.equal(
        wrapper
          .find("h3")
          .at(0)
          .text(),
        "Endpoint variant"
      );
    });
    it("should render a spoc in DiscoveryStreamAdmin component", () => {
      state.spocs = {
        frequency_caps: [],
        data: {
          spocs: [
            {
              id: 12345,
            },
          ],
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
      wrapper
        .find("button")
        .at(0)
        .simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS,
        })
      );
    });
    it("should fire config change with DISCOVERY_STREAM_CONFIG_CHANGE", () => {
      wrapper
        .find("button")
        .at(1)
        .simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_CONFIG_CHANGE,
          data: { enabled: true, layout_endpoint: "" },
        })
      );
    });
    it("should fire expireCache with DISCOVERY_STREAM_DEV_EXPIRE_CACHE", () => {
      wrapper
        .find("button")
        .at(2)
        .simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_DEV_EXPIRE_CACHE,
        })
      );
    });
    it("should fire systemTick with DISCOVERY_STREAM_DEV_SYSTEM_TICK", () => {
      wrapper
        .find("button")
        .at(3)
        .simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_DEV_SYSTEM_TICK,
        })
      );
    });
    it("should fire idleDaily with DISCOVERY_STREAM_DEV_IDLE_DAILY", () => {
      wrapper
        .find("button")
        .at(4)
        .simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_DEV_IDLE_DAILY,
        })
      );
    });
    it("should fire syncRemoteSettings with DISCOVERY_STREAM_DEV_SYNC_RS", () => {
      wrapper
        .find("button")
        .at(5)
        .simulate("click");
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
              version: 1,
              lastUpdated: 1000,
              initialized: true,
            },
          }}
        />
      );
    });
    it("should render with buttons, version, and lastUpdated", () => {
      assert.equal(
        wrapper
          .find("button")
          .at(0)
          .text(),
        "Enable V2 Personalization"
      );
      assert.equal(
        wrapper
          .find("td")
          .at(1)
          .text(),
        "1"
      );
      assert.equal(
        wrapper
          .find("td")
          .at(3)
          .text(),
        "12/31/1969, 7:00:01 PM"
      );
    });
    it("should render with proper version 2 state", () => {
      wrapper = shallow(
        <Personalization
          dispatch={dispatch}
          state={{
            Personalization: {
              version: 2,
              lastUpdated: 1000,
              initialized: true,
            },
          }}
        />
      );
      assert.equal(
        wrapper
          .find("button")
          .at(0)
          .text(),
        "Enable V1 Personalization"
      );
      assert.equal(
        wrapper
          .find("td")
          .at(1)
          .text(),
        "2"
      );
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
      assert.equal(
        wrapper
          .find("td")
          .at(3)
          .text(),
        "(no data)"
      );
    });
    it("should fire DISCOVERY_STREAM_PERSONALIZATION_VERSION_TOGGLE with version", () => {
      wrapper
        .find("button")
        .at(0)
        .simulate("click");
      assert.calledWith(
        dispatch,
        ac.OnlyToMain({
          type: at.DISCOVERY_STREAM_PERSONALIZATION_VERSION_TOGGLE,
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

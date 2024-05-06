import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import {
  DiscoveryStreamAdminInner,
  CollapseToggle,
  DiscoveryStreamAdminUI,
  Personalization,
  ToggleStoryButton,
} from "content-src/components/DiscoveryStreamAdmin/DiscoveryStreamAdmin";
import React from "react";
import { shallow } from "enzyme";

describe("DiscoveryStreamAdmin", () => {
  let sandbox;
  let wrapper;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    wrapper = shallow(
      <DiscoveryStreamAdminInner
        collapsed={false}
        location={{ routes: [""] }}
        Prefs={{}}
      />
    );
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should render DiscoveryStreamAdmin component", () => {
    assert.ok(wrapper.exists());
  });
  it("should set a .collapsed class on the outer div if props.collapsed is true", () => {
    wrapper.setProps({ collapsed: true });
    assert.isTrue(wrapper.find(".discoverystream-admin").hasClass("collapsed"));
  });
  it("should set a .expanded class on the outer div if props.collapsed is false", () => {
    wrapper.setProps({ collapsed: false });
    assert.isTrue(wrapper.find(".discoverystream-admin").hasClass("expanded"));
    assert.isFalse(
      wrapper.find(".discoverystream-admin").hasClass("collapsed")
    );
  });
  it("should render a DS section", () => {
    assert.equal(wrapper.find("h1").at(0).text(), "Discovery Stream Admin");
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
        <DiscoveryStreamAdminUI
          dispatch={dispatch}
          otherPrefs={{}}
          state={{
            DiscoveryStream: state,
            Weather: {
              suggestions: [],
            },
          }}
        />
      );
    });
    it("should render a DiscoveryStreamAdminUI component", () => {
      assert.equal(wrapper.find("h3").at(0).text(), "Layout");
    });
    it("should render a spoc in DiscoveryStreamAdminUI component", () => {
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
        <DiscoveryStreamAdminUI
          otherPrefs={{}}
          state={{
            DiscoveryStream: state,
            Weather: {
              suggestions: [],
            },
          }}
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
    it("should not render DiscoveryStreamAdminInner for about:newtab (no hash)", () => {
      wrapper.setProps({ location: { hash: "", routes: [""] } });
      assert.lengthOf(wrapper.find(DiscoveryStreamAdminInner), 0);
    });

    it("should render DiscoveryStreamAdminInner for about:newtab#devtools and subroutes", () => {
      wrapper.setProps({ location: { hash: "#devtools", routes: [""] } });
      assert.lengthOf(wrapper.find(DiscoveryStreamAdminInner), 1);

      wrapper.setProps({ location: { hash: "#devtools-foo", routes: [""] } });
      assert.lengthOf(wrapper.find(DiscoveryStreamAdminInner), 1);
    });
  });
});

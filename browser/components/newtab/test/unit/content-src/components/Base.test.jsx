import {
  _Base as Base,
  BaseContent,
  PrefsButton,
} from "content-src/components/Base/Base";
import { DiscoveryStreamAdmin } from "content-src/components/DiscoveryStreamAdmin/DiscoveryStreamAdmin";
import { ErrorBoundary } from "content-src/components/ErrorBoundary/ErrorBoundary";
import React from "react";
import { Search } from "content-src/components/Search/Search";
import { shallow } from "enzyme";
import { actionCreators as ac } from "common/Actions.mjs";

describe("<Base>", () => {
  let DEFAULT_PROPS = {
    store: { getState: () => {} },
    App: { initialized: true },
    Prefs: { values: {} },
    Sections: [],
    DiscoveryStream: { config: { enabled: false } },
    dispatch: () => {},
    adminContent: {
      message: {},
    },
    document: {
      visibilityState: "visible",
      addEventListener: sinon.stub(),
      removeEventListener: sinon.stub(),
    },
  };

  it("should render Base component", () => {
    const wrapper = shallow(<Base {...DEFAULT_PROPS} />);
    assert.ok(wrapper.exists());
  });

  it("should render the BaseContent component, passing through all props", () => {
    const wrapper = shallow(<Base {...DEFAULT_PROPS} />);
    const props = wrapper.find(BaseContent).props();
    assert.deepEqual(
      props,
      DEFAULT_PROPS,
      JSON.stringify([props, DEFAULT_PROPS], null, 3)
    );
  });

  it("should render an ErrorBoundary with class base-content-fallback", () => {
    const wrapper = shallow(<Base {...DEFAULT_PROPS} />);

    assert.equal(
      wrapper.find(ErrorBoundary).first().prop("className"),
      "base-content-fallback"
    );
  });

  it("should render an DiscoveryStreamAdmin if the devtools pref is true", () => {
    const wrapper = shallow(
      <Base
        {...DEFAULT_PROPS}
        Prefs={{ values: { "asrouter.devtoolsEnabled": true } }}
      />
    );
    assert.lengthOf(wrapper.find(DiscoveryStreamAdmin), 1);
  });

  it("should not render an DiscoveryStreamAdmin if the devtools pref is false", () => {
    const wrapper = shallow(
      <Base
        {...DEFAULT_PROPS}
        Prefs={{ values: { "asrouter.devtoolsEnabled": false } }}
      />
    );
    assert.lengthOf(wrapper.find(DiscoveryStreamAdmin), 0);
  });
});

describe("<BaseContent>", () => {
  let DEFAULT_PROPS = {
    store: { getState: () => {} },
    App: { initialized: true },
    Prefs: { values: {} },
    Sections: [],
    DiscoveryStream: { config: { enabled: false } },
    dispatch: () => {},
    document: {
      visibilityState: "visible",
      addEventListener: sinon.stub(),
      removeEventListener: sinon.stub(),
    },
  };

  it("should render an ErrorBoundary with a Search child", () => {
    const searchEnabledProps = Object.assign({}, DEFAULT_PROPS, {
      Prefs: { values: { showSearch: true } },
    });

    const wrapper = shallow(<BaseContent {...searchEnabledProps} />);

    assert.isTrue(wrapper.find(Search).parent().is(ErrorBoundary));
  });

  it("should dispatch a user event when the customize menu is opened or closed", () => {
    const dispatch = sinon.stub();
    const wrapper = shallow(
      <BaseContent
        {...DEFAULT_PROPS}
        dispatch={dispatch}
        App={{ customizeMenuVisible: true }}
      />
    );
    wrapper.instance().openCustomizationMenu();
    assert.calledWith(dispatch, { type: "SHOW_PERSONALIZE" });
    assert.calledWith(dispatch, ac.UserEvent({ event: "SHOW_PERSONALIZE" }));
    wrapper.instance().closeCustomizationMenu();
    assert.calledWith(dispatch, { type: "HIDE_PERSONALIZE" });
    assert.calledWith(dispatch, ac.UserEvent({ event: "HIDE_PERSONALIZE" }));
  });

  it("should render only search if no Sections are enabled", () => {
    const onlySearchProps = Object.assign({}, DEFAULT_PROPS, {
      Sections: [{ id: "highlights", enabled: false }],
      Prefs: { values: { showSearch: true } },
    });

    const wrapper = shallow(<BaseContent {...onlySearchProps} />);
    assert.lengthOf(wrapper.find(".only-search"), 1);
  });

  it("should update firstVisibleTimestamp if it is visible immediately with no event listener", () => {
    const props = Object.assign({}, DEFAULT_PROPS, {
      document: {
        visibilityState: "visible",
        addEventListener: sinon.spy(),
        removeEventListener: sinon.spy(),
      },
    });

    const wrapper = shallow(<BaseContent {...props} />);
    assert.notCalled(props.document.addEventListener);
    assert.isDefined(wrapper.state("firstVisibleTimestamp"));
  });
  it("should attach an event listener for visibility change if it is not visible", () => {
    const props = Object.assign({}, DEFAULT_PROPS, {
      document: {
        visibilityState: "hidden",
        addEventListener: sinon.spy(),
        removeEventListener: sinon.spy(),
      },
    });

    const wrapper = shallow(<BaseContent {...props} />);
    assert.calledWith(props.document.addEventListener, "visibilitychange");
    assert.notExists(wrapper.state("firstVisibleTimestamp"));
  });
  it("should remove the event listener for visibility change when unmounted", () => {
    const props = Object.assign({}, DEFAULT_PROPS, {
      document: {
        visibilityState: "hidden",
        addEventListener: sinon.spy(),
        removeEventListener: sinon.spy(),
      },
    });

    const wrapper = shallow(<BaseContent {...props} />);
    const [, listener] = props.document.addEventListener.firstCall.args;

    wrapper.unmount();
    assert.calledWith(
      props.document.removeEventListener,
      "visibilitychange",
      listener
    );
  });
  it("should remove the event listener for visibility change after becoming visible", () => {
    const listeners = new Set();
    const props = Object.assign({}, DEFAULT_PROPS, {
      document: {
        visibilityState: "hidden",
        addEventListener: (ev, cb) => listeners.add(cb),
        removeEventListener: (ev, cb) => listeners.delete(cb),
      },
    });

    const wrapper = shallow(<BaseContent {...props} />);
    assert.equal(listeners.size, 1);
    assert.notExists(wrapper.state("firstVisibleTimestamp"));

    // Simulate listeners getting called
    props.document.visibilityState = "visible";
    listeners.forEach(l => l());

    assert.equal(listeners.size, 0);
    assert.isDefined(wrapper.state("firstVisibleTimestamp"));
  });
});

describe("<PrefsButton>", () => {
  it("should render icon-settings if props.icon is empty", () => {
    const wrapper = shallow(<PrefsButton icon="" />);

    assert.isTrue(wrapper.find("button").hasClass("icon-settings"));
  });
  it("should render props.icon as a className", () => {
    const wrapper = shallow(<PrefsButton icon="icon-happy" />);

    assert.isTrue(wrapper.find("button").hasClass("icon-happy"));
  });
});

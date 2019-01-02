import {GlobalOverrider, mountWithIntl, shallowWithIntl} from "test/unit/utils";
import React from "react";
import {_Search as Search} from "content-src/components/Search/Search";

const DEFAULT_PROPS = {dispatch() {}};

describe("<Search>", () => {
  let globals;
  let sandbox;
  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;

    global.ContentSearchUIController.prototype = {search: sandbox.spy()};
  });
  afterEach(() => {
    globals.restore();
  });

  it("should render a Search element", () => {
    const wrapper = shallowWithIntl(<Search {...DEFAULT_PROPS} />);
    assert.ok(wrapper.exists());
  });
  it("should not use a <form> element", () => {
    const wrapper = mountWithIntl(<Search {...DEFAULT_PROPS} />);

    assert.equal(wrapper.find("form").length, 0);
  });
  it("should listen for ContentSearchClient on render", () => {
    const spy = globals.set("addEventListener", sandbox.spy());

    const wrapper = mountWithIntl(<Search {...DEFAULT_PROPS} />);

    assert.calledOnce(spy.withArgs("ContentSearchClient", wrapper.instance()));
  });
  it("should stop listening for ContentSearchClient on unmount", () => {
    const spy = globals.set("removeEventListener", sandbox.spy());
    const wrapper = mountWithIntl(<Search {...DEFAULT_PROPS} />);
    // cache the instance as we can't call this method after unmount is called
    const instance = wrapper.instance();

    wrapper.unmount();

    assert.calledOnce(spy.withArgs("ContentSearchClient", instance));
  });
  it("should add gContentSearchController as a global", () => {
    // current about:home tests need gContentSearchController to exist as a global
    // so let's test it here too to ensure we don't break this behaviour
    mountWithIntl(<Search {...DEFAULT_PROPS} />);
    assert.property(window, "gContentSearchController");
    assert.ok(window.gContentSearchController);
  });
  it("should pass along search when clicking the search button", () => {
    const wrapper = mountWithIntl(<Search {...DEFAULT_PROPS} />);

    wrapper.find(".search-button").simulate("click");

    const {search} = window.gContentSearchController;
    assert.calledOnce(search);
    assert.propertyVal(search.firstCall.args[0], "type", "click");
  });
  it("should send a UserEvent action", () => {
    global.ContentSearchUIController.prototype.search = () => {
      dispatchEvent(new CustomEvent("ContentSearchClient", {detail: {type: "Search"}}));
    };
    const dispatch = sinon.spy();
    const wrapper = mountWithIntl(<Search {...DEFAULT_PROPS} dispatch={dispatch} />);

    wrapper.find(".search-button").simulate("click");

    assert.calledOnce(dispatch);
    const [action] = dispatch.firstCall.args;
    assert.isUserEventAction(action);
    assert.propertyVal(action.data, "event", "SEARCH");
  });

  describe("Search Hand-off", () => {
    it("should render a Search element when hand-off is enabled", () => {
      const wrapper = shallowWithIntl(<Search {...DEFAULT_PROPS} handoffEnabled={true} />);
      assert.ok(wrapper.exists());
      assert.equal(wrapper.find(".search-handoff-button").length, 1);
    });
    it("should hand-off search when button is clicked with mouse", () => {
      const dispatch = sinon.spy();
      const wrapper = shallowWithIntl(<Search {...DEFAULT_PROPS} handoffEnabled={true} dispatch={dispatch} />);
      wrapper.instance().onSearchHandoffClick({clientX: 101, clientY: 102});
      assert.calledWith(dispatch, {
        data: {hiddenFocus: true},
        meta: {from: "ActivityStream:Content", skipLocal: true, to: "ActivityStream:Main"},
        type: "HANDOFF_SEARCH_TO_AWESOMEBAR",
      });
      assert.calledWith(dispatch, {type: "FOCUS_SEARCH"});
    });
    it("should hand-off search when button is clicked with keyboard", () => {
      const dispatch = sinon.spy();
      const wrapper = shallowWithIntl(<Search {...DEFAULT_PROPS} handoffEnabled={true} dispatch={dispatch} />);
      wrapper.instance().onSearchHandoffClick({clientX: 0, clientY: 0});
      assert.calledWith(dispatch, {
        data: {hiddenFocus: false},
        meta: {from: "ActivityStream:Content", skipLocal: true, to: "ActivityStream:Main"},
        type: "HANDOFF_SEARCH_TO_AWESOMEBAR",
      });
      assert.calledWith(dispatch, {type: "FOCUS_SEARCH"});
    });
  });
});

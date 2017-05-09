const React = require("react");
const {mountWithIntl, shallowWithIntl} = require("test/unit/utils");
const {_unconnected: Search} = require("content-src/components/Search/Search");
const {actionTypes: at, actionUtils: au} = require("common/Actions.jsm");

const DEFAULT_PROPS = {
  Search: {
    currentEngine: {
      name: "google",
      icon: "google.jpg"
    }
  },
  dispatch() {}
};

describe("<Search>", () => {
  it("should render a Search element", () => {
    const wrapper = shallowWithIntl(<Search {...DEFAULT_PROPS} />);
    assert.ok(wrapper.exists());
  });

  describe("#performSearch", () => {
    function clickButtonAndGetAction(wrapper) {
      const dispatch = wrapper.prop("dispatch");
      wrapper.find(".search-button").simulate("click");
      assert.calledOnce(dispatch);
      return dispatch.firstCall.args[0];
    }
    it("should send a SendToMain action with type PERFORM_SEARCH when you click the search button", () => {
      const wrapper = mountWithIntl(<Search {...DEFAULT_PROPS} dispatch={sinon.spy()} />);

      const action = clickButtonAndGetAction(wrapper);

      assert.propertyVal(action, "type", at.PERFORM_SEARCH);
      assert.isTrue(au.isSendToMain(action));
    });
    it("should send an action with the right engineName ", () => {
      const props = {Search: {currentEngine: {name: "foo123"}}, dispatch: sinon.spy()};
      const wrapper = mountWithIntl(<Search {...props} />);

      const action = clickButtonAndGetAction(wrapper);

      assert.propertyVal(action.data, "engineName", "foo123");
    });
    it("should send an action with the right searchString ", () => {
      const wrapper = mountWithIntl(<Search {...DEFAULT_PROPS} dispatch={sinon.spy()} />);
      wrapper.setState({searchString: "hello123"});

      const action = clickButtonAndGetAction(wrapper);

      assert.propertyVal(action.data, "searchString", "hello123");
    });
  });

  it("should update state.searchString on a change event", () => {
    const wrapper = mountWithIntl(<Search {...DEFAULT_PROPS} />);
    const inputEl = wrapper.find("#search-input");
    // as the value in the input field changes, it will update the search string
    inputEl.simulate("change", {target: {value: "hello"}});

    assert.equal(wrapper.state().searchString, "hello");
  });
});

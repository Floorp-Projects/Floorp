import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {_DarkModeMessage as DarkModeMessage} from "content-src/components/DarkModeMessage/DarkModeMessage";
import React from "react";
import {shallow} from "enzyme";

describe("<DarkModeMessage>", () => {
  let dispatch;
  let wrapper;
  beforeEach(() => {
    dispatch = sinon.stub();
    wrapper = shallow(<DarkModeMessage dispatch={dispatch} />);
  });
  it("should render the component", () => {
    assert.isNotNull(wrapper.getElement());
  });
  describe("actions", () => {
    it("should render two buttons", () => {
      assert.equal(wrapper.find(".actions").children().length, 2);
    });
    it("Cancel btn should dispatch correct call", () => {
      const cancelBtn = wrapper.find(".actions").childAt(0);

      cancelBtn.simulate("click");

      assert.calledOnce(dispatch);
      assert.calledWith(dispatch, ac.SetPref("darkModeMessage", false));
    });
    it("Switch to older version should dispatch correct call", () => {
      const switchBtn = wrapper.find(".actions").childAt(1);

      switchBtn.simulate("click");

      assert.calledOnce(dispatch);
      assert.calledWith(dispatch, ac.AlsoToMain({type: at.DISCOVERY_STREAM_OPT_OUT}));
    });
  });
});

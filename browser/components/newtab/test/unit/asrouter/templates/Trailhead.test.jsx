import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {mount} from "enzyme";
import {OnboardingMessageProvider} from "lib/OnboardingMessageProvider.jsm";
import React from "react";
import {Trailhead} from "content-src/asrouter/templates/Trailhead/Trailhead";

describe("<Trailhead>", () => {
  let wrapper;
  let dispatch;
  let sandbox;

  beforeEach(async () => {
    sandbox = sinon.sandbox.create();
    dispatch = sandbox.stub();
    sandbox.stub(global, "fetch")
      .resolves({ok: true, status: 200, json: () => Promise.resolve({flowId: 123, flowBeginTime: 456})});

    const message = (await OnboardingMessageProvider.getUntranslatedMessages()).find(msg => msg.template === "trailhead");
    wrapper = mount(<Trailhead message={message} fxaEndpoint="https://accounts.firefox.com/endpoint" dispatch={dispatch} />);
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should emit UserEvent SKIPPED_SIGNIN when you click the start browsing button", () => {
    let skipButton = wrapper.find(".trailheadStart");
    assert.ok(skipButton.exists());
    skipButton.simulate("click");

    assert.calledOnce(dispatch);
    assert.isUserEventAction(dispatch.firstCall.args[0]);
    assert.calledWith(dispatch, ac.UserEvent({event: at.SKIPPED_SIGNIN, value: {has_flow_params: false}}));
  });

  it("should emit UserEvent SUBMIT_EMAIL when you submit the form", () => {
    let form = wrapper.find("form");
    assert.ok(form.exists());
    form.simulate("submit");

    assert.calledOnce(dispatch);
    assert.isUserEventAction(dispatch.firstCall.args[0]);
    assert.calledWith(dispatch, ac.UserEvent({event: at.SUBMIT_EMAIL, value: {has_flow_params: false}}));
  });
});

import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {mount} from "enzyme";
import {OnboardingMessageProvider} from "lib/OnboardingMessageProvider.jsm";
import React from "react";
import {_Trailhead as Trailhead} from "content-src/asrouter/templates/Trailhead/Trailhead";

const CARDS = [{
  content: {
    title: {string_id: "onboarding-private-browsing-title"},
    text: {string_id: "onboarding-private-browsing-text"},
    icon: "icon",
    primary_button: {
      label: {string_id: "onboarding-button-label-try-now"},
      action: {
        type: "OPEN_URL",
        data: {args: "https://example.com/"},
      },
    },
  },
}];

describe("<Trailhead>", () => {
  let wrapper;
  let dummyNode;
  let dispatch;
  let onAction;
  let sandbox;

  beforeEach(async () => {
    sandbox = sinon.sandbox.create();
    dispatch = sandbox.stub();
    onAction = sandbox.stub();
    sandbox.stub(global, "fetch")
      .resolves({ok: true, status: 200, json: () => Promise.resolve({flowId: 123, flowBeginTime: 456})});

    dummyNode = document.createElement("body");
    sandbox.stub(dummyNode, "querySelector").returns(dummyNode);
    const fakeDocument = {
      get activeElement() {
        return dummyNode;
      },
      get body() {
        return dummyNode;
      },
      getElementById() {
        return dummyNode;
      },
    };

    const message = (await OnboardingMessageProvider.getUntranslatedMessages()).find(msg => msg.id === "TRAILHEAD_1");
    message.cards = CARDS;
    wrapper = mount(<Trailhead message={message} fxaEndpoint="https://accounts.firefox.com/endpoint" dispatch={dispatch} onAction={onAction} document={fakeDocument} />);
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

  it("should NOT emit UserEvent SKIPPED_SIGNIN when closeModal is triggered by visibilitychange event", () => {
    wrapper.instance().closeModal({type: "visibilitychange"});
    assert.notCalled(dispatch);
  });

  it("should prevent submissions with no email", () => {
    const form = wrapper.find("form");
    const preventDefault = sandbox.stub();

    form.simulate("submit", {preventDefault});

    assert.calledOnce(preventDefault);
    assert.notCalled(dispatch);
  });
  it("should emit UserEvent SUBMIT_EMAIL when you submit a valid email", () => {
    let form = wrapper.find("form");
    assert.ok(form.exists());
    form.getDOMNode().elements.email.value = "a@b.c";

    form.simulate("submit");

    assert.calledOnce(dispatch);
    assert.isUserEventAction(dispatch.firstCall.args[0]);
    assert.calledWith(dispatch, ac.UserEvent({event: at.SUBMIT_EMAIL, value: {has_flow_params: false}}));
  });

  it("should add utm_* query params to card actions", () => {
    let {action} = CARDS[0].content.primary_button;
    wrapper.instance().onCardAction(action);
    assert.calledOnce(onAction);
    const url = onAction.firstCall.args[0].data.args;
    assert.equal(url, "https://example.com/?utm_source=activity-stream&utm_campaign=firstrun&utm_medium=referral&utm_term=trailhead-join-card");
  });

  it("should add flow parameters to card action urls if addFlowParams is true", () => {
    let action = {
      type: "OPEN_URL",
      addFlowParams: true,
      data: {args: "https://example.com/path?foo=bar"},
    };
    wrapper.setState({
      deviceId: "abc",
      flowId: "123",
      flowBeginTime: 456,
    });
    wrapper.instance().onCardAction(action);
    assert.calledOnce(onAction);
    const url = onAction.firstCall.args[0].data.args;
    assert.equal(url, "https://example.com/path?foo=bar&utm_source=activity-stream&utm_campaign=firstrun&utm_medium=referral&utm_term=trailhead-join-card&device_id=abc&flow_id=123&flow_begin_time=456");
  });

  it("should keep focus in dialog when blurring start button", () => {
    const skipButton = wrapper.find(".trailheadStart");
    sandbox.stub(dummyNode, "focus");

    skipButton.simulate("blur", {relatedTarget: dummyNode});

    assert.calledOnce(dummyNode.focus);
  });
});

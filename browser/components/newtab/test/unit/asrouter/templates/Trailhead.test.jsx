import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { mount } from "enzyme";
import { OnboardingMessageProvider } from "lib/OnboardingMessageProvider.jsm";
import React from "react";
import { Trailhead } from "content-src/asrouter/templates/Trailhead/Trailhead";

export const CARDS = [
  {
    content: {
      title: { string_id: "onboarding-private-browsing-title" },
      text: { string_id: "onboarding-private-browsing-text" },
      icon: "icon",
      primary_button: {
        label: { string_id: "onboarding-button-label-try-now" },
        action: {
          type: "OPEN_URL",
          data: { args: "https://example.com/" },
        },
      },
    },
  },
];

describe("<Trailhead>", () => {
  let wrapper;
  let dummyNode;
  let dispatch;
  let onAction;
  let sandbox;
  let onNextScene;

  beforeEach(async () => {
    sandbox = sinon.sandbox.create();
    dispatch = sandbox.stub();
    onAction = sandbox.stub();
    onNextScene = sandbox.stub();
    sandbox.stub(global, "fetch").resolves({
      ok: true,
      status: 200,
      json: () => Promise.resolve({ flowId: 123, flowBeginTime: 456 }),
    });

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

    const message = (await OnboardingMessageProvider.getUntranslatedMessages()).find(
      msg => msg.id === "TRAILHEAD_1"
    );
    message.cards = CARDS;
    wrapper = mount(
      <Trailhead
        message={message}
        UTMTerm={message.utm_term}
        fxaEndpoint="https://accounts.firefox.com/endpoint"
        dispatch={dispatch}
        onAction={onAction}
        document={fakeDocument}
        onNextScene={onNextScene}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should emit UserEvent SKIPPED_SIGNIN and call nextScene when you click the start browsing button", () => {
    let skipButton = wrapper.find(".trailheadStart");
    assert.ok(skipButton.exists());
    skipButton.simulate("click");

    assert.calledOnce(onNextScene);

    assert.calledOnce(dispatch);
    assert.isUserEventAction(dispatch.firstCall.args[0]);
    assert.calledWith(
      dispatch,
      ac.UserEvent({
        event: at.SKIPPED_SIGNIN,
        value: { has_flow_params: false },
      })
    );
  });

  it("should NOT emit UserEvent SKIPPED_SIGNIN when closeModal is triggered by visibilitychange event", () => {
    wrapper.instance().closeModal({ type: "visibilitychange" });
    assert.notCalled(dispatch);
  });

  it("should prevent submissions with no email", () => {
    const form = wrapper.find("form");
    const preventDefault = sandbox.stub();

    form.simulate("submit", { preventDefault });

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
    assert.calledWith(
      dispatch,
      ac.UserEvent({
        event: at.SUBMIT_EMAIL,
        value: { has_flow_params: false },
      })
    );
  });

  it("should keep focus in dialog when blurring start button", () => {
    const skipButton = wrapper.find(".trailheadStart");
    sandbox.stub(dummyNode, "focus");

    skipButton.simulate("blur", { relatedTarget: dummyNode });

    assert.calledOnce(dummyNode.focus);
  });
});

import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { FxASignupForm } from "content-src/asrouter/components/FxASignupForm/FxASignupForm";
import { mount } from "enzyme";
import React from "react";

describe("<FxASignupForm>", () => {
  let wrapper;
  let dummyNode;
  let dispatch;
  let onClose;
  let sandbox;

  const FAKE_FLOW_PARAMS = {
    deviceId: "foo",
    flowId: "abc1",
    flowBeginTime: 1234,
  };

  const FAKE_MESSAGE_CONTENT = {
    title: { string_id: "onboarding-welcome-body" },
    learn: {
      text: { string_id: "onboarding-welcome-learn-more" },
      url: "https://www.mozilla.org/firefox/accounts/",
    },
    form: {
      title: { string_id: "onboarding-welcome-form-header" },
      text: { string_id: "onboarding-join-form-body" },
      email: { string_id: "onboarding-fullpage-form-email" },
      button: { string_id: "onboarding-join-form-continue" },
    },
  };

  beforeEach(async () => {
    sandbox = sinon.sandbox.create();
    dispatch = sandbox.stub();
    onClose = sandbox.stub();

    dummyNode = document.createElement("body");
    sandbox.stub(dummyNode, "querySelector").returns(dummyNode);
    const fakeDocument = {
      getElementById() {
        return dummyNode;
      },
    };

    wrapper = mount(
      <FxASignupForm
        document={fakeDocument}
        content={FAKE_MESSAGE_CONTENT}
        dispatch={dispatch}
        fxaEndpoint="https://accounts.firefox.com/endpoint/"
        UTMTerm="test-utm-term"
        flowParams={FAKE_FLOW_PARAMS}
        onClose={onClose}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should prevent submissions with no email", () => {
    const form = wrapper.find("form");
    const preventDefault = sandbox.stub();

    form.simulate("submit", { preventDefault });

    assert.calledOnce(preventDefault);
    assert.notCalled(dispatch);
  });

  it("should not display signin link by default", () => {
    assert.notOk(
      wrapper
        .find("button[data-l10n-id='onboarding-join-form-signin']")
        .exists()
    );
  });

  it("should display signin when showSignInLink is true", () => {
    wrapper.setProps({ showSignInLink: true });
    let signIn = wrapper.find(
      "button[data-l10n-id='onboarding-join-form-signin']"
    );
    assert.exists(signIn);
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
        value: { has_flow_params: true },
      })
    );
  });

  it("should emit UserEvent SUBMIT_SIGNIN when submit with email disabled", () => {
    let form = wrapper.find("form");
    form.getDOMNode().elements.email.disabled = true;

    form.simulate("submit");
    assert.calledOnce(dispatch);
    assert.isUserEventAction(dispatch.firstCall.args[0]);
    assert.calledWith(
      dispatch,
      ac.UserEvent({
        event: at.SUBMIT_SIGNIN,
        value: { has_flow_params: true },
      })
    );
  });
});

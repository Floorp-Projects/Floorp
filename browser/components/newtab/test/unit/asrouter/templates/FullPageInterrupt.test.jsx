import { mount } from "enzyme";
import { OnboardingMessageProvider } from "lib/OnboardingMessageProvider.jsm";
import React from "react";
import {
  FullPageInterrupt,
  FxAccounts,
  FxCards,
} from "content-src/asrouter/templates/FullPageInterrupt/FullPageInterrupt";
import { FxASignupForm } from "content-src/asrouter/components/FxASignupForm/FxASignupForm";
import { OnboardingCard } from "content-src/asrouter/templates/OnboardingMessage/OnboardingMessage";

const CARDS = [
  {
    id: "CARD_1",
    content: {
      title: { string_id: "onboarding-private-browsing-title" },
      text: { string_id: "onboarding-private-browsing-text" },
      icon: "icon",
      primary_button: {
        label: { string_id: "onboarding-button-label-get-started" },
        action: {
          type: "OPEN_URL",
          data: { args: "https://example.com/" },
        },
      },
    },
  },
];

const FAKE_FLOW_PARAMS = {
  deviceId: "foo",
  flowId: "abc1",
  flowBeginTime: 1234,
};

describe("<FullPageInterrupt>", () => {
  let wrapper;
  let dummyNode;
  let dispatch;
  let onBlock;
  let sandbox;
  let onAction;
  let onBlockById;
  let sendTelemetryStub;

  beforeEach(async () => {
    sandbox = sinon.createSandbox();
    dispatch = sandbox.stub();
    onBlock = sandbox.stub();
    onAction = sandbox.stub();
    onBlockById = sandbox.stub();
    sendTelemetryStub = sandbox.stub();

    dummyNode = document.createElement("body");
    sandbox.stub(dummyNode, "querySelector").returns(dummyNode);
    const fakeDocument = {
      getElementById() {
        return dummyNode;
      },
    };

    const message = (await OnboardingMessageProvider.getUntranslatedMessages()).find(
      msg => msg.id === "FULL_PAGE_1"
    );

    wrapper = mount(
      <FullPageInterrupt
        message={message}
        UTMTerm={message.utm_term}
        fxaEndpoint="https://accounts.firefox.com/endpoint/"
        dispatch={dispatch}
        onBlock={onBlock}
        onAction={onAction}
        onBlockById={onBlockById}
        cards={CARDS}
        document={fakeDocument}
        sendUserActionTelemetry={sendTelemetryStub}
        flowParams={FAKE_FLOW_PARAMS}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should trigger onBlock on removeOverlay", () => {
    wrapper.instance().removeOverlay();
    assert.calledOnce(onBlock);
  });

  it("should render Full Page interrupt with accounts and triplet cards section", () => {
    assert.lengthOf(wrapper.find(FxAccounts), 1);
    assert.lengthOf(wrapper.find(FxCards), 1);
  });

  it("should render FxASignupForm inside FxAccounts", () => {
    assert.lengthOf(wrapper.find(FxASignupForm), 1);
  });

  it("should display learn more link on full page", () => {
    assert.ok(wrapper.find("a.fullpage-left-link").exists());
  });

  it("should add utm_* query params to card actions and send the right ping when a card button is clicked", () => {
    wrapper
      .find(OnboardingCard)
      .find("button.onboardingButton")
      .simulate("click");

    assert.calledOnce(onAction);
    const url = onAction.firstCall.args[0].data.args;
    assert.equal(
      url,
      "https://example.com/?utm_source=activity-stream&utm_campaign=firstrun&utm_medium=referral&utm_term=trailhead-full_page_d"
    );
    assert.calledWith(sendTelemetryStub, {
      event: "CLICK_BUTTON",
      message_id: CARDS[0].id,
      id: "TRAILHEAD",
    });
  });
  it("should not call blockById by default when a card button is clicked", () => {
    wrapper
      .find(OnboardingCard)
      .find("button.onboardingButton")
      .simulate("click");
    assert.notCalled(onBlockById);
  });
  it("should call blockById when blockOnClick on message is true", () => {
    CARDS[0].blockOnClick = true;
    wrapper
      .find(OnboardingCard)
      .find("button.onboardingButton")
      .simulate("click");
    assert.calledOnce(onBlockById);
    assert.calledWith(onBlockById, CARDS[0].id);
  });
});

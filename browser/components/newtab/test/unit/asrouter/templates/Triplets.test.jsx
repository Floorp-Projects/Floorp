import { mount } from "enzyme";
import { Triplets } from "content-src/asrouter/templates/FirstRun/Triplets";
import { OnboardingCard } from "content-src/asrouter/templates/OnboardingMessage/OnboardingMessage";
import React from "react";

const CARDS = [
  {
    id: "CARD_1",
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

describe("<Triplets>", () => {
  let wrapper;
  let sandbox;
  let sendTelemetryStub;
  let onAction;
  let onHide;

  async function setup() {
    sandbox = sinon.createSandbox();
    sendTelemetryStub = sandbox.stub();
    onAction = sandbox.stub();
    onHide = sandbox.stub();

    wrapper = mount(
      <Triplets
        cards={CARDS}
        showCardPanel={true}
        showContent={true}
        hideContainer={onHide}
        onAction={onAction}
        UTMTerm="trailhead-join-card"
        sendUserActionTelemetry={sendTelemetryStub}
      />
    );
  }

  beforeEach(setup);
  afterEach(() => {
    sandbox.restore();
  });

  it("should add an expanded class to container if props.showCardPanel is true", () => {
    wrapper.setProps({ showCardPanel: true });
    assert.isTrue(
      wrapper.find(".trailheadCards").hasClass("expanded"),
      "has .expanded)"
    );
  });
  it("should add a collapsed class to container if props.showCardPanel is true", () => {
    wrapper.setProps({ showCardPanel: false });
    assert.isFalse(
      wrapper.find(".trailheadCards").hasClass("expanded"),
      "has .expanded)"
    );
  });
  it("should send telemetry and call props.hideContainer when the dismiss button is clicked", () => {
    wrapper.find("button.icon-dismiss").simulate("click");
    assert.calledOnce(onHide);
    assert.calledWith(sendTelemetryStub, {
      event: "DISMISS",
      message_id: CARDS[0].id,
      id: "onboarding-cards",
      action: "onboarding_user_event",
    });
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
      "https://example.com/?utm_source=activity-stream&utm_campaign=firstrun&utm_medium=referral&utm_term=trailhead-join-card"
    );
    assert.calledWith(sendTelemetryStub, {
      event: "CLICK_BUTTON",
      message_id: CARDS[0].id,
      id: "TRAILHEAD",
    });
  });
});

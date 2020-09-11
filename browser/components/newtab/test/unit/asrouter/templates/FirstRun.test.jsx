import {
  FirstRun,
  FLUENT_FILES,
} from "content-src/asrouter/templates/FirstRun/FirstRun";
import { Triplets } from "content-src/asrouter/templates/FirstRun/Triplets";
import { OnboardingMessageProvider } from "lib/OnboardingMessageProvider.jsm";
import { mount } from "enzyme";
import React from "react";

const FAKE_TRIPLETS_BUNDLE_1 = [
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

const FAKE_TRIPLETS_BUNDLE_2 = [
  {
    id: "CARD_2",
    content: {
      title: { string_id: "onboarding-data-sync-title" },
      text: { string_id: "onboarding-data-sync-text2" },
      icon: "icon",
      primary_button: {
        label: { string_id: "onboarding-data-sync-button2" },
        action: {
          type: "OPEN_URL",
          data: { args: "https://foo.com/" },
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

async function getTestMessage(id, requestNewBundle) {
  const message = (
    await OnboardingMessageProvider.getUntranslatedMessages()
  ).find(msg => msg.id === id);

  // Simulate dynamic triplets by returning a different bundle
  if (requestNewBundle) {
    return { ...message, bundle: FAKE_TRIPLETS_BUNDLE_2 };
  }
  return { ...message, bundle: FAKE_TRIPLETS_BUNDLE_1 };
}

describe("<FirstRun>", () => {
  let wrapper;
  let message;
  let fakeDoc;
  let sandbox;
  let clock;
  let onBlockByIdStub;

  async function setup() {
    sandbox = sinon.createSandbox();
    clock = sandbox.useFakeTimers();
    message = await getTestMessage("EXTENDED_TRIPLETS_1");
    fakeDoc = {
      body: document.createElement("body"),
      head: document.createElement("head"),
      createElement: type => document.createElement(type),
      getElementById: () => document.createElement("div"),
      activeElement: document.createElement("div"),
    };
    onBlockByIdStub = sandbox.stub();

    sandbox
      .stub(global, "fetch")
      .withArgs("http://fake.com/endpoint")
      .resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(FAKE_FLOW_PARAMS),
      });

    wrapper = mount(
      <FirstRun
        message={message}
        document={fakeDoc}
        dispatch={() => {}}
        sendUserActionTelemetry={() => {}}
        onBlockById={onBlockByIdStub}
      />
    );
  }

  beforeEach(setup);
  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper);
  });
  describe("with triplets", () => {
    it("should render triplets", () => {
      assert.lengthOf(wrapper.find(Triplets), 1, "<Triplets>");
    });
    it("should show the card panel and the content on the Triplets", () => {
      const tripletsProps = wrapper.find(Triplets).props();
      assert.propertyVal(tripletsProps, "showCardPanel", true);
    });
    it("should set the UTM term to trailhead-cards-card (for the extended-triplet message)", () => {
      const tProps = wrapper.find(Triplets).props();
      assert.propertyVal(tProps, "UTMTerm", "trailhead-cards-card");
    });
  });

  describe("with no triplets", () => {
    it("should render empty", () => {
      message = { type: "FOO_123" };
      wrapper = mount(<FirstRun message={message} document={fakeDoc} />);

      assert.isTrue(wrapper.isEmptyRender());
    });
  });

  it("should pass along executeAction appropriately", () => {
    const stub = sandbox.stub();
    wrapper = mount(
      <FirstRun message={message} document={fakeDoc} executeAction={stub} />
    );
    assert.propertyVal(wrapper.find(Triplets).props(), "onAction", stub);
  });

  it("should load flow params on mount if fxaEndpoint is defined", () => {
    const stub = sandbox.stub();
    wrapper = mount(
      <FirstRun
        message={message}
        document={fakeDoc}
        fetchFlowParams={stub}
        fxaEndpoint="https://foo.com"
      />
    );
    assert.calledOnce(stub);
  });

  it("should load flow params onUpdate if fxaEndpoint is not defined on mount and then later defined", () => {
    const stub = sandbox.stub();
    wrapper = mount(
      <FirstRun message={message} document={fakeDoc} fetchFlowParams={stub} />
    );
    assert.notCalled(stub);
    wrapper.setProps({ fxaEndpoint: "https://foo.com" });
    assert.calledOnce(stub);
  });

  it("should not load flow params again onUpdate if they were already set", () => {
    const stub = sandbox.stub();
    wrapper = mount(
      <FirstRun
        message={message}
        document={fakeDoc}
        fetchFlowParams={stub}
        fxaEndpoint="https://foo.com"
      />
    );
    wrapper.setProps({ foo: "bar" });
    wrapper.setProps({ foo: "baz" });
    assert.calledOnce(stub);
  });

  it("should load fluent files on mount", () => {
    assert.lengthOf(fakeDoc.head.querySelectorAll("link"), FLUENT_FILES.length);
  });

  it("should update didUserClearTriplets state to true on close of triplet", () => {
    assert.isFalse(wrapper.state().didUserClearTriplets);
    // Simulate calling close Triplets
    wrapper
      .find(Triplets)
      .find(".icon-dismiss")
      .simulate("click");
    assert.isTrue(wrapper.state().didUserClearTriplets);
  });

  it("should hide triplets when closeTriplets is called and block extended triplets after 500ms", () => {
    // Simulate calling next scene
    wrapper
      .find(Triplets)
      .find(".icon-dismiss")
      .simulate("click");

    assert.isFalse(
      wrapper
        .find(Triplets)
        .find(".trailheadCardGrid")
        .hasClass("show"),
      "Show triplet content"
    );

    assert.notCalled(onBlockByIdStub);
    clock.tick(500);
    assert.calledWith(onBlockByIdStub, "EXTENDED_TRIPLETS_1");
  });

  it("should update triplets card when cards in message bundle changes", async () => {
    let tripletsProps = wrapper.find(Triplets).props();
    assert.propertyVal(tripletsProps, "cards", FAKE_TRIPLETS_BUNDLE_1);

    const messageWithNewBundle = await getTestMessage("TRAILHEAD_1", true);
    wrapper.setProps({ message: messageWithNewBundle });
    tripletsProps = wrapper.find(Triplets).props();
    assert.propertyVal(tripletsProps, "cards", FAKE_TRIPLETS_BUNDLE_2);
  });
});

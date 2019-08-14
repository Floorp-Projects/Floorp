import {
  FirstRun,
  FLUENT_FILES,
} from "content-src/asrouter/templates/FirstRun/FirstRun";
import { Interrupt } from "content-src/asrouter/templates/FirstRun/Interrupt";
import { Triplets } from "content-src/asrouter/templates/FirstRun/Triplets";
import { OnboardingMessageProvider } from "lib/OnboardingMessageProvider.jsm";
import { mount } from "enzyme";
import React from "react";

const FAKE_TRIPLETS = [
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

const FAKE_FLOW_PARAMS = {
  deviceId: "foo",
  flowId: "abc1",
  flowBeginTime: 1234,
};

async function getTestMessage(id) {
  const message = (await OnboardingMessageProvider.getUntranslatedMessages()).find(
    msg => msg.id === id
  );
  return { ...message, bundle: FAKE_TRIPLETS };
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
    message = await getTestMessage("TRAILHEAD_1");
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
  describe("with both interrupt and triplets", () => {
    it("should render interrupt and triplets", () => {
      assert.lengthOf(wrapper.find(Interrupt), 1, "<Interrupt>");
      assert.lengthOf(wrapper.find(Triplets), 1, "<Triplets>");
    });
    it("should show the card panel and hide the content on the Triplets", () => {
      // This is so the container shows up in the background but we can fade in the content when intterupt is closed.
      const tripletsProps = wrapper.find(Triplets).props();
      assert.propertyVal(tripletsProps, "showCardPanel", true);
      assert.propertyVal(tripletsProps, "showContent", false);
    });
    it("should set the UTM term to trailhead-join (for the traihead-join message)", () => {
      const iProps = wrapper.find(Interrupt).props();
      const tProps = wrapper.find(Triplets).props();
      assert.propertyVal(iProps, "UTMTerm", "trailhead-join");
      assert.propertyVal(tProps, "UTMTerm", "trailhead-join-card");
    });
  });

  describe("with an interrupt but no triplets", () => {
    beforeEach(() => {
      message.bundle = []; // Empty triplets
      wrapper = mount(<FirstRun message={message} document={fakeDoc} />);
    });
    it("should render interrupt but no triplets", () => {
      assert.lengthOf(wrapper.find(Interrupt), 1, "<Interrupt>");
      assert.lengthOf(wrapper.find(Triplets), 0, "<Triplets>");
    });
  });

  describe("with triplets but no interrupt", () => {
    it("should render interrupt but no triplets", () => {
      delete message.content; // Empty interrupt
      wrapper = mount(<FirstRun message={message} document={fakeDoc} />);

      assert.lengthOf(wrapper.find(Interrupt), 0, "<Interrupt>");
      assert.lengthOf(wrapper.find(Triplets), 1, "<Triplets>");
    });
  });

  describe("with no triplets or interrupt", () => {
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

    assert.propertyVal(wrapper.find(Interrupt).props(), "executeAction", stub);
    assert.propertyVal(wrapper.find(Triplets).props(), "onAction", stub);
  });

  it("should load flow params on mount if fxaEndpoint is defined", () => {
    const stub = sandbox.stub();
    wrapper = mount(
      <FirstRun
        message={message}
        document={fakeDoc}
        dispatch={() => {}}
        fetchFlowParams={stub}
        fxaEndpoint="https://foo.com"
      />
    );
    assert.calledOnce(stub);
  });

  it("should load flow params onUpdate if fxaEndpoint is not defined on mount and then later defined", () => {
    const stub = sandbox.stub();
    wrapper = mount(
      <FirstRun
        message={message}
        document={fakeDoc}
        fetchFlowParams={stub}
        dispatch={() => {}}
      />
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
        dispatch={() => {}}
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

  it("should hide the interrupt and show the triplets when onNextScene is called", () => {
    // Simulate calling next scene
    wrapper
      .find(Interrupt)
      .find(".trailheadStart")
      .simulate("click");

    assert.lengthOf(wrapper.find(Interrupt), 0, "Interrupt hidden");
    assert.isTrue(
      wrapper
        .find(Triplets)
        .find(".trailheadCardGrid")
        .hasClass("show"),
      "Show triplet content"
    );
  });

  it("should hide the interrupt when props.interruptCleared changes to true", () => {
    assert.lengthOf(wrapper.find(Interrupt), 1, "Interrupt shown");
    wrapper.setProps({ interruptCleared: true });

    assert.lengthOf(wrapper.find(Interrupt), 0, "Interrupt hidden");
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
});

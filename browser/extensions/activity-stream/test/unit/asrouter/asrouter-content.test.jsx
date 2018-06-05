import {ASRouterUISurface, ASRouterUtils} from "content-src/asrouter/asrouter-content";
import {OUTGOING_MESSAGE_NAME as AS_GENERAL_OUTGOING_MESSAGE_NAME} from "content-src/lib/init-store";
import {FAKE_LOCAL_MESSAGES} from "./constants";
import {GlobalOverrider} from "test/unit/utils";
import {mount} from "enzyme";
import React from "react";
let [FAKE_MESSAGE] = FAKE_LOCAL_MESSAGES;

FAKE_MESSAGE = Object.assign({}, FAKE_MESSAGE, {provider: "fakeprovider"});
const FAKE_BUNDLED_MESSAGE = {bundle: [{id: "foo", template: "onboarding", content: {title: "Foo", body: "Foo123"}}], template: "onboarding"};

describe("ASRouterUtils", () => {
  let global;
  let sandbox;
  let fakeSendAsyncMessage;
  beforeEach(() => {
    global = new GlobalOverrider();
    sandbox = sinon.sandbox.create();
    fakeSendAsyncMessage = sandbox.stub();
    global.set({sendAsyncMessage: fakeSendAsyncMessage});
  });
  afterEach(() => {
    sandbox.restore();
    global.restore();
  });
  it("should send a message with the right payload data", () => {
    ASRouterUtils.sendTelemetry({id: 1, event: "CLICK"});

    assert.calledOnce(fakeSendAsyncMessage);
    assert.calledWith(fakeSendAsyncMessage, AS_GENERAL_OUTGOING_MESSAGE_NAME);
    const [, payload] = fakeSendAsyncMessage.firstCall.args;
    assert.propertyVal(payload.data, "id", 1);
    assert.propertyVal(payload.data, "event", "CLICK");
  });
});

describe("ASRouterUISurface", () => {
  let wrapper;
  let global;
  let sandbox;
  let fakeDocument;

  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    fakeDocument = {
      _listeners: new Set(),
      _visibilityState: "hidden",
      get visibilityState() {
        return this._visibilityState;
      },
      set visibilityState(value) {
        if (this._visibilityState === value) {
          return;
        }
        this._visibilityState = value;
        this._listeners.forEach(l => l());
      },
      addEventListener(event, listener) {
        this._listeners.add(listener);
      },
      removeEventListener(event, listener) {
        this._listeners.delete(listener);
      }
    };
    global = new GlobalOverrider();
    global.set({
      addMessageListener: sandbox.stub(),
      removeMessageListener: sandbox.stub(),
      sendAsyncMessage: sandbox.stub()
    });

    sandbox.stub(ASRouterUtils, "sendTelemetry");

    wrapper = mount(<ASRouterUISurface document={fakeDocument} />);
  });

  afterEach(() => {
    sandbox.restore();
    global.restore();
  });

  it("should render the component if a message id is defined", () => {
    wrapper.setState({message: FAKE_MESSAGE});
    assert.isTrue(wrapper.exists());
  });

  it("should render the component if a bundle of messages is defined", () => {
    wrapper.setState({bundle: FAKE_BUNDLED_MESSAGE});
    assert.isTrue(wrapper.exists());
  });

  describe("snippets", () => {
    it("should send correct event and source when snippet link is clicked", () => {
      const content = {button_url: "https://foo.com", button_type: "anchor", button_label: "foo", ...FAKE_MESSAGE.content};
      const message = Object.assign({}, FAKE_MESSAGE, {content});
      wrapper.setState({message});

      wrapper.find("a.ASRouterAnchor").simulate("click");
      assert.propertyVal(ASRouterUtils.sendTelemetry.firstCall.args[0], "event", "CLICK_BUTTON");
      assert.propertyVal(ASRouterUtils.sendTelemetry.firstCall.args[0], "source", "NEWTAB_FOOTER_BAR");
    });
    it("should send correct event and source when snippet is blocked", () => {
      wrapper.setState({message: FAKE_MESSAGE});

      wrapper.find(".blockButton").simulate("click");
      assert.propertyVal(ASRouterUtils.sendTelemetry.firstCall.args[0], "event", "BLOCK");
      assert.propertyVal(ASRouterUtils.sendTelemetry.firstCall.args[0], "source", "NEWTAB_FOOTER_BAR");
    });
  });

  describe("impressions", () => {
    function simulateVisibilityChange(value) {
      fakeDocument.visibilityState = value;
    }

    it("should not send an impression if no message exists", () => {
      simulateVisibilityChange("visible");

      assert.notCalled(ASRouterUtils.sendTelemetry);
    });

    it("should not send an impression if the page is not visible", () => {
      simulateVisibilityChange("hidden");
      wrapper.setState({message: FAKE_MESSAGE});

      assert.notCalled(ASRouterUtils.sendTelemetry);
    });

    it("should send an impression ping when there is a message and the page becomes visible", () => {
      wrapper.setState({message: FAKE_MESSAGE});
      assert.notCalled(ASRouterUtils.sendTelemetry);

      simulateVisibilityChange("visible");
      assert.calledOnce(ASRouterUtils.sendTelemetry);
    });

    it("should send the correct impression source", () => {
      wrapper.setState({message: FAKE_MESSAGE});
      simulateVisibilityChange("visible");

      assert.calledOnce(ASRouterUtils.sendTelemetry);
      assert.propertyVal(ASRouterUtils.sendTelemetry.firstCall.args[0], "event", "IMPRESSION");
      assert.propertyVal(ASRouterUtils.sendTelemetry.firstCall.args[0], "source", "NEWTAB_FOOTER_BAR");
    });

    it("should send an impression ping when the page is visible and a message gets loaded", () => {
      simulateVisibilityChange("visible");
      wrapper.setState({message: {}});
      assert.notCalled(ASRouterUtils.sendTelemetry);

      wrapper.setState({message: FAKE_MESSAGE});
      assert.calledOnce(ASRouterUtils.sendTelemetry);
    });

    it("should send another impression ping if the message id changes", () => {
      simulateVisibilityChange("visible");
      wrapper.setState({message: FAKE_MESSAGE});
      assert.calledOnce(ASRouterUtils.sendTelemetry);

      wrapper.setState({message: FAKE_LOCAL_MESSAGES[1]});
      assert.calledTwice(ASRouterUtils.sendTelemetry);
    });

    it("should not send another impression ping if the message id has not changed", () => {
      simulateVisibilityChange("visible");
      wrapper.setState({message: FAKE_MESSAGE});
      assert.calledOnce(ASRouterUtils.sendTelemetry);

      wrapper.setState({somethingElse: 123});
      assert.calledOnce(ASRouterUtils.sendTelemetry);
    });

    it("should not send another impression ping if the message is cleared", () => {
      simulateVisibilityChange("visible");
      wrapper.setState({message: FAKE_MESSAGE});
      assert.calledOnce(ASRouterUtils.sendTelemetry);

      wrapper.setState({message: {}});
      assert.calledOnce(ASRouterUtils.sendTelemetry);
    });

    it("should call .sendTelemetry with the right message data", () => {
      simulateVisibilityChange("visible");
      wrapper.setState({message: FAKE_MESSAGE});

      assert.calledOnce(ASRouterUtils.sendTelemetry);
      const [payload] = ASRouterUtils.sendTelemetry.firstCall.args;

      assert.propertyVal(payload, "message_id", FAKE_MESSAGE.id);
      assert.propertyVal(payload, "event", "IMPRESSION");
      assert.propertyVal(payload, "action", `${FAKE_MESSAGE.provider}_user_event`);
      assert.propertyVal(payload, "source", "NEWTAB_FOOTER_BAR");
    });
  });
});

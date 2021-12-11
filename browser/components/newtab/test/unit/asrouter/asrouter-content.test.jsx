import { ASRouterUISurface } from "content-src/asrouter/asrouter-content";
import { ASRouterUtils } from "content-src/asrouter/asrouter-utils";
import { GlobalOverrider } from "test/unit/utils";
import { FAKE_LOCAL_MESSAGES } from "./constants";
import React from "react";
import { mount } from "enzyme";

let [FAKE_MESSAGE] = FAKE_LOCAL_MESSAGES;
const FAKE_NEWSLETTER_SNIPPET = FAKE_LOCAL_MESSAGES.find(
  msg => msg.id === "newsletter"
);
const FAKE_FXA_SNIPPET = FAKE_LOCAL_MESSAGES.find(msg => msg.id === "fxa");
const FAKE_BELOW_SEARCH_SNIPPET = FAKE_LOCAL_MESSAGES.find(
  msg => msg.id === "belowsearch"
);

FAKE_MESSAGE = Object.assign({}, FAKE_MESSAGE, { provider: "fakeprovider" });

describe("ASRouterUtils", () => {
  let globalOverrider;
  let sandbox;
  let globals;
  beforeEach(() => {
    globalOverrider = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    globals = {
      ASRouterMessage: sandbox.stub(),
    };
    globalOverrider.set(globals);
  });
  afterEach(() => {
    sandbox.restore();
    globalOverrider.restore();
  });
  it("should send a message with the right payload data", () => {
    ASRouterUtils.sendTelemetry({ id: 1, event: "CLICK" });

    assert.calledOnce(globals.ASRouterMessage);
    assert.calledWith(globals.ASRouterMessage, {
      type: "AS_ROUTER_TELEMETRY_USER_EVENT",
      meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
      data: {
        id: 1,
        event: "CLICK",
      },
    });
  });
});

describe("ASRouterUISurface", () => {
  let wrapper;
  let globalOverrider;
  let sandbox;
  let headerPortal;
  let footerPortal;
  let root;
  let fakeDocument;
  let globals;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    headerPortal = document.createElement("div");
    footerPortal = document.createElement("div");
    root = document.createElement("div");
    sandbox.stub(footerPortal, "querySelector").returns(footerPortal);
    fakeDocument = {
      location: { href: "" },
      _listeners: new Set(),
      _visibilityState: "hidden",
      head: {
        appendChild(el) {
          return el;
        },
      },
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
      },
      get body() {
        return document.createElement("body");
      },
      getElementById(id) {
        switch (id) {
          case "header-asrouter-container":
            return headerPortal;
          case "root":
            return root;
          default:
            return footerPortal;
        }
      },
      createElement(tag) {
        return document.createElement(tag);
      },
    };
    globals = {
      ASRouterMessage: sandbox.stub().resolves(),
      ASRouterAddParentListener: sandbox.stub(),
      ASRouterRemoveParentListener: sandbox.stub(),
      fetch: sandbox.stub().resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve({}),
      }),
    };
    globalOverrider = new GlobalOverrider();
    globalOverrider.set(globals);
    sandbox.stub(ASRouterUtils, "sendTelemetry");

    wrapper = mount(<ASRouterUISurface document={fakeDocument} />);
  });

  afterEach(() => {
    sandbox.restore();
    globalOverrider.restore();
  });

  it("should render the component if a message id is defined", () => {
    wrapper.setState({ message: FAKE_MESSAGE });
    assert.isTrue(wrapper.exists());
  });

  it("should pass in the correct form_method for newsletter snippets", () => {
    wrapper.setState({ message: FAKE_NEWSLETTER_SNIPPET });

    assert.isTrue(wrapper.find("SubmitFormSnippet").exists());
    assert.propertyVal(
      wrapper.find("SubmitFormSnippet").props(),
      "form_method",
      "POST"
    );
  });

  it("should pass in the correct form_method for fxa snippets", () => {
    wrapper.setState({ message: FAKE_FXA_SNIPPET });

    assert.isTrue(wrapper.find("SubmitFormSnippet").exists());
    assert.propertyVal(
      wrapper.find("SubmitFormSnippet").props(),
      "form_method",
      "GET"
    );
  });

  it("should render a preview banner if message provider is preview", () => {
    wrapper.setState({ message: { ...FAKE_MESSAGE, provider: "preview" } });
    assert.isTrue(wrapper.find(".snippets-preview-banner").exists());
  });

  it("should not render a preview banner if message provider is not preview", () => {
    wrapper.setState({ message: FAKE_MESSAGE });
    assert.isFalse(wrapper.find(".snippets-preview-banner").exists());
  });

  it("should render a SimpleSnippet in the footer portal", () => {
    wrapper.setState({ message: FAKE_MESSAGE });
    assert.isTrue(footerPortal.childElementCount > 0);
    assert.equal(headerPortal.childElementCount, 0);
  });

  it("should not render a SimpleBelowSearchSnippet in a portal", () => {
    wrapper.setState({ message: FAKE_BELOW_SEARCH_SNIPPET });
    assert.equal(headerPortal.childElementCount, 0);
    assert.equal(footerPortal.childElementCount, 0);
  });

  it("should dispatch an event to select the correct theme", () => {
    const stub = sandbox.stub(window, "dispatchEvent");
    sandbox
      .stub(ASRouterUtils, "getPreviewEndpoint")
      .returns({ theme: "dark" });

    wrapper = mount(<ASRouterUISurface document={fakeDocument} />);

    assert.calledOnce(stub);
    assert.property(stub.firstCall.args[0].detail.data, "ntp_background");
    assert.property(stub.firstCall.args[0].detail.data, "ntp_text");
    assert.property(stub.firstCall.args[0].detail.data, "sidebar");
    assert.property(stub.firstCall.args[0].detail.data, "sidebar_text");
  });

  it("should set `dir=rtl` on the page's <html> element if the dir param is set", () => {
    assert.notPropertyVal(fakeDocument, "dir", "rtl");
    sandbox.stub(ASRouterUtils, "getPreviewEndpoint").returns({ dir: "rtl" });

    wrapper = mount(<ASRouterUISurface document={fakeDocument} />);
    assert.propertyVal(fakeDocument, "dir", "rtl");
  });

  describe("snippets", () => {
    it("should send correct event and source when snippet is blocked", () => {
      wrapper.setState({ message: FAKE_MESSAGE });

      wrapper.find(".blockButton").simulate("click");
      assert.propertyVal(
        ASRouterUtils.sendTelemetry.firstCall.args[0],
        "event",
        "BLOCK"
      );
      assert.propertyVal(
        ASRouterUtils.sendTelemetry.firstCall.args[0],
        "source",
        "NEWTAB_FOOTER_BAR"
      );
    });

    it("should not send telemetry when a preview snippet is blocked", () => {
      wrapper.setState({ message: { ...FAKE_MESSAGE, provider: "preview" } });

      wrapper.find(".blockButton").simulate("click");
      assert.notCalled(ASRouterUtils.sendTelemetry);
    });
  });

  describe("impressions", () => {
    function simulateVisibilityChange(value) {
      fakeDocument.visibilityState = value;
    }

    it("should call blockById after CTA link is clicked", () => {
      wrapper.setState({ message: FAKE_MESSAGE });
      sandbox.stub(ASRouterUtils, "blockById").resolves();
      wrapper.instance().sendClick({ target: { dataset: { metric: "" } } });

      assert.calledOnce(ASRouterUtils.blockById);
      assert.calledWith(ASRouterUtils.blockById, FAKE_MESSAGE.id);
    });

    it("should executeAction if defined on the anchor", () => {
      wrapper.setState({ message: FAKE_MESSAGE });
      sandbox.spy(ASRouterUtils, "executeAction");
      wrapper.instance().sendClick({
        target: { dataset: { action: "OPEN_MENU", args: "appMenu" } },
      });

      assert.calledOnce(ASRouterUtils.executeAction);
      assert.calledWithExactly(ASRouterUtils.executeAction, {
        type: "OPEN_MENU",
        data: { args: "appMenu" },
      });
    });

    it("should not call blockById if do_not_autoblock is true", () => {
      wrapper.setState({
        message: {
          ...FAKE_MESSAGE,
          ...{ content: { ...FAKE_MESSAGE.content, do_not_autoblock: true } },
        },
      });
      sandbox.stub(ASRouterUtils, "blockById");
      wrapper.instance().sendClick({ target: { dataset: { metric: "" } } });

      assert.notCalled(ASRouterUtils.blockById);
    });

    it("should not send an impression if no message exists", () => {
      simulateVisibilityChange("visible");

      assert.notCalled(ASRouterUtils.sendTelemetry);
    });

    it("should not send an impression if the page is not visible", () => {
      simulateVisibilityChange("hidden");
      wrapper.setState({ message: FAKE_MESSAGE });

      assert.notCalled(ASRouterUtils.sendTelemetry);
    });

    it("should not send an impression for a preview message", () => {
      wrapper.setState({ message: { ...FAKE_MESSAGE, provider: "preview" } });
      assert.notCalled(ASRouterUtils.sendTelemetry);

      simulateVisibilityChange("visible");
      assert.notCalled(ASRouterUtils.sendTelemetry);
    });

    it("should send an impression ping when there is a message and the page becomes visible", () => {
      wrapper.setState({ message: FAKE_MESSAGE });
      assert.notCalled(ASRouterUtils.sendTelemetry);

      simulateVisibilityChange("visible");
      assert.calledOnce(ASRouterUtils.sendTelemetry);
    });

    it("should send the correct impression source", () => {
      wrapper.setState({ message: FAKE_MESSAGE });
      simulateVisibilityChange("visible");

      assert.calledOnce(ASRouterUtils.sendTelemetry);
      assert.propertyVal(
        ASRouterUtils.sendTelemetry.firstCall.args[0],
        "event",
        "IMPRESSION"
      );
      assert.propertyVal(
        ASRouterUtils.sendTelemetry.firstCall.args[0],
        "source",
        "NEWTAB_FOOTER_BAR"
      );
    });

    it("should send an impression ping when the page is visible and a message gets loaded", () => {
      simulateVisibilityChange("visible");
      wrapper.setState({ message: {} });
      assert.notCalled(ASRouterUtils.sendTelemetry);

      wrapper.setState({ message: FAKE_MESSAGE });
      assert.calledOnce(ASRouterUtils.sendTelemetry);
    });

    it("should send another impression ping if the message id changes", () => {
      simulateVisibilityChange("visible");
      wrapper.setState({ message: FAKE_MESSAGE });
      assert.calledOnce(ASRouterUtils.sendTelemetry);

      wrapper.setState({ message: FAKE_LOCAL_MESSAGES[1] });
      assert.calledTwice(ASRouterUtils.sendTelemetry);
    });

    it("should not send another impression ping if the message id has not changed", () => {
      simulateVisibilityChange("visible");
      wrapper.setState({ message: FAKE_MESSAGE });
      assert.calledOnce(ASRouterUtils.sendTelemetry);

      wrapper.setState({ somethingElse: 123 });
      assert.calledOnce(ASRouterUtils.sendTelemetry);
    });

    it("should not send another impression ping if the message is cleared", () => {
      simulateVisibilityChange("visible");
      wrapper.setState({ message: FAKE_MESSAGE });
      assert.calledOnce(ASRouterUtils.sendTelemetry);

      wrapper.setState({ message: {} });
      assert.calledOnce(ASRouterUtils.sendTelemetry);
    });

    it("should call .sendTelemetry with the right message data", () => {
      simulateVisibilityChange("visible");
      wrapper.setState({ message: FAKE_MESSAGE });

      assert.calledOnce(ASRouterUtils.sendTelemetry);
      const [payload] = ASRouterUtils.sendTelemetry.firstCall.args;

      assert.propertyVal(payload, "message_id", FAKE_MESSAGE.id);
      assert.propertyVal(payload, "event", "IMPRESSION");
      assert.propertyVal(
        payload,
        "action",
        `${FAKE_MESSAGE.provider}_user_event`
      );
      assert.propertyVal(payload, "source", "NEWTAB_FOOTER_BAR");
    });

    it("should construct a OPEN_ABOUT_PAGE action with attribution", () => {
      wrapper.setState({ message: FAKE_MESSAGE });
      const stub = sandbox.stub(ASRouterUtils, "executeAction");

      wrapper.instance().sendClick({
        target: {
          dataset: {
            metric: "",
            entrypoint_value: "snippet",
            action: "OPEN_PREFERENCES_PAGE",
            args: "home",
          },
        },
      });

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, {
        type: "OPEN_PREFERENCES_PAGE",
        data: { args: "home", entrypoint: "snippet" },
      });
    });

    it("should construct a OPEN_ABOUT_PAGE action with attribution", () => {
      wrapper.setState({ message: FAKE_MESSAGE });
      const stub = sandbox.stub(ASRouterUtils, "executeAction");

      wrapper.instance().sendClick({
        target: {
          dataset: {
            metric: "",
            entrypoint_name: "entryPoint",
            entrypoint_value: "snippet",
            action: "OPEN_ABOUT_PAGE",
            args: "logins",
          },
        },
      });

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, {
        type: "OPEN_ABOUT_PAGE",
        data: { args: "logins", entrypoint: "entryPoint=snippet" },
      });
    });
  });

  describe(".fetchFlowParams", () => {
    let dispatchStub;
    const assertCalledWithURL = url =>
      assert.calledWith(globals.fetch, new URL(url).toString(), {
        credentials: "omit",
      });
    beforeEach(() => {
      dispatchStub = sandbox.stub();
      wrapper = mount(
        <ASRouterUISurface
          dispatch={dispatchStub}
          fxaEndpoint="https://accounts.firefox.com"
        />
      );
    });
    it("should use the base url returned from the endpoint pref", async () => {
      wrapper = mount(
        <ASRouterUISurface
          dispatch={dispatchStub}
          fxaEndpoint="https://foo.com"
        />
      );
      await wrapper.instance().fetchFlowParams();

      assertCalledWithURL("https://foo.com/metrics-flow");
    });
    it("should add given search params to the URL", async () => {
      const params = { foo: "1", bar: "2" };

      await wrapper.instance().fetchFlowParams(params);

      assertCalledWithURL(
        "https://accounts.firefox.com/metrics-flow?foo=1&bar=2"
      );
    });
    it("should return flowId, flowBeginTime, deviceId on a 200 response", async () => {
      const flowInfo = { flowId: "foo", flowBeginTime: 123, deviceId: "bar" };
      globals.fetch
        .withArgs("https://accounts.firefox.com/metrics-flow")
        .resolves({
          ok: true,
          status: 200,
          json: () => Promise.resolve(flowInfo),
        });

      const result = await wrapper.instance().fetchFlowParams();
      assert.deepEqual(result, flowInfo);
    });

    describe(".onUserAction", () => {
      it("if the action.type is ENABLE_FIREFOX_MONITOR, it should generate the right monitor URL given some flowParams", async () => {
        const flowInfo = { flowId: "foo", flowBeginTime: 123, deviceId: "bar" };
        globals.fetch
          .withArgs(
            "https://accounts.firefox.com/metrics-flow?utm_term=avocado"
          )
          .resolves({
            ok: true,
            status: 200,
            json: () => Promise.resolve(flowInfo),
          });

        sandbox.spy(ASRouterUtils, "executeAction");

        const msg = {
          type: "ENABLE_FIREFOX_MONITOR",
          data: {
            args: {
              url: "https://monitor.firefox.com?foo=bar",
              flowRequestParams: {
                utm_term: "avocado",
              },
            },
          },
        };

        await wrapper.instance().onUserAction(msg);

        assertCalledWithURL(
          "https://accounts.firefox.com/metrics-flow?utm_term=avocado"
        );
        assert.calledWith(ASRouterUtils.executeAction, {
          type: "OPEN_URL",
          data: {
            args: new URL(
              "https://monitor.firefox.com?foo=bar&deviceId=bar&flowId=foo&flowBeginTime=123"
            ).toString(),
          },
        });
      });
      it("if the action.type is not ENABLE_FIREFOX_MONITOR, it should just call ASRouterUtils.executeAction", async () => {
        const msg = {
          type: "FOO",
          data: {
            args: "bar",
          },
        };
        sandbox.spy(ASRouterUtils, "executeAction");
        await wrapper.instance().onUserAction(msg);
        assert.calledWith(ASRouterUtils.executeAction, msg);
      });
    });
  });
});

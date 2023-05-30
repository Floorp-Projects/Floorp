import { GlobalOverrider } from "test/unit/utils";
import { PanelTestProvider } from "lib/PanelTestProvider.sys.mjs";
import { _MomentsPageHub } from "lib/MomentsPageHub.jsm";
const HOMEPAGE_OVERRIDE_PREF = "browser.startup.homepage_override.once";

describe("MomentsPageHub", () => {
  let globals;
  let sandbox;
  let instance;
  let handleMessageRequestStub;
  let addImpressionStub;
  let blockMessageByIdStub;
  let sendTelemetryStub;
  let getStringPrefStub;
  let setStringPrefStub;
  let setIntervalStub;
  let clearIntervalStub;

  beforeEach(async () => {
    globals = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    instance = new _MomentsPageHub();
    const messages = (await PanelTestProvider.getMessages()).filter(
      ({ template }) => template === "update_action"
    );
    handleMessageRequestStub = sandbox.stub().resolves(messages);
    addImpressionStub = sandbox.stub();
    blockMessageByIdStub = sandbox.stub();
    getStringPrefStub = sandbox.stub();
    setStringPrefStub = sandbox.stub();
    setIntervalStub = sandbox.stub();
    clearIntervalStub = sandbox.stub();
    sendTelemetryStub = sandbox.stub();
    globals.set({
      setInterval: setIntervalStub,
      clearInterval: clearIntervalStub,
      Services: {
        prefs: {
          getStringPref: getStringPrefStub,
          setStringPref: setStringPrefStub,
        },
        telemetry: {
          recordEvent: () => {},
        },
      },
    });
  });

  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });

  it("should create an instance", async () => {
    setIntervalStub.returns(42);
    assert.ok(instance);
    await instance.init(Promise.resolve(), {
      handleMessageRequest: handleMessageRequestStub,
      addImpression: addImpressionStub,
      blockMessageById: blockMessageByIdStub,
    });
    assert.equal(instance.state._intervalId, 42);
  });

  it("should init only once", async () => {
    assert.notCalled(handleMessageRequestStub);

    await instance.init(Promise.resolve(), {
      handleMessageRequest: handleMessageRequestStub,
      addImpression: addImpressionStub,
      blockMessageById: blockMessageByIdStub,
    });
    await instance.init(Promise.resolve(), {
      handleMessageRequest: handleMessageRequestStub,
      addImpression: addImpressionStub,
      blockMessageById: blockMessageByIdStub,
    });

    assert.calledOnce(handleMessageRequestStub);

    instance.uninit();

    await instance.init(Promise.resolve(), {
      handleMessageRequest: handleMessageRequestStub,
      addImpression: addImpressionStub,
      blockMessageById: blockMessageByIdStub,
    });

    assert.calledTwice(handleMessageRequestStub);
  });

  it("should uninit the instance", () => {
    instance.uninit();
    assert.calledOnce(clearIntervalStub);
  });

  it("should setInterval for `checkHomepageOverridePref`", async () => {
    await instance.init(sandbox.stub().resolves(), {});
    sandbox.stub(instance, "checkHomepageOverridePref");

    assert.calledOnce(setIntervalStub);
    assert.calledWithExactly(setIntervalStub, sinon.match.func, 5 * 60 * 1000);

    assert.notCalled(instance.checkHomepageOverridePref);
    const [cb] = setIntervalStub.firstCall.args;

    cb();

    assert.calledOnce(instance.checkHomepageOverridePref);
  });

  describe("#messageRequest", () => {
    beforeEach(async () => {
      await instance.init(Promise.resolve(), {
        handleMessageRequest: handleMessageRequestStub,
        addImpression: addImpressionStub,
        blockMessageById: blockMessageByIdStub,
        sendTelemetry: sendTelemetryStub,
      });
    });
    afterEach(() => {
      instance.uninit();
    });
    it("should fetch a message with the provided trigger and template", async () => {
      await instance.messageRequest({
        triggerId: "trigger",
        template: "template",
      });

      assert.calledTwice(handleMessageRequestStub);
      assert.calledWithExactly(handleMessageRequestStub, {
        triggerId: "trigger",
        template: "template",
        returnAll: true,
      });
    });
    it("shouldn't do anything if no message is provided", async () => {
      // Reset the call from `instance.init`
      setStringPrefStub.reset();
      handleMessageRequestStub.resolves([]);
      await instance.messageRequest({ triggerId: "trigger" });

      assert.notCalled(setStringPrefStub);
    });
    it("should record telemetry events", async () => {
      const startTelemetryStopwatch = sandbox.stub(
        global.TelemetryStopwatch,
        "start"
      );
      const finishTelemetryStopwatch = sandbox.stub(
        global.TelemetryStopwatch,
        "finish"
      );

      await instance.messageRequest({ triggerId: "trigger" });

      assert.calledOnce(startTelemetryStopwatch);
      assert.calledWithExactly(
        startTelemetryStopwatch,
        "MS_MESSAGE_REQUEST_TIME_MS",
        { triggerId: "trigger" }
      );
      assert.calledOnce(finishTelemetryStopwatch);
      assert.calledWithExactly(
        finishTelemetryStopwatch,
        "MS_MESSAGE_REQUEST_TIME_MS",
        { triggerId: "trigger" }
      );
    });
    it("should record Reach event for the Moments page experiment", async () => {
      const momentsMessages = (await PanelTestProvider.getMessages()).filter(
        ({ template }) => template === "update_action"
      );
      const messages = [
        {
          forReachEvent: { sent: false },
          experimentSlug: "foo",
          branchSlug: "bar",
        },
        ...momentsMessages,
      ];
      handleMessageRequestStub.resolves(messages);
      sandbox.spy(global.Services.telemetry, "recordEvent");
      sandbox.spy(instance, "executeAction");

      await instance.messageRequest({ triggerId: "trigger" });

      assert.calledOnce(global.Services.telemetry.recordEvent);
      assert.calledOnce(instance.executeAction);
    });
    it("should not record the Reach event if it's already sent", async () => {
      const messages = [
        {
          forReachEvent: { sent: true },
          experimentSlug: "foo",
          branchSlug: "bar",
        },
      ];
      handleMessageRequestStub.resolves(messages);
      sandbox.spy(global.Services.telemetry, "recordEvent");

      await instance.messageRequest({ triggerId: "trigger" });

      assert.notCalled(global.Services.telemetry.recordEvent);
    });
    it("should not trigger the action if it's only for the Reach event", async () => {
      const messages = [
        {
          forReachEvent: { sent: false },
          experimentSlug: "foo",
          branchSlug: "bar",
        },
      ];
      handleMessageRequestStub.resolves(messages);
      sandbox.spy(global.Services.telemetry, "recordEvent");
      sandbox.spy(instance, "executeAction");

      await instance.messageRequest({ triggerId: "trigger" });

      assert.calledOnce(global.Services.telemetry.recordEvent);
      assert.notCalled(instance.executeAction);
    });
  });
  describe("executeAction", () => {
    beforeEach(async () => {
      blockMessageByIdStub = sandbox.stub();
      await instance.init(sandbox.stub().resolves(), {
        addImpression: addImpressionStub,
        blockMessageById: blockMessageByIdStub,
        sendTelemetry: sendTelemetryStub,
      });
    });
    it("should set HOMEPAGE_OVERRIDE_PREF on `moments-wnp` action", async () => {
      const [msg] = await handleMessageRequestStub();
      sandbox.useFakeTimers();
      instance.executeAction(msg);

      assert.calledOnce(setStringPrefStub);
      assert.calledWithExactly(
        setStringPrefStub,
        HOMEPAGE_OVERRIDE_PREF,
        JSON.stringify({
          message_id: msg.id,
          url: msg.content.action.data.url,
          expire: instance.getExpirationDate(
            msg.content.action.data.expireDelta
          ),
        })
      );
    });
    it("should block after taking the action", async () => {
      const [msg] = await handleMessageRequestStub();
      instance.executeAction(msg);

      assert.calledOnce(blockMessageByIdStub);
      assert.calledWithExactly(blockMessageByIdStub, msg.id);
    });
    it("should compute expire based on expireDelta", async () => {
      sandbox.spy(instance, "getExpirationDate");

      const [msg] = await handleMessageRequestStub();
      instance.executeAction(msg);

      assert.calledOnce(instance.getExpirationDate);
      assert.calledWithExactly(
        instance.getExpirationDate,
        msg.content.action.data.expireDelta
      );
    });
    it("should compute expire based on expireDelta", async () => {
      sandbox.spy(instance, "getExpirationDate");

      const [msg] = await handleMessageRequestStub();
      const msgWithExpire = {
        ...msg,
        content: {
          ...msg.content,
          action: {
            ...msg.content.action,
            data: { ...msg.content.action.data, expire: 41 },
          },
        },
      };
      instance.executeAction(msgWithExpire);

      assert.notCalled(instance.getExpirationDate);
      assert.calledOnce(setStringPrefStub);
      assert.calledWithExactly(
        setStringPrefStub,
        HOMEPAGE_OVERRIDE_PREF,
        JSON.stringify({
          message_id: msg.id,
          url: msg.content.action.data.url,
          expire: 41,
        })
      );
    });
    it("should send user telemetry", async () => {
      const [msg] = await handleMessageRequestStub();
      const sendUserEventTelemetrySpy = sandbox.spy(
        instance,
        "sendUserEventTelemetry"
      );
      instance.executeAction(msg);

      assert.calledOnce(sendTelemetryStub);
      assert.calledWithExactly(sendUserEventTelemetrySpy, msg);
      assert.calledWithExactly(sendTelemetryStub, {
        type: "MOMENTS_PAGE_TELEMETRY",
        data: {
          action: "moments_user_event",
          bucket_id: "WNP_THANK_YOU",
          event: "MOMENTS_PAGE_SET",
          message_id: "WNP_THANK_YOU",
        },
      });
    });
  });
  describe("#checkHomepageOverridePref", () => {
    let messageRequestStub;
    beforeEach(() => {
      messageRequestStub = sandbox.stub(instance, "messageRequest");
    });
    it("should catch parse errors", () => {
      getStringPrefStub.returns({});

      instance.checkHomepageOverridePref();

      assert.calledOnce(messageRequestStub);
      assert.calledWithExactly(messageRequestStub, {
        template: "update_action",
        triggerId: "momentsUpdate",
      });
    });
  });
});

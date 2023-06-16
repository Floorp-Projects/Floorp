import { _ToolbarBadgeHub } from "lib/ToolbarBadgeHub.jsm";
import { GlobalOverrider } from "test/unit/utils";
import { OnboardingMessageProvider } from "lib/OnboardingMessageProvider.jsm";
import { _ToolbarPanelHub, ToolbarPanelHub } from "lib/ToolbarPanelHub.jsm";

describe("ToolbarBadgeHub", () => {
  let sandbox;
  let instance;
  let fakeAddImpression;
  let fakeSendTelemetry;
  let isBrowserPrivateStub;
  let fxaMessage;
  let whatsnewMessage;
  let fakeElement;
  let globals;
  let everyWindowStub;
  let clearTimeoutStub;
  let setTimeoutStub;
  let addObserverStub;
  let removeObserverStub;
  let getStringPrefStub;
  let clearUserPrefStub;
  let setStringPrefStub;
  let requestIdleCallbackStub;
  let fakeWindow;
  beforeEach(async () => {
    globals = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    instance = new _ToolbarBadgeHub();
    fakeAddImpression = sandbox.stub();
    fakeSendTelemetry = sandbox.stub();
    isBrowserPrivateStub = sandbox.stub();
    const onboardingMsgs =
      await OnboardingMessageProvider.getUntranslatedMessages();
    fxaMessage = onboardingMsgs.find(({ id }) => id === "FXA_ACCOUNTS_BADGE");
    whatsnewMessage = {
      id: `WHATS_NEW_BADGE_71`,
      template: "toolbar_badge",
      content: {
        delay: 1000,
        target: "whats-new-menu-button",
        action: { id: "show-whatsnew-button" },
        badgeDescription: { string_id: "cfr-badge-reader-label-newfeature" },
      },
      priority: 1,
      trigger: { id: "toolbarBadgeUpdate" },
      frequency: {
        // Makes it so that we track impressions for this message while at the
        // same time it can have unlimited impressions
        lifetime: Infinity,
      },
      // Never saw this message or saw it in the past 4 days or more recent
      targeting: `isWhatsNewPanelEnabled &&
      (!messageImpressions['WHATS_NEW_BADGE_71'] ||
        (messageImpressions['WHATS_NEW_BADGE_71']|length >= 1 &&
          currentDate|date - messageImpressions['WHATS_NEW_BADGE_71'][0] <= 4 * 24 * 3600 * 1000))`,
    };
    fakeElement = {
      classList: {
        add: sandbox.stub(),
        remove: sandbox.stub(),
      },
      setAttribute: sandbox.stub(),
      removeAttribute: sandbox.stub(),
      querySelector: sandbox.stub(),
      addEventListener: sandbox.stub(),
      remove: sandbox.stub(),
      appendChild: sandbox.stub(),
    };
    // Share the same element when selecting child nodes
    fakeElement.querySelector.returns(fakeElement);
    everyWindowStub = {
      registerCallback: sandbox.stub(),
      unregisterCallback: sandbox.stub(),
    };
    clearTimeoutStub = sandbox.stub();
    setTimeoutStub = sandbox.stub();
    fakeWindow = {
      MozXULElement: { insertFTLIfNeeded: sandbox.stub() },
      ownerGlobal: {
        gBrowser: {
          selectedBrowser: "browser",
        },
      },
    };
    addObserverStub = sandbox.stub();
    removeObserverStub = sandbox.stub();
    getStringPrefStub = sandbox.stub();
    clearUserPrefStub = sandbox.stub();
    setStringPrefStub = sandbox.stub();
    requestIdleCallbackStub = sandbox.stub().callsFake(fn => fn());
    globals.set({
      ToolbarPanelHub,
      requestIdleCallback: requestIdleCallbackStub,
      EveryWindow: everyWindowStub,
      PrivateBrowsingUtils: { isBrowserPrivate: isBrowserPrivateStub },
      setTimeout: setTimeoutStub,
      clearTimeout: clearTimeoutStub,
      Services: {
        wm: {
          getMostRecentWindow: () => fakeWindow,
        },
        prefs: {
          addObserver: addObserverStub,
          removeObserver: removeObserverStub,
          getStringPref: getStringPrefStub,
          clearUserPref: clearUserPrefStub,
          setStringPref: setStringPrefStub,
        },
      },
    });
  });
  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });
  it("should create an instance", () => {
    assert.ok(instance);
  });
  describe("#init", () => {
    it("should make a single messageRequest on init", async () => {
      sandbox.stub(instance, "messageRequest");
      const waitForInitialized = sandbox.stub().resolves();

      await instance.init(waitForInitialized, {});
      await instance.init(waitForInitialized, {});
      assert.calledOnce(instance.messageRequest);
      assert.calledWithExactly(instance.messageRequest, {
        template: "toolbar_badge",
        triggerId: "toolbarBadgeUpdate",
      });

      instance.uninit();

      await instance.init(waitForInitialized, {});

      assert.calledTwice(instance.messageRequest);
    });
    it("should add a pref observer", async () => {
      await instance.init(sandbox.stub().resolves(), {});

      assert.calledOnce(addObserverStub);
      assert.calledWithExactly(
        addObserverStub,
        instance.prefs.WHATSNEW_TOOLBAR_PANEL,
        instance
      );
    });
  });
  describe("#uninit", () => {
    beforeEach(async () => {
      await instance.init(sandbox.stub().resolves(), {});
    });
    it("should clear any setTimeout cbs", async () => {
      await instance.init(sandbox.stub().resolves(), {});

      instance.state.showBadgeTimeoutId = 2;

      instance.uninit();

      assert.calledOnce(clearTimeoutStub);
      assert.calledWithExactly(clearTimeoutStub, 2);
    });
    it("should remove the pref observer", () => {
      instance.uninit();

      assert.calledOnce(removeObserverStub);
      assert.calledWithExactly(
        removeObserverStub,
        instance.prefs.WHATSNEW_TOOLBAR_PANEL,
        instance
      );
    });
  });
  describe("messageRequest", () => {
    let handleMessageRequestStub;
    beforeEach(() => {
      handleMessageRequestStub = sandbox.stub().returns(fxaMessage);
      sandbox
        .stub(instance, "_handleMessageRequest")
        .value(handleMessageRequestStub);
      sandbox.stub(instance, "registerBadgeNotificationListener");
    });
    it("should fetch a message with the provided trigger and template", async () => {
      await instance.messageRequest({
        triggerId: "trigger",
        template: "template",
      });

      assert.calledOnce(handleMessageRequestStub);
      assert.calledWithExactly(handleMessageRequestStub, {
        triggerId: "trigger",
        template: "template",
      });
    });
    it("should call addToolbarNotification with browser window and message", async () => {
      await instance.messageRequest("trigger");

      assert.calledOnce(instance.registerBadgeNotificationListener);
      assert.calledWithExactly(
        instance.registerBadgeNotificationListener,
        fxaMessage
      );
    });
    it("shouldn't do anything if no message is provided", async () => {
      handleMessageRequestStub.resolves(null);
      await instance.messageRequest({ triggerId: "trigger" });

      assert.notCalled(instance.registerBadgeNotificationListener);
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
      handleMessageRequestStub.returns(null);

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
  });
  describe("addToolbarNotification", () => {
    let target;
    let fakeDocument;
    beforeEach(async () => {
      await instance.init(sandbox.stub().resolves(), {
        addImpression: fakeAddImpression,
        sendTelemetry: fakeSendTelemetry,
      });
      fakeDocument = {
        getElementById: sandbox.stub().returns(fakeElement),
        createElement: sandbox.stub().returns(fakeElement),
        l10n: { setAttributes: sandbox.stub() },
      };
      target = { ...fakeWindow, browser: { ownerDocument: fakeDocument } };
    });
    afterEach(() => {
      instance.uninit();
    });
    it("shouldn't do anything if target element is not found", () => {
      fakeDocument.getElementById.returns(null);
      instance.addToolbarNotification(target, fxaMessage);

      assert.notCalled(fakeElement.setAttribute);
    });
    it("should target the element specified in the message", () => {
      instance.addToolbarNotification(target, fxaMessage);

      assert.calledOnce(fakeDocument.getElementById);
      assert.calledWithExactly(
        fakeDocument.getElementById,
        fxaMessage.content.target
      );
    });
    it("should show a notification", () => {
      instance.addToolbarNotification(target, fxaMessage);

      assert.calledOnce(fakeElement.setAttribute);
      assert.calledWithExactly(fakeElement.setAttribute, "badged", true);
      assert.calledWithExactly(fakeElement.classList.add, "feature-callout");
    });
    it("should attach a cb on the notification", () => {
      instance.addToolbarNotification(target, fxaMessage);

      assert.calledTwice(fakeElement.addEventListener);
      assert.calledWithExactly(
        fakeElement.addEventListener,
        "mousedown",
        instance.removeAllNotifications
      );
      assert.calledWithExactly(
        fakeElement.addEventListener,
        "keypress",
        instance.removeAllNotifications
      );
    });
    it("should execute actions if they exist", () => {
      sandbox.stub(instance, "executeAction");
      instance.addToolbarNotification(target, whatsnewMessage);

      assert.calledOnce(instance.executeAction);
      assert.calledWithExactly(instance.executeAction, {
        ...whatsnewMessage.content.action,
        message_id: whatsnewMessage.id,
      });
    });
    it("should create a description element", () => {
      sandbox.stub(instance, "executeAction");
      instance.addToolbarNotification(target, whatsnewMessage);

      assert.calledOnce(fakeDocument.createElement);
      assert.calledWithExactly(fakeDocument.createElement, "span");
    });
    it("should set description id to element and to button", () => {
      sandbox.stub(instance, "executeAction");
      instance.addToolbarNotification(target, whatsnewMessage);

      assert.calledWithExactly(
        fakeElement.setAttribute,
        "id",
        "toolbarbutton-notification-description"
      );
      assert.calledWithExactly(
        fakeElement.setAttribute,
        "aria-labelledby",
        `toolbarbutton-notification-description ${whatsnewMessage.content.target}`
      );
    });
    it("should attach fluent id to description", () => {
      sandbox.stub(instance, "executeAction");
      instance.addToolbarNotification(target, whatsnewMessage);

      assert.calledOnce(fakeDocument.l10n.setAttributes);
      assert.calledWithExactly(
        fakeDocument.l10n.setAttributes,
        fakeElement,
        whatsnewMessage.content.badgeDescription.string_id
      );
    });
    it("should add an impression for the message", () => {
      instance.addToolbarNotification(target, whatsnewMessage);

      assert.calledOnce(instance._addImpression);
      assert.calledWithExactly(instance._addImpression, whatsnewMessage);
    });
    it("should send an impression ping", async () => {
      sandbox.stub(instance, "sendUserEventTelemetry");
      instance.addToolbarNotification(target, whatsnewMessage);

      assert.calledOnce(instance.sendUserEventTelemetry);
      assert.calledWithExactly(
        instance.sendUserEventTelemetry,
        "IMPRESSION",
        whatsnewMessage
      );
    });
  });
  describe("registerBadgeNotificationListener", () => {
    let msg_no_delay;
    beforeEach(async () => {
      await instance.init(sandbox.stub().resolves(), {
        addImpression: fakeAddImpression,
        sendTelemetry: fakeSendTelemetry,
      });
      sandbox.stub(instance, "addToolbarNotification").returns(fakeElement);
      sandbox.stub(instance, "removeToolbarNotification");
      msg_no_delay = {
        ...fxaMessage,
        content: {
          ...fxaMessage.content,
          delay: 0,
        },
      };
    });
    afterEach(() => {
      instance.uninit();
    });
    it("should register a callback that adds/removes the notification", () => {
      instance.registerBadgeNotificationListener(msg_no_delay);

      assert.calledOnce(everyWindowStub.registerCallback);
      assert.calledWithExactly(
        everyWindowStub.registerCallback,
        instance.id,
        sinon.match.func,
        sinon.match.func
      );

      const [, initFn, uninitFn] =
        everyWindowStub.registerCallback.firstCall.args;

      initFn(window);
      // Test that it doesn't try to add a second notification
      initFn(window);

      assert.calledOnce(instance.addToolbarNotification);
      assert.calledWithExactly(
        instance.addToolbarNotification,
        window,
        msg_no_delay
      );

      uninitFn(window);

      assert.calledOnce(instance.removeToolbarNotification);
      assert.calledWithExactly(instance.removeToolbarNotification, fakeElement);
    });
    it("should unregister notifications when forcing a badge via devtools", () => {
      instance.registerBadgeNotificationListener(msg_no_delay, { force: true });

      assert.calledOnce(everyWindowStub.unregisterCallback);
      assert.calledWithExactly(everyWindowStub.unregisterCallback, instance.id);
    });
    it("should only call executeAction for 'update_action' messages", () => {
      const stub = sandbox.stub(instance, "executeAction");
      const updateActionMsg = { ...msg_no_delay, template: "update_action" };

      instance.registerBadgeNotificationListener(updateActionMsg);

      assert.notCalled(everyWindowStub.registerCallback);
      assert.calledOnce(stub);
    });
  });
  describe("executeAction", () => {
    let blockMessageByIdStub;
    beforeEach(async () => {
      blockMessageByIdStub = sandbox.stub();
      await instance.init(sandbox.stub().resolves(), {
        blockMessageById: blockMessageByIdStub,
      });
    });
    it("should call ToolbarPanelHub.enableToolbarButton", () => {
      const stub = sandbox.stub(
        _ToolbarPanelHub.prototype,
        "enableToolbarButton"
      );

      instance.executeAction({ id: "show-whatsnew-button" });

      assert.calledOnce(stub);
    });
    it("should call ToolbarPanelHub.enableAppmenuButton", () => {
      const stub = sandbox.stub(
        _ToolbarPanelHub.prototype,
        "enableAppmenuButton"
      );

      instance.executeAction({ id: "show-whatsnew-button" });

      assert.calledOnce(stub);
    });
  });
  describe("removeToolbarNotification", () => {
    it("should remove the notification", () => {
      instance.removeToolbarNotification(fakeElement);

      assert.calledThrice(fakeElement.removeAttribute);
      assert.calledWithExactly(fakeElement.removeAttribute, "badged");
      assert.calledWithExactly(fakeElement.removeAttribute, "aria-labelledby");
      assert.calledWithExactly(fakeElement.removeAttribute, "aria-describedby");
      assert.calledOnce(fakeElement.classList.remove);
      assert.calledWithExactly(fakeElement.classList.remove, "feature-callout");
      assert.calledOnce(fakeElement.remove);
    });
  });
  describe("removeAllNotifications", () => {
    let blockMessageByIdStub;
    let fakeEvent;
    beforeEach(async () => {
      await instance.init(sandbox.stub().resolves(), {
        sendTelemetry: fakeSendTelemetry,
      });
      blockMessageByIdStub = sandbox.stub();
      sandbox.stub(instance, "_blockMessageById").value(blockMessageByIdStub);
      instance.state = { notification: { id: fxaMessage.id } };
      fakeEvent = { target: { removeEventListener: sandbox.stub() } };
    });
    it("should call to block the message", () => {
      instance.removeAllNotifications();

      assert.calledOnce(blockMessageByIdStub);
      assert.calledWithExactly(blockMessageByIdStub, fxaMessage.id);
    });
    it("should remove the window listener", () => {
      instance.removeAllNotifications();

      assert.calledOnce(everyWindowStub.unregisterCallback);
      assert.calledWithExactly(everyWindowStub.unregisterCallback, instance.id);
    });
    it("should ignore right mouse button (mousedown event)", () => {
      fakeEvent.type = "mousedown";
      fakeEvent.button = 1; // not left click

      instance.removeAllNotifications(fakeEvent);

      assert.notCalled(fakeEvent.target.removeEventListener);
      assert.notCalled(everyWindowStub.unregisterCallback);
    });
    it("should ignore right mouse button (click event)", () => {
      fakeEvent.type = "click";
      fakeEvent.button = 1; // not left click

      instance.removeAllNotifications(fakeEvent);

      assert.notCalled(fakeEvent.target.removeEventListener);
      assert.notCalled(everyWindowStub.unregisterCallback);
    });
    it("should ignore keypresses that are not meant to focus the target", () => {
      fakeEvent.type = "keypress";
      fakeEvent.key = "\t"; // not enter

      instance.removeAllNotifications(fakeEvent);

      assert.notCalled(fakeEvent.target.removeEventListener);
      assert.notCalled(everyWindowStub.unregisterCallback);
    });
    it("should remove the event listeners after succesfully focusing the element", () => {
      fakeEvent.type = "click";
      fakeEvent.button = 0;

      instance.removeAllNotifications(fakeEvent);

      assert.calledTwice(fakeEvent.target.removeEventListener);
      assert.calledWithExactly(
        fakeEvent.target.removeEventListener,
        "mousedown",
        instance.removeAllNotifications
      );
      assert.calledWithExactly(
        fakeEvent.target.removeEventListener,
        "keypress",
        instance.removeAllNotifications
      );
    });
    it("should send telemetry", () => {
      fakeEvent.type = "click";
      fakeEvent.button = 0;
      sandbox.stub(instance, "sendUserEventTelemetry");

      instance.removeAllNotifications(fakeEvent);

      assert.calledOnce(instance.sendUserEventTelemetry);
      assert.calledWithExactly(instance.sendUserEventTelemetry, "CLICK", {
        id: "FXA_ACCOUNTS_BADGE",
      });
    });
    it("should remove the event listeners after succesfully focusing the element", () => {
      fakeEvent.type = "keypress";
      fakeEvent.key = "Enter";

      instance.removeAllNotifications(fakeEvent);

      assert.calledTwice(fakeEvent.target.removeEventListener);
      assert.calledWithExactly(
        fakeEvent.target.removeEventListener,
        "mousedown",
        instance.removeAllNotifications
      );
      assert.calledWithExactly(
        fakeEvent.target.removeEventListener,
        "keypress",
        instance.removeAllNotifications
      );
    });
  });
  describe("message with delay", () => {
    let msg_with_delay;
    beforeEach(async () => {
      await instance.init(sandbox.stub().resolves(), {
        addImpression: fakeAddImpression,
      });
      msg_with_delay = {
        ...fxaMessage,
        content: {
          ...fxaMessage.content,
          delay: 500,
        },
      };
      sandbox.stub(instance, "registerBadgeToAllWindows");
    });
    afterEach(() => {
      instance.uninit();
    });
    it("should register a cb to fire after msg.content.delay ms", () => {
      instance.registerBadgeNotificationListener(msg_with_delay);

      assert.calledOnce(setTimeoutStub);
      assert.calledWithExactly(
        setTimeoutStub,
        sinon.match.func,
        msg_with_delay.content.delay
      );

      const [cb] = setTimeoutStub.firstCall.args;

      assert.notCalled(instance.registerBadgeToAllWindows);

      cb();

      assert.calledOnce(instance.registerBadgeToAllWindows);
      assert.calledWithExactly(
        instance.registerBadgeToAllWindows,
        msg_with_delay
      );
      // Delayed actions should be executed inside requestIdleCallback
      assert.calledOnce(requestIdleCallbackStub);
    });
  });
  describe("#sendUserEventTelemetry", () => {
    beforeEach(async () => {
      await instance.init(sandbox.stub().resolves(), {
        sendTelemetry: fakeSendTelemetry,
      });
    });
    it("should check for private window and not send", () => {
      isBrowserPrivateStub.returns(true);

      instance.sendUserEventTelemetry("CLICK", { id: fxaMessage });

      assert.notCalled(instance._sendTelemetry);
    });
    it("should check for private window and send", () => {
      isBrowserPrivateStub.returns(false);

      instance.sendUserEventTelemetry("CLICK", { id: fxaMessage });

      assert.calledOnce(fakeSendTelemetry);
      const [ping] = instance._sendTelemetry.firstCall.args;
      assert.propertyVal(ping, "type", "TOOLBAR_BADGE_TELEMETRY");
      assert.propertyVal(ping.data, "event", "CLICK");
    });
  });
  describe("#observe", () => {
    it("should make a message request when the whats new pref is changed", () => {
      sandbox.stub(instance, "messageRequest");

      instance.observe("", "", instance.prefs.WHATSNEW_TOOLBAR_PANEL);

      assert.calledOnce(instance.messageRequest);
      assert.calledWithExactly(instance.messageRequest, {
        template: "toolbar_badge",
        triggerId: "toolbarBadgeUpdate",
      });
    });
    it("should not react to other pref changes", () => {
      sandbox.stub(instance, "messageRequest");

      instance.observe("", "", "foo");

      assert.notCalled(instance.messageRequest);
    });
  });
});

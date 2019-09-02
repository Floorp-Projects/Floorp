import { _ToolbarBadgeHub } from "lib/ToolbarBadgeHub.jsm";
import { GlobalOverrider } from "test/unit/utils";
import { OnboardingMessageProvider } from "lib/OnboardingMessageProvider.jsm";
import { _ToolbarPanelHub } from "lib/ToolbarPanelHub.jsm";

describe("ToolbarBadgeHub", () => {
  let sandbox;
  let instance;
  let fakeAddImpression;
  let fakeDispatch;
  let isBrowserPrivateStub;
  let fxaMessage;
  let whatsnewMessage;
  let fakeElement;
  let globals;
  let everyWindowStub;
  let clearTimeoutStub;
  let setTimeoutStub;
  let setIntervalStub;
  let addObserverStub;
  let removeObserverStub;
  let getStringPrefStub;
  let clearUserPrefStub;
  let setStringPrefStub;
  let requestIdleCallbackStub;
  beforeEach(async () => {
    globals = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    instance = new _ToolbarBadgeHub();
    fakeAddImpression = sandbox.stub();
    fakeDispatch = sandbox.stub();
    isBrowserPrivateStub = sandbox.stub();
    const onboardingMsgs = await OnboardingMessageProvider.getUntranslatedMessages();
    fxaMessage = onboardingMsgs.find(({ id }) => id === "FXA_ACCOUNTS_BADGE");
    whatsnewMessage = onboardingMsgs.find(({ id }) =>
      id.includes("WHATS_NEW_BADGE_")
    );
    fakeElement = {
      classList: {
        add: sandbox.stub(),
        remove: sandbox.stub(),
      },
      setAttribute: sandbox.stub(),
      removeAttribute: sandbox.stub(),
      querySelector: sandbox.stub(),
      addEventListener: sandbox.stub(),
    };
    // Share the same element when selecting child nodes
    fakeElement.querySelector.returns(fakeElement);
    everyWindowStub = {
      registerCallback: sandbox.stub(),
      unregisterCallback: sandbox.stub(),
    };
    clearTimeoutStub = sandbox.stub();
    setTimeoutStub = sandbox.stub();
    setIntervalStub = sandbox.stub();
    const fakeWindow = {
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
      requestIdleCallback: requestIdleCallbackStub,
      EveryWindow: everyWindowStub,
      PrivateBrowsingUtils: { isBrowserPrivate: isBrowserPrivateStub },
      setTimeout: setTimeoutStub,
      clearTimeout: clearTimeoutStub,
      setInterval: setIntervalStub,
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
    it("should make a messageRequest on init", async () => {
      sandbox.stub(instance, "messageRequest");
      const waitForInitialized = sandbox.stub().resolves();

      await instance.init(waitForInitialized, {});
      assert.calledOnce(instance.messageRequest);
      assert.calledWithExactly(instance.messageRequest, {
        template: "toolbar_badge",
        triggerId: "toolbarBadgeUpdate",
      });
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
    it("should setInterval for `checkHomepageOverridePref`", async () => {
      await instance.init(sandbox.stub().resolves(), {});
      sandbox.stub(instance, "checkHomepageOverridePref");

      assert.calledOnce(setIntervalStub);
      assert.calledWithExactly(
        setIntervalStub,
        sinon.match.func,
        5 * 60 * 1000
      );

      assert.notCalled(instance.checkHomepageOverridePref);
      const [cb] = setIntervalStub.firstCall.args;

      cb();

      assert.calledOnce(instance.checkHomepageOverridePref);
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
    it("shouldn't do anything if no message is provided", () => {
      handleMessageRequestStub.returns(null);
      instance.messageRequest("trigger");

      assert.notCalled(instance.registerBadgeNotificationListener);
    });
  });
  describe("addToolbarNotification", () => {
    let target;
    let fakeDocument;
    beforeEach(() => {
      fakeDocument = { getElementById: sandbox.stub().returns(fakeElement) };
      target = { browser: { ownerDocument: fakeDocument } };
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
        "click",
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
  });
  describe("registerBadgeNotificationListener", () => {
    let msg_no_delay;
    beforeEach(async () => {
      await instance.init(sandbox.stub().resolves(), {
        addImpression: fakeAddImpression,
        dispatch: fakeDispatch,
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
    it("should add an impression for the message", () => {
      instance.registerBadgeNotificationListener(msg_no_delay);

      assert.calledOnce(instance._addImpression);
      assert.calledWithExactly(instance._addImpression, msg_no_delay);
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

      const [
        ,
        initFn,
        uninitFn,
      ] = everyWindowStub.registerCallback.firstCall.args;

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
    it("should send an impression", async () => {
      sandbox.stub(instance, "sendUserEventTelemetry");

      instance.registerBadgeNotificationListener(msg_no_delay);

      assert.calledOnce(instance.sendUserEventTelemetry);
      assert.calledWithExactly(
        instance.sendUserEventTelemetry,
        "IMPRESSION",
        msg_no_delay
      );
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
    it("should set HOMEPAGE_OVERRIDE_PREF on `moments-wnp` action", () => {
      instance.executeAction({
        id: "moments-wnp",
        data: {
          url: "foo.com",
          expire: 1,
        },
        message_id: "bar",
      });

      assert.calledOnce(setStringPrefStub);
      assert.calledWithExactly(
        setStringPrefStub,
        instance.prefs.HOMEPAGE_OVERRIDE_PREF,
        JSON.stringify({ message_id: "bar", url: "foo.com", expire: 1 })
      );
    });
    it("should block after taking the action", () => {
      instance.executeAction({
        id: "moments-wnp",
        data: {
          url: "foo.com",
          expire: 1,
        },
        message_id: "bar",
      });

      assert.calledOnce(blockMessageByIdStub);
      assert.calledWithExactly(blockMessageByIdStub, "bar");
    });
    it("should compute expire based on expireDelta", () => {
      sandbox.spy(instance, "getExpirationDate");

      instance.executeAction({
        id: "moments-wnp",
        data: {
          url: "foo.com",
          expireDelta: 10,
        },
        message_id: "bar",
      });

      assert.calledOnce(instance.getExpirationDate);
      assert.calledWithExactly(instance.getExpirationDate, 10);
    });
  });
  describe("removeToolbarNotification", () => {
    it("should remove the notification", () => {
      instance.removeToolbarNotification(fakeElement);

      assert.calledOnce(fakeElement.removeAttribute);
      assert.calledWithExactly(fakeElement.removeAttribute, "badged");
      assert.calledOnce(fakeElement.classList.remove);
      assert.calledWithExactly(fakeElement.classList.remove, "feature-callout");
    });
  });
  describe("removeAllNotifications", () => {
    let blockMessageByIdStub;
    let fakeEvent;
    beforeEach(async () => {
      await instance.init(sandbox.stub().resolves(), {
        dispatch: fakeDispatch,
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
        "click",
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
        "click",
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
        dispatch: fakeDispatch,
      });
    });
    it("should check for private window and not send", () => {
      isBrowserPrivateStub.returns(true);

      instance.sendUserEventTelemetry("CLICK", { id: fxaMessage });

      assert.notCalled(instance._dispatch);
    });
    it("should check for private window and send", () => {
      isBrowserPrivateStub.returns(false);

      instance.sendUserEventTelemetry("CLICK", { id: fxaMessage });

      assert.calledOnce(fakeDispatch);
      const [ping] = instance._dispatch.firstCall.args;
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
  describe("#checkHomepageOverridePref", () => {
    let messageRequestStub;
    let unblockMessageByIdStub;
    beforeEach(async () => {
      unblockMessageByIdStub = sandbox.stub();
      await instance.init(sandbox.stub().resolves(), {
        unblockMessageById: unblockMessageByIdStub,
      });
      messageRequestStub = sandbox.stub(instance, "messageRequest");
    });
    it("should reset HOMEPAGE_OVERRIDE_PREF if set", () => {
      getStringPrefStub.returns(true);

      instance.checkHomepageOverridePref();

      assert.calledOnce(getStringPrefStub);
      assert.calledWithExactly(
        getStringPrefStub,
        instance.prefs.HOMEPAGE_OVERRIDE_PREF,
        ""
      );
      assert.calledOnce(clearUserPrefStub);
      assert.calledWithExactly(
        clearUserPrefStub,
        instance.prefs.HOMEPAGE_OVERRIDE_PREF
      );
    });
    it("should unblock the message set in the pref", () => {
      getStringPrefStub.returns(JSON.stringify({ message_id: "foo" }));

      instance.checkHomepageOverridePref();

      assert.calledOnce(unblockMessageByIdStub);
      assert.calledWithExactly(unblockMessageByIdStub, "foo");
    });
    it("should catch parse errors", () => {
      getStringPrefStub.returns({});

      instance.checkHomepageOverridePref();

      assert.notCalled(unblockMessageByIdStub);
      assert.calledOnce(messageRequestStub);
      assert.calledWithExactly(messageRequestStub, {
        template: "update_action",
        triggerId: "momentsUpdate",
      });
    });
  });
});

import { _ToolbarBadgeHub } from "lib/ToolbarBadgeHub.jsm";
import { GlobalOverrider } from "test/unit/utils";
import { PanelTestProvider } from "lib/PanelTestProvider.jsm";
import { _ToolbarPanelHub } from "lib/ToolbarPanelHub.jsm";

describe("ToolbarBadgeHub", () => {
  let sandbox;
  let instance;
  let fakeAddImpression;
  let fxaMessage;
  let whatsnewMessage;
  let fakeElement;
  let globals;
  let everyWindowStub;
  let clearTimeoutStub;
  let setTimeoutStub;
  beforeEach(async () => {
    globals = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    instance = new _ToolbarBadgeHub();
    fakeAddImpression = sandbox.stub();
    const msgs = await PanelTestProvider.getMessages();
    fxaMessage = msgs.find(({ id }) => id === "FXA_ACCOUNTS_BADGE");
    whatsnewMessage = msgs.find(({ id }) => id.includes("WHATS_NEW_BADGE_"));
    fakeElement = {
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
    globals.set("EveryWindow", everyWindowStub);
    globals.set("setTimeout", setTimeoutStub);
    globals.set("clearTimeout", clearTimeoutStub);
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
      assert.calledWithExactly(instance.messageRequest, "toolbarBadgeUpdate");
    });
  });
  describe("#uninit", () => {
    it("should clear any setTimeout cbs", () => {
      instance.init(sandbox.stub().resolves(), {});

      instance.state.showBadgeTimeoutId = 2;

      instance.uninit();

      assert.calledOnce(clearTimeoutStub);
      assert.calledWithExactly(clearTimeoutStub, 2);
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
      await instance.messageRequest("trigger");

      assert.calledOnce(handleMessageRequestStub);
      assert.calledWithExactly(handleMessageRequestStub, {
        triggerId: "trigger",
        template: instance.template,
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

      assert.calledTwice(fakeElement.setAttribute);
      assert.calledWithExactly(fakeElement.setAttribute, "badged", true);
      assert.calledWithExactly(fakeElement.setAttribute, "value", "x");
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
      assert.calledWithExactly(
        instance.executeAction,
        whatsnewMessage.content.action
      );
    });
  });
  describe("registerBadgeNotificationListener", () => {
    beforeEach(() => {
      instance.init(sandbox.stub().resolves(), {
        addImpression: fakeAddImpression,
      });
      sandbox.stub(instance, "addToolbarNotification").returns(fakeElement);
      sandbox.stub(instance, "removeToolbarNotification");
    });
    afterEach(() => {
      instance.uninit();
    });
    it("should add an impression for the message", () => {
      instance.registerBadgeNotificationListener(fxaMessage);

      assert.calledOnce(instance._addImpression);
      assert.calledWithExactly(instance._addImpression, fxaMessage);
    });
    it("should register a callback that adds/removes the notification", () => {
      instance.registerBadgeNotificationListener(fxaMessage);

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
        fxaMessage
      );

      uninitFn(window);

      assert.calledOnce(instance.removeToolbarNotification);
      assert.calledWithExactly(instance.removeToolbarNotification, fakeElement);
    });
    it("should unregister notifications when forcing a badge via devtools", () => {
      instance.registerBadgeNotificationListener(fxaMessage, { force: true });

      assert.calledOnce(everyWindowStub.unregisterCallback);
      assert.calledWithExactly(everyWindowStub.unregisterCallback, instance.id);
    });
  });
  describe("executeAction", () => {
    it("should call ToolbarPanelHub.enableToolbarButton", () => {
      const stub = sandbox.stub(
        _ToolbarPanelHub.prototype,
        "enableToolbarButton"
      );

      instance.executeAction({ id: "show-whatsnew-button" });

      assert.calledOnce(stub);
    });
  });
  describe("removeToolbarNotification", () => {
    it("should remove the notification", () => {
      instance.removeToolbarNotification(fakeElement);

      assert.calledTwice(fakeElement.removeAttribute);
      assert.calledWithExactly(fakeElement.removeAttribute, "badged");
    });
  });
  describe("removeAllNotifications", () => {
    let blockMessageByIdStub;
    let fakeEvent;
    beforeEach(() => {
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
    beforeEach(() => {
      instance.init(sandbox.stub().resolves(), {
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
    });
  });
});

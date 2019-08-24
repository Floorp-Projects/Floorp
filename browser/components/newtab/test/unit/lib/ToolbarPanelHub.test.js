import { _ToolbarPanelHub } from "lib/ToolbarPanelHub.jsm";
import { GlobalOverrider } from "test/unit/utils";
import { OnboardingMessageProvider } from "lib/OnboardingMessageProvider.jsm";
import { PanelTestProvider } from "lib/PanelTestProvider.jsm";

describe("ToolbarPanelHub", () => {
  let globals;
  let sandbox;
  let instance;
  let everyWindowStub;
  let fakeDocument;
  let fakeWindow;
  let fakeElementById;
  let createdElements = [];
  let eventListeners = {};
  let addObserverStub;
  let removeObserverStub;
  let getBoolPrefStub;
  let setBoolPrefStub;
  let waitForInitializedStub;
  let isBrowserPrivateStub;
  let fakeDispatch;

  beforeEach(async () => {
    sandbox = sinon.createSandbox();
    globals = new GlobalOverrider();
    instance = new _ToolbarPanelHub();
    waitForInitializedStub = sandbox.stub().resolves();
    fakeElementById = {
      setAttribute: sandbox.stub(),
      removeAttribute: sandbox.stub(),
      querySelector: sandbox.stub().returns(null),
      appendChild: sandbox.stub(),
      addEventListener: sandbox.stub(),
      hasAttribute: sandbox.stub(),
      toggleAttribute: sandbox.stub(),
    };
    fakeDocument = {
      l10n: {
        setAttributes: sandbox.stub(),
      },
      getElementById: sandbox.stub().returns(fakeElementById),
      querySelector: sandbox.stub().returns({}),
      createElementNS: (ns, tagName) => {
        const element = {
          tagName,
          classList: {
            add: sandbox.stub(),
          },
          addEventListener: (ev, fn) => {
            eventListeners[ev] = fn;
          },
          appendChild: sandbox.stub(),
          setAttribute: sandbox.stub(),
        };
        createdElements.push(element);
        return element;
      },
    };
    fakeWindow = {
      document: fakeDocument,
      browser: {
        ownerDocument: fakeDocument,
      },
      MozXULElement: { insertFTLIfNeeded: sandbox.stub() },
      ownerGlobal: {
        openLinkIn: sandbox.stub(),
        gBrowser: "gBrowser",
      },
      PanelUI: {
        whatsNewPanel: fakeElementById,
      },
    };
    everyWindowStub = {
      registerCallback: sandbox.stub(),
      unregisterCallback: sandbox.stub(),
    };
    addObserverStub = sandbox.stub();
    removeObserverStub = sandbox.stub();
    getBoolPrefStub = sandbox.stub();
    setBoolPrefStub = sandbox.stub();
    fakeDispatch = sandbox.stub();
    isBrowserPrivateStub = sandbox.stub();
    globals.set("EveryWindow", everyWindowStub);
    globals.set("Services", {
      ...Services,
      prefs: {
        addObserver: addObserverStub,
        removeObserver: removeObserverStub,
        getBoolPref: getBoolPrefStub,
        setBoolPref: setBoolPrefStub,
      },
    });
    globals.set("PrivateBrowsingUtils", {
      isBrowserPrivate: isBrowserPrivateStub,
    });
  });
  afterEach(() => {
    instance.uninit();
    sandbox.restore();
    globals.restore();
    eventListeners = {};
    createdElements = [];
  });
  it("should create an instance", () => {
    assert.ok(instance);
  });
  it("should not enableAppmenuButton() on init() if pref is not enabled", async () => {
    getBoolPrefStub.returns(false);
    instance.enableAppmenuButton = sandbox.stub();
    await instance.init(waitForInitializedStub, { getMessages: () => {} });
    assert.notCalled(instance.enableAppmenuButton);
  });
  it("should enableAppmenuButton() on init() if pref is enabled", async () => {
    getBoolPrefStub.returns(true);
    instance.enableAppmenuButton = sandbox.stub();

    await instance.init(waitForInitializedStub, { getMessages: () => {} });

    assert.calledOnce(instance.enableAppmenuButton);
  });
  it("should unregisterCallback on uninit()", () => {
    instance.uninit();
    assert.calledTwice(everyWindowStub.unregisterCallback);
  });
  it("should observe pref changes on init", async () => {
    await instance.init(waitForInitializedStub, {});

    assert.calledOnce(addObserverStub);
    assert.calledWithExactly(
      addObserverStub,
      "browser.messaging-system.whatsNewPanel.enabled",
      instance
    );
  });
  it("should remove the observer on uninit", () => {
    instance.uninit();

    assert.calledOnce(removeObserverStub);
    assert.calledWithExactly(
      removeObserverStub,
      "browser.messaging-system.whatsNewPanel.enabled",
      instance
    );
  });
  describe("#observe", () => {
    it("should uninit if the pref is turned off", () => {
      sandbox.stub(instance, "uninit");
      getBoolPrefStub.returns(false);

      instance.observe(
        "",
        "",
        "browser.messaging-system.whatsNewPanel.enabled"
      );

      assert.calledOnce(instance.uninit);
    });
    it("shouldn't do anything if the pref is true", () => {
      sandbox.stub(instance, "uninit");
      getBoolPrefStub.returns(true);

      instance.observe(
        "",
        "",
        "browser.messaging-system.whatsNewPanel.enabled"
      );

      assert.notCalled(instance.uninit);
    });
  });
  describe("#enableAppmenuButton", () => {
    it("should registerCallback on enableAppmenuButton() if there are messages", async () => {
      instance.init(waitForInitializedStub, {
        getMessages: sandbox.stub().resolves([{}, {}]),
      });
      // init calls `enableAppmenuButton`
      everyWindowStub.registerCallback.resetHistory();

      await instance.enableAppmenuButton();

      assert.calledOnce(everyWindowStub.registerCallback);
      assert.calledWithExactly(
        everyWindowStub.registerCallback,
        "appMenu-whatsnew-button",
        sinon.match.func,
        sinon.match.func
      );
    });
    it("should not registerCallback on enableAppmenuButton() if there are no messages", async () => {
      instance.init(waitForInitializedStub, {
        getMessages: sandbox.stub().resolves([]),
      });
      // init calls `enableAppmenuButton`
      everyWindowStub.registerCallback.resetHistory();

      await instance.enableAppmenuButton();

      assert.notCalled(everyWindowStub.registerCallback);
    });
  });
  describe("#enableToolbarButton", () => {
    it("should registerCallback on enableToolbarButton if messages.length", async () => {
      instance.init(waitForInitializedStub, {
        getMessages: sandbox.stub().resolves([{}, {}]),
      });

      await instance.enableToolbarButton();

      assert.calledOnce(everyWindowStub.registerCallback);
    });
    it("should not registerCallback on enableToolbarButton if no messages", async () => {
      instance.init(waitForInitializedStub, {
        getMessages: sandbox.stub().resolves([]),
      });

      await instance.enableToolbarButton();

      assert.notCalled(everyWindowStub.registerCallback);
    });
  });
  it("should unhide appmenu button on _showAppmenuButton()", () => {
    instance._showAppmenuButton(fakeWindow);
    assert.calledWith(fakeElementById.removeAttribute, "hidden");
  });
  it("should hide appmenu button on _hideAppmenuButton()", () => {
    instance._hideAppmenuButton(fakeWindow);
    assert.calledWith(fakeElementById.setAttribute, "hidden", true);
  });
  it("should unhide toolbar button on _showToolbarButton()", () => {
    instance._showToolbarButton(fakeWindow);
    assert.calledWith(fakeElementById.removeAttribute, "hidden");
  });
  it("should hide toolbar button on _hideToolbarButton()", () => {
    instance._hideToolbarButton(fakeWindow);
    assert.calledWith(fakeElementById.setAttribute, "hidden", true);
  });
  describe("#renderMessages", () => {
    let getMessagesStub;
    beforeEach(() => {
      getMessagesStub = sandbox.stub();
      instance.init(waitForInitializedStub, {
        getMessages: getMessagesStub,
        dispatch: fakeDispatch,
      });
    });
    it("should render messages to the panel on renderMessages()", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );
      messages[0].content.link_text = { string_id: "link_text_id" };

      getMessagesStub.returns([messages[0], messages[2], messages[1]]);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      for (let message of messages) {
        assert.ok(
          createdElements.find(
            el =>
              el.tagName === "h2" && el.textContent === message.content.title
          )
        );
        assert.ok(
          createdElements.find(
            el => el.tagName === "p" && el.textContent === message.content.body
          )
        );
      }
      // Call the click handler to make coverage happy.
      eventListeners.click();
      assert.calledOnce(fakeWindow.ownerGlobal.openLinkIn);
    });
    it("should accept string for image attributes", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );
      getMessagesStub.returns([messages[0], messages[2], messages[1]]);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      const imageEl = createdElements.find(el => el.tagName === "img");
      assert.calledOnce(imageEl.setAttribute);
      assert.calledWithExactly(
        imageEl.setAttribute,
        "alt",
        "Firefox Send Logo"
      );
    });
    it("should accept fluent ids for image attributes", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );
      messages[0].content.icon_alt = { string_id: "foo" };
      getMessagesStub.returns([messages[0], messages[2], messages[1]]);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      const imageEl = createdElements.find(el => el.tagName === "img");
      assert.calledOnce(fakeDocument.l10n.setAttributes);
      assert.calledWithExactly(fakeDocument.l10n.setAttributes, imageEl, "foo");
    });
    it("should only render unique dates (no duplicates)", async () => {
      instance._createDateElement = sandbox.stub();
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );
      const uniqueDates = [
        ...new Set(messages.map(m => m.content.published_date)),
      ];
      getMessagesStub.returns(messages);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      assert.callCount(instance._createDateElement, uniqueDates.length);
    });
    it("should listen for panelhidden and remove the toolbar button", async () => {
      getMessagesStub.returns([]);
      fakeDocument.getElementById
        .withArgs("customizationui-widget-panel")
        .returns(null);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      assert.notCalled(fakeElementById.addEventListener);
    });
    it("should listen for panelhidden and remove the toolbar button", async () => {
      getMessagesStub.returns([]);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      assert.calledOnce(fakeElementById.addEventListener);
      assert.calledWithExactly(
        fakeElementById.addEventListener,
        "popuphidden",
        sinon.match.func,
        {
          once: true,
        }
      );
      const [, cb] = fakeElementById.addEventListener.firstCall.args;

      assert.notCalled(everyWindowStub.unregisterCallback);

      cb();

      assert.calledOnce(everyWindowStub.unregisterCallback);
      assert.calledWithExactly(
        everyWindowStub.unregisterCallback,
        "whats-new-menu-button"
      );
    });
    describe("#IMPRESSION", () => {
      it("should dispatch a IMPRESSION for messages", async () => {
        // means panel is triggered from the toolbar button
        fakeElementById.hasAttribute.returns(true);
        const messages = (await PanelTestProvider.getMessages()).filter(
          m => m.template === "whatsnew_panel_message"
        );
        getMessagesStub.returns(messages);
        const spy = sandbox.spy(instance, "sendUserEventTelemetry");

        await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

        assert.calledOnce(spy);
        assert.calledOnce(fakeDispatch);
        assert.propertyVal(
          spy.firstCall.args[2],
          "id",
          messages
            .map(({ id }) => id)
            .sort()
            .join(",")
        );
      });
      it("should dispatch a CLICK for clicking a message", async () => {
        // means panel is triggered from the toolbar button
        fakeElementById.hasAttribute.returns(true);
        // Force to render the message
        fakeElementById.querySelector.returns(null);
        const messages = (await PanelTestProvider.getMessages()).filter(
          m => m.template === "whatsnew_panel_message"
        );
        getMessagesStub.returns([messages[0]]);
        const spy = sandbox.spy(instance, "sendUserEventTelemetry");

        await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

        assert.calledOnce(spy);
        assert.calledOnce(fakeDispatch);

        spy.resetHistory();

        // Message click event listener cb
        eventListeners.click();

        assert.calledOnce(spy);
        assert.calledWithExactly(spy, fakeWindow, "CLICK", messages[0]);
      });
      it("should dispatch a IMPRESSION with toolbar_dropdown", async () => {
        // means panel is triggered from the toolbar button
        fakeElementById.hasAttribute.returns(true);
        const messages = (await PanelTestProvider.getMessages()).filter(
          m => m.template === "whatsnew_panel_message"
        );
        getMessagesStub.resolves(messages);
        const spy = sandbox.spy(instance, "sendUserEventTelemetry");
        const panelPingId = messages
          .map(({ id }) => id)
          .sort()
          .join(",");

        await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

        assert.calledOnce(spy);
        assert.calledWithExactly(
          spy,
          fakeWindow,
          "IMPRESSION",
          {
            id: panelPingId,
          },
          {
            value: {
              view: "toolbar_dropdown",
            },
          }
        );
        assert.calledOnce(fakeDispatch);
        const {
          args: [dispatchPayload],
        } = fakeDispatch.lastCall;
        assert.propertyVal(dispatchPayload, "type", "TOOLBAR_PANEL_TELEMETRY");
        assert.propertyVal(dispatchPayload.data, "message_id", panelPingId);
        assert.propertyVal(
          dispatchPayload.data.value,
          "view",
          "toolbar_dropdown"
        );
      });
      it("should dispatch a IMPRESSION with application_menu", async () => {
        // means panel is triggered as a subview in the application menu
        fakeElementById.hasAttribute.returns(false);
        const messages = (await PanelTestProvider.getMessages()).filter(
          m => m.template === "whatsnew_panel_message"
        );
        getMessagesStub.resolves(messages);
        const spy = sandbox.spy(instance, "sendUserEventTelemetry");
        const panelPingId = messages
          .map(({ id }) => id)
          .sort()
          .join(",");

        await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

        assert.calledOnce(spy);
        assert.calledWithExactly(
          spy,
          fakeWindow,
          "IMPRESSION",
          {
            id: panelPingId,
          },
          {
            value: {
              view: "application_menu",
            },
          }
        );
        assert.calledOnce(fakeDispatch);
        const {
          args: [dispatchPayload],
        } = fakeDispatch.lastCall;
        assert.propertyVal(dispatchPayload, "type", "TOOLBAR_PANEL_TELEMETRY");
        assert.propertyVal(dispatchPayload.data, "message_id", panelPingId);
        assert.propertyVal(
          dispatchPayload.data.value,
          "view",
          "application_menu"
        );
      });
    });
  });
  describe("#insertProtectionPanelMessage", () => {
    const fakeInsert = () =>
      instance.insertProtectionPanelMessage({
        target: { ownerGlobal: fakeWindow, ownerDocument: fakeDocument },
      });
    beforeEach(async () => {
      const onboardingMsgs = await OnboardingMessageProvider.getUntranslatedMessages();
      await instance.init(waitForInitializedStub, {
        dispatch: fakeDispatch,
        getMessages: () =>
          onboardingMsgs.find(msg => msg.template === "protections_panel"),
      });
    });
    it("should remember it showed", async () => {
      await fakeInsert();

      assert.calledWithExactly(
        setBoolPrefStub,
        "browser.protections_panel.infoMessage.seen",
        true
      );
    });
    it("should toggle/expand when default collapsed/disabled", async () => {
      fakeElementById.hasAttribute.returns(true);

      await fakeInsert();

      assert.calledTwice(fakeElementById.toggleAttribute);
    });
    it("should toggle again when popup hides", async () => {
      fakeElementById.addEventListener.callsArg(1);

      await fakeInsert();

      assert.callCount(fakeElementById.toggleAttribute, 4);
    });
    it("should open link on click", async () => {
      await fakeInsert();

      eventListeners.click();

      assert.calledOnce(fakeWindow.ownerGlobal.openLinkIn);
    });
  });
});

import { _ToolbarPanelHub } from "lib/ToolbarPanelHub.jsm";
import { GlobalOverrider } from "test/unit/utils";
import { PanelTestProvider } from "lib/PanelTestProvider.sys.mjs";

describe("ToolbarPanelHub", () => {
  let globals;
  let sandbox;
  let instance;
  let everyWindowStub;
  let fakeDocument;
  let fakeWindow;
  let fakeElementById;
  let fakeElementByTagName;
  let createdCustomElements = [];
  let eventListeners = {};
  let addObserverStub;
  let removeObserverStub;
  let getBoolPrefStub;
  let setBoolPrefStub;
  let waitForInitializedStub;
  let isBrowserPrivateStub;
  let fakeSendTelemetry;
  let getEarliestRecordedDateStub;
  let getEventsByDateRangeStub;
  let defaultSearchStub;
  let scriptloaderStub;
  let fakeRemoteL10n;
  let getViewNodeStub;

  beforeEach(async () => {
    sandbox = sinon.createSandbox();
    globals = new GlobalOverrider();
    instance = new _ToolbarPanelHub();
    waitForInitializedStub = sandbox.stub().resolves();
    fakeElementById = {
      setAttribute: sandbox.stub(),
      removeAttribute: sandbox.stub(),
      querySelector: sandbox.stub().returns(null),
      querySelectorAll: sandbox.stub().returns([]),
      appendChild: sandbox.stub(),
      addEventListener: sandbox.stub(),
      hasAttribute: sandbox.stub(),
      toggleAttribute: sandbox.stub(),
      remove: sandbox.stub(),
      removeChild: sandbox.stub(),
    };
    fakeElementByTagName = {
      setAttribute: sandbox.stub(),
      removeAttribute: sandbox.stub(),
      querySelector: sandbox.stub().returns(null),
      querySelectorAll: sandbox.stub().returns([]),
      appendChild: sandbox.stub(),
      addEventListener: sandbox.stub(),
      hasAttribute: sandbox.stub(),
      toggleAttribute: sandbox.stub(),
      remove: sandbox.stub(),
      removeChild: sandbox.stub(),
    };
    fakeDocument = {
      getElementById: sandbox.stub().returns(fakeElementById),
      getElementsByTagName: sandbox.stub().returns(fakeElementByTagName),
      querySelector: sandbox.stub().returns({}),
      createElement: tagName => {
        const element = {
          tagName,
          classList: {},
          addEventListener: (ev, fn) => {
            eventListeners[ev] = fn;
          },
          appendChild: sandbox.stub(),
          setAttribute: sandbox.stub(),
          textContent: "",
        };
        element.classList.add = sandbox.stub();
        element.classList.includes = className =>
          element.classList.add.firstCall.args[0] === className;
        createdCustomElements.push(element);
        return element;
      },
      l10n: {
        translateElements: sandbox.stub(),
        translateFragment: sandbox.stub(),
        formatMessages: sandbox.stub().resolves([{}]),
        setAttributes: sandbox.stub(),
      },
    };
    fakeWindow = {
      // eslint-disable-next-line object-shorthand
      DocumentFragment: function () {
        return fakeElementById;
      },
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
        panel: fakeElementById,
        whatsNewPanel: fakeElementById,
      },
      customElements: { get: sandbox.stub() },
    };
    everyWindowStub = {
      registerCallback: sandbox.stub(),
      unregisterCallback: sandbox.stub(),
    };
    scriptloaderStub = { loadSubScript: sandbox.stub() };
    addObserverStub = sandbox.stub();
    removeObserverStub = sandbox.stub();
    getBoolPrefStub = sandbox.stub();
    setBoolPrefStub = sandbox.stub();
    fakeSendTelemetry = sandbox.stub();
    isBrowserPrivateStub = sandbox.stub();
    getEarliestRecordedDateStub = sandbox.stub().returns(
      // A random date that's not the current timestamp
      new Date() - 500
    );
    getEventsByDateRangeStub = sandbox.stub().returns([]);
    getViewNodeStub = sandbox.stub().returns(fakeElementById);
    defaultSearchStub = { defaultEngine: { name: "DDG" } };
    fakeRemoteL10n = {
      l10n: {},
      reloadL10n: sandbox.stub(),
      createElement: sandbox
        .stub()
        .callsFake((doc, el) => fakeDocument.createElement(el)),
    };
    globals.set({
      EveryWindow: everyWindowStub,
      Services: {
        ...Services,
        prefs: {
          addObserver: addObserverStub,
          removeObserver: removeObserverStub,
          getBoolPref: getBoolPrefStub,
          setBoolPref: setBoolPrefStub,
        },
        search: defaultSearchStub,
        scriptloader: scriptloaderStub,
      },
      PrivateBrowsingUtils: {
        isBrowserPrivate: isBrowserPrivateStub,
      },
      TrackingDBService: {
        getEarliestRecordedDate: getEarliestRecordedDateStub,
        getEventsByDateRange: getEventsByDateRangeStub,
      },
      SpecialMessageActions: {
        handleAction: sandbox.stub(),
      },
      RemoteL10n: fakeRemoteL10n,
      PanelMultiView: {
        getViewNode: getViewNodeStub,
      },
    });
  });
  afterEach(() => {
    instance.uninit();
    sandbox.restore();
    globals.restore();
    eventListeners = {};
    createdCustomElements = [];
  });
  it("should create an instance", () => {
    assert.ok(instance);
  });
  it("should enableAppmenuButton() on init() just once", async () => {
    instance.enableAppmenuButton = sandbox.stub();

    await instance.init(waitForInitializedStub, { getMessages: () => {} });
    await instance.init(waitForInitializedStub, { getMessages: () => {} });

    assert.calledOnce(instance.enableAppmenuButton);

    instance.uninit();

    await instance.init(waitForInitializedStub, { getMessages: () => {} });

    assert.calledTwice(instance.enableAppmenuButton);
  });
  it("should unregisterCallback on uninit()", () => {
    instance.uninit();
    assert.calledTwice(everyWindowStub.unregisterCallback);
  });
  describe("#maybeLoadCustomElement", () => {
    it("should not load customElements a second time", () => {
      instance.maybeLoadCustomElement({ customElements: new Map() });
      instance.maybeLoadCustomElement({
        customElements: new Map([["remote-text", true]]),
      });

      assert.calledOnce(scriptloaderStub.loadSubScript);
    });
  });
  describe("#toggleWhatsNewPref", () => {
    it("should call Services.prefs.setBoolPref() with the opposite value", () => {
      let checkbox = {};
      let event = { target: checkbox };
      // checkbox starts false
      checkbox.checked = false;

      // toggling the checkbox to set the value to true;
      // Preferences.set() gets called before the checkbox changes,
      // so we have to call it with the opposite value.
      instance.toggleWhatsNewPref(event);

      assert.calledOnce(setBoolPrefStub);
      assert.calledWith(
        setBoolPrefStub,
        "browser.messaging-system.whatsNewPanel.enabled",
        !checkbox.checked
      );
    });
    it("should report telemetry with the opposite value", () => {
      let sendUserEventTelemetryStub = sandbox.stub(
        instance,
        "sendUserEventTelemetry"
      );
      let event = {
        target: { checked: true, ownerGlobal: fakeWindow },
      };

      instance.toggleWhatsNewPref(event);

      assert.calledOnce(sendUserEventTelemetryStub);
      const { args } = sendUserEventTelemetryStub.firstCall;
      assert.equal(args[1], "WNP_PREF_TOGGLE");
      assert.propertyVal(args[3].value, "prefValue", false);
    });
  });
  describe("#enableAppmenuButton", () => {
    it("should registerCallback on enableAppmenuButton() if there are messages", async () => {
      await instance.init(waitForInitializedStub, {
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
  describe("#disableAppmenuButton", () => {
    it("should call the unregisterCallback", () => {
      assert.notCalled(everyWindowStub.unregisterCallback);

      instance.disableAppmenuButton();

      assert.calledOnce(everyWindowStub.unregisterCallback);
      assert.calledWithExactly(
        everyWindowStub.unregisterCallback,
        "appMenu-whatsnew-button"
      );
    });
  });
  describe("#enableToolbarButton", () => {
    it("should registerCallback on enableToolbarButton if messages.length", async () => {
      await instance.init(waitForInitializedStub, {
        getMessages: sandbox.stub().resolves([{}, {}]),
      });
      // init calls `enableAppmenuButton`
      everyWindowStub.registerCallback.resetHistory();

      await instance.enableToolbarButton();

      assert.calledOnce(everyWindowStub.registerCallback);
      assert.calledWithExactly(
        everyWindowStub.registerCallback,
        "whats-new-menu-button",
        sinon.match.func,
        sinon.match.func
      );
    });
    it("should not registerCallback on enableToolbarButton if no messages", async () => {
      await instance.init(waitForInitializedStub, {
        getMessages: sandbox.stub().resolves([]),
      });

      await instance.enableToolbarButton();

      assert.notCalled(everyWindowStub.registerCallback);
    });
  });
  describe("Show/Hide functions", () => {
    it("should unhide appmenu button on _showAppmenuButton()", async () => {
      await instance._showAppmenuButton(fakeWindow);

      assert.equal(fakeElementById.hidden, false);
    });
    it("should hide appmenu button on _hideAppmenuButton()", () => {
      instance._hideAppmenuButton(fakeWindow);
      assert.equal(fakeElementById.hidden, true);
    });
    it("should not do anything if the window is closed", () => {
      instance._hideAppmenuButton(fakeWindow, true);
      assert.notCalled(global.PanelMultiView.getViewNode);
    });
    it("should not throw if the element does not exist", () => {
      let fn = instance._hideAppmenuButton.bind(null, {
        browser: { ownerDocument: {} },
      });
      getViewNodeStub.returns(undefined);
      assert.doesNotThrow(fn);
    });
    it("should unhide toolbar button on _showToolbarButton()", async () => {
      await instance._showToolbarButton(fakeWindow);

      assert.equal(fakeElementById.hidden, false);
    });
    it("should hide toolbar button on _hideToolbarButton()", () => {
      instance._hideToolbarButton(fakeWindow);
      assert.equal(fakeElementById.hidden, true);
    });
  });
  describe("#renderMessages", () => {
    let getMessagesStub;
    beforeEach(() => {
      getMessagesStub = sandbox.stub();
      instance.init(waitForInitializedStub, {
        getMessages: getMessagesStub,
        sendTelemetry: fakeSendTelemetry,
      });
    });
    it("should have correct state", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );

      getMessagesStub.returns(messages);
      const ev1 = sandbox.stub();
      ev1.withArgs("type").returns(1); // tracker
      ev1.withArgs("count").returns(4);
      const ev2 = sandbox.stub();
      ev2.withArgs("type").returns(4); // fingerprinter
      ev2.withArgs("count").returns(3);
      getEventsByDateRangeStub.returns([
        { getResultByName: ev1 },
        { getResultByName: ev2 },
      ]);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      assert.propertyVal(instance.state.contentArguments, "trackerCount", 4);
      assert.propertyVal(
        instance.state.contentArguments,
        "fingerprinterCount",
        3
      );
    });
    it("should render messages to the panel on renderMessages()", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );
      messages[0].content.link_text = { string_id: "link_text_id" };

      getMessagesStub.returns(messages);
      const ev1 = sandbox.stub();
      ev1.withArgs("type").returns(1); // tracker
      ev1.withArgs("count").returns(4);
      const ev2 = sandbox.stub();
      ev2.withArgs("type").returns(4); // fingerprinter
      ev2.withArgs("count").returns(3);
      getEventsByDateRangeStub.returns([
        { getResultByName: ev1 },
        { getResultByName: ev2 },
      ]);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      for (let message of messages) {
        assert.ok(
          fakeRemoteL10n.createElement.args.find(
            ([doc, el, args]) =>
              args && args.classList === "whatsNew-message-title"
          )
        );
        if (message.content.layout === "tracking-protections") {
          assert.ok(
            fakeRemoteL10n.createElement.args.find(
              ([doc, el, args]) =>
                args && args.classList === "whatsNew-message-subtitle"
            )
          );
        }
        if (message.id === "WHATS_NEW_FINGERPRINTER_COUNTER_72") {
          assert.ok(
            fakeRemoteL10n.createElement.args.find(
              ([doc, el, args]) => el === "h2" && args.content === 3
            )
          );
        }
        assert.ok(
          fakeRemoteL10n.createElement.args.find(
            ([doc, el, args]) =>
              args && args.classList === "whatsNew-message-content"
          )
        );
      }
      // Call the click handler to make coverage happy.
      eventListeners.mouseup();
      assert.calledOnce(global.SpecialMessageActions.handleAction);
    });
    it("should clear previous messages on 2nd renderMessages()", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );
      const removeStub = sandbox.stub();
      fakeElementById.querySelectorAll.onCall(0).returns([]);
      fakeElementById.querySelectorAll
        .onCall(1)
        .returns([{ remove: removeStub }, { remove: removeStub }]);

      getMessagesStub.returns(messages);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");
      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      assert.calledTwice(removeStub);
    });
    it("should sort based on order field value", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m =>
          m.template === "whatsnew_panel_message" &&
          m.content.published_date === 1560969794394
      );

      messages.forEach(m => (m.content.title = m.order));

      getMessagesStub.returns(messages);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      // Select the title elements that are supposed to be set to the same
      // value as the `order` field of the message
      const titleEls = fakeRemoteL10n.createElement.args
        .filter(
          ([doc, el, args]) =>
            args && args.classList === "whatsNew-message-title"
        )
        .map(([doc, el, args]) => args.content);
      assert.deepEqual(titleEls, [1, 2, 3]);
    });
    it("should accept string for image attributes", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.id === "WHATS_NEW_70_1"
      );
      getMessagesStub.returns(messages);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      const imageEl = createdCustomElements.find(el => el.tagName === "img");
      assert.calledOnce(imageEl.setAttribute);
      assert.calledWithExactly(
        imageEl.setAttribute,
        "alt",
        "Firefox Send Logo"
      );
    });
    it("should set state values as data-attribute", async () => {
      const message = (await PanelTestProvider.getMessages()).find(
        m => m.template === "whatsnew_panel_message"
      );
      getMessagesStub.returns([message]);
      instance.state.contentArguments = { foo: "foo", bar: "bar" };

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      const [, , args] = fakeRemoteL10n.createElement.args.find(
        ([doc, el, elArgs]) => elArgs && elArgs.attributes
      );
      assert.ok(args);
      // Currently this.state.contentArguments has 8 different entries
      assert.lengthOf(Object.keys(args.attributes), 8);
      assert.equal(
        args.attributes.searchEngineName,
        defaultSearchStub.defaultEngine.name
      );
    });
    it("should only render unique dates (no duplicates)", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );
      const uniqueDates = [
        ...new Set(messages.map(m => m.content.published_date)),
      ];
      getMessagesStub.returns(messages);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      const dateElements = fakeRemoteL10n.createElement.args.filter(
        ([doc, el, args]) =>
          el === "p" && args.classList === "whatsNew-message-date"
      );
      assert.lengthOf(dateElements, uniqueDates.length);
    });
    it("should listen for panelhidden and remove the toolbar button", async () => {
      getMessagesStub.returns([]);
      fakeDocument.getElementById
        .withArgs("customizationui-widget-panel")
        .returns(null);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      assert.notCalled(fakeElementById.addEventListener);
    });
    it("should attach doCommand cbs that handle user actions", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );
      getMessagesStub.returns(messages);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      const messageEl = createdCustomElements.find(
        el =>
          el.tagName === "div" && el.classList.includes("whatsNew-message-body")
      );
      const anchorEl = createdCustomElements.find(el => el.tagName === "a");

      assert.notCalled(global.SpecialMessageActions.handleAction);

      messageEl.doCommand();
      anchorEl.doCommand();

      assert.calledTwice(global.SpecialMessageActions.handleAction);
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
        assert.calledOnce(fakeSendTelemetry);
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
        assert.calledOnce(fakeSendTelemetry);

        spy.resetHistory();

        // Message click event listener cb
        eventListeners.mouseup();

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
        assert.calledOnce(fakeSendTelemetry);
        const {
          args: [dispatchPayload],
        } = fakeSendTelemetry.lastCall;
        assert.propertyVal(dispatchPayload, "type", "TOOLBAR_PANEL_TELEMETRY");
        assert.propertyVal(dispatchPayload.data, "message_id", panelPingId);
        assert.deepEqual(dispatchPayload.data.event_context, {
          view: "toolbar_dropdown",
        });
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
        assert.calledOnce(fakeSendTelemetry);
        const {
          args: [dispatchPayload],
        } = fakeSendTelemetry.lastCall;
        assert.propertyVal(dispatchPayload, "type", "TOOLBAR_PANEL_TELEMETRY");
        assert.propertyVal(dispatchPayload.data, "message_id", panelPingId);
        assert.deepEqual(dispatchPayload.data.event_context, {
          view: "application_menu",
        });
      });
    });
    describe("#forceShowMessage", () => {
      const panelSelector = "PanelUI-whatsNew-message-container";
      let removeMessagesSpy;
      let renderMessagesStub;
      let addEventListenerStub;
      let messages;
      let browser;
      beforeEach(async () => {
        messages = (await PanelTestProvider.getMessages()).find(
          m => m.id === "WHATS_NEW_70_1"
        );
        removeMessagesSpy = sandbox.spy(instance, "removeMessages");
        renderMessagesStub = sandbox.spy(instance, "renderMessages");
        addEventListenerStub = fakeElementById.addEventListener;
        browser = { ownerGlobal: fakeWindow, ownerDocument: fakeDocument };
        fakeElementById.querySelectorAll.returns([fakeElementById]);
      });
      it("should call removeMessages when forcing a message to show", () => {
        instance.forceShowMessage(browser, messages);

        assert.calledWithExactly(removeMessagesSpy, fakeWindow, panelSelector);
      });
      it("should call renderMessages when forcing a message to show", () => {
        instance.forceShowMessage(browser, messages);

        assert.calledOnce(renderMessagesStub);
        assert.calledWithExactly(
          renderMessagesStub,
          fakeWindow,
          fakeDocument,
          panelSelector,
          {
            force: true,
            messages: Array.isArray(messages) ? messages : [messages],
          }
        );
      });
      it("should cleanup after the panel is hidden when forcing a message to show", () => {
        instance.forceShowMessage(browser, messages);

        assert.calledOnce(addEventListenerStub);
        assert.calledWithExactly(
          addEventListenerStub,
          "popuphidden",
          sinon.match.func
        );

        const [, cb] = addEventListenerStub.firstCall.args;
        // Reset the call count from the first `forceShowMessage` call
        removeMessagesSpy.resetHistory();
        cb({ target: { ownerGlobal: fakeWindow } });

        assert.calledOnce(removeMessagesSpy);
        assert.calledWithExactly(removeMessagesSpy, fakeWindow, panelSelector);
      });
      it("should exit gracefully if called before a browser exists", () => {
        instance.forceShowMessage(null, messages);
        assert.neverCalledWith(removeMessagesSpy, fakeWindow, panelSelector);
      });
    });
  });
});

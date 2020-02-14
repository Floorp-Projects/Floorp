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
  let getEarliestRecordedDateStub;
  let getEventsByDateRangeStub;
  let handleUserActionStub;
  let defaultSearchStub;

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
    fakeDocument = {
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
      // eslint-disable-next-line object-shorthand
      DocumentFragment: function() {
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
    getEarliestRecordedDateStub = sandbox.stub().returns(
      // A random date that's not the current timestamp
      new Date() - 500
    );
    getEventsByDateRangeStub = sandbox.stub().returns([]);
    handleUserActionStub = sandbox.stub();
    defaultSearchStub = { defaultEngine: { name: "DDG" } };
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
      },
      PrivateBrowsingUtils: {
        isBrowserPrivate: isBrowserPrivateStub,
      },
      TrackingDBService: {
        getEarliestRecordedDate: getEarliestRecordedDateStub,
        getEventsByDateRange: getEventsByDateRangeStub,
      },
      RemoteL10n: {
        l10n: {
          translateElements: sandbox.stub(),
          translateFragment: sandbox.stub(),
          formatMessages: sandbox.stub().resolves([{}]),
        },
      },
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
  it("should unhide appmenu button on _showAppmenuButton()", async () => {
    await instance._showAppmenuButton(fakeWindow);

    assert.calledWith(fakeElementById.removeAttribute, "hidden");
  });
  it("should hide appmenu button on _hideAppmenuButton()", () => {
    instance._hideAppmenuButton(fakeWindow);
    assert.calledWith(fakeElementById.setAttribute, "hidden", true);
  });
  it("should unhide toolbar button on _showToolbarButton()", async () => {
    await instance._showToolbarButton(fakeWindow);

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
        handleUserAction: handleUserActionStub,
      });
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
        assert.ok(createdElements.find(el => el.tagName === "h2"));
        if (message.content.layout === "tracking-protections") {
          assert.ok(createdElements.find(el => el.tagName === "h4"));
        }
        if (message.id === "WHATS_NEW_FINGERPRINTER_COUNTER_72") {
          assert.ok(createdElements.find(el => el.tagName === "h4"));
          assert.ok(
            createdElements.find(
              el => el.tagName === "h2" && el.textContent === 3
            )
          );
        }
        assert.ok(createdElements.find(el => el.tagName === "p"));
      }
      // Call the click handler to make coverage happy.
      eventListeners.mouseup();
      assert.calledOnce(handleUserActionStub);
    });
    it("should clear previous messages on 2nd renderMessages()", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );
      fakeElementById.querySelectorAll.onCall(0).returns([]);
      fakeElementById.querySelectorAll.onCall(1).returns(["a", "b", "c"]);

      getMessagesStub.returns(messages);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");
      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      assert.calledThrice(fakeElementById.removeChild);
      assert.equal(fakeElementById.removeChild.firstCall.args[0], "a");
      assert.equal(fakeElementById.removeChild.secondCall.args[0], "b");
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
      const titleEls = createdElements
        .filter(
          el =>
            el.classList.add.firstCall &&
            el.classList.add.firstCall.args[0] === "whatsNew-message-title"
        )
        .map(el => el.textContent);
      assert.deepEqual(titleEls, [1, 2, 3]);
    });
    it("should accept string for image attributes", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.id === "WHATS_NEW_70_1"
      );
      getMessagesStub.returns(messages);

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
        m => m.id === "WHATS_NEW_70_1"
      );
      messages[0].content.icon_alt = { string_id: "foo" };
      getMessagesStub.returns(messages);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      assert.calledWithExactly(global.RemoteL10n.l10n.formatMessages, [
        {
          id: "foo",
          args: instance.state.contentArguments,
        },
      ]);
    });
    it("handle fluent attributes", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.id === "WHATS_NEW_70_1"
      );
      messages[0].content.icon_alt = { string_id: "foo" };
      getMessagesStub.returns(messages);
      global.RemoteL10n.l10n.formatMessages
        .withArgs([{ id: "foo", args: sinon.match.object }])
        .resolves([{ attributes: [{ name: "alt", value: "bar" }] }]);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");
      const imgEl = createdElements.find(e => e.tagName === "img");

      assert.calledWithExactly(imgEl.setAttribute, "alt", "bar");
    });
    it("should accept fluent ids for elements attributes", async () => {
      const [message] = (await PanelTestProvider.getMessages()).filter(
        m =>
          m.template === "whatsnew_panel_message" &&
          m.content.layout === "tracking-protections"
      );
      getMessagesStub.returns([message]);
      instance.state.contentArguments = { foo: "foo", bar: "bar" };

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      assert.calledWithExactly(global.RemoteL10n.l10n.formatMessages, [
        {
          id: message.content.subtitle.string_id,
          args: instance.state.contentArguments,
        },
      ]);
    });
    it("should correctly compute blocker trackers and date", async () => {
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );
      getMessagesStub.returns(messages);
      const ev1 = sandbox.stub();
      ev1.withArgs("type").returns(2); // cookie
      ev1.withArgs("count").returns(4);
      const ev2 = sandbox.stub();
      ev2.withArgs("type").returns(2); // cookie
      ev2.withArgs("count").returns(3);
      getEventsByDateRangeStub.returns([
        { getResultByName: ev1 },
        { getResultByName: ev2 },
      ]);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      assert.calledWithExactly(global.RemoteL10n.l10n.formatMessages, [
        {
          id: sinon.match.string,
          args: {
            blockedCount: 7,
            earliestDate: getEarliestRecordedDateStub(),
            cookieCount: 7,
            cryptominerCount: 0,
            socialCount: 0,
            trackerCount: 0,
            fingerprinterCount: 0,
            searchEngineName: Services.search.defaultEngine.name,
          },
        },
      ]);
    });
    it("should correctly compute event counts per type", async () => {
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

      assert.calledWithExactly(global.RemoteL10n.l10n.formatMessages, [
        {
          id: sinon.match.string,
          args: {
            blockedCount: 7,
            earliestDate: getEarliestRecordedDateStub(),
            trackerCount: 4,
            fingerprinterCount: 3,
            cookieCount: 0,
            cryptominerCount: 0,
            socialCount: 0,
            searchEngineName: Services.search.defaultEngine.name,
          },
        },
      ]);
    });
    it("should fallback to undefined search engine name", async () => {
      globals.set("Services", {
        ...global.Services,
        search: { defaultEngine: null },
      });
      const messages = (await PanelTestProvider.getMessages()).filter(
        m => m.template === "whatsnew_panel_message"
      );
      getMessagesStub.returns(messages);

      await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

      assert.calledWithExactly(global.RemoteL10n.l10n.formatMessages, [
        {
          id: sinon.match.string,
          args: {
            ...instance.state.contentArguments,
            searchEngineName: "undefined",
          },
        },
      ]);
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

      const dateElements = createdElements.filter(
        el =>
          el.tagName === "p" &&
          el.classList.add.firstCall &&
          el.classList.add.firstCall.args[0] === "whatsNew-message-date"
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

      const buttonEl = createdElements.find(el => el.tagName === "button");
      const anchorEl = createdElements.find(el => el.tagName === "a");

      assert.notCalled(handleUserActionStub);

      buttonEl.doCommand();
      anchorEl.doCommand();

      assert.calledTwice(handleUserActionStub);
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
        assert.calledOnce(fakeDispatch);
        const {
          args: [dispatchPayload],
        } = fakeDispatch.lastCall;
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
        assert.calledOnce(fakeDispatch);
        const {
          args: [dispatchPayload],
        } = fakeDispatch.lastCall;
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
        browser = {
          browser: { ownerGlobal: fakeWindow, ownerDocument: fakeDocument },
        };
        fakeElementById.querySelectorAll.returns([fakeElementById]);
      });
      it("should call removeMessages when forcing a message to show", () => {
        instance.forceShowMessage(browser, messages);

        assert.calledOnce(removeMessagesSpy);
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
    });
  });
  describe("#insertProtectionPanelMessage", () => {
    const fakeInsert = () =>
      instance.insertProtectionPanelMessage({
        target: { ownerGlobal: fakeWindow, ownerDocument: fakeDocument },
      });
    let getMessagesStub;
    beforeEach(async () => {
      const onboardingMsgs = await OnboardingMessageProvider.getUntranslatedMessages();
      getMessagesStub = sandbox
        .stub()
        .resolves(
          onboardingMsgs.find(msg => msg.template === "protections_panel")
        );
      await instance.init(waitForInitializedStub, {
        dispatch: fakeDispatch,
        getMessages: getMessagesStub,
        handleUserAction: handleUserActionStub,
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

      assert.calledThrice(fakeElementById.toggleAttribute);
    });
    it("should toggle again when popup hides", async () => {
      fakeElementById.addEventListener.callsArg(1);

      await fakeInsert();

      assert.callCount(fakeElementById.toggleAttribute, 6);
    });
    it("should open link on click (separate link element)", async () => {
      const sendTelemetryStub = sandbox.stub(
        instance,
        "sendUserEventTelemetry"
      );
      const onboardingMsgs = await OnboardingMessageProvider.getUntranslatedMessages();
      const msg = onboardingMsgs.find(m => m.template === "protections_panel");

      await fakeInsert();

      assert.calledOnce(sendTelemetryStub);
      assert.calledWithExactly(
        sendTelemetryStub,
        fakeWindow,
        "IMPRESSION",
        msg
      );

      eventListeners.mouseup();

      assert.calledOnce(handleUserActionStub);
      assert.calledWithExactly(handleUserActionStub, {
        target: fakeWindow,
        data: {
          type: "OPEN_URL",
          data: {
            args: sinon.match.string,
            where: "tabshifted",
          },
        },
      });
    });
    it("should format the url", async () => {
      const stub = sandbox
        .stub(global.Services.urlFormatter, "formatURL")
        .returns("formattedURL");
      const onboardingMsgs = await OnboardingMessageProvider.getUntranslatedMessages();
      const msg = onboardingMsgs.find(m => m.template === "protections_panel");

      await fakeInsert();

      eventListeners.mouseup();

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, msg.content.cta_url);
      assert.calledOnce(handleUserActionStub);
      assert.calledWithExactly(handleUserActionStub, {
        target: fakeWindow,
        data: {
          type: "OPEN_URL",
          data: {
            args: "formattedURL",
            where: "tabshifted",
          },
        },
      });
    });
    it("should report format url errors", async () => {
      const stub = sandbox
        .stub(global.Services.urlFormatter, "formatURL")
        .throws();
      const onboardingMsgs = await OnboardingMessageProvider.getUntranslatedMessages();
      const msg = onboardingMsgs.find(m => m.template === "protections_panel");
      sandbox.spy(global.Cu, "reportError");

      await fakeInsert();

      eventListeners.mouseup();

      assert.calledOnce(stub);
      assert.calledOnce(global.Cu.reportError);
      assert.calledOnce(handleUserActionStub);
      assert.calledWithExactly(handleUserActionStub, {
        target: fakeWindow,
        data: {
          type: "OPEN_URL",
          data: {
            args: msg.content.cta_url,
            where: "tabshifted",
          },
        },
      });
    });
    it("should open link on click (directly attached to the message)", async () => {
      const onboardingMsgs = await OnboardingMessageProvider.getUntranslatedMessages();
      const msg = onboardingMsgs.find(m => m.template === "protections_panel");
      getMessagesStub.resolves({
        ...msg,
        content: { ...msg.content, link_text: null },
      });
      await fakeInsert();

      eventListeners.mouseup();

      assert.calledOnce(handleUserActionStub);
      assert.calledWithExactly(handleUserActionStub, {
        target: fakeWindow,
        data: {
          type: "OPEN_URL",
          data: {
            args: sinon.match.string,
            where: "tabshifted",
          },
        },
      });
    });
    it("should handleUserAction from mouseup and keyup", async () => {
      await fakeInsert();

      eventListeners.mouseup();
      eventListeners.keyup({ key: "Enter" });
      eventListeners.keyup({ key: " " });
      assert.calledThrice(handleUserActionStub);
    });
  });
});

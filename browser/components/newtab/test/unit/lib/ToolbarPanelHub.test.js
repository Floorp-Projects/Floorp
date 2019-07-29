import { _ToolbarPanelHub } from "lib/ToolbarPanelHub.jsm";
import { GlobalOverrider } from "test/unit/utils";
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
  let waitForInitializedStub;

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
      },
    };
    everyWindowStub = {
      registerCallback: sandbox.stub(),
      unregisterCallback: sandbox.stub(),
    };
    addObserverStub = sandbox.stub();
    removeObserverStub = sandbox.stub();
    getBoolPrefStub = sandbox.stub();
    globals.set("EveryWindow", everyWindowStub);
    globals.set("Services", {
      ...Services,
      prefs: {
        addObserver: addObserverStub,
        removeObserver: removeObserverStub,
        getBoolPref: getBoolPrefStub,
      },
    });
  });
  afterEach(() => {
    instance.uninit();
    sandbox.restore();
    globals.restore();
  });
  it("should create an instance", () => {
    assert.ok(instance);
  });
  it("should not enableAppmenuButton() on init() if pref is not enabled", () => {
    getBoolPrefStub.returns(false);
    instance.enableAppmenuButton = sandbox.stub();
    instance.init(waitForInitializedStub, { getMessages: () => {} });
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
  it("should render messages to the panel on renderMessages()", async () => {
    const messages = (await PanelTestProvider.getMessages()).filter(
      m => m.template === "whatsnew_panel_message"
    );
    messages[0].content.link_text = { string_id: "link_text_id" };
    instance.init(waitForInitializedStub, {
      getMessages: sandbox
        .stub()
        .returns([messages[0], messages[2], messages[1]]),
    });
    await instance.renderMessages(fakeWindow, fakeDocument, "container-id");
    for (let message of messages) {
      assert.ok(
        createdElements.find(
          el => el.tagName === "h2" && el.textContent === message.content.title
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
  it("should only render unique dates (no duplicates)", async () => {
    instance._createDateElement = sandbox.stub();
    const messages = (await PanelTestProvider.getMessages()).filter(
      m => m.template === "whatsnew_panel_message"
    );
    const uniqueDates = [
      ...new Set(messages.map(m => m.content.published_date)),
    ];
    instance.init(waitForInitializedStub, {
      getMessages: sandbox.stub().returns(messages),
    });
    await instance.renderMessages(fakeWindow, fakeDocument, "container-id");
    assert.callCount(instance._createDateElement, uniqueDates.length);
  });
  it("should listen for panelhidden and remove the toolbar button", async () => {
    instance.init(waitForInitializedStub, {
      getMessages: sandbox.stub().returns([]),
    });
    fakeDocument.getElementById
      .withArgs("customizationui-widget-panel")
      .returns(null);

    await instance.renderMessages(fakeWindow, fakeDocument, "container-id");

    assert.notCalled(fakeElementById.addEventListener);
  });
  it("should listen for panelhidden and remove the toolbar button", async () => {
    instance.init(waitForInitializedStub, {
      getMessages: sandbox.stub().returns([]),
    });

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
});

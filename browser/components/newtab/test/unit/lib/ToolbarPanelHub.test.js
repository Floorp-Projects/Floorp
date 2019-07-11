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

  beforeEach(async () => {
    sandbox = sinon.createSandbox();
    globals = new GlobalOverrider();
    instance = new _ToolbarPanelHub();
    fakeElementById = {
      setAttribute: sandbox.stub(),
      removeAttribute: sandbox.stub(),
      querySelector: sandbox.stub().returns(null),
      appendChild: sandbox.stub(),
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
    globals.set("EveryWindow", everyWindowStub);
  });
  afterEach(() => {
    instance.uninit();
    sandbox.restore();
  });
  it("should create an instance", () => {
    assert.ok(instance);
  });
  it("should not enableAppmenuButton() on init() if pref is not enabled", () => {
    sandbox.stub(global.Services.prefs, "getBoolPref").returns(false);
    instance.enableAppmenuButton = sandbox.stub();
    instance.init({ getMessages: () => {} });
    assert.notCalled(instance.enableAppmenuButton);
  });
  it("should enableAppmenuButton() on init() if pref is enabled", () => {
    sandbox.stub(global.Services.prefs, "getBoolPref").returns(true);
    instance.enableAppmenuButton = sandbox.stub();
    instance.init({ getMessages: () => {} });
    assert.calledOnce(instance.enableAppmenuButton);
  });
  it("should unregisterCallback on uninit()", () => {
    instance.uninit();
    assert.calledTwice(everyWindowStub.unregisterCallback);
  });
  it("should registerCallback on enableAppmenuButton()", () => {
    instance.enableAppmenuButton();
    assert.calledOnce(everyWindowStub.registerCallback);
  });
  it("should registerCallback on enableToolbarButton()", () => {
    instance.enableToolbarButton();
    assert.calledOnce(everyWindowStub.registerCallback);
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
    instance.init({
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
    instance.init({
      getMessages: sandbox.stub().returns(messages),
    });
    await instance.renderMessages(fakeWindow, fakeDocument, "container-id");
    assert.callCount(instance._createDateElement, uniqueDates.length);
  });
});

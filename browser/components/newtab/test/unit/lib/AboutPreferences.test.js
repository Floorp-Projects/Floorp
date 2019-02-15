/* global Services */
import {AboutPreferences, PREFERENCES_LOADED_EVENT} from "lib/AboutPreferences.jsm";
import {actionTypes as at} from "common/Actions.jsm";
import {GlobalOverrider} from "test/unit/utils";

describe("AboutPreferences Feed", () => {
  let globals;
  let sandbox;
  let Sections;
  let DiscoveryStream;
  let instance;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    Sections = [];
    DiscoveryStream = {config: {enabled: false}};
    instance = new AboutPreferences();
    instance.store = {getState: () => ({Sections, DiscoveryStream})};
  });
  afterEach(() => {
    globals.restore();
  });

  describe("#onAction", () => {
    it("should call .init() on an INIT action", () => {
      const stub = sandbox.stub(instance, "init");

      instance.onAction({type: at.INIT});

      assert.calledOnce(stub);
    });
    it("should call .uninit() on an UNINIT action", () => {
      const stub = sandbox.stub(instance, "uninit");

      instance.onAction({type: at.UNINIT});

      assert.calledOnce(stub);
    });
    it("should call .openPreferences on SETTINGS_OPEN", () => {
      const action = {type: at.SETTINGS_OPEN, _target: {browser: {ownerGlobal: {openPreferences: sinon.spy()}}}};
      instance.onAction(action);
      assert.calledOnce(action._target.browser.ownerGlobal.openPreferences);
    });
    it("should call .BrowserOpenAddonsMgr with the extension id on OPEN_WEBEXT_SETTINGS", () => {
      const action = {type: at.OPEN_WEBEXT_SETTINGS, data: "foo", _target: {browser: {ownerGlobal: {BrowserOpenAddonsMgr: sinon.spy()}}}};
      instance.onAction(action);
      assert.calledWith(
        action._target.browser.ownerGlobal.BrowserOpenAddonsMgr,
        "addons://detail/foo"
      );
    });
  });
  describe("#observe", () => {
    it("should watch for about:preferences loading", () => {
      sandbox.stub(Services.obs, "addObserver");

      instance.init();

      assert.calledOnce(Services.obs.addObserver);
      assert.calledWith(Services.obs.addObserver, instance, PREFERENCES_LOADED_EVENT);
    });
    it("should stop watching on uninit", () => {
      sandbox.stub(Services.obs, "removeObserver");

      instance.uninit();

      assert.calledOnce(Services.obs.removeObserver);
      assert.calledWith(Services.obs.removeObserver, instance, PREFERENCES_LOADED_EVENT);
    });
    it("should try to render on event", async () => {
      const stub = sandbox.stub(instance, "renderPreferences");
      instance._strings = {};
      Sections.push({});

      await instance.observe(window, PREFERENCES_LOADED_EVENT);

      assert.calledOnce(stub);
      assert.equal(stub.firstCall.args[0], window);
      assert.deepEqual(stub.firstCall.args[1], instance._strings);
      assert.include(stub.firstCall.args[2], Sections[0]);
    });
  });
  describe("#strings", () => {
    let activityStreamLocale;
    let fetchStub;
    let fetchText;
    beforeEach(() => {
      global.Cc["@mozilla.org/browser/aboutnewtab-service;1"] = {
        getService() {
          return {activityStreamLocale};
        },
      };
      fetchStub = sandbox.stub().resolves({text: () => Promise.resolve(fetchText)});
      globals.set("fetch", fetchStub);
    });
    it("should use existing strings if they exist", async () => {
      instance._strings = {};

      const strings = await instance.strings;

      assert.equal(strings, instance._strings);
    });
    it("should report failure if missing", async () => {
      sandbox.stub(Cu, "reportError");

      const strings = await instance.strings;

      assert.calledOnce(Cu.reportError);
      assert.deepEqual(strings, {});
    });
    it("should fetch with the appropriate locale", async () => {
      activityStreamLocale = "en-TEST";

      await instance.strings;

      assert.calledOnce(fetchStub);
      assert.include(fetchStub.firstCall.args[0], activityStreamLocale);
    });
    it("should extract strings from js text", async () => {
      const testStrings = {hello: "world"};
      fetchText = `var strings = ${JSON.stringify(testStrings)};`;

      const strings = await instance.strings;

      assert.deepEqual(strings, testStrings);
    });
  });
  describe("#renderPreferences", () => {
    let node;
    let strings;
    let prefStructure;
    let Preferences;
    let gHomePane;
    const testRender = () => instance.renderPreferences({
      document: {
        createXULElement: sandbox.stub().returns(node),
        createProcessingInstruction: sandbox.stub(),
        createElementNS: sandbox.stub().callsFake((NS, el) => node),
        getElementById: sandbox.stub().returns(node),
        insertBefore: sandbox.stub().returnsArg(0),
        querySelector: sandbox.stub().returns({appendChild: sandbox.stub()}),
      },
      Preferences,
      gHomePane,
    }, strings, prefStructure, DiscoveryStream.config);
    beforeEach(() => {
      node = {
        appendChild: sandbox.stub().returnsArg(0),
        addEventListener: sandbox.stub(),
        classList: {add: sandbox.stub(), remove: sandbox.stub()},
        cloneNode: sandbox.stub().returnsThis(),
        insertAdjacentElement: sandbox.stub().returnsArg(1),
        setAttribute: sandbox.stub(),
        remove: sandbox.stub(),
        style: {},
      };
      strings = {};
      prefStructure = [];
      Preferences = {
        add: sandbox.stub(),
        get: sandbox.stub().returns({}),
      };
      gHomePane = {toggleRestoreDefaultsBtn: sandbox.stub()};
    });
    describe("#formatString", () => {
      it("should fall back to string id if missing", () => {
        testRender();

        assert.equal(node.textContent, "prefs_home_description");
      });
      it("should use provided plain string", () => {
        strings = {prefs_home_description: "hello"};

        testRender();

        assert.equal(node.textContent, "hello");
      });
      it("should fall back to string object if missing", () => {
        const titleString = {id: "foo"};
        prefStructure = [{pref: {titleString}}];

        testRender();

        assert.calledWith(node.setAttribute, "label", JSON.stringify(titleString));
      });
      it("should use provided string object id", () => {
        strings = {foo: "bar"};
        prefStructure = [{pref: {titleString: {id: "foo"}}}];

        testRender();

        assert.calledWith(node.setAttribute, "label", "bar");
      });
      it("should use values in string object", () => {
        strings = {foo: "l{n}{n}t"};
        prefStructure = [{pref: {titleString: {id: "foo", values: {n: 3}}}}];

        testRender();

        assert.calledWith(node.setAttribute, "label", "l33t");
      });
    });
    describe("#linkPref", () => {
      it("should add a pref to the global", () => {
        prefStructure = [{}];

        testRender();

        assert.calledOnce(Preferences.add);
      });
      it("should skip adding if not shown", () => {
        prefStructure = [{shouldHidePref: true}];

        testRender();

        assert.notCalled(Preferences.add);
      });
    });
    describe("pref icon", () => {
      it("should default to webextension icon", () => {
        prefStructure = [{}];

        testRender();

        assert.calledWith(node.setAttribute, "src", "resource://activity-stream/data/content/assets/glyph-webextension-16.svg");
      });
      it("should use desired glyph icon", () => {
        prefStructure = [{icon: "highlights"}];

        testRender();

        assert.calledWith(node.setAttribute, "src", "resource://activity-stream/data/content/assets/glyph-highlights-16.svg");
      });
      it("should use specified chrome icon", () => {
        const icon = "chrome://the/icon.svg";
        prefStructure = [{icon}];

        testRender();

        assert.calledWith(node.setAttribute, "src", icon);
      });
    });
    describe("title line", () => {
      it("should render a title", () => {
        const titleString = "the_title";
        prefStructure = [{pref: {titleString}}];

        testRender();

        assert.calledWith(node.setAttribute, "label", titleString);
      });
      it("should add a link for top stories", () => {
        const href = "https://disclaimer/";
        prefStructure = [{learnMore: {link: {href}}, id: "topstories"}];

        testRender();
        assert.calledWith(node.setAttribute, "href", href);
      });
    });
    describe("description line", () => {
      it("should render a description", () => {
        const descString = "the_desc";
        prefStructure = [{pref: {descString}}];

        testRender();

        assert.equal(node.textContent, descString);
      });
      it("should render rows dropdown with appropriate number", () => {
        prefStructure = [{rowsPref: "row_pref", maxRows: 3, pref: {descString: "foo"}}];

        testRender();

        assert.calledWith(node.setAttribute, "value", 1);
        assert.calledWith(node.setAttribute, "value", 2);
        assert.calledWith(node.setAttribute, "value", 3);
      });
    });
    describe("nested prefs", () => {
      it("should render a nested pref", () => {
        const titleString = "im_nested";
        prefStructure = [{pref: {nestedPrefs: [{titleString}]}}];

        testRender();

        assert.calledWith(node.setAttribute, "label", titleString);
      });
    });
    describe("restore defaults btn", () => {
      it("should call toggleRestoreDefaultsBtn", () => {
        testRender();

        assert.calledOnce(gHomePane.toggleRestoreDefaultsBtn);
      });
    });
    describe("#DiscoveryStream", () => {
      let PreferenceExperimentsStub;
      beforeEach(() => {
        PreferenceExperimentsStub = {
          getAllActive: sandbox.stub().resolves([{name: "discoverystream", preferenceName: "browser.newtabpage.activity-stream.discoverystream.config"}]),
          stop: sandbox.stub().resolves(),
        };
        globals.set("PreferenceExperiments", PreferenceExperimentsStub);
      });
      it("should not render the Discovery Stream section", () => {
        testRender();

        assert.isFalse(node.textContent.includes("prefs_content_discovery"));
      });
      it("should render the Discovery Stream section", () => {
        DiscoveryStream = {config: {enabled: true, show_spocs: false}};
        const spy = sandbox.spy(instance, "renderPreferences");

        testRender();

        const documentStub = spy.firstCall.args[0].document;

        assert.equal(node.textContent, "prefs_content_discovery_button");
        assert.calledWith(documentStub.createElementNS, "http://www.w3.org/1999/xhtml", "button");
        // node points to contentsGroup, style is set to collapse if Discovery
        // Stream is enabled
        assert.propertyVal(node.style, "visibility", "collapse");
      });
      it("should toggle the Discovery Stream pref on button click", async () => {
        DiscoveryStream = {config: {enabled: true}};
        PreferenceExperimentsStub = {
          getAllActive: sandbox.stub().resolves([{name: "discoverystream", preferenceName: "browser.newtabpage.activity-stream.discoverystream.config"}]),
          stop: sandbox.stub().resolves(),
        };
        globals.set("PreferenceExperiments", PreferenceExperimentsStub);

        testRender();

        assert.calledOnce(node.addEventListener);

        // Trigger the button click listener
        await node.addEventListener.firstCall.args[1]();

        assert.calledOnce(PreferenceExperimentsStub.getAllActive);
        assert.calledOnce(PreferenceExperimentsStub.stop);
        assert.calledWithExactly(PreferenceExperimentsStub.stop, "discoverystream", {resetValue: true, reason: "individual-opt-out"});
      });
      it("should render the spocs opt out checkbox if show_spocs is true", () => {
        const spy = sandbox.spy(instance, "renderPreferences");
        DiscoveryStream = {config: {enabled: true, show_spocs: true}};
        testRender();

        const {createXULElement} = spy.firstCall.args[0].document;
        assert.calledWith(createXULElement, "checkbox");
        assert.calledWith(Preferences.add, {id: "browser.newtabpage.activity-stream.showSponsored", type: "bool"});
      });
      it("should not render the spocs opt out checkbox if show_spocs is false", () => {
        const spy = sandbox.spy(instance, "renderPreferences");
        DiscoveryStream = {config: {enabled: true, show_spocs: false}};
        testRender();

        const {createXULElement} = spy.firstCall.args[0].document;
        assert.neverCalledWith(createXULElement, "checkbox");
        assert.neverCalledWith(Preferences.add, {id: "browser.newtabpage.activity-stream.showSponsored", type: "bool"});
      });
      describe("spocs pref checkbox", () => {
        beforeEach(() => {
          DiscoveryStream = {config: {enabled: true, show_spocs: true}};
          prefStructure = [{pref: {nestedPrefs: [{titleString: "spocs", name: "showSponsored"}]}}];
        });
        it("should remove the topstories spocs checkbox", () => {
          testRender();

          assert.calledOnce(node.remove);
          assert.calledOnce(node.classList.remove);
          assert.calledWith(node.classList.remove, "indent");
        });
        it("should restore the checkbox when leaving the experiment", async () => {
          const spy = sandbox.spy(instance, "renderPreferences");

          testRender();

          const [{document}] = spy.firstCall.args;

          // Trigger the button click listener
          await node.addEventListener.firstCall.args[1]();
          assert.calledOnce(document.querySelector);
          assert.calledWith(document.querySelector, "[data-subcategory='topstories'] .indent");
        });
      });
    });
  });
});

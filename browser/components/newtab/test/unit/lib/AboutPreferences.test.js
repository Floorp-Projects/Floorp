/* global Services */
import {
  AboutPreferences,
  PREFERENCES_LOADED_EVENT,
} from "lib/AboutPreferences.jsm";
import { actionTypes as at, actionCreators as ac } from "common/Actions.jsm";
import { GlobalOverrider } from "test/unit/utils";

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
    DiscoveryStream = { config: { enabled: false } };
    instance = new AboutPreferences();
    instance.store = {
      dispatch: sandbox.stub(),
      getState: () => ({ Sections, DiscoveryStream }),
    };
    globals.set("NimbusFeatures", { newtab: { getValue: sandbox.stub() } });
  });
  afterEach(() => {
    globals.restore();
  });

  describe("#onAction", () => {
    it("should call .init() on an INIT action", () => {
      const stub = sandbox.stub(instance, "init");

      instance.onAction({ type: at.INIT });

      assert.calledOnce(stub);
    });
    it("should call .uninit() on an UNINIT action", () => {
      const stub = sandbox.stub(instance, "uninit");

      instance.onAction({ type: at.UNINIT });

      assert.calledOnce(stub);
    });
    it("should call .openPreferences on SETTINGS_OPEN", () => {
      const action = {
        type: at.SETTINGS_OPEN,
        _target: { browser: { ownerGlobal: { openPreferences: sinon.spy() } } },
      };
      instance.onAction(action);
      assert.calledOnce(action._target.browser.ownerGlobal.openPreferences);
    });
    it("should call .BrowserOpenAddonsMgr with the extension id on OPEN_WEBEXT_SETTINGS", () => {
      const action = {
        type: at.OPEN_WEBEXT_SETTINGS,
        data: "foo",
        _target: {
          browser: { ownerGlobal: { BrowserOpenAddonsMgr: sinon.spy() } },
        },
      };
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
      assert.calledWith(
        Services.obs.addObserver,
        instance,
        PREFERENCES_LOADED_EVENT
      );
    });
    it("should stop watching on uninit", () => {
      sandbox.stub(Services.obs, "removeObserver");

      instance.uninit();

      assert.calledOnce(Services.obs.removeObserver);
      assert.calledWith(
        Services.obs.removeObserver,
        instance,
        PREFERENCES_LOADED_EVENT
      );
    });
    it("should try to render on event", async () => {
      const stub = sandbox.stub(instance, "renderPreferences");
      Sections.push({});

      await instance.observe(window, PREFERENCES_LOADED_EVENT);

      assert.calledOnce(stub);
      assert.equal(stub.firstCall.args[0], window);
      assert.include(stub.firstCall.args[1], Sections[0]);
    });
    it("Hide topstories rows select in sections if discovery stream is enabled", async () => {
      const stub = sandbox.stub(instance, "renderPreferences");

      Sections.push({
        rowsPref: "row_pref",
        maxRows: 3,
        pref: { descString: "foo" },
        learnMore: { link: "https://foo.com" },
        id: "topstories",
      });
      DiscoveryStream = { config: { enabled: true } };

      await instance.observe(window, PREFERENCES_LOADED_EVENT);

      assert.calledOnce(stub);
      const [, structure] = stub.firstCall.args;
      assert.equal(structure[0].id, "search");
      assert.equal(structure[1].id, "topsites");
      assert.equal(structure[2].id, "topstories");
      assert.isEmpty(structure[2].rowsPref);
    });
  });
  describe("#renderPreferences", () => {
    let node;
    let prefStructure;
    let Preferences;
    let gHomePane;
    const testRender = () =>
      instance.renderPreferences(
        {
          document: {
            createXULElement: sandbox.stub().returns(node),
            l10n: {
              setAttributes(el, id, args) {
                el.setAttribute("data-l10n-id", id);
                el.setAttribute("data-l10n-args", JSON.stringify(args));
              },
            },
            createProcessingInstruction: sandbox.stub(),
            createElementNS: sandbox.stub().callsFake((NS, el) => node),
            getElementById: sandbox.stub().returns(node),
            insertBefore: sandbox.stub().returnsArg(0),
            querySelector: sandbox
              .stub()
              .returns({ appendChild: sandbox.stub() }),
          },
          Preferences,
          gHomePane,
        },
        prefStructure,
        DiscoveryStream.config
      );
    beforeEach(() => {
      node = {
        appendChild: sandbox.stub().returnsArg(0),
        addEventListener: sandbox.stub(),
        classList: { add: sandbox.stub(), remove: sandbox.stub() },
        cloneNode: sandbox.stub().returnsThis(),
        insertAdjacentElement: sandbox.stub().returnsArg(1),
        setAttribute: sandbox.stub(),
        remove: sandbox.stub(),
        style: {},
      };
      prefStructure = [];
      Preferences = {
        add: sandbox.stub(),
        get: sandbox.stub().returns({
          on: sandbox.stub(),
        }),
      };
      gHomePane = { toggleRestoreDefaultsBtn: sandbox.stub() };
    });
    describe("#getString", () => {
      it("should not fail if titleString is not provided", () => {
        prefStructure = [{ pref: {} }];

        testRender();
        assert.calledWith(
          node.setAttribute,
          "data-l10n-id",
          sinon.match.typeOf("undefined")
        );
      });
      it("should return the string id if titleString is just a string", () => {
        const titleString = "foo";
        prefStructure = [{ pref: { titleString } }];

        testRender();
        assert.calledWith(node.setAttribute, "data-l10n-id", titleString);
      });
      it("should set id and args if titleString is an object with id and values", () => {
        const titleString = { id: "foo", values: { provider: "bar" } };
        prefStructure = [{ pref: { titleString } }];

        testRender();
        assert.calledWith(node.setAttribute, "data-l10n-id", titleString.id);
        assert.calledWith(
          node.setAttribute,
          "data-l10n-args",
          JSON.stringify(titleString.values)
        );
      });
    });
    describe("#linkPref", () => {
      it("should add a pref to the global", () => {
        prefStructure = [{ pref: { feed: "feed" } }];

        testRender();

        assert.calledOnce(Preferences.add);
      });
      it("should skip adding if not shown", () => {
        prefStructure = [{ shouldHidePref: true }];

        testRender();

        assert.notCalled(Preferences.add);
      });
    });
    describe("pref icon", () => {
      it("should default to webextension icon", () => {
        prefStructure = [{ pref: { feed: "feed" } }];

        testRender();

        assert.calledWith(
          node.setAttribute,
          "src",
          "chrome://activity-stream/content/data/content/assets/glyph-webextension-16.svg"
        );
      });
      it("should use desired glyph icon", () => {
        prefStructure = [{ icon: "highlights", pref: { feed: "feed" } }];

        testRender();

        assert.calledWith(
          node.setAttribute,
          "src",
          "chrome://activity-stream/content/data/content/assets/glyph-highlights-16.svg"
        );
      });
      it("should use specified chrome icon", () => {
        const icon = "chrome://the/icon.svg";
        prefStructure = [{ icon, pref: { feed: "feed" } }];

        testRender();

        assert.calledWith(node.setAttribute, "src", icon);
      });
    });
    describe("title line", () => {
      it("should render a title", () => {
        const titleString = "the_title";
        prefStructure = [{ pref: { titleString } }];

        testRender();

        assert.calledWith(node.setAttribute, "data-l10n-id", titleString);
      });
    });
    describe("top stories", () => {
      const href = "https://disclaimer/";
      const eventSource = "https://disclaimer/";
      beforeEach(() => {
        prefStructure = [
          {
            id: "topstories",
            pref: { feed: "feed", learnMore: { link: { href } } },
            eventSource,
          },
        ];
      });
      it("should add a link for top stories", () => {
        testRender();
        assert.calledWith(node.setAttribute, "href", href);
      });
      it("should setup a user event for top stories eventSource", () => {
        sinon.spy(instance, "setupUserEvent");
        testRender();
        assert.calledWith(node.addEventListener, "command");
        assert.calledWith(instance.setupUserEvent, node, eventSource);
      });
      it("should setup a user event for top stories nested pref eventSource", () => {
        sinon.spy(instance, "setupUserEvent");
        prefStructure = [
          {
            id: "topstories",
            pref: {
              feed: "feed",
              learnMore: { link: { href } },
              nestedPrefs: [
                {
                  name: "showSponsored",
                  titleString:
                    "home-prefs-recommended-by-option-sponsored-stories",
                  icon: "icon-info",
                  eventSource: "POCKET_SPOCS",
                },
              ],
            },
          },
        ];
        testRender();
        assert.calledWith(node.addEventListener, "command");
        assert.calledWith(instance.setupUserEvent, node, "POCKET_SPOCS");
      });
      it("should fire store dispatch with onCommand", () => {
        const element = {
          addEventListener: (command, action) => {
            // Trigger the action right away because we only care about testing the action here.
            action({ target: { checked: true } });
          },
        };
        instance.setupUserEvent(element, eventSource);
        assert.calledWith(
          instance.store.dispatch,
          ac.UserEvent({
            event: "PREF_CHANGED",
            source: eventSource,
            value: { menu_source: "ABOUT_PREFERENCES", status: true },
          })
        );
      });
    });
    describe("description line", () => {
      it("should render a description", () => {
        const descString = "the_desc";
        prefStructure = [{ pref: { descString } }];

        testRender();

        assert.calledWith(node.setAttribute, "data-l10n-id", descString);
      });
      it("should render rows dropdown with appropriate number", () => {
        prefStructure = [
          { rowsPref: "row_pref", maxRows: 3, pref: { descString: "foo" } },
        ];

        testRender();

        assert.calledWith(node.setAttribute, "value", 1);
        assert.calledWith(node.setAttribute, "value", 2);
        assert.calledWith(node.setAttribute, "value", 3);
      });
    });
    describe("nested prefs", () => {
      const titleString = "im_nested";
      beforeEach(() => {
        prefStructure = [{ pref: { nestedPrefs: [{ titleString }] } }];
      });
      it("should render a nested pref", () => {
        testRender();

        assert.calledWith(node.setAttribute, "data-l10n-id", titleString);
      });
      it("should set node hidden to true", () => {
        prefStructure[0].pref.nestedPrefs[0].hidden = true;

        testRender();

        assert.isTrue(node.hidden);
      });
      it("should add a change event", () => {
        testRender();

        assert.calledOnce(Preferences.get().on);
        assert.calledWith(Preferences.get().on, "change");
      });
      it("should default node disabled to false", async () => {
        Preferences.get = sandbox.stub().returns({
          on: sandbox.stub(),
          _value: true,
        });

        testRender();

        assert.isFalse(node.disabled);
      });
      it("should default node disabled to true", async () => {
        testRender();

        assert.isTrue(node.disabled);
      });
      it("should set node disabled to true", async () => {
        const pref = {
          on: sandbox.stub(),
          _value: true,
        };
        Preferences.get = sandbox.stub().returns(pref);

        testRender();
        pref._value = !pref._value;
        await Preferences.get().on.firstCall.args[1]();

        assert.isTrue(node.disabled);
      });
      it("should set node disabled to false", async () => {
        const pref = {
          on: sandbox.stub(),
          _value: false,
        };
        Preferences.get = sandbox.stub().returns(pref);

        testRender();
        pref._value = !pref._value;
        await Preferences.get().on.firstCall.args[1]();

        assert.isFalse(node.disabled);
      });
    });
    describe("restore defaults btn", () => {
      it("should call toggleRestoreDefaultsBtn", () => {
        testRender();

        assert.calledOnce(gHomePane.toggleRestoreDefaultsBtn);
      });
    });
  });
});

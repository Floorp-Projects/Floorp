import { RemoteL10n, _RemoteL10n } from "lib/RemoteL10n.jsm";
import { GlobalOverrider } from "test/unit/utils";

describe("RemoteL10n", () => {
  let sandbox;
  let globals;
  let domL10nStub;
  let l10nRegStub;
  let l10nRegInstance;
  let fileSourceStub;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    globals = new GlobalOverrider();
    domL10nStub = sandbox.stub();
    l10nRegInstance = {
      hasSource: sandbox.stub(),
      registerSources: sandbox.stub(),
      removeSources: sandbox.stub(),
    };

    fileSourceStub = sandbox.stub();
    l10nRegStub = {
      getInstance: () => {
        return l10nRegInstance;
      },
    };
    globals.set("DOMLocalization", domL10nStub);
    globals.set("L10nRegistry", l10nRegStub);
    globals.set("L10nFileSource", fileSourceStub);
  });
  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });
  describe("#RemoteL10n", () => {
    it("should create a new instance", () => {
      assert.ok(new _RemoteL10n());
    });
    it("should create a DOMLocalization instance", () => {
      domL10nStub.returns({ instance: true });
      const instance = new _RemoteL10n();

      assert.propertyVal(instance._createDOML10n(), "instance", true);
      assert.calledOnce(domL10nStub);
    });
    it("should create a new instance", () => {
      domL10nStub.returns({ instance: true });
      const instance = new _RemoteL10n();

      assert.ok(instance.l10n);

      instance.reloadL10n();

      assert.ok(instance.l10n);

      assert.calledTwice(domL10nStub);
    });
    it("should reuse the instance", () => {
      domL10nStub.returns({ instance: true });
      const instance = new _RemoteL10n();

      assert.ok(instance.l10n);
      assert.ok(instance.l10n);

      assert.calledOnce(domL10nStub);
    });
  });
  describe("#_createDOML10n", () => {
    it("should load the remote Fluent file if USE_REMOTE_L10N_PREF is true", async () => {
      sandbox.stub(global.Services.prefs, "getBoolPref").returns(true);
      l10nRegInstance.hasSource.returns(false);
      RemoteL10n._createDOML10n();

      assert.calledOnce(domL10nStub);
      const { args } = domL10nStub.firstCall;
      // The first arg is the resource array,
      // the second one is false (use async),
      // and the third one is the bundle generator.
      assert.equal(args.length, 2);
      assert.deepEqual(args[0], [
        "browser/newtab/asrouter.ftl",
        "browser/branding/brandings.ftl",
        "browser/branding/sync-brand.ftl",
        "branding/brand.ftl",
        "browser/defaultBrowserNotification.ftl",
      ]);
      assert.isFalse(args[1]);
      assert.calledOnce(l10nRegInstance.hasSource);
      assert.calledOnce(l10nRegInstance.registerSources);
      assert.notCalled(l10nRegInstance.removeSources);
    });
    it("should load the local Fluent file if USE_REMOTE_L10N_PREF is false", () => {
      sandbox.stub(global.Services.prefs, "getBoolPref").returns(false);
      l10nRegInstance.hasSource.returns(true);
      RemoteL10n._createDOML10n();

      const { args } = domL10nStub.firstCall;
      // The first arg is the resource array,
      // the second one is false (use async),
      // and the third one is null.
      assert.equal(args.length, 2);
      assert.deepEqual(args[0], [
        "browser/newtab/asrouter.ftl",
        "browser/branding/brandings.ftl",
        "browser/branding/sync-brand.ftl",
        "branding/brand.ftl",
        "browser/defaultBrowserNotification.ftl",
      ]);
      assert.isFalse(args[1]);
      assert.calledOnce(l10nRegInstance.hasSource);
      assert.notCalled(l10nRegInstance.registerSources);
      assert.calledOnce(l10nRegInstance.removeSources);
    });
  });
  describe("#createElement", () => {
    let doc;
    let instance;
    let setStringStub;
    let elem;
    beforeEach(() => {
      elem = document.createElement("div");
      doc = {
        createElement: sandbox.stub().returns(elem),
        createElementNS: sandbox.stub().returns(elem),
      };
      instance = new _RemoteL10n();
      setStringStub = sandbox.stub(instance, "setString");
    });
    it("should call createElement if string_id is defined", () => {
      instance.createElement(doc, "span", { content: { string_id: "foo" } });

      assert.calledOnce(doc.createElement);
    });
    it("should call createElementNS if string_id is not present", () => {
      instance.createElement(doc, "span", { content: "foo" });

      assert.calledOnce(doc.createElementNS);
    });
    it("should set classList", () => {
      instance.createElement(doc, "span", { classList: "foo" });

      assert.isTrue(elem.classList.contains("foo"));
    });
    it("should call setString", () => {
      const options = { classList: "foo" };
      instance.createElement(doc, "span", options);

      assert.calledOnce(setStringStub);
      assert.calledWithExactly(setStringStub, elem, options);
    });
  });
  describe("#setString", () => {
    let instance;
    beforeEach(() => {
      instance = new _RemoteL10n();
    });
    it("should set fluent variables and id", () => {
      let el = { setAttribute: sandbox.stub() };
      instance.setString(el, {
        content: { string_id: "foo" },
        attributes: { bar: "bar", baz: "baz" },
      });

      assert.calledThrice(el.setAttribute);
      assert.calledWithExactly(el.setAttribute, "fluent-variable-bar", "bar");
      assert.calledWithExactly(el.setAttribute, "fluent-variable-baz", "baz");
      assert.calledWithExactly(el.setAttribute, "fluent-remote-id", "foo");
    });
    it("should set content if no string_id", () => {
      let el = { setAttribute: sandbox.stub() };
      instance.setString(el, { content: "foo" });

      assert.notCalled(el.setAttribute);
      assert.equal(el.textContent, "foo");
    });
  });
  describe("#isLocaleSupported", () => {
    it("should return true if the locale is en-US", () => {
      assert.ok(RemoteL10n.isLocaleSupported("en-US"));
    });
    it("should return true if the locale is in all-locales", () => {
      assert.ok(RemoteL10n.isLocaleSupported("en-CA"));
    });
    it("should return false if the locale is not in all-locales", () => {
      assert.ok(!RemoteL10n.isLocaleSupported("und"));
    });
  });
  describe("#formatLocalizableText", () => {
    let instance;
    let formatValueStub;
    beforeEach(() => {
      instance = new _RemoteL10n();
      formatValueStub = sandbox.stub();
      sandbox
        .stub(instance, "l10n")
        .get(() => ({ formatValue: formatValueStub }));
    });
    it("should localize a string_id", async () => {
      formatValueStub.resolves("VALUE");

      assert.equal(
        await instance.formatLocalizableText({ string_id: "ID" }),
        "VALUE"
      );
      assert.calledOnce(formatValueStub);
    });
    it("should pass through a string", async () => {
      formatValueStub.reset();

      assert.equal(
        await instance.formatLocalizableText("unchanged"),
        "unchanged"
      );
      assert.isFalse(formatValueStub.called);
    });
  });
});

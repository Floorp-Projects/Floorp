import { RemoteL10n, _RemoteL10n } from "lib/RemoteL10n.jsm";
import { GlobalOverrider } from "test/unit/utils";

describe("RemoteL10n", () => {
  let sandbox;
  let globals;
  let domL10nStub;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    globals = new GlobalOverrider();
    domL10nStub = sandbox.stub();
    globals.set("DOMLocalization", domL10nStub);
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
      RemoteL10n._createDOML10n();

      assert.calledOnce(domL10nStub);
      const { args } = domL10nStub.firstCall;
      // The first arg is the resource array,
      // the second one is false (use async),
      // and the third one is the bundle generator.
      assert.equal(args.length, 3);
      assert.deepEqual(args[0], [
        "browser/newtab/asrouter.ftl",
        "browser/branding/brandings.ftl",
        "browser/branding/sync-brand.ftl",
        "branding/brand.ftl",
        "browser/defaultBrowserNotification.ftl",
      ]);
      assert.isFalse(args[1]);
      assert.isFunction(args[2].generateBundles);
    });
    it("should load the local Fluent file if USE_REMOTE_L10N_PREF is false", () => {
      sandbox.stub(global.Services.prefs, "getBoolPref").returns(false);
      RemoteL10n._createDOML10n();

      const { args } = domL10nStub.firstCall;
      // The first arg is the resource array,
      // the second one is false (use async),
      // and the third one is null.
      assert.equal(args.length, 3);
      assert.deepEqual(args[0], [
        "browser/newtab/asrouter.ftl",
        "browser/branding/brandings.ftl",
        "browser/branding/sync-brand.ftl",
        "branding/brand.ftl",
        "browser/defaultBrowserNotification.ftl",
      ]);
      assert.isFalse(args[1]);
      assert.isEmpty(args[2]);
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
});

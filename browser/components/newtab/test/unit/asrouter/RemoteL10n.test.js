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
      ]);
      assert.isFalse(args[1]);
      assert.isEmpty(args[2]);
    });
  });
});

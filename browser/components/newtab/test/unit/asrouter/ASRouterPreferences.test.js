import {_ASRouterPreferences, ASRouterPreferences as ASRouterPreferencesSingleton, TEST_PROVIDERS} from "lib/ASRouterPreferences.jsm";
const FAKE_PROVIDERS = [{id: "foo"}, {id: "bar"}];

const PROVIDER_PREF_BRANCH = "browser.newtabpage.activity-stream.asrouter.providers.";
const DEVTOOLS_PREF = "browser.newtabpage.activity-stream.asrouter.devtoolsEnabled";
const SNIPPETS_USER_PREF = "browser.newtabpage.activity-stream.feeds.snippets";
const CFR_USER_PREF_ADDONS = "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons";
const CFR_USER_PREF_FEATURES = "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features";

/** NUMBER_OF_PREFS_TO_OBSERVE includes:
 *  1. asrouter.providers. pref branch
 *  2. asrouter.devtoolsEnabled
 *  3. browser.newtabpage.activity-stream.feeds.snippets (user preference - snippets)
 *  4. browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons (user preference - cfr)
 *  4. browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features (user preference - cfr)
 *  5. services.sync.username
 */
const NUMBER_OF_PREFS_TO_OBSERVE = 6;

describe("ASRouterPreferences", () => {
  let ASRouterPreferences;
  let sandbox;
  let addObserverStub;
  let stringPrefStub;
  let boolPrefStub;

  beforeEach(() => {
    ASRouterPreferences = new _ASRouterPreferences();

    sandbox = sinon.createSandbox();
    addObserverStub = sandbox.stub(global.Services.prefs, "addObserver");
    stringPrefStub =  sandbox.stub(global.Services.prefs, "getStringPref");
    FAKE_PROVIDERS.forEach(provider => {
      stringPrefStub.withArgs(`${PROVIDER_PREF_BRANCH}${provider.id}`).returns(JSON.stringify(provider));
    });
    sandbox.stub(global.Services.prefs, "getChildList")
      .withArgs(PROVIDER_PREF_BRANCH).returns(FAKE_PROVIDERS.map(provider => `${PROVIDER_PREF_BRANCH}${provider.id}`));

    boolPrefStub = sandbox.stub(global.Services.prefs, "getBoolPref").returns(false);
  });

  afterEach(() => {
    sandbox.restore();
  });

  function getPrefNameForProvider(providerId) {
    return `${PROVIDER_PREF_BRANCH}${providerId}`;
  }

  function setPrefForProvider(providerId, value) {
    stringPrefStub.withArgs(getPrefNameForProvider(providerId)).returns(JSON.stringify(value));
  }

  it("ASRouterPreferences should be an instance of _ASRouterPreferences", () => {
    assert.instanceOf(ASRouterPreferencesSingleton, _ASRouterPreferences);
  });
  describe("#init", () => {
    it("should set ._initialized to true", () => {
      ASRouterPreferences.init();
      assert.isTrue(ASRouterPreferences._initialized);
    });
    it(`should set ${NUMBER_OF_PREFS_TO_OBSERVE} observers and not re-initialize if already initialized`, () => {
      ASRouterPreferences.init();
      assert.callCount(addObserverStub, NUMBER_OF_PREFS_TO_OBSERVE);
      ASRouterPreferences.init();
      ASRouterPreferences.init();
      assert.callCount(addObserverStub, NUMBER_OF_PREFS_TO_OBSERVE);
    });
  });
  describe("#uninit", () => {
    it("should set ._initialized to false", () => {
      ASRouterPreferences.init();
      ASRouterPreferences.uninit();
      assert.isFalse(ASRouterPreferences._initialized);
    });
    it("should clear cached values for ._initialized, .devtoolsEnabled", () => {
      ASRouterPreferences.init();
      // trigger caching
      const result = [ASRouterPreferences.providers, ASRouterPreferences.devtoolsEnabled]; // eslint-disable-line no-unused-vars
      assert.isNotNull(ASRouterPreferences._providers, "providers should not be null");
      assert.isNotNull(ASRouterPreferences._devtoolsEnabled, "devtolosEnabled should not be null");

      ASRouterPreferences.uninit();
      assert.isNull(ASRouterPreferences._providers);
      assert.isNull(ASRouterPreferences._devtoolsEnabled);
    });
    it("should clear all listeners and remove observers (only once)", () => {
      const removeStub = sandbox.stub(global.Services.prefs, "removeObserver");
      ASRouterPreferences.init();
      ASRouterPreferences.addListener(() => {});
      ASRouterPreferences.addListener(() => {});
      assert.equal(ASRouterPreferences._callbacks.size, 2);
      ASRouterPreferences.uninit();
      // Tests to make sure we don't remove observers that weren't set
      ASRouterPreferences.uninit();

      assert.callCount(removeStub, NUMBER_OF_PREFS_TO_OBSERVE);
      assert.calledWith(removeStub, PROVIDER_PREF_BRANCH);
      assert.calledWith(removeStub, DEVTOOLS_PREF);
      assert.isEmpty(ASRouterPreferences._callbacks);
    });
  });
  describe(".providers", () => {
    it("should return the value the first time .providers is accessed", () => {
      ASRouterPreferences.init();

      const result = ASRouterPreferences.providers;
      assert.deepEqual(result, FAKE_PROVIDERS);
      // once per pref
      assert.calledTwice(stringPrefStub);
    });
    it("should return the cached value the second time .providers is accessed", () => {
      ASRouterPreferences.init();
      const [, secondCall] = [ASRouterPreferences.providers, ASRouterPreferences.providers];

      assert.deepEqual(secondCall, FAKE_PROVIDERS);
      // once per pref
      assert.calledTwice(stringPrefStub);
    });
    it("should just parse the pref each time if ASRouterPreferences hasn't been initialized yet", () => {
      // Intentionally not initialized
      const [firstCall, secondCall] = [ASRouterPreferences.providers, ASRouterPreferences.providers];

      assert.deepEqual(firstCall, FAKE_PROVIDERS);
      assert.deepEqual(secondCall, FAKE_PROVIDERS);
      assert.callCount(stringPrefStub, 4);
    });
    it("should skip the pref without throwing if a pref is not parsable", () => {
      stringPrefStub.withArgs(`${PROVIDER_PREF_BRANCH}foo`).returns("not json");
      ASRouterPreferences.init();

      assert.deepEqual(ASRouterPreferences.providers, [{id: "bar"}]);
    });
    it("should include TEST_PROVIDERS if devtools is turned on", () => {
      boolPrefStub.withArgs(DEVTOOLS_PREF).returns(true);
      ASRouterPreferences.init();

      assert.deepEqual(ASRouterPreferences.providers, [...TEST_PROVIDERS, ...FAKE_PROVIDERS]);
    });
  });
  describe(".devtoolsEnabled", () => {
    it("should read the pref the first time .devtoolsEnabled is accessed", () => {
      ASRouterPreferences.init();

      const result = ASRouterPreferences.devtoolsEnabled;
      assert.deepEqual(result, false);
      assert.calledOnce(boolPrefStub);
    });
    it("should return the cached value the second time .devtoolsEnabled is accessed", () => {
      ASRouterPreferences.init();
      const [, secondCall] = [ASRouterPreferences.devtoolsEnabled, ASRouterPreferences.devtoolsEnabled];

      assert.deepEqual(secondCall, false);
      assert.calledOnce(boolPrefStub);
    });
    it("should just parse the pref each time if ASRouterPreferences hasn't been initialized yet", () => {
      // Intentionally not initialized
      const [firstCall, secondCall] = [ASRouterPreferences.devtoolsEnabled, ASRouterPreferences.devtoolsEnabled];

      assert.deepEqual(firstCall, false);
      assert.deepEqual(secondCall, false);
      assert.calledTwice(boolPrefStub);
    });
  });
  describe("#getUserPreference(providerId)", () => {
    it("should return the user preference for snippets", () => {
      boolPrefStub.withArgs(SNIPPETS_USER_PREF).returns(true);
      assert.isTrue(ASRouterPreferences.getUserPreference("snippets"));
    });
  });
  describe("#getAllUserPreferences", () => {
    it("should return all user preferences", () => {
      boolPrefStub.withArgs(SNIPPETS_USER_PREF).returns(true);
      boolPrefStub.withArgs(CFR_USER_PREF_ADDONS).returns(false);
      boolPrefStub.withArgs(CFR_USER_PREF_FEATURES).returns(true);
      const result = ASRouterPreferences.getAllUserPreferences();
      assert.deepEqual(result, {snippets: true, cfrAddons: false, cfrFeatures: true});
    });
  });
  describe("#enableOrDisableProvider", () => {
    it("should enable an existing provider if second param is true", () => {
      const setStub = sandbox.stub(global.Services.prefs, "setStringPref");
      setPrefForProvider("foo", {id: "foo", enabled: false});
      assert.isFalse(ASRouterPreferences.providers[0].enabled);

      ASRouterPreferences.enableOrDisableProvider("foo", true);

      assert.calledWith(setStub, getPrefNameForProvider("foo"), JSON.stringify({id: "foo", enabled: true}));
    });
    it("should disable an existing provider if second param is false", () => {
      const setStub = sandbox.stub(global.Services.prefs, "setStringPref");
      setPrefForProvider("foo", {id: "foo", enabled: true});
      assert.isTrue(ASRouterPreferences.providers[0].enabled);

      ASRouterPreferences.enableOrDisableProvider("foo", false);

      assert.calledWith(setStub, getPrefNameForProvider("foo"), JSON.stringify({id: "foo", enabled: false}));
    });
    it("should not throw if the id does not exist", () => {
      assert.doesNotThrow(() => {
        ASRouterPreferences.enableOrDisableProvider("does_not_exist", true);
      });
    });
    it("should not throw if pref is not parseable", () => {
      stringPrefStub.withArgs(getPrefNameForProvider("foo")).returns("not valid");
      assert.doesNotThrow(() => {
        ASRouterPreferences.enableOrDisableProvider("foo", true);
      });
    });
  });
  describe("#setUserPreference", () => {
    it("should do nothing if the pref doesn't exist", () => {
      ASRouterPreferences.setUserPreference("foo", true);
      assert.notCalled(boolPrefStub);
    });
    it("should set the given pref", () => {
      const setStub = sandbox.stub(global.Services.prefs, "setBoolPref");
      ASRouterPreferences.setUserPreference("snippets", true);
      assert.calledWith(setStub, SNIPPETS_USER_PREF, true);
    });
  });
  describe("#resetProviderPref", () => {
    it("should reset the pref and user prefs", () => {
      const resetStub = sandbox.stub(global.Services.prefs, "clearUserPref");
      ASRouterPreferences.resetProviderPref();
      FAKE_PROVIDERS.forEach(provider => {
        assert.calledWith(resetStub, getPrefNameForProvider(provider.id));
      });
      assert.calledWith(resetStub, SNIPPETS_USER_PREF);
      assert.calledWith(resetStub, CFR_USER_PREF_ADDONS);
      assert.calledWith(resetStub, CFR_USER_PREF_FEATURES);
    });
  });
  describe("observer, listeners", () => {
    it("should invalidate .providers when the pref is changed", () => {
      const testProvider = {id: "newstuff"};
      const newProviders = [...FAKE_PROVIDERS, testProvider];

      ASRouterPreferences.init();

      assert.deepEqual(ASRouterPreferences.providers, FAKE_PROVIDERS);
      stringPrefStub.withArgs(getPrefNameForProvider(testProvider.id)).returns(JSON.stringify(testProvider));
      global.Services.prefs.getChildList
        .withArgs(PROVIDER_PREF_BRANCH).returns(newProviders.map(provider => getPrefNameForProvider(provider.id)));
      ASRouterPreferences.observe(null, null, getPrefNameForProvider(testProvider.id));

      // Cache should be invalidated so we access the new value of the pref now
      assert.deepEqual(ASRouterPreferences.providers, newProviders);
    });
    it("should invalidate .devtoolsEnabled and .providers when the pref is changed", () => {
      ASRouterPreferences.init();

      assert.isFalse(ASRouterPreferences.devtoolsEnabled);
      boolPrefStub.withArgs(DEVTOOLS_PREF).returns(true);
      global.Services.prefs.getChildList
        .withArgs(PROVIDER_PREF_BRANCH).returns([]);
      ASRouterPreferences.observe(null, null, DEVTOOLS_PREF);

      // Cache should be invalidated so we access the new value of the pref now
      // Note that providers needs to be invalidated because devtools adds test content to it.
      assert.isTrue(ASRouterPreferences.devtoolsEnabled);
      assert.deepEqual(ASRouterPreferences.providers, TEST_PROVIDERS);
    });
    it("should call listeners added with .addListener", () => {
      const callback1 = sinon.stub();
      const callback2 = sinon.stub();
      ASRouterPreferences.init();
      ASRouterPreferences.addListener(callback1);
      ASRouterPreferences.addListener(callback2);

      ASRouterPreferences.observe(null, null, getPrefNameForProvider("foo"));
      assert.calledWith(callback1, getPrefNameForProvider("foo"));

      ASRouterPreferences.observe(null, null, DEVTOOLS_PREF);
      assert.calledWith(callback2, DEVTOOLS_PREF);
    });
    it("should not call listeners after they are removed with .removeListeners", () => {
      const callback = sinon.stub();
      ASRouterPreferences.init();
      ASRouterPreferences.addListener(callback);

      ASRouterPreferences.observe(null, null, getPrefNameForProvider("foo"));
      assert.calledWith(callback, getPrefNameForProvider("foo"));

      callback.reset();
      ASRouterPreferences.removeListener(callback);

      ASRouterPreferences.observe(null, null, DEVTOOLS_PREF);
      assert.notCalled(callback);
    });
  });
  describe("_migratePrefs", () => {
    beforeEach(() => {
      sandbox.stub(global.Services.prefs, "setBoolPref");
      sandbox.stub(global.Services.prefs, "clearUserPref");
    });
    it("should not do anything if userpref was not modified", () => {
      ASRouterPreferences.init();

      assert.notCalled(global.Services.prefs.getBoolPref);
      assert.notCalled(global.Services.prefs.setBoolPref);
    });
    it("should not do migration if newPref was modified", () => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);

      ASRouterPreferences.init();

      assert.notCalled(global.Services.prefs.getBoolPref);
      assert.notCalled(global.Services.prefs.setBoolPref);
      assert.calledOnce(global.Services.prefs.clearUserPref);
      assert.calledWith(global.Services.prefs.clearUserPref, "browser.newtabpage.activity-stream.asrouter.userprefs.cfr");
    });
    it("should migrate userprefs.cfr", () => {
      const hasUserValueStub = sandbox.stub(global.Services.prefs, "prefHasUserValue");
      hasUserValueStub.onCall(0).returns(true);
      hasUserValueStub.returns(false);

      ASRouterPreferences.init();

      assert.calledOnce(global.Services.prefs.getBoolPref);
      assert.calledWith(global.Services.prefs.getBoolPref, "browser.newtabpage.activity-stream.asrouter.userprefs.cfr");
      assert.calledOnce(global.Services.prefs.setBoolPref);
      assert.calledWith(global.Services.prefs.setBoolPref, CFR_USER_PREF_ADDONS, false);
      assert.calledOnce(global.Services.prefs.clearUserPref);
      assert.calledWith(global.Services.prefs.clearUserPref, "browser.newtabpage.activity-stream.asrouter.userprefs.cfr");
    });
  });
});

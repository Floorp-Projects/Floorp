import {_ASRouterPreferences, ASRouterPreferences as ASRouterPreferencesSingleton} from "lib/ASRouterPreferences.jsm";
const FAKE_PROVIDERS = [{id: "foo"}, {id: "bar"}];

const PROVIDER_PREF = "browser.newtabpage.activity-stream.asrouter.messageProviders";
const DEVTOOLS_PREF = "browser.newtabpage.activity-stream.asrouter.devtoolsEnabled";
const SNIPPETS_USER_PREF = "browser.newtabpage.activity-stream.feeds.snippets";

/** NUMBER_OF_PREFS_TO_OBSERVE includes:
 *  1. asrouter.messageProvider
 *  2. asrouter.devtoolsEnabled
 *  3. browser.newtabpage.activity-stream.feeds.snippets (user preference - snippets)
 */
const NUMBER_OF_PREFS_TO_OBSERVE = 3;

describe("ASRouterPreferences", () => {
  let ASRouterPreferences;
  let sandbox;
  let addObserverStub;
  let stringPrefStub;
  let boolPrefStub;
  beforeEach(() => {
    ASRouterPreferences = new _ASRouterPreferences();

    sandbox = sinon.sandbox.create();
    addObserverStub = sandbox.stub(global.Services.prefs, "addObserver");
    stringPrefStub =  sandbox.stub(global.Services.prefs, "getStringPref").withArgs(PROVIDER_PREF).returns(JSON.stringify(FAKE_PROVIDERS));
    boolPrefStub = sandbox.stub(global.Services.prefs, "getBoolPref").returns(false);
  });
  afterEach(() => {
    sandbox.restore();
  });
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
      assert.calledWith(removeStub, PROVIDER_PREF);
      assert.calledWith(removeStub, DEVTOOLS_PREF);
      assert.isEmpty(ASRouterPreferences._callbacks);
    });
  });
  describe(".providers", () => {
    it("should return the value the first time .providers is accessed", () => {
      ASRouterPreferences.init();

      const result = ASRouterPreferences.providers;
      assert.deepEqual(result, FAKE_PROVIDERS);
      assert.calledOnce(stringPrefStub);
    });
    it("should return the cached value the second time .providers is accessed", () => {
      ASRouterPreferences.init();
      const [, secondCall] = [ASRouterPreferences.providers, ASRouterPreferences.providers];

      assert.deepEqual(secondCall, FAKE_PROVIDERS);
      assert.calledOnce(stringPrefStub);
    });
    it("should just parse the pref each time if ASRouterPreferences hasn't been initialized yet", () => {
      // Intentionally not initialized
      const [firstCall, secondCall] = [ASRouterPreferences.providers, ASRouterPreferences.providers];

      assert.deepEqual(firstCall, FAKE_PROVIDERS);
      assert.deepEqual(secondCall, FAKE_PROVIDERS);
      assert.calledTwice(stringPrefStub);
    });
    it("should return [] if the pref was not parsable", () => {
      stringPrefStub.withArgs(PROVIDER_PREF).returns("not json");
      ASRouterPreferences.init();

      assert.deepEqual(ASRouterPreferences.providers, []);
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
  describe(".specialConditions", () => {
    it("should return .allowLegacyOnboarding=true if onboarding is not present, disabled, or doesn't have cohort", () => {
      ASRouterPreferences.init();
      let testProviders = [];
      sandbox.stub(ASRouterPreferences, "providers").get(() => testProviders);

      testProviders = [{id: "foo"}];
      assert.isTrue(ASRouterPreferences.specialConditions.allowLegacyOnboarding);

      testProviders = [{id: "onboarding", enabled: false, cohort: "test"}];
      assert.isTrue(ASRouterPreferences.specialConditions.allowLegacyOnboarding);

      testProviders = [{id: "onboarding", enabled: true}];
      assert.isTrue(ASRouterPreferences.specialConditions.allowLegacyOnboarding);
    });
    it("should return .allowLegacyOnboarding=false if onboarding is enabled and has a cohort", () => {
      ASRouterPreferences.init();
      sandbox.stub(ASRouterPreferences, "providers").get(() => [{id: "onboarding", enabled: true, cohort: "test"}]);

      assert.isFalse(ASRouterPreferences.specialConditions.allowLegacyOnboarding);
    });
    it("should return .allowLegacySnippets=true if snippets is not present or disabled", () => {
      ASRouterPreferences.init();
      let testProviders = [];
      sandbox.stub(ASRouterPreferences, "providers").get(() => testProviders);

      testProviders = [{id: "foo"}];
      assert.isTrue(ASRouterPreferences.specialConditions.allowLegacySnippets);

      testProviders = [{id: "snippets", enabled: false}];
      assert.isTrue(ASRouterPreferences.specialConditions.allowLegacySnippets);
    });
    it("should return .allowLegacySnippets=false if snippets is enabled", () => {
      ASRouterPreferences.init();
      sandbox.stub(ASRouterPreferences, "providers").get(() => [{id: "snippets", enabled: true}]);

      assert.isFalse(ASRouterPreferences.specialConditions.allowLegacySnippets);
    });
  });
  describe("#getUserPreference(providerId)", () => {
    it("should return the user preference for snippets", () => {
      boolPrefStub.withArgs(SNIPPETS_USER_PREF).returns(true);
      assert.isTrue(ASRouterPreferences.getUserPreference("snippets"));
    });
  });
  describe("observer, listeners", () => {
    it("should invalidate .providers when the pref is changed", () => {
      const testProviders = [{id: "newstuff"}];

      ASRouterPreferences.init();

      assert.deepEqual(ASRouterPreferences.providers, FAKE_PROVIDERS);
      stringPrefStub.withArgs(PROVIDER_PREF).returns(JSON.stringify(testProviders));
      ASRouterPreferences.observe(null, null, PROVIDER_PREF);

      // Cache should be invalidated so we access the new value of the pref now
      assert.deepEqual(ASRouterPreferences.providers, testProviders);
    });
    it("should invalidate .devtoolsEnabled when the pref is changed", () => {
      ASRouterPreferences.init();

      assert.isFalse(ASRouterPreferences.devtoolsEnabled);
      boolPrefStub.withArgs(DEVTOOLS_PREF).returns(true);
      ASRouterPreferences.observe(null, null, DEVTOOLS_PREF);

      // Cache should be invalidated so we access the new value of the pref now
      assert.isTrue(ASRouterPreferences.devtoolsEnabled);
    });
    it("should call listeners added with .addListener", () => {
      const callback1 = sinon.stub();
      const callback2 = sinon.stub();
      ASRouterPreferences.init();
      ASRouterPreferences.addListener(callback1);
      ASRouterPreferences.addListener(callback2);

      ASRouterPreferences.observe(null, null, PROVIDER_PREF);
      assert.calledWith(callback1, PROVIDER_PREF);

      ASRouterPreferences.observe(null, null, DEVTOOLS_PREF);
      assert.calledWith(callback2, DEVTOOLS_PREF);
    });
    it("should not call listeners after they are removed with .removeListeners", () => {
      const callback = sinon.stub();
      ASRouterPreferences.init();
      ASRouterPreferences.addListener(callback);

      ASRouterPreferences.observe(null, null, PROVIDER_PREF);
      assert.calledWith(callback, PROVIDER_PREF);

      callback.reset();
      ASRouterPreferences.removeListener(callback);

      ASRouterPreferences.observe(null, null, DEVTOOLS_PREF);
      assert.notCalled(callback);
    });
  });
});

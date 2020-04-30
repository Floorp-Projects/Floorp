import { GlobalOverrider } from "test/unit/utils";
import { PersonalityProvider } from "lib/PersonalityProvider/PersonalityProvider.jsm";

const TIME_SEGMENTS = [
  { id: "hour", startTime: 3600, endTime: 0, weightPosition: 1 },
  { id: "day", startTime: 86400, endTime: 3600, weightPosition: 0.75 },
  { id: "week", startTime: 604800, endTime: 86400, weightPosition: 0.5 },
  { id: "weekPlus", startTime: null, endTime: 604800, weightPosition: 0.25 },
];

const PARAMETER_SETS = {
  paramSet1: {
    recencyFactor: 0.5,
    frequencyFactor: 0.5,
    combinedDomainFactor: 0.5,
    perfectFrequencyVisits: 10,
    perfectCombinedDomainScore: 2,
    multiDomainBoost: 0.1,
    itemScoreFactor: 0,
  },
  paramSet2: {
    recencyFactor: 1,
    frequencyFactor: 0.7,
    combinedDomainFactor: 0.8,
    perfectFrequencyVisits: 10,
    perfectCombinedDomainScore: 2,
    multiDomainBoost: 0.1,
    itemScoreFactor: 0,
  },
};

describe("Personality Provider", () => {
  let instance;
  let RemoteSettingsStub;
  let RemoteSettingsOnStub;
  let RemoteSettingsOffStub;
  let RemoteSettingsGetStub;
  let sandbox;
  let globals;
  let baseURLStub;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    globals = new GlobalOverrider();

    RemoteSettingsOnStub = sandbox.stub().returns();
    RemoteSettingsOffStub = sandbox.stub().returns();
    RemoteSettingsGetStub = sandbox.stub().returns([]);

    RemoteSettingsStub = name => ({
      get: RemoteSettingsGetStub,
      on: RemoteSettingsOnStub,
      off: RemoteSettingsOffStub,
    });

    sinon.spy(global, "BasePromiseWorker");
    sinon.spy(global.BasePromiseWorker.prototype, "post");
    baseURLStub = "https://baseattachmentsurl";
    global.fetch = async server => ({
      ok: true,
      json: async () => {
        if (server === "services.settings.server/") {
          return { capabilities: { attachments: { base_url: baseURLStub } } };
        }
        return {};
      },
    });
    globals.sandbox
      .stub(global.Services.prefs, "getCharPref")
      .callsFake(pref => pref);
    globals.set("RemoteSettings", RemoteSettingsStub);

    instance = new PersonalityProvider();
    instance.setAffinities(TIME_SEGMENTS, PARAMETER_SETS);
    instance.interestConfig = {
      history_item_builder: "history_item_builder",
      history_required_fields: ["a", "b", "c"],
      interest_finalizer: "interest_finalizer",
      item_to_rank_builder: "item_to_rank_builder",
      item_ranker: "item_ranker",
      interest_combiner: "interest_combiner",
    };
  });
  afterEach(() => {
    sinon.restore();
    sandbox.restore();
    globals.restore();
  });
  describe("#personalityProviderWorker", () => {
    it("should create a new promise worker on first call", async () => {
      const { personalityProviderWorker } = instance;
      assert.calledOnce(global.BasePromiseWorker);
      assert.isDefined(personalityProviderWorker);
    });
    it("should cache _personalityProviderWorker on first call", async () => {
      instance._personalityProviderWorker = null;
      const { personalityProviderWorker } = instance;
      assert.isDefined(instance._personalityProviderWorker);
      assert.isDefined(personalityProviderWorker);
    });
    it("should use old promise worker on second call", async () => {
      let { personalityProviderWorker } = instance;
      personalityProviderWorker = instance.personalityProviderWorker;
      assert.calledOnce(global.BasePromiseWorker);
      assert.isDefined(personalityProviderWorker);
    });
  });
  describe("#_getBaseAttachmentsURL", () => {
    it("should return a fresh value", async () => {
      await instance._getBaseAttachmentsURL();
      assert.equal(instance._baseAttachmentsURL, baseURLStub);
    });
    it("should return a cached value", async () => {
      const cachedURL = "cached";
      instance._baseAttachmentsURL = cachedURL;
      await instance._getBaseAttachmentsURL();
      assert.equal(instance._baseAttachmentsURL, cachedURL);
    });
  });
  describe("#setup", () => {
    it("should setup two sync attachments", () => {
      sinon.spy(instance, "setupSyncAttachment");
      instance.setup();
      assert.calledTwice(instance.setupSyncAttachment);
    });
  });
  describe("#teardown", () => {
    it("should teardown two sync attachments", () => {
      sinon.spy(instance, "teardownSyncAttachment");
      instance.teardown();
      assert.calledTwice(instance.teardownSyncAttachment);
    });
    it("should terminate worker", () => {
      const terminateStub = sandbox.stub().returns();
      instance._personalityProviderWorker = {
        terminate: terminateStub,
      };
      instance.teardown();
      assert.calledOnce(terminateStub);
    });
  });
  describe("#setupSyncAttachment", () => {
    it("should call remote settings on twice for setupSyncAttachment", () => {
      assert.calledTwice(RemoteSettingsOnStub);
    });
  });
  describe("#teardownSyncAttachment", () => {
    it("should call remote settings off for teardownSyncAttachment", () => {
      instance.teardownSyncAttachment();
      assert.calledOnce(RemoteSettingsOffStub);
    });
  });
  describe("#onSync", () => {
    it("should call worker onSync", () => {
      instance.onSync();
      assert.calledWith(global.BasePromiseWorker.prototype.post, "onSync");
    });
  });
  describe("#getAttachment", () => {
    it("should call worker onSync", () => {
      instance.getAttachment();
      assert.calledWith(
        global.BasePromiseWorker.prototype.post,
        "getAttachment"
      );
    });
  });
  describe("#getRecipe", () => {
    it("should call worker getRecipe and remote settings get", async () => {
      RemoteSettingsGetStub = sandbox.stub().returns([
        {
          key: 1,
        },
      ]);
      sinon.spy(instance, "getAttachment");
      RemoteSettingsStub = name => ({
        get: RemoteSettingsGetStub,
        on: RemoteSettingsOnStub,
        off: RemoteSettingsOffStub,
      });
      globals.set("RemoteSettings", RemoteSettingsStub);

      const result = await instance.getRecipe();
      assert.calledOnce(RemoteSettingsGetStub);
      assert.calledOnce(instance.getAttachment);
      assert.equal(result.recordKey, 1);
    });
  });
  describe("#fetchHistory", () => {
    it("should return a history object for fetchHistory", async () => {
      const history = await instance.fetchHistory(["requiredColumn"], 1, 1);
      assert.equal(
        history.sql,
        `SELECT url, title, visit_count, frecency, last_visit_date, description\n    FROM moz_places\n    WHERE last_visit_date >= 1000000\n    AND last_visit_date < 1000000 AND IFNULL(requiredColumn, '') <> '' LIMIT 30000`
      );
      assert.equal(history.options.columns.length, 1);
      assert.equal(Object.keys(history.options.params).length, 0);
    });
  });
  describe("#getHistory", () => {
    it("should return an empty array", async () => {
      instance.interestConfig = {
        history_required_fields: [],
      };
      const result = await instance.getHistory();
      assert.equal(result.length, 0);
    });
    it("should call fetchHistory", async () => {
      sinon.spy(instance, "fetchHistory");
      await instance.getHistory();
    });
  });
  describe("#setBaseAttachmentsURL", () => {
    it("should call worker setBaseAttachmentsURL", async () => {
      await instance.setBaseAttachmentsURL();
      assert.calledWith(
        global.BasePromiseWorker.prototype.post,
        "setBaseAttachmentsURL"
      );
    });
  });
  describe("#setInterestConfig", () => {
    it("should call worker setInterestConfig", async () => {
      await instance.setInterestConfig();
      assert.calledWith(
        global.BasePromiseWorker.prototype.post,
        "setInterestConfig"
      );
    });
  });
  describe("#setInterestVector", () => {
    it("should call worker setInterestVector", async () => {
      await instance.setInterestVector();
      assert.calledWith(
        global.BasePromiseWorker.prototype.post,
        "setInterestVector"
      );
    });
  });
  describe("#fetchModels", () => {
    it("should call worker fetchModels and remote settings get", async () => {
      await instance.fetchModels();
      assert.calledOnce(RemoteSettingsGetStub);
      assert.calledWith(global.BasePromiseWorker.prototype.post, "fetchModels");
    });
  });
  describe("#generateTaggers", () => {
    it("should call worker generateTaggers", async () => {
      await instance.generateTaggers();
      assert.calledWith(
        global.BasePromiseWorker.prototype.post,
        "generateTaggers"
      );
    });
  });
  describe("#generateRecipeExecutor", () => {
    it("should call worker generateRecipeExecutor", async () => {
      await instance.generateRecipeExecutor();
      assert.calledWith(
        global.BasePromiseWorker.prototype.post,
        "generateRecipeExecutor"
      );
    });
  });
  describe("#createInterestVector", () => {
    it("should call worker createInterestVector", async () => {
      await instance.createInterestVector();
      assert.calledWith(
        global.BasePromiseWorker.prototype.post,
        "createInterestVector"
      );
    });
  });
  describe("#init", () => {
    beforeEach(() => {
      sandbox.stub(instance, "dispatch").returns();
    });
    it("should return early if setInterestConfig fails", async () => {
      sandbox.stub(instance, "setBaseAttachmentsURL").returns();
      sandbox.stub(instance, "setInterestConfig").returns();
      instance.interestConfig = null;
      await instance.init();
      assert.calledWithMatch(instance.dispatch, {
        data: {
          event: "PERSONALIZATION_V2_GET_RECIPE_ERROR",
        },
      });
    });
    it("should return early if fetchModels fails", async () => {
      sandbox.stub(instance, "setBaseAttachmentsURL").returns();
      sandbox.stub(instance, "setInterestConfig").returns();
      sandbox.stub(instance, "fetchModels").resolves({
        ok: false,
      });
      await instance.init();
      assert.calledWithMatch(instance.dispatch, {
        data: {
          event: "PERSONALIZATION_V2_FETCH_MODELS_ERROR",
        },
      });
    });
    it("should return early if createInterestVector fails", async () => {
      sandbox.stub(instance, "setBaseAttachmentsURL").returns();
      sandbox.stub(instance, "setInterestConfig").returns();
      sandbox.stub(instance, "fetchModels").resolves({
        ok: true,
      });
      sandbox.stub(instance, "generateRecipeExecutor").resolves({
        ok: true,
      });
      sandbox.stub(instance, "createInterestVector").resolves({
        ok: false,
      });
      await instance.init();
      assert.calledWithMatch(instance.dispatch, {
        data: {
          event: "PERSONALIZATION_V2_CREATE_INTEREST_VECTOR_ERROR",
        },
      });
    });
    it("should call callback on successful init", async () => {
      sandbox.stub(instance, "setBaseAttachmentsURL").returns();
      sandbox.stub(instance, "setInterestConfig").returns();
      sandbox.stub(instance, "fetchModels").resolves({
        ok: true,
      });
      sandbox.stub(instance, "generateRecipeExecutor").resolves({
        ok: true,
      });
      sandbox.stub(instance, "createInterestVector").resolves({
        ok: true,
      });
      sandbox.stub(instance, "setInterestVector").resolves();
      const callback = globals.sandbox.stub();
      await instance.init(callback);
      assert.calledOnce(callback);
      assert.isTrue(instance.initialized);
    });
    it("should do generic init stuff when calling init with no cache", async () => {
      sandbox.stub(instance, "setBaseAttachmentsURL").returns();
      sandbox.stub(instance, "setInterestConfig").returns();
      sandbox.stub(instance, "fetchModels").resolves({
        ok: true,
      });
      sandbox.stub(instance, "generateRecipeExecutor").resolves({
        ok: true,
      });
      sandbox.stub(instance, "createInterestVector").resolves({
        ok: true,
        interestVector: "interestVector",
      });
      sandbox.stub(instance, "setInterestVector").resolves();
      await instance.init();
      assert.calledOnce(instance.setBaseAttachmentsURL);
      assert.calledOnce(instance.setInterestConfig);
      assert.calledOnce(instance.fetchModels);
      assert.calledOnce(instance.generateRecipeExecutor);
      assert.calledOnce(instance.createInterestVector);
      assert.calledOnce(instance.setInterestVector);
    });
  });
  describe("#dispatchRelevanceScoreDuration", () => {
    beforeEach(() => {
      sandbox.stub(instance, "dispatch").returns();
    });
    it("should dispatch PERSONALIZATION_V2_ITEM_RELEVANCE_SCORE_DURATION only if initialized", () => {
      let dispatch = globals.sandbox.stub();
      instance.dispatch = dispatch;

      instance.initialized = false;
      instance.dispatchRelevanceScoreDuration(1000);

      assert.notCalled(dispatch);

      instance.initialized = true;
      instance.dispatchRelevanceScoreDuration(1000);

      assert.calledOnce(dispatch);

      assert.equal(
        dispatch.firstCall.args[0].data.event,
        "PERSONALIZATION_V2_ITEM_RELEVANCE_SCORE_DURATION"
      );
    });
  });
  describe("#calculateItemRelevanceScore", () => {
    it("should return score for uninitialized provider", () => {
      instance.initialized = false;
      assert.equal(instance.calculateItemRelevanceScore({ item_score: 2 }), 2);
    });
    it("should post calculateItemRelevanceScore to PersonalityProviderWorker", async () => {
      instance.initialized = true;
      await instance.calculateItemRelevanceScore({ item_score: 2 });
      assert.calledWith(
        global.BasePromiseWorker.prototype.post,
        "calculateItemRelevanceScore"
      );
    });
  });
  describe("#getAffinities", () => {
    it("should return correct data for getAffinities", () => {
      const affinities = instance.getAffinities();
      assert.isDefined(affinities.timeSegments);
      assert.isDefined(affinities.parameterSets);
    });
  });
});

import { GlobalOverrider } from "test/unit/utils";
import { PersonalityProviderWorker } from "lib/PersonalityProvider/PersonalityProviderWorkerClass.jsm";
import {
  tokenize,
  toksToTfIdfVector,
} from "lib/PersonalityProvider/Tokenize.jsm";
import { RecipeExecutor } from "lib/PersonalityProvider/RecipeExecutor.jsm";
import { NmfTextTagger } from "lib/PersonalityProvider/NmfTextTagger.jsm";
import { NaiveBayesTextTagger } from "lib/PersonalityProvider/NaiveBayesTextTagger.jsm";

describe("Personality Provider Worker Class", () => {
  let instance;
  let globals;
  let sandbox;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    globals = new GlobalOverrider();
    globals.set("tokenize", tokenize);
    globals.set("toksToTfIdfVector", toksToTfIdfVector);
    globals.set("NaiveBayesTextTagger", NaiveBayesTextTagger);
    globals.set("NmfTextTagger", NmfTextTagger);
    globals.set("RecipeExecutor", RecipeExecutor);
    instance = new PersonalityProviderWorker();

    // mock the RecipeExecutor
    instance.recipeExecutor = {
      executeRecipe: (item, recipe) => {
        if (recipe === "history_item_builder") {
          if (item.title === "fail") {
            return null;
          }
          return {
            title: item.title,
            score: item.frecency,
            type: "history_item",
          };
        } else if (recipe === "interest_finalizer") {
          return {
            title: item.title,
            score: item.score * 100,
            type: "interest_vector",
          };
        } else if (recipe === "item_to_rank_builder") {
          if (item.title === "fail") {
            return null;
          }
          return {
            item_title: item.title,
            item_score: item.score,
            type: "item_to_rank",
          };
        } else if (recipe === "item_ranker") {
          if (item.title === "fail" || item.item_title === "fail") {
            return null;
          }
          return {
            title: item.title,
            score: item.item_score * item.score,
            type: "ranked_item",
          };
        }
        return null;
      },
      executeCombinerRecipe: (item1, item2, recipe) => {
        if (recipe === "interest_combiner") {
          if (
            item1.title === "combiner_fail" ||
            item2.title === "combiner_fail"
          ) {
            return null;
          }
          if (item1.type === undefined) {
            item1.type = "combined_iv";
          }
          if (item1.score === undefined) {
            item1.score = 0;
          }
          return { type: item1.type, score: item1.score + item2.score };
        }
        return null;
      },
    };

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
  describe("#_getFileStr", () => {
    it("should decode file from filepath", () => {
      globals.set(
        "TextDecoder",
        class {
          decode() {
            return "DECODED!";
          }
        }
      );
      const result = instance._getFileStr("filepath");
      assert.equal(result, "DECODED!");
    });
  });
  describe("#setBaseAttachmentsURL", () => {
    it("should set baseAttachmentsURL", () => {
      instance.setBaseAttachmentsURL("url");
      assert.equal(instance.baseAttachmentsURL, "url");
    });
  });
  describe("#setInterestConfig", () => {
    it("should set interestConfig", () => {
      instance.setInterestConfig("config");
      assert.equal(instance.interestConfig, "config");
    });
  });
  describe("#setInterestVector", () => {
    it("should set interestVector", () => {
      instance.setInterestVector("vector");
      assert.equal(instance.interestVector, "vector");
    });
  });
  describe("#onSync", () => {
    it("should sync remote settings collection from onSync", () => {
      sinon.stub(instance, "deleteAttachment").returns({});
      sinon.stub(instance, "maybeDownloadAttachment").returns({});

      instance.onSync({
        data: {
          created: ["create-1", "create-2"],
          updated: [
            { old: "update-old-1", new: "update-new-1" },
            { old: "update-old-2", new: "update-new-2" },
          ],
          deleted: ["delete-2", "delete-1"],
        },
      });

      assert(instance.maybeDownloadAttachment.withArgs("create-1").calledOnce);
      assert(instance.maybeDownloadAttachment.withArgs("create-2").calledOnce);
      assert(
        instance.maybeDownloadAttachment.withArgs("update-new-1").calledOnce
      );
      assert(
        instance.maybeDownloadAttachment.withArgs("update-new-2").calledOnce
      );

      assert(instance.deleteAttachment.withArgs("delete-1").calledOnce);
      assert(instance.deleteAttachment.withArgs("delete-2").calledOnce);
      assert(instance.deleteAttachment.withArgs("update-old-1").calledOnce);
      assert(instance.deleteAttachment.withArgs("update-old-2").calledOnce);
    });
  });
  describe("#maybeDownloadAttachment", () => {
    it("should attempt _downloadAttachment three times for maybeDownloadAttachment", () => {
      let existsStub;
      let statStub;
      let attachmentStub;
      sinon.stub(instance, "_downloadAttachment").returns();
      const makeDirStub = globals.sandbox
        .stub(global.OS.File, "makeDir")
        .returns();
      globals.sandbox
        .stub(global.OS.Path, "join")
        .callsFake((first, second) => first + second);

      existsStub = globals.sandbox.stub(global.OS.File, "exists").returns(true);
      statStub = globals.sandbox
        .stub(global.OS.File, "stat")
        .returns({ size: "1" });

      attachmentStub = {
        attachment: {
          filename: "file",
          size: "1",
        },
      };

      instance.maybeDownloadAttachment(attachmentStub);
      assert.calledWith(makeDirStub, "/");
      assert.calledOnce(existsStub);
      assert.calledOnce(statStub);
      assert.notCalled(instance._downloadAttachment);

      existsStub.resetHistory();
      statStub.resetHistory();

      attachmentStub = {
        attachment: {
          filename: "file",
          size: "2",
        },
      };

      instance.maybeDownloadAttachment(attachmentStub);
      assert.calledThrice(existsStub);
      assert.calledThrice(statStub);
      assert.calledThrice(instance._downloadAttachment);
    });
  });
  describe("#_downloadAttachment", () => {
    beforeEach(() => {
      globals.sandbox
        .stub(global.OS.Path, "join")
        .callsFake((first, second) => first + second);
      globals.set("Uint8Array", class Uint8Array {});
    });
    it("should write a file from _downloadAttachment", () => {
      globals.set(
        "XMLHttpRequest",
        class {
          constructor() {
            this.status = 200;
            this.response = "response!";
          }
          open() {}
          setRequestHeader() {}
          send() {}
        }
      );

      const writeAtomicStub = globals.sandbox
        .stub(global.OS.File, "writeAtomic")
        .resolves();

      instance._downloadAttachment({
        attachment: { location: "location", filename: "filename" },
      });

      const writeArgs = writeAtomicStub.firstCall.args;
      assert.equal(writeArgs[0], "/filename");
      assert.equal(writeArgs[2].tmpPath, "/filename.tmp");
    });
    it("should call console.error from _downloadAttachment if not valid response", () => {
      globals.set(
        "XMLHttpRequest",
        class {
          constructor() {
            this.status = 0;
            this.response = "response!";
          }
          open() {}
          setRequestHeader() {}
          send() {}
        }
      );

      const writeAtomicStub = globals.sandbox
        .stub(global.OS.File, "writeAtomic")
        .resolves();

      instance._downloadAttachment({
        attachment: { location: "location", filename: "filename" },
      });

      assert.notCalled(writeAtomicStub);
    });
  });
  describe("#deleteAttachment", () => {
    it("should remove attachments when calling deleteAttachment", () => {
      const makeDirStub = globals.sandbox
        .stub(global.OS.File, "makeDir")
        .returns();
      const removeStub = globals.sandbox
        .stub(global.OS.File, "remove")
        .returns();
      const removeEmptyDirStub = globals.sandbox
        .stub(global.OS.File, "removeEmptyDir")
        .returns();
      globals.sandbox
        .stub(global.OS.Path, "join")
        .callsFake((first, second) => first + second);
      instance.deleteAttachment({ attachment: { filename: "filename" } });
      assert.calledOnce(makeDirStub);
      assert.calledOnce(removeStub);
      assert.calledOnce(removeEmptyDirStub);
      assert.calledWith(removeStub, "/filename", { ignoreAbsent: true });
    });
  });
  describe("#getAttachment", () => {
    it("should return JSON when calling getAttachment", () => {
      sinon.stub(instance, "maybeDownloadAttachment").returns();
      sinon.stub(instance, "_getFileStr").returns("{}");
      globals.sandbox
        .stub(global.OS.Path, "join")
        .callsFake((first, second) => first + second);
      const record = { attachment: { filename: "filename" } };
      let returnValue = instance.getAttachment(record);

      assert.calledOnce(instance._getFileStr);
      assert.calledWith(instance._getFileStr, "/filename");
      assert.calledOnce(instance.maybeDownloadAttachment);
      assert.calledWith(instance.maybeDownloadAttachment, record);
      assert.deepEqual(returnValue, {});

      instance._getFileStr.restore();
      sinon.stub(instance, "_getFileStr").returns({});
      returnValue = instance.getAttachment(record);
      assert.deepEqual(returnValue, {});
    });
  });
  describe("#fetchModels", () => {
    it("should return ok true", () => {
      sinon.stub(instance, "getAttachment").returns();
      const result = instance.fetchModels([{ key: 1234 }]);
      assert.isTrue(result.ok);
      assert.deepEqual(instance.models, [{ recordKey: 1234 }]);
    });
    it("should return ok false", () => {
      sinon.stub(instance, "getAttachment").returns();
      const result = instance.fetchModels([]);
      assert.isTrue(!result.ok);
    });
  });
  describe("#generateTaggers", () => {
    it("should generate taggers from modelKeys", () => {
      const modelKeys = ["nb_model_sports", "nmf_model_sports"];

      instance.models = [
        { recordKey: "nb_model_sports", model_type: "nb" },
        {
          recordKey: "nmf_model_sports",
          model_type: "nmf",
          parent_tag: "nmf_sports_parent_tag",
        },
      ];

      instance.generateTaggers(modelKeys);
      assert.equal(instance.taggers.nbTaggers.length, 1);
      assert.equal(Object.keys(instance.taggers.nmfTaggers).length, 1);
    });
    it("should skip any models not in modelKeys", () => {
      const modelKeys = ["nb_model_sports"];

      instance.models = [
        { recordKey: "nb_model_sports", model_type: "nb" },
        {
          recordKey: "nmf_model_sports",
          model_type: "nmf",
          parent_tag: "nmf_sports_parent_tag",
        },
      ];

      instance.generateTaggers(modelKeys);
      assert.equal(instance.taggers.nbTaggers.length, 1);
      assert.equal(Object.keys(instance.taggers.nmfTaggers).length, 0);
    });
    it("should skip any models not defined", () => {
      const modelKeys = ["nb_model_sports", "nmf_model_sports"];

      instance.models = [{ recordKey: "nb_model_sports", model_type: "nb" }];
      instance.generateTaggers(modelKeys);
      assert.equal(instance.taggers.nbTaggers.length, 1);
      assert.equal(Object.keys(instance.taggers.nmfTaggers).length, 0);
    });
  });
  describe("#generateRecipeExecutor", () => {
    it("should generate a recipeExecutor", () => {
      instance.recipeExecutor = null;
      instance.taggers = {};
      instance.generateRecipeExecutor();
      assert.isNotNull(instance.recipeExecutor);
    });
  });
  describe("#createInterestVector", () => {
    let mockHistory = [];
    beforeEach(() => {
      mockHistory = [
        {
          title: "automotive",
          description: "something about automotive",
          url: "http://example.com/automotive",
          frecency: 10,
        },
        {
          title: "fashion",
          description: "something about fashion",
          url: "http://example.com/fashion",
          frecency: 5,
        },
        {
          title: "tech",
          description: "something about tech",
          url: "http://example.com/tech",
          frecency: 1,
        },
      ];
    });
    it("should gracefully handle history entries that fail", () => {
      mockHistory.push({ title: "fail" });
      assert.isNotNull(instance.createInterestVector(mockHistory));
    });

    it("should fail if the combiner fails", () => {
      mockHistory.push({ title: "combiner_fail", frecency: 111 });
      let actual = instance.createInterestVector(mockHistory);
      assert.isNull(actual);
    });

    it("should process history, combine, and finalize", () => {
      let actual = instance.createInterestVector(mockHistory);
      assert.equal(actual.interestVector.score, 1600);
    });
  });
  describe("#calculateItemRelevanceScore", () => {
    it("should return -1 for busted item", () => {
      assert.equal(instance.calculateItemRelevanceScore({ title: "fail" }), -1);
    });
    it("should return -1 for a busted ranking", () => {
      instance.interestVector = { title: "fail", score: 10 };
      assert.equal(
        instance.calculateItemRelevanceScore({ title: "some item", score: 6 }),
        -1
      );
    });
    it("should return a score, and not change with interestVector", () => {
      instance.interestVector = { score: 10 };
      assert.equal(instance.calculateItemRelevanceScore({ score: 2 }), 20);
      assert.deepEqual(instance.interestVector, { score: 10 });
    });
  });
});

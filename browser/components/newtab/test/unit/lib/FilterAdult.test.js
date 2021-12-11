import { FilterAdult } from "lib/FilterAdult.jsm";
import { GlobalOverrider } from "test/unit/utils";

describe("FilterAdult", () => {
  let hashStub;
  let hashValue;
  let globals;

  beforeEach(() => {
    globals = new GlobalOverrider();
    hashStub = {
      finish: sinon.stub().callsFake(() => hashValue),
      init: sinon.stub(),
      update: sinon.stub(),
    };
    globals.set("Cc", {
      "@mozilla.org/security/hash;1": {
        createInstance() {
          return hashStub;
        },
      },
    });
    globals.set("gFilterAdultEnabled", true);
  });

  afterEach(() => {
    hashValue = "";
    globals.restore();
  });

  describe("filter", () => {
    it("should default to include on unexpected urls", () => {
      const empty = {};

      const result = FilterAdult.filter([empty]);

      assert.equal(result.length, 1);
      assert.equal(result[0], empty);
    });
    it("should not filter out non-adult urls", () => {
      const link = { url: "https://mozilla.org/" };

      const result = FilterAdult.filter([link]);

      assert.equal(result.length, 1);
      assert.equal(result[0], link);
    });
    it("should filter out adult urls", () => {
      // Use a hash value that is in the adult set
      hashValue = "+/UCpAhZhz368iGioEO8aQ==";
      const link = { url: "https://some-adult-site/" };

      const result = FilterAdult.filter([link]);

      assert.equal(result.length, 0);
    });
    it("should not filter out adult urls if the preference is turned off", () => {
      // Use a hash value that is in the adult set
      hashValue = "+/UCpAhZhz368iGioEO8aQ==";
      globals.set("gFilterAdultEnabled", false);
      const link = { url: "https://some-adult-site/" };

      const result = FilterAdult.filter([link]);

      assert.equal(result.length, 1);
      assert.equal(result[0], link);
    });
  });

  describe("isAdultUrl", () => {
    it("should default to false on unexpected urls", () => {
      const result = FilterAdult.isAdultUrl("");

      assert.equal(result, false);
    });
    it("should return false for non-adult urls", () => {
      const result = FilterAdult.isAdultUrl("https://mozilla.org/");

      assert.equal(result, false);
    });
    it("should return true for adult urls", () => {
      // Use a hash value that is in the adult set
      hashValue = "+/UCpAhZhz368iGioEO8aQ==";
      const result = FilterAdult.isAdultUrl("https://some-adult-site/");

      assert.equal(result, true);
    });
    it("should return false for adult urls when the preference is turned off", () => {
      // Use a hash value that is in the adult set
      hashValue = "+/UCpAhZhz368iGioEO8aQ==";
      globals.set("gFilterAdultEnabled", false);
      const result = FilterAdult.isAdultUrl("https://some-adult-site/");

      assert.equal(result, false);
    });

    describe("test functions", () => {
      it("should add and remove a filter in the adult list", () => {
        // Use a hash value that is in the adult set
        FilterAdult.addDomainToList("https://some-adult-site/");
        let result = FilterAdult.isAdultUrl("https://some-adult-site/");

        assert.equal(result, true);

        FilterAdult.removeDomainFromList("https://some-adult-site/");
        result = FilterAdult.isAdultUrl("https://some-adult-site/");

        assert.equal(result, false);
      });
    });
  });
});

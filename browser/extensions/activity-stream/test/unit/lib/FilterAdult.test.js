import {filterAdult} from "lib/FilterAdult.jsm";

describe("filterAdult", () => {
  let hashStub;
  let hashValue;

  beforeEach(() => {
    hashStub = {
      finish: sinon.stub().callsFake(() => hashValue),
      init: sinon.stub(),
      update: sinon.stub()
    };
    global.Components.classes["@mozilla.org/security/hash;1"] = {
      createInstance() {
        return hashStub;
      }
    };
  });

  it("should default to include on unexpected urls", () => {
    const empty = {};

    const result = filterAdult([empty]);

    assert.equal(result.length, 1);
    assert.equal(result[0], empty);
  });
  it("should not filter out non-adult urls", () => {
    const link = {url: "https://mozilla.org/"};

    const result = filterAdult([link]);

    assert.equal(result.length, 1);
    assert.equal(result[0], link);
  });
  it("should filter out adult urls", () => {
    // Use a hash value that is in the adult set
    hashValue = "+/UCpAhZhz368iGioEO8aQ==";
    const link = {url: "https://some-adult-site/"};

    const result = filterAdult([link]);

    assert.equal(result.length, 0);
  });
});

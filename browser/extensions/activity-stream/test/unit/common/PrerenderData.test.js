import {_PrerenderData, PrerenderData} from "common/PrerenderData.jsm";

describe("_PrerenderData", () => {
  describe("properties", () => {
    it("should set .initialPrefs", () => {
      const initialPrefs = {foo: true};
      const instance = new _PrerenderData({initialPrefs});

      assert.equal(instance.initialPrefs, initialPrefs);
    });
    it("should set .initialSections", () => {
      const initialSections = [{id: "foo"}];
      const instance = new _PrerenderData({initialSections});

      assert.equal(instance.initialSections, initialSections);
    });
    it("should set .validation and .invalidatingPrefs in the constructor", () => {
      const validation = ["foo", "bar", {oneOf: ["baz", "qux"]}];
      const instance = new _PrerenderData({validation});

      assert.equal(instance.validation, validation);
      assert.deepEqual(instance.invalidatingPrefs, ["foo", "bar", "baz", "qux"]);
    });
    it("should also set .invalidatingPrefs when .validation is set", () => {
      const validation = ["foo", "bar", {oneOf: ["baz", "qux"]}];
      const instance = new _PrerenderData({validation});

      const newValidation = ["foo", {oneOf: ["blah", "gloo"]}];
      instance.validation = newValidation;
      assert.equal(instance.validation, newValidation);
      assert.deepEqual(instance.invalidatingPrefs, ["foo", "blah", "gloo"]);
    });
    it("should throw if an invalid validation config is set", () => {
      // {stuff: []} is not a valid configuration type
      assert.throws(() => new _PrerenderData({validation: ["foo", {stuff: ["bar"]}]}));
    });
  });
  describe("#arePrefsValid", () => {
    let FAKE_PREFS;
    const getPrefs = pref => FAKE_PREFS[pref];
    beforeEach(() => {
      FAKE_PREFS = {};
    });
    it("should return true if all prefs match", () => {
      FAKE_PREFS = {foo: true, bar: false};
      const instance = new _PrerenderData({
        initialPrefs: FAKE_PREFS,
        validation: ["foo", "bar"]
      });
      assert.isTrue(instance.arePrefsValid(getPrefs));
    });
    it("should return true if all *invalidating* prefs match", () => {
      FAKE_PREFS = {foo: true, bar: false};
      const instance = new _PrerenderData({
        initialPrefs: {foo: true, bar: true},
        validation: ["foo"]
      });

      assert.isTrue(instance.arePrefsValid(getPrefs));
    });
    it("should return true if one each oneOf group matches", () => {
      FAKE_PREFS = {foo: false, floo: true, bar: false, blar: true};
      const instance = new _PrerenderData({
        initialPrefs: {foo: true, floo: true, bar: true, blar: true},
        validation: [{oneOf: ["foo", "floo"]}, {oneOf: ["bar", "blar"]}]
      });

      assert.isTrue(instance.arePrefsValid(getPrefs));
    });
    it("should return false if an invalidating pref is mismatched", () => {
      FAKE_PREFS = {foo: true, bar: false};
      const instance = new _PrerenderData({
        initialPrefs: {foo: true, bar: true},
        validation: ["foo", "bar"]
      });

      assert.isFalse(instance.arePrefsValid(getPrefs));
    });
    it("should return false if none of the oneOf group matches", () => {
      FAKE_PREFS = {foo: true, bar: false, baz: false};
      const instance = new _PrerenderData({
        initialPrefs: {foo: true, bar: true, baz: true},
        validation: ["foo", {oneOf: ["bar", "baz"]}]
      });

      assert.isFalse(instance.arePrefsValid(getPrefs));
    });
  });
});

// This is the instance used by Activity Stream
describe("PrerenderData", () => {
  it("should set initial values for all invalidating prefs", () => {
    PrerenderData.invalidatingPrefs.forEach(pref => {
      assert.property(PrerenderData.initialPrefs, pref);
    });
  });
});

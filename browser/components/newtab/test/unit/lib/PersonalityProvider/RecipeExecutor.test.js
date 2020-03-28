import { RecipeExecutor } from "lib/PersonalityProvider/RecipeExecutor.jsm";
import { tokenize } from "lib/PersonalityProvider/Tokenize.jsm";

class MockTagger {
  constructor(mode, tagScoreMap) {
    this.mode = mode;
    this.tagScoreMap = tagScoreMap;
  }
  tagTokens(tokens) {
    if (this.mode === "nb") {
      // eslint-disable-next-line prefer-destructuring
      let tag = Object.keys(this.tagScoreMap)[0];
      // eslint-disable-next-line prefer-destructuring
      let prob = this.tagScoreMap[tag];
      let conf = prob >= 0.85;
      return {
        label: tag,
        logProb: Math.log(prob),
        confident: conf,
      };
    }
    return this.tagScoreMap;
  }
  tag(text) {
    return this.tagTokens([text]);
  }
}

describe("RecipeExecutor", () => {
  let makeItem = () => {
    let x = {
      lhs: 2,
      one: 1,
      two: 2,
      three: 3,
      foo: "FOO",
      bar: "BAR",
      baz: ["one", "two", "three"],
      qux: 42,
      text: "This Is A_sentence.",
      url:
        "http://www.wonder.example.com/dir1/dir2a-dir2b/dir3+4?key1&key2=val2&key3&%26amp=%3D3+4",
      url2:
        "http://wonder.example.com/dir1/dir2a-dir2b/dir3+4?key1&key2=val2&key3&%26amp=%3D3+4",
      map: {
        c: 3,
        a: 1,
        b: 2,
      },
      map2: {
        b: 2,
        c: 3,
        d: 4,
      },
      arr1: [2, 3, 4],
      arr2: [3, 4, 5],
      long: [3, 4, 5, 6, 7],
      tags: {
        a: {
          aa: 0.1,
          ab: 0.2,
          ac: 0.3,
        },
        b: {
          ba: 4,
          bb: 5,
          bc: 6,
        },
      },
      bogus: {
        a: {
          aa: "0.1",
          ab: "0.2",
          ac: "0.3",
        },
        b: {
          ba: "4",
          bb: "5",
          bc: "6",
        },
      },
      zero: {
        a: 0,
        b: 0,
      },
      zaro: [0, 0],
    };
    return x;
  };

  let EPSILON = 0.00001;

  let instance = new RecipeExecutor(
    [
      new MockTagger("nb", { tag1: 0.7 }),
      new MockTagger("nb", { tag2: 0.86 }),
      new MockTagger("nb", { tag3: 0.9 }),
      new MockTagger("nb", { tag5: 0.9 }),
    ],
    {
      tag1: new MockTagger("nmf", {
        tag11: 0.9,
        tag12: 0.8,
        tag13: 0.7,
      }),
      tag2: new MockTagger("nmf", {
        tag21: 0.8,
        tag22: 0.7,
        tag23: 0.6,
      }),
      tag3: new MockTagger("nmf", {
        tag31: 0.7,
        tag32: 0.6,
        tag33: 0.5,
      }),
      tag4: new MockTagger("nmf", { tag41: 0.99 }),
    },
    tokenize
  );
  let item = null;

  beforeEach(() => {
    item = makeItem();
  });

  describe("#_assembleText", () => {
    it("should simply copy a single string", () => {
      assert.equal(instance._assembleText(item, ["foo"]), "FOO");
    });
    it("should append some strings with a space", () => {
      assert.equal(instance._assembleText(item, ["foo", "bar"]), "FOO BAR");
    });
    it("should give an empty string for a missing field", () => {
      assert.equal(instance._assembleText(item, ["missing"]), "");
    });
    it("should not double space an interior missing field", () => {
      assert.equal(
        instance._assembleText(item, ["foo", "missing", "bar"]),
        "FOO BAR"
      );
    });
    it("should splice in an array of strings", () => {
      assert.equal(
        instance._assembleText(item, ["foo", "baz", "bar"]),
        "FOO one two three BAR"
      );
    });
    it("should handle numbers", () => {
      assert.equal(
        instance._assembleText(item, ["foo", "qux", "bar"]),
        "FOO 42 BAR"
      );
    });
  });

  describe("#naiveBayesTag", () => {
    it("should understand NaiveBayesTextTagger", () => {
      item = instance.naiveBayesTag(item, { fields: ["text"] });
      assert.isTrue("nb_tags" in item);
      assert.isTrue(!("tag1" in item.nb_tags));
      assert.equal(item.nb_tags.tag2, 0.86);
      assert.equal(item.nb_tags.tag3, 0.9);
      assert.equal(item.nb_tags.tag5, 0.9);
      assert.isTrue("nb_tokens" in item);
      assert.deepEqual(item.nb_tokens, ["this", "is", "a", "sentence"]);
      assert.isTrue("nb_tags_extended" in item);
      assert.isTrue(!("tag1" in item.nb_tags_extended));
      assert.deepEqual(item.nb_tags_extended.tag2, {
        label: "tag2",
        logProb: Math.log(0.86),
        confident: true,
      });
      assert.deepEqual(item.nb_tags_extended.tag3, {
        label: "tag3",
        logProb: Math.log(0.9),
        confident: true,
      });
      assert.deepEqual(item.nb_tags_extended.tag5, {
        label: "tag5",
        logProb: Math.log(0.9),
        confident: true,
      });
      assert.isTrue("nb_tokens" in item);
      assert.deepEqual(item.nb_tokens, ["this", "is", "a", "sentence"]);
    });
  });

  describe("#conditionallyNmfTag", () => {
    it("should do nothing if it's not nb tagged", () => {
      item = instance.conditionallyNmfTag(item, {});
      assert.equal(item, null);
    });
    it("should populate nmf tags for the nb tags", () => {
      item = instance.naiveBayesTag(item, { fields: ["text"] });
      item = instance.conditionallyNmfTag(item, {});
      assert.isTrue("nb_tags" in item);
      assert.deepEqual(item.nmf_tags, {
        tag2: {
          tag21: 0.8,
          tag22: 0.7,
          tag23: 0.6,
        },
        tag3: {
          tag31: 0.7,
          tag32: 0.6,
          tag33: 0.5,
        },
      });
      assert.deepEqual(item.nmf_tags_parent, {
        tag21: "tag2",
        tag22: "tag2",
        tag23: "tag2",
        tag31: "tag3",
        tag32: "tag3",
        tag33: "tag3",
      });
    });
    it("should not populate nmf tags for things that were not nb tagged", () => {
      item = instance.naiveBayesTag(item, { fields: ["text"] });
      item = instance.conditionallyNmfTag(item, {});
      assert.isTrue("nmf_tags" in item);
      assert.isTrue(!("tag4" in item.nmf_tags));
      assert.isTrue("nmf_tags_parent" in item);
      assert.isTrue(!("tag4" in item.nmf_tags_parent));
    });
  });

  describe("#acceptItemByFieldValue", () => {
    it("should implement ==", () => {
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "==",
          rhsValue: 2,
        }) !== null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "==",
          rhsValue: 3,
        }) === null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "==",
          rhsField: "two",
        }) !== null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "==",
          rhsField: "three",
        }) === null
      );
    });
    it("should implement !=", () => {
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "!=",
          rhsValue: 2,
        }) === null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "!=",
          rhsValue: 3,
        }) !== null
      );
    });
    it("should implement < ", () => {
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "<",
          rhsValue: 1,
        }) === null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "<",
          rhsValue: 2,
        }) === null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "<",
          rhsValue: 3,
        }) !== null
      );
    });
    it("should implement <= ", () => {
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "<=",
          rhsValue: 1,
        }) === null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "<=",
          rhsValue: 2,
        }) !== null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "<=",
          rhsValue: 3,
        }) !== null
      );
    });
    it("should implement > ", () => {
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: ">",
          rhsValue: 1,
        }) !== null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: ">",
          rhsValue: 2,
        }) === null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: ">",
          rhsValue: 3,
        }) === null
      );
    });
    it("should implement >= ", () => {
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: ">=",
          rhsValue: 1,
        }) !== null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: ">=",
          rhsValue: 2,
        }) !== null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: ">=",
          rhsValue: 3,
        }) === null
      );
    });
    it("should skip items with missing fields", () => {
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "no-left",
          op: "==",
          rhsValue: 1,
        }) === null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "==",
          rhsField: "no-right",
        }) === null
      );
      assert.isTrue(
        instance.acceptItemByFieldValue(item, { field: "lhs", op: "==" }) ===
          null
      );
    });
    it("should skip items with bogus operators", () => {
      assert.isTrue(
        instance.acceptItemByFieldValue(item, {
          field: "lhs",
          op: "bogus",
          rhsField: "two",
        }) === null
      );
    });
  });

  describe("#tokenizeUrl", () => {
    it("should strip the leading www from a url", () => {
      item = instance.tokenizeUrl(item, { field: "url", dest: "url_toks" });
      assert.deepEqual(
        [
          "wonder",
          "example",
          "com",
          "dir1",
          "dir2a",
          "dir2b",
          "dir3",
          "4",
          "key1",
          "key2",
          "val2",
          "key3",
          "amp",
          "3",
          "4",
        ],
        item.url_toks
      );
    });
    it("should tokenize the not strip the leading non-wwww token from a url", () => {
      item = instance.tokenizeUrl(item, { field: "url2", dest: "url_toks" });
      assert.deepEqual(
        [
          "wonder",
          "example",
          "com",
          "dir1",
          "dir2a",
          "dir2b",
          "dir3",
          "4",
          "key1",
          "key2",
          "val2",
          "key3",
          "amp",
          "3",
          "4",
        ],
        item.url_toks
      );
    });
    it("should error for a missing url", () => {
      item = instance.tokenizeUrl(item, { field: "missing", dest: "url_toks" });
      assert.equal(item, null);
    });
  });

  describe("#getUrlDomain", () => {
    it("should get only the hostname skipping the www", () => {
      item = instance.getUrlDomain(item, { field: "url", dest: "url_domain" });
      assert.isTrue("url_domain" in item);
      assert.deepEqual("wonder.example.com", item.url_domain);
    });
    it("should get only the hostname", () => {
      item = instance.getUrlDomain(item, { field: "url2", dest: "url_domain" });
      assert.isTrue("url_domain" in item);
      assert.deepEqual("wonder.example.com", item.url_domain);
    });
    it("should get the hostname and 2 levels of directories", () => {
      item = instance.getUrlDomain(item, {
        field: "url",
        path_length: 2,
        dest: "url_plus_2",
      });
      assert.isTrue("url_plus_2" in item);
      assert.deepEqual("wonder.example.com/dir1/dir2a-dir2b", item.url_plus_2);
    });
    it("should error for a missing url", () => {
      item = instance.getUrlDomain(item, {
        field: "missing",
        dest: "url_domain",
      });
      assert.equal(item, null);
    });
  });

  describe("#tokenizeField", () => {
    it("should tokenize the field", () => {
      item = instance.tokenizeField(item, { field: "text", dest: "toks" });
      assert.isTrue("toks" in item);
      assert.deepEqual(["this", "is", "a", "sentence"], item.toks);
    });
    it("should error for a missing field", () => {
      item = instance.tokenizeField(item, { field: "missing", dest: "toks" });
      assert.equal(item, null);
    });
    it("should error for a broken config", () => {
      item = instance.tokenizeField(item, {});
      assert.equal(item, null);
    });
  });

  describe("#_typeOf", () => {
    it("should know this is a map", () => {
      assert.equal(instance._typeOf({}), "map");
    });
    it("should know this is an array", () => {
      assert.equal(instance._typeOf([]), "array");
    });
    it("should know this is a string", () => {
      assert.equal(instance._typeOf("blah"), "string");
    });
    it("should know this is a boolean", () => {
      assert.equal(instance._typeOf(true), "boolean");
    });

    it("should know this is a null", () => {
      assert.equal(instance._typeOf(null), "null");
    });
  });

  describe("#_lookupScalar", () => {
    it("should return the constant", () => {
      assert.equal(instance._lookupScalar({}, 1, 0), 1);
    });
    it("should return the default", () => {
      assert.equal(instance._lookupScalar({}, "blah", 42), 42);
    });
    it("should return the field's value", () => {
      assert.equal(instance._lookupScalar({ blah: 11 }, "blah", 42), 11);
    });
  });

  describe("#copyValue", () => {
    it("should copy values", () => {
      item = instance.copyValue(item, { src: "one", dest: "again" });
      assert.isTrue("again" in item);
      assert.equal(item.again, 1);
      item.one = 100;
      assert.equal(item.one, 100);
      assert.equal(item.again, 1);
    });
    it("should handle maps corrects", () => {
      item = instance.copyValue(item, { src: "map", dest: "again" });
      assert.deepEqual(item.again, { a: 1, b: 2, c: 3 });
      item.map.c = 100;
      assert.deepEqual(item.again, { a: 1, b: 2, c: 3 });
      item.map = 342;
      assert.deepEqual(item.again, { a: 1, b: 2, c: 3 });
    });
    it("should error for a missing field", () => {
      item = instance.copyValue(item, { src: "missing", dest: "toks" });
      assert.equal(item, null);
    });
  });

  describe("#keepTopK", () => {
    it("should keep the 2 smallest", () => {
      item = instance.keepTopK(item, { field: "map", k: 2, descending: false });
      assert.equal(Object.keys(item.map).length, 2);
      assert.isTrue("a" in item.map);
      assert.equal(item.map.a, 1);
      assert.isTrue("b" in item.map);
      assert.equal(item.map.b, 2);
      assert.isTrue(!("c" in item.map));
    });
    it("should keep the 2 largest", () => {
      item = instance.keepTopK(item, { field: "map", k: 2, descending: true });
      assert.equal(Object.keys(item.map).length, 2);
      assert.isTrue(!("a" in item.map));
      assert.isTrue("b" in item.map);
      assert.equal(item.map.b, 2);
      assert.isTrue("c" in item.map);
      assert.equal(item.map.c, 3);
    });
    it("should still keep the 2 largest", () => {
      item = instance.keepTopK(item, { field: "map", k: 2 });
      assert.equal(Object.keys(item.map).length, 2);
      assert.isTrue(!("a" in item.map));
      assert.isTrue("b" in item.map);
      assert.equal(item.map.b, 2);
      assert.isTrue("c" in item.map);
      assert.equal(item.map.c, 3);
    });
    it("should promote up nested fields", () => {
      item = instance.keepTopK(item, { field: "tags", k: 2 });
      assert.equal(Object.keys(item.tags).length, 2);
      assert.deepEqual(item.tags, { bb: 5, bc: 6 });
    });
    it("should error for a missing field", () => {
      item = instance.keepTopK(item, { field: "missing", k: 3 });
      assert.equal(item, null);
    });
  });

  describe("#scalarMultiply", () => {
    it("should use constants", () => {
      item = instance.scalarMultiply(item, { field: "map", k: 2 });
      assert.equal(item.map.a, 2);
      assert.equal(item.map.b, 4);
      assert.equal(item.map.c, 6);
    });
    it("should use fields", () => {
      item = instance.scalarMultiply(item, { field: "map", k: "three" });
      assert.equal(item.map.a, 3);
      assert.equal(item.map.b, 6);
      assert.equal(item.map.c, 9);
    });
    it("should use default", () => {
      item = instance.scalarMultiply(item, {
        field: "map",
        k: "missing",
        dfault: 4,
      });
      assert.equal(item.map.a, 4);
      assert.equal(item.map.b, 8);
      assert.equal(item.map.c, 12);
    });
    it("should error for a missing field", () => {
      item = instance.scalarMultiply(item, { field: "missing", k: 3 });
      assert.equal(item, null);
    });
    it("should multiply numbers", () => {
      item = instance.scalarMultiply(item, { field: "lhs", k: 2 });
      assert.equal(item.lhs, 4);
    });
    it("should multiply arrays", () => {
      item = instance.scalarMultiply(item, { field: "arr1", k: 2 });
      assert.deepEqual(item.arr1, [4, 6, 8]);
    });
    it("should should error on strings", () => {
      item = instance.scalarMultiply(item, { field: "foo", k: 2 });
      assert.equal(item, null);
    });
  });

  describe("#elementwiseMultiply", () => {
    it("should handle maps", () => {
      item = instance.elementwiseMultiply(item, {
        left: "tags",
        right: "map2",
      });
      assert.deepEqual(item.tags, {
        a: { aa: 0, ab: 0, ac: 0 },
        b: { ba: 8, bb: 10, bc: 12 },
      });
    });
    it("should handle arrays of same length", () => {
      item = instance.elementwiseMultiply(item, {
        left: "arr1",
        right: "arr2",
      });
      assert.deepEqual(item.arr1, [6, 12, 20]);
    });
    it("should error for arrays of different lengths", () => {
      item = instance.elementwiseMultiply(item, {
        left: "arr1",
        right: "long",
      });
      assert.equal(item, null);
    });
    it("should error for a missing left", () => {
      item = instance.elementwiseMultiply(item, {
        left: "missing",
        right: "arr2",
      });
      assert.equal(item, null);
    });
    it("should error for a missing right", () => {
      item = instance.elementwiseMultiply(item, {
        left: "arr1",
        right: "missing",
      });
      assert.equal(item, null);
    });
    it("should handle numbers", () => {
      item = instance.elementwiseMultiply(item, {
        left: "three",
        right: "two",
      });
      assert.equal(item.three, 6);
    });
    it("should error for mismatched types", () => {
      item = instance.elementwiseMultiply(item, { left: "arr1", right: "two" });
      assert.equal(item, null);
    });
    it("should error for strings", () => {
      item = instance.elementwiseMultiply(item, { left: "foo", right: "bar" });
      assert.equal(item, null);
    });
  });

  describe("#vectorMultiply", () => {
    it("should calculate dot products from maps", () => {
      item = instance.vectorMultiply(item, {
        left: "map",
        right: "map2",
        dest: "dot",
      });
      assert.equal(item.dot, 13);
    });
    it("should calculate dot products from arrays", () => {
      item = instance.vectorMultiply(item, {
        left: "arr1",
        right: "arr2",
        dest: "dot",
      });
      assert.equal(item.dot, 38);
    });
    it("should error for arrays of different lengths", () => {
      item = instance.vectorMultiply(item, { left: "arr1", right: "long" });
      assert.equal(item, null);
    });
    it("should error for a missing left", () => {
      item = instance.vectorMultiply(item, { left: "missing", right: "arr2" });
      assert.equal(item, null);
    });
    it("should error for a missing right", () => {
      item = instance.vectorMultiply(item, { left: "arr1", right: "missing" });
      assert.equal(item, null);
    });
    it("should error for mismatched types", () => {
      item = instance.vectorMultiply(item, { left: "arr1", right: "two" });
      assert.equal(item, null);
    });
    it("should error for strings", () => {
      item = instance.vectorMultiply(item, { left: "foo", right: "bar" });
      assert.equal(item, null);
    });
  });

  describe("#scalarAdd", () => {
    it("should error for a missing field", () => {
      item = instance.scalarAdd(item, { field: "missing", k: 10 });
      assert.equal(item, null);
    });
    it("should error for strings", () => {
      item = instance.scalarAdd(item, { field: "foo", k: 10 });
      assert.equal(item, null);
    });
    it("should work for numbers", () => {
      item = instance.scalarAdd(item, { field: "one", k: 10 });
      assert.equal(item.one, 11);
    });
    it("should add a constant to every cell on a map", () => {
      item = instance.scalarAdd(item, { field: "map", k: 10 });
      assert.deepEqual(item.map, { a: 11, b: 12, c: 13 });
    });
    it("should add a value from a field to every cell on a map", () => {
      item = instance.scalarAdd(item, { field: "map", k: "qux" });
      assert.deepEqual(item.map, { a: 43, b: 44, c: 45 });
    });
    it("should add a constant to every cell on an array", () => {
      item = instance.scalarAdd(item, { field: "arr1", k: 10 });
      assert.deepEqual(item.arr1, [12, 13, 14]);
    });
  });

  describe("#vectorAdd", () => {
    it("should calculate add vectors from maps", () => {
      item = instance.vectorAdd(item, { left: "map", right: "map2" });
      assert.equal(Object.keys(item.map).length, 4);
      assert.isTrue("a" in item.map);
      assert.equal(item.map.a, 1);
      assert.isTrue("b" in item.map);
      assert.equal(item.map.b, 4);
      assert.isTrue("c" in item.map);
      assert.equal(item.map.c, 6);
      assert.isTrue("d" in item.map);
      assert.equal(item.map.d, 4);
    });
    it("should work for missing left", () => {
      item = instance.vectorAdd(item, { left: "missing", right: "arr2" });
      assert.deepEqual(item.missing, [3, 4, 5]);
    });
    it("should error for missing right", () => {
      item = instance.vectorAdd(item, { left: "arr2", right: "missing" });
      assert.equal(item, null);
    });
    it("should error error for strings", () => {
      item = instance.vectorAdd(item, { left: "foo", right: "bar" });
      assert.equal(item, null);
    });
    it("should error for different types", () => {
      item = instance.vectorAdd(item, { left: "arr2", right: "map" });
      assert.equal(item, null);
    });
    it("should calculate add vectors from arrays", () => {
      item = instance.vectorAdd(item, { left: "arr1", right: "arr2" });
      assert.deepEqual(item.arr1, [5, 7, 9]);
    });
    it("should abort on different sized arrays", () => {
      item = instance.vectorAdd(item, { left: "arr1", right: "long" });
      assert.equal(item, null);
    });
    it("should calculate add vectors from arrays", () => {
      item = instance.vectorAdd(item, { left: "arr1", right: "arr2" });
      assert.deepEqual(item.arr1, [5, 7, 9]);
    });
  });

  describe("#makeBoolean", () => {
    it("should error for missing field", () => {
      item = instance.makeBoolean(item, { field: "missing", threshold: 2 });
      assert.equal(item, null);
    });
    it("should 0/1 a map", () => {
      item = instance.makeBoolean(item, { field: "map", threshold: 2 });
      assert.deepEqual(item.map, { a: 0, b: 0, c: 1 });
    });
    it("should a map of all 1s", () => {
      item = instance.makeBoolean(item, { field: "map" });
      assert.deepEqual(item.map, { a: 1, b: 1, c: 1 });
    });
    it("should -1/1 a map", () => {
      item = instance.makeBoolean(item, {
        field: "map",
        threshold: 2,
        keep_negative: true,
      });
      assert.deepEqual(item.map, { a: -1, b: -1, c: 1 });
    });
    it("should work an array", () => {
      item = instance.makeBoolean(item, { field: "arr1", threshold: 3 });
      assert.deepEqual(item.arr1, [0, 0, 1]);
    });
    it("should -1/1 an array", () => {
      item = instance.makeBoolean(item, {
        field: "arr1",
        threshold: 3,
        keep_negative: true,
      });
      assert.deepEqual(item.arr1, [-1, -1, 1]);
    });
    it("should 1 a high number", () => {
      item = instance.makeBoolean(item, { field: "qux", threshold: 3 });
      assert.equal(item.qux, 1);
    });
    it("should 0 a low number", () => {
      item = instance.makeBoolean(item, { field: "qux", threshold: 70 });
      assert.equal(item.qux, 0);
    });
    it("should -1 a low number", () => {
      item = instance.makeBoolean(item, {
        field: "qux",
        threshold: 83,
        keep_negative: true,
      });
      assert.equal(item.qux, -1);
    });
    it("should fail a string", () => {
      item = instance.makeBoolean(item, { field: "foo", threshold: 3 });
      assert.equal(item, null);
    });
  });

  describe("#whitelistFields", () => {
    it("should filter the keys out of a map", () => {
      item = instance.whitelistFields(item, {
        fields: ["foo", "missing", "bar"],
      });
      assert.deepEqual(item, { foo: "FOO", bar: "BAR" });
    });
  });

  describe("#filterByValue", () => {
    it("should fail on missing field", () => {
      item = instance.filterByValue(item, { field: "missing", threshold: 2 });
      assert.equal(item, null);
    });
    it("should filter the keys out of a map", () => {
      item = instance.filterByValue(item, { field: "map", threshold: 2 });
      assert.deepEqual(item.map, { c: 3 });
    });
  });

  describe("#l2Normalize", () => {
    it("should fail on missing field", () => {
      item = instance.l2Normalize(item, { field: "missing" });
      assert.equal(item, null);
    });
    it("should L2 normalize an array", () => {
      item = instance.l2Normalize(item, { field: "arr1" });
      assert.deepEqual(item.arr1, [
        0.3713906763541037,
        0.5570860145311556,
        0.7427813527082074,
      ]);
    });
    it("should L2 normalize a map", () => {
      item = instance.l2Normalize(item, { field: "map" });
      assert.deepEqual(item.map, {
        a: 0.2672612419124244,
        b: 0.5345224838248488,
        c: 0.8017837257372732,
      });
    });
    it("should fail a string", () => {
      item = instance.l2Normalize(item, { field: "foo" });
      assert.equal(item, null);
    });
    it("should not bomb on a zero vector", () => {
      item = instance.l2Normalize(item, { field: "zero" });
      assert.deepEqual(item.zero, { a: 0, b: 0 });
      item = instance.l2Normalize(item, { field: "zaro" });
      assert.deepEqual(item.zaro, [0, 0]);
    });
  });

  describe("#probNormalize", () => {
    it("should fail on missing field", () => {
      item = instance.probNormalize(item, { field: "missing" });
      assert.equal(item, null);
    });
    it("should normalize an array to sum to 1", () => {
      item = instance.probNormalize(item, { field: "arr1" });
      assert.deepEqual(item.arr1, [
        0.2222222222222222,
        0.3333333333333333,
        0.4444444444444444,
      ]);
    });
    it("should normalize a map to sum to 1", () => {
      item = instance.probNormalize(item, { field: "map" });
      assert.equal(Object.keys(item.map).length, 3);
      assert.isTrue("a" in item.map);
      assert.isTrue(Math.abs(item.map.a - 0.16667) <= EPSILON);
      assert.isTrue("b" in item.map);
      assert.isTrue(Math.abs(item.map.b - 0.33333) <= EPSILON);
      assert.isTrue("c" in item.map);
      assert.isTrue(Math.abs(item.map.c - 0.5) <= EPSILON);
    });
    it("should fail a string", () => {
      item = instance.probNormalize(item, { field: "foo" });
      assert.equal(item, null);
    });
    it("should not bomb on a zero vector", () => {
      item = instance.probNormalize(item, { field: "zero" });
      assert.deepEqual(item.zero, { a: 0, b: 0 });
      item = instance.probNormalize(item, { field: "zaro" });
      assert.deepEqual(item.zaro, [0, 0]);
    });
  });

  describe("#scalarMultiplyTag", () => {
    it("should fail on missing field", () => {
      item = instance.scalarMultiplyTag(item, { field: "missing", k: 3 });
      assert.equal(item, null);
    });
    it("should scalar multiply a nested map", () => {
      item = instance.scalarMultiplyTag(item, {
        field: "tags",
        k: 3,
        log_scale: false,
      });
      assert.isTrue(Math.abs(item.tags.a.aa - 0.3) <= EPSILON);
      assert.isTrue(Math.abs(item.tags.a.ab - 0.6) <= EPSILON);
      assert.isTrue(Math.abs(item.tags.a.ac - 0.9) <= EPSILON);
      assert.isTrue(Math.abs(item.tags.b.ba - 12) <= EPSILON);
      assert.isTrue(Math.abs(item.tags.b.bb - 15) <= EPSILON);
      assert.isTrue(Math.abs(item.tags.b.bc - 18) <= EPSILON);
    });
    it("should scalar multiply a nested map with logrithms", () => {
      item = instance.scalarMultiplyTag(item, {
        field: "tags",
        k: 3,
        log_scale: true,
      });
      assert.isTrue(
        Math.abs(item.tags.a.aa - Math.log(0.1 + 0.000001) * 3) <= EPSILON
      );
      assert.isTrue(
        Math.abs(item.tags.a.ab - Math.log(0.2 + 0.000001) * 3) <= EPSILON
      );
      assert.isTrue(
        Math.abs(item.tags.a.ac - Math.log(0.3 + 0.000001) * 3) <= EPSILON
      );
      assert.isTrue(
        Math.abs(item.tags.b.ba - Math.log(4.0 + 0.000001) * 3) <= EPSILON
      );
      assert.isTrue(
        Math.abs(item.tags.b.bb - Math.log(5.0 + 0.000001) * 3) <= EPSILON
      );
      assert.isTrue(
        Math.abs(item.tags.b.bc - Math.log(6.0 + 0.000001) * 3) <= EPSILON
      );
    });
    it("should fail a string", () => {
      item = instance.scalarMultiplyTag(item, { field: "foo", k: 3 });
      assert.equal(item, null);
    });
  });

  describe("#setDefault", () => {
    it("should store a missing value", () => {
      item = instance.setDefault(item, { field: "missing", value: 1111 });
      assert.equal(item.missing, 1111);
    });
    it("should not overwrite an existing value", () => {
      item = instance.setDefault(item, { field: "lhs", value: 1111 });
      assert.equal(item.lhs, 2);
    });
    it("should store a complex value", () => {
      item = instance.setDefault(item, { field: "missing", value: { a: 1 } });
      assert.deepEqual(item.missing, { a: 1 });
    });
  });

  describe("#lookupValue", () => {
    it("should promote a value", () => {
      item = instance.lookupValue(item, {
        haystack: "map",
        needle: "c",
        dest: "ccc",
      });
      assert.equal(item.ccc, 3);
    });
    it("should handle a missing haystack", () => {
      item = instance.lookupValue(item, {
        haystack: "missing",
        needle: "c",
        dest: "ccc",
      });
      assert.isTrue(!("ccc" in item));
    });
    it("should handle a missing needle", () => {
      item = instance.lookupValue(item, {
        haystack: "map",
        needle: "missing",
        dest: "ccc",
      });
      assert.isTrue(!("ccc" in item));
    });
  });

  describe("#copyToMap", () => {
    it("should copy a value to a map", () => {
      item = instance.copyToMap(item, {
        src: "qux",
        dest_map: "map",
        dest_key: "zzz",
      });
      assert.isTrue("zzz" in item.map);
      assert.equal(item.map.zzz, item.qux);
    });
    it("should create a new map to hold the key", () => {
      item = instance.copyToMap(item, {
        src: "qux",
        dest_map: "missing",
        dest_key: "zzz",
      });
      assert.equal(Object.keys(item.missing).length, 1);
      assert.equal(item.missing.zzz, item.qux);
    });
    it("should not create an empty map if the src is missing", () => {
      item = instance.copyToMap(item, {
        src: "missing",
        dest_map: "no_map",
        dest_key: "zzz",
      });
      assert.isTrue(!("no_map" in item));
    });
  });

  describe("#applySoftmaxTags", () => {
    it("should error on missing field", () => {
      item = instance.applySoftmaxTags(item, { field: "missing" });
      assert.equal(item, null);
    });
    it("should error on nonmaps", () => {
      item = instance.applySoftmaxTags(item, { field: "arr1" });
      assert.equal(item, null);
    });
    it("should error on unnested maps", () => {
      item = instance.applySoftmaxTags(item, { field: "map" });
      assert.equal(item, null);
    });
    it("should error on wrong nested maps", () => {
      item = instance.applySoftmaxTags(item, { field: "bogus" });
      assert.equal(item, null);
    });
    it("should apply softmax across the subtags", () => {
      item = instance.applySoftmaxTags(item, { field: "tags" });
      assert.isTrue("a" in item.tags);
      assert.isTrue("aa" in item.tags.a);
      assert.isTrue("ab" in item.tags.a);
      assert.isTrue("ac" in item.tags.a);
      assert.isTrue(Math.abs(item.tags.a.aa - 0.30061) <= EPSILON);
      assert.isTrue(Math.abs(item.tags.a.ab - 0.33222) <= EPSILON);
      assert.isTrue(Math.abs(item.tags.a.ac - 0.36717) <= EPSILON);

      assert.isTrue("b" in item.tags);
      assert.isTrue("ba" in item.tags.b);
      assert.isTrue("bb" in item.tags.b);
      assert.isTrue("bc" in item.tags.b);
      assert.isTrue(Math.abs(item.tags.b.ba - 0.09003) <= EPSILON);
      assert.isTrue(Math.abs(item.tags.b.bb - 0.24473) <= EPSILON);
      assert.isTrue(Math.abs(item.tags.b.bc - 0.66524) <= EPSILON);
    });
  });

  describe("#combinerAdd", () => {
    it("should do nothing when right field is missing", () => {
      let right = makeItem();
      let combined = instance.combinerAdd(item, right, { field: "missing" });
      assert.deepEqual(combined, item);
    });
    it("should handle missing left maps", () => {
      let right = makeItem();
      right.missingmap = { a: 5, b: -1, c: 3 };
      let combined = instance.combinerAdd(item, right, { field: "missingmap" });
      assert.deepEqual(combined.missingmap, { a: 5, b: -1, c: 3 });
    });
    it("should add equal sized maps", () => {
      let right = makeItem();
      let combined = instance.combinerAdd(item, right, { field: "map" });
      assert.deepEqual(combined.map, { a: 2, b: 4, c: 6 });
    });
    it("should add long map to short map", () => {
      let right = makeItem();
      right.map.d = 999;
      let combined = instance.combinerAdd(item, right, { field: "map" });
      assert.deepEqual(combined.map, { a: 2, b: 4, c: 6, d: 999 });
    });
    it("should add short map to long map", () => {
      let right = makeItem();
      item.map.d = 999;
      let combined = instance.combinerAdd(item, right, { field: "map" });
      assert.deepEqual(combined.map, { a: 2, b: 4, c: 6, d: 999 });
    });
    it("should add equal sized arrays", () => {
      let right = makeItem();
      let combined = instance.combinerAdd(item, right, { field: "arr1" });
      assert.deepEqual(combined.arr1, [4, 6, 8]);
    });
    it("should handle missing left arrays", () => {
      let right = makeItem();
      right.missingarray = [5, 1, 4];
      let combined = instance.combinerAdd(item, right, {
        field: "missingarray",
      });
      assert.deepEqual(combined.missingarray, [5, 1, 4]);
    });
    it("should add long array to short array", () => {
      let right = makeItem();
      right.arr1 = [2, 3, 4, 12];
      let combined = instance.combinerAdd(item, right, { field: "arr1" });
      assert.deepEqual(combined.arr1, [4, 6, 8, 12]);
    });
    it("should add short array to long array", () => {
      let right = makeItem();
      item.arr1 = [2, 3, 4, 12];
      let combined = instance.combinerAdd(item, right, { field: "arr1" });
      assert.deepEqual(combined.arr1, [4, 6, 8, 12]);
    });
    it("should handle missing left number", () => {
      let right = makeItem();
      right.missingnumber = 999;
      let combined = instance.combinerAdd(item, right, {
        field: "missingnumber",
      });
      assert.deepEqual(combined.missingnumber, 999);
    });
    it("should add numbers", () => {
      let right = makeItem();
      let combined = instance.combinerAdd(item, right, { field: "lhs" });
      assert.equal(combined.lhs, 4);
    });
    it("should error on missing left, and right is a string", () => {
      let right = makeItem();
      right.error = "error";
      let combined = instance.combinerAdd(item, right, { field: "error" });
      assert.equal(combined, null);
    });
    it("should error on left string", () => {
      let right = makeItem();
      let combined = instance.combinerAdd(item, right, { field: "foo" });
      assert.equal(combined, null);
    });
    it("should error on mismatch types", () => {
      let right = makeItem();
      right.lhs = [1, 2, 3];
      let combined = instance.combinerAdd(item, right, { field: "lhs" });
      assert.equal(combined, null);
    });
  });

  describe("#combinerMax", () => {
    it("should do nothing when right field is missing", () => {
      let right = makeItem();
      let combined = instance.combinerMax(item, right, { field: "missing" });
      assert.deepEqual(combined, item);
    });
    it("should handle missing left maps", () => {
      let right = makeItem();
      right.missingmap = { a: 5, b: -1, c: 3 };
      let combined = instance.combinerMax(item, right, { field: "missingmap" });
      assert.deepEqual(combined.missingmap, { a: 5, b: -1, c: 3 });
    });
    it("should handle equal sized maps", () => {
      let right = makeItem();
      right.map = { a: 5, b: -1, c: 3 };
      let combined = instance.combinerMax(item, right, { field: "map" });
      assert.deepEqual(combined.map, { a: 5, b: 2, c: 3 });
    });
    it("should handle short map to long map", () => {
      let right = makeItem();
      right.map = { a: 5, b: -1, c: 3, d: 999 };
      let combined = instance.combinerMax(item, right, { field: "map" });
      assert.deepEqual(combined.map, { a: 5, b: 2, c: 3, d: 999 });
    });
    it("should handle long map to short map", () => {
      let right = makeItem();
      right.map = { a: 5, b: -1, c: 3 };
      item.map.d = 999;
      let combined = instance.combinerMax(item, right, { field: "map" });
      assert.deepEqual(combined.map, { a: 5, b: 2, c: 3, d: 999 });
    });
    it("should handle equal sized arrays", () => {
      let right = makeItem();
      right.arr1 = [5, 1, 4];
      let combined = instance.combinerMax(item, right, { field: "arr1" });
      assert.deepEqual(combined.arr1, [5, 3, 4]);
    });
    it("should handle missing left arrays", () => {
      let right = makeItem();
      right.missingarray = [5, 1, 4];
      let combined = instance.combinerMax(item, right, {
        field: "missingarray",
      });
      assert.deepEqual(combined.missingarray, [5, 1, 4]);
    });
    it("should handle short array to long array", () => {
      let right = makeItem();
      right.arr1 = [5, 1, 4, 7];
      let combined = instance.combinerMax(item, right, { field: "arr1" });
      assert.deepEqual(combined.arr1, [5, 3, 4, 7]);
    });
    it("should handle long array to short array", () => {
      let right = makeItem();
      right.arr1 = [5, 1, 4];
      item.arr1.push(7);
      let combined = instance.combinerMax(item, right, { field: "arr1" });
      assert.deepEqual(combined.arr1, [5, 3, 4, 7]);
    });
    it("should handle missing left number", () => {
      let right = makeItem();
      right.missingnumber = 999;
      let combined = instance.combinerMax(item, right, {
        field: "missingnumber",
      });
      assert.deepEqual(combined.missingnumber, 999);
    });
    it("should handle big number", () => {
      let right = makeItem();
      right.lhs = 99;
      let combined = instance.combinerMax(item, right, { field: "lhs" });
      assert.equal(combined.lhs, 99);
    });
    it("should handle small number", () => {
      let right = makeItem();
      item.lhs = 99;
      let combined = instance.combinerMax(item, right, { field: "lhs" });
      assert.equal(combined.lhs, 99);
    });
    it("should error on missing left, and right is a string", () => {
      let right = makeItem();
      right.error = "error";
      let combined = instance.combinerMax(item, right, { field: "error" });
      assert.equal(combined, null);
    });
    it("should error on left string", () => {
      let right = makeItem();
      let combined = instance.combinerMax(item, right, { field: "foo" });
      assert.equal(combined, null);
    });
    it("should error on mismatch types", () => {
      let right = makeItem();
      right.lhs = [1, 2, 3];
      let combined = instance.combinerMax(item, right, { field: "lhs" });
      assert.equal(combined, null);
    });
  });

  describe("#combinerCollectValues", () => {
    it("should error on bogus operation", () => {
      let right = makeItem();
      right.url_domain = "maseratiusa.com/maserati";
      right.time = 41;
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "missing",
      });
      assert.equal(combined, null);
    });
    it("should sum when missing left", () => {
      let right = makeItem();
      right.url_domain = "maseratiusa.com/maserati";
      right.time = 41;
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "sum",
      });
      assert.deepEqual(combined.combined_map, {
        "maseratiusa.com/maserati": 41,
      });
    });
    it("should sum when missing right", () => {
      let right = makeItem();
      item.combined_map = { fake: 42 };
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "sum",
      });
      assert.deepEqual(combined.combined_map, { fake: 42 });
    });
    it("should sum when both", () => {
      let right = makeItem();
      right.url_domain = "maseratiusa.com/maserati";
      right.time = 41;
      item.combined_map = { fake: 42, "maseratiusa.com/maserati": 41 };
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "sum",
      });
      assert.deepEqual(combined.combined_map, {
        fake: 42,
        "maseratiusa.com/maserati": 82,
      });
    });

    it("should max when missing left", () => {
      let right = makeItem();
      right.url_domain = "maseratiusa.com/maserati";
      right.time = 41;
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "max",
      });
      assert.deepEqual(combined.combined_map, {
        "maseratiusa.com/maserati": 41,
      });
    });
    it("should max when missing right", () => {
      let right = makeItem();
      item.combined_map = { fake: 42 };
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "max",
      });
      assert.deepEqual(combined.combined_map, { fake: 42 });
    });
    it("should max when both (right)", () => {
      let right = makeItem();
      right.url_domain = "maseratiusa.com/maserati";
      right.time = 99;
      item.combined_map = { fake: 42, "maseratiusa.com/maserati": 41 };
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "max",
      });
      assert.deepEqual(combined.combined_map, {
        fake: 42,
        "maseratiusa.com/maserati": 99,
      });
    });
    it("should max when both (left)", () => {
      let right = makeItem();
      right.url_domain = "maseratiusa.com/maserati";
      right.time = -99;
      item.combined_map = { fake: 42, "maseratiusa.com/maserati": 41 };
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "max",
      });
      assert.deepEqual(combined.combined_map, {
        fake: 42,
        "maseratiusa.com/maserati": 41,
      });
    });

    it("should overwrite when missing left", () => {
      let right = makeItem();
      right.url_domain = "maseratiusa.com/maserati";
      right.time = 41;
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "overwrite",
      });
      assert.deepEqual(combined.combined_map, {
        "maseratiusa.com/maserati": 41,
      });
    });
    it("should overwrite when missing right", () => {
      let right = makeItem();
      item.combined_map = { fake: 42 };
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "overwrite",
      });
      assert.deepEqual(combined.combined_map, { fake: 42 });
    });
    it("should overwrite when both", () => {
      let right = makeItem();
      right.url_domain = "maseratiusa.com/maserati";
      right.time = 41;
      item.combined_map = { fake: 42, "maseratiusa.com/maserati": 77 };
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "overwrite",
      });
      assert.deepEqual(combined.combined_map, {
        fake: 42,
        "maseratiusa.com/maserati": 41,
      });
    });

    it("should count when missing left", () => {
      let right = makeItem();
      right.url_domain = "maseratiusa.com/maserati";
      right.time = 41;
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "count",
      });
      assert.deepEqual(combined.combined_map, {
        "maseratiusa.com/maserati": 1,
      });
    });
    it("should count when missing right", () => {
      let right = makeItem();
      item.combined_map = { fake: 42 };
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "count",
      });
      assert.deepEqual(combined.combined_map, { fake: 42 });
    });
    it("should count when both", () => {
      let right = makeItem();
      right.url_domain = "maseratiusa.com/maserati";
      right.time = 41;
      item.combined_map = { fake: 42, "maseratiusa.com/maserati": 1 };
      let combined = instance.combinerCollectValues(item, right, {
        left_field: "combined_map",
        right_key_field: "url_domain",
        right_value_field: "time",
        operation: "count",
      });
      assert.deepEqual(combined.combined_map, {
        fake: 42,
        "maseratiusa.com/maserati": 2,
      });
    });
  });

  describe("#executeRecipe", () => {
    it("should handle working steps", () => {
      let final = instance.executeRecipe({}, [
        { function: "set_default", field: "foo", value: 1 },
        { function: "set_default", field: "bar", value: 10 },
      ]);
      assert.equal(final.foo, 1);
      assert.equal(final.bar, 10);
    });
    it("should handle unknown steps", () => {
      let final = instance.executeRecipe({}, [
        { function: "set_default", field: "foo", value: 1 },
        { function: "missing" },
        { function: "set_default", field: "bar", value: 10 },
      ]);
      assert.equal(final, null);
    });
    it("should handle erroring steps", () => {
      let final = instance.executeRecipe({}, [
        { function: "set_default", field: "foo", value: 1 },
        {
          function: "accept_item_by_field_value",
          field: "missing",
          op: "invalid",
          rhsField: "moot",
          rhsValue: "m00t",
        },
        { function: "set_default", field: "bar", value: 10 },
      ]);
      assert.equal(final, null);
    });
  });

  describe("#executeCombinerRecipe", () => {
    it("should handle working steps", () => {
      let final = instance.executeCombinerRecipe(
        { foo: 1, bar: 10 },
        { foo: 1, bar: 10 },
        [
          { function: "combiner_add", field: "foo" },
          { function: "combiner_add", field: "bar" },
        ]
      );
      assert.equal(final.foo, 2);
      assert.equal(final.bar, 20);
    });
    it("should handle unknown steps", () => {
      let final = instance.executeCombinerRecipe(
        { foo: 1, bar: 10 },
        { foo: 1, bar: 10 },
        [
          { function: "combiner_add", field: "foo" },
          { function: "missing" },
          { function: "combiner_add", field: "bar" },
        ]
      );
      assert.equal(final, null);
    });
    it("should handle erroring steps", () => {
      let final = instance.executeCombinerRecipe(
        { foo: 1, bar: 10, baz: 0 },
        { foo: 1, bar: 10, baz: "hundred" },
        [
          { function: "combiner_add", field: "foo" },
          { function: "combiner_add", field: "baz" },
          { function: "combiner_add", field: "bar" },
        ]
      );
      assert.equal(final, null);
    });
  });
});

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const apiUtils = require("sdk/deprecated/api-utils");

exports.testValidateOptionsEmpty = function (assert) {
  let val = apiUtils.validateOptions(null, {});

  assert.deepEqual(val, {});

  val = apiUtils.validateOptions(null, { foo: {} });
  assert.deepEqual(val, {});

  val = apiUtils.validateOptions({}, {});
  assert.deepEqual(val, {});

  val = apiUtils.validateOptions({}, { foo: {} });
  assert.deepEqual(val, {});
};

exports.testValidateOptionsNonempty = function (assert) {
  let val = apiUtils.validateOptions({ foo: 123 }, {});
  assert.deepEqual(val, {});

  val = apiUtils.validateOptions({ foo: 123, bar: 456 },
                                 { foo: {}, bar: {}, baz: {} });

  assert.deepEqual(val, { foo: 123, bar: 456 });
};

exports.testValidateOptionsMap = function (assert) {
  let val = apiUtils.validateOptions({ foo: 3, bar: 2 }, {
    foo: { map: v => v * v },
    bar: { map: v => undefined }
  });
  assert.deepEqual(val, { foo: 9, bar: undefined });
};

exports.testValidateOptionsMapException = function (assert) {
  let val = apiUtils.validateOptions({ foo: 3 }, {
    foo: { map: function () { throw new Error(); }}
  });
  assert.deepEqual(val, { foo: 3 });
};

exports.testValidateOptionsOk = function (assert) {
  let val = apiUtils.validateOptions({ foo: 3, bar: 2, baz: 1 }, {
    foo: { ok: v => v },
    bar: { ok: v => v }
  });
  assert.deepEqual(val, { foo: 3, bar: 2 });

  assert.throws(
    () => apiUtils.validateOptions({ foo: 2, bar: 2 }, {
      bar: { ok: v => v > 2 }
    }),
    /^The option "bar" is invalid/,
    "ok should raise exception on invalid option"
  );

  assert.throws(
    () => apiUtils.validateOptions(null, { foo: { ok: v => v }}),
    /^The option "foo" is invalid/,
    "ok should raise exception on invalid option"
  );
};

exports.testValidateOptionsIs = function (assert) {
  let opts = {
    array: [],
    boolean: true,
    func: function () {},
    nul: null,
    number: 1337,
    object: {},
    string: "foo",
    undef1: undefined
  };
  let requirements = {
    array: { is: ["array"] },
    boolean: { is: ["boolean"] },
    func: { is: ["function"] },
    nul: { is: ["null"] },
    number: { is: ["number"] },
    object: { is: ["object"] },
    string: { is: ["string"] },
    undef1: { is: ["undefined"] },
    undef2: { is: ["undefined"] }
  };
  let val = apiUtils.validateOptions(opts, requirements);
  assert.deepEqual(val, opts);

  assert.throws(
    () => apiUtils.validateOptions(null, {
      foo: { is: ["object", "number"] }
    }),
    /^The option "foo" must be one of the following types: object, number/,
    "Invalid type should raise exception"
  );
};

exports.testValidateOptionsIsWithExportedValue = function (assert) {
  let { string, number, boolean, object } = apiUtils;

  let opts = {
    boolean: true,
    number: 1337,
    object: {},
    string: "foo"
  };
  let requirements = {
    string: { is: string },
    number: { is: number },
    boolean: { is: boolean },
    object: { is: object }
  };
  let val = apiUtils.validateOptions(opts, requirements);
  assert.deepEqual(val, opts);

  // Test the types are optional by default
  val = apiUtils.validateOptions({foo: 'bar'}, requirements);
  assert.deepEqual(val, {});
};

exports.testValidateOptionsIsWithEither = function (assert) {
  let { string, number, boolean, either } = apiUtils;
  let text = { is: either(string, number) };

  let requirements = {
    text: text,
    boolOrText: { is: either(text, boolean) }
  };

  let val = apiUtils.validateOptions({text: 12}, requirements);
  assert.deepEqual(val, {text: 12});

  val = apiUtils.validateOptions({text: "12"}, requirements);
  assert.deepEqual(val, {text: "12"});

  val = apiUtils.validateOptions({boolOrText: true}, requirements);
  assert.deepEqual(val, {boolOrText: true});

  val = apiUtils.validateOptions({boolOrText: "true"}, requirements);
  assert.deepEqual(val, {boolOrText: "true"});

  val = apiUtils.validateOptions({boolOrText: 1}, requirements);
  assert.deepEqual(val, {boolOrText: 1});

  assert.throws(
    () => apiUtils.validateOptions({text: true}, requirements),
    /^The option "text" must be one of the following types/,
    "Invalid type should raise exception"
  );

  assert.throws(
    () => apiUtils.validateOptions({boolOrText: []}, requirements),
    /^The option "boolOrText" must be one of the following types/,
    "Invalid type should raise exception"
  );
};

exports.testValidateOptionsWithRequiredAndOptional = function (assert) {
  let { string, number, required, optional } = apiUtils;

  let opts = {
    number: 1337,
    string: "foo"
  };

  let requirements = {
    string: required(string),
    number: number
  };

  let val = apiUtils.validateOptions(opts, requirements);
  assert.deepEqual(val, opts);

  val = apiUtils.validateOptions({string: "foo"}, requirements);
  assert.deepEqual(val, {string: "foo"});

  assert.throws(
    () => apiUtils.validateOptions({number: 10}, requirements),
    /^The option "string" must be one of the following types/,
    "Invalid type should raise exception"
  );

  // Makes string optional
  requirements.string = optional(requirements.string);

  val = apiUtils.validateOptions({number: 10}, requirements),
  assert.deepEqual(val, {number: 10});

};



exports.testValidateOptionsWithExportedValue = function (assert) {
  let { string, number, boolean, object } = apiUtils;

  let opts = {
    boolean: true,
    number: 1337,
    object: {},
    string: "foo"
  };
  let requirements = {
    string: string,
    number: number,
    boolean: boolean,
    object: object
  };
  let val = apiUtils.validateOptions(opts, requirements);
  assert.deepEqual(val, opts);

  // Test the types are optional by default
  val = apiUtils.validateOptions({foo: 'bar'}, requirements);
  assert.deepEqual(val, {});
};


exports.testValidateOptionsMapIsOk = function (assert) {
  let [map, is, ok] = [false, false, false];
  let val = apiUtils.validateOptions({ foo: 1337 }, {
    foo: {
      map: v => v.toString(),
      is: ["string"],
      ok: v => v.length > 0
    }
  });
  assert.deepEqual(val, { foo: "1337" });

  let requirements = {
    foo: {
      is: ["object"],
      ok: () => assert.fail("is should have caused us to throw by now")
    }
  };
  assert.throws(
    () => apiUtils.validateOptions(null, requirements),
    /^The option "foo" must be one of the following types: object/,
    "is should be used before ok is called"
  );
};

exports.testValidateOptionsErrorMsg = function (assert) {
  assert.throws(
    () => apiUtils.validateOptions(null, {
      foo: { ok: v => v, msg: "foo!" }
    }),
    /^foo!/,
    "ok should raise exception with customized message"
  );
};

exports.testValidateMapWithMissingKey = function (assert) {
  let val = apiUtils.validateOptions({ }, {
    foo: {
      map: v => v || "bar"
    }
  });
  assert.deepEqual(val, { foo: "bar" });

  val = apiUtils.validateOptions({ }, {
    foo: {
      map: v => { throw "bar" }
    }
  });
  assert.deepEqual(val, { });
};

exports.testValidateMapWithMissingKeyAndThrown = function (assert) {
  let val = apiUtils.validateOptions({}, {
    bar: {
      map: function(v) { throw "bar" }
    },
    baz: {
      map: v => "foo"
    }
  });
  assert.deepEqual(val, { baz: "foo" });
};

exports.testAddIterator = function testAddIterator (assert) {
  let obj = {};
  let keys = ["foo", "bar", "baz"];
  let vals = [1, 2, 3];
  let keysVals = [["foo", 1], ["bar", 2], ["baz", 3]];
  apiUtils.addIterator(
    obj,
    function keysValsGen() {
      for (let keyVal of keysVals)
        yield keyVal;
    }
  );

  let keysItr = [];
  for (let key in obj)
    keysItr.push(key);

  assert.equal(keysItr.length, keys.length,
                   "the keys iterator returns the correct number of items");
  for (let i = 0; i < keys.length; i++)
    assert.equal(keysItr[i], keys[i], "the key is correct");

  let valsItr = [];
  for each (let val in obj)
    valsItr.push(val);
  assert.equal(valsItr.length, vals.length,
                   "the vals iterator returns the correct number of items");
  for (let i = 0; i < vals.length; i++)
    assert.equal(valsItr[i], vals[i], "the val is correct");

};

require("sdk/test").run(exports);

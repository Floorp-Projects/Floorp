/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ERR_CONFLICT = 'Remaining conflicting property: ',
      ERR_REQUIRED = 'Missing required property: ';

function assertSametrait(test, trait1, trait2) {
  let names1 = Object.getOwnPropertyNames(trait1),
      names2 = Object.getOwnPropertyNames(trait2);

  test.assertEqual(
    names1.length,
    names2.length,
    'equal traits must have same amount of properties'
  );

  for (let i = 0; i < names1.length; i++) {
    let name = names1[i];
    test.assertNotEqual(
      -1,
      names2.indexOf(name),
      'equal traits must contain same named properties: ' + name
    );
    assertSameDescriptor(test, name, trait1[name], trait2[name]);
  }
}

function assertSameDescriptor(test, name, desc1, desc2) {
  if (desc1.conflict || desc2.conflict) {
    test.assertEqual(
      desc1.conflict,
      desc2.conflict,
      'if one of same descriptors has `conflict` another must have it: '
        + name
    );
  } else if (desc1.required || desc2.required) {
    test.assertEqual(
      desc1.required,
      desc2.required,
      'if one of same descriptors is has `required` another must have it: '
        + name
    );
  } else {
    test.assertEqual(
      desc1.get,
      desc2.get,
      'get must be the same on both descriptors: ' + name
    );
    test.assertEqual(
      desc1.set,
      desc2.set,
      'set must be the same on both descriptors: ' + name
    );
    test.assertEqual(
      desc1.value,
      desc2.value,
      'value must be the same on both descriptors: ' + name
    );
    test.assertEqual(
      desc1.enumerable,
      desc2.enumerable,
      'enumerable must be the same on both descriptors: ' + name
    );
    test.assertEqual(
      desc1.required,
      desc2.required,
      'value must be the same on both descriptors: ' + name
    );
  }
}

function Data(value, enumerable, confligurable, writable) {
  return {
    value: value,
    enumerable: false !== enumerable,
    confligurable: false !== confligurable,
    writable: false !== writable
  };
}

function Method(method, enumerable, confligurable, writable) {
  return {
    value: method,
    enumerable: false !== enumerable,
    confligurable: false !== confligurable,
    writable: false !== writable
  };
}

function Accessor(get, set, enumerable, confligurable) {
  return {
    get: get,
    set: set,
    enumerable: false !== enumerable,
    confligurable: false !== confligurable,
  };
}

function Required(name) {
  function required() { throw new Error(ERR_REQUIRED + name) }
  return {
    get: required,
    set: required,
    required: true
  };
}

function Conflict(name) {
  function conflict() { throw new Error(ERR_CONFLICT + name) }
  return {
    get: conflict,
    set: conflict,
    conflict: true
  };
}

function testMethod() {};

const { trait, compose, resolve, required, override, create } =
  require('sdk/deprecated/traits/core');


exports['test:empty trait'] = function(test) {
  assertSametrait(
    test,
    trait({}),
    {}
  );
};

exports['test:simple trait'] = function(test) {
  assertSametrait(
    test,
    trait({
      a: 0,
      b: testMethod
    }),
    {
      a: Data(0, true, true, true),
      b: Method(testMethod, true, true, true)
    }
  );
};

exports['test:simple trait with required prop'] = function(test) {
  assertSametrait(
    test,
    trait({
      a: required,
      b: 1
    }),
    {
      a: Required('a'),
      b: Data(1)
    }
  );
};

exports['test:ordering of trait properties is irrelevant'] = function(test) {
  assertSametrait(test,
    trait({ a: 0, b: 1, c: required }),
    trait({ b: 1, c: required, a: 0 })
  );
};

exports['test:trait with accessor property'] = function(test) {
  let record = { get a() {}, set a(v) {} };
  let get = Object.getOwnPropertyDescriptor(record,'a').get;
  let set = Object.getOwnPropertyDescriptor(record,'a').set;
  assertSametrait(test,
    trait(record),
    { a: Accessor(get, set ) }
  );
};

exports['test:simple composition'] = function(test) {
  assertSametrait(test,
    compose(
      trait({ a: 0, b: 1 }),
      trait({ c: 2, d: testMethod })
    ),
    {
      a: Data(0),
      b: Data(1),
      c: Data(2),
      d: Method(testMethod)
    }
  );
};

exports['test:composition with conflict'] = function(test) {
  assertSametrait(test,
    compose(
      trait({ a: 0, b: 1 }),
      trait({ a: 2, c: testMethod })
    ),
    {
      a: Conflict('a'),
      b: Data(1),
      c: Method(testMethod)
    }
  );
};

exports['test:composition of identical props does not cause conflict'] =
function(test) {
  assertSametrait(test,
    compose(
      trait({ a: 0, b: 1 }),
      trait({ a: 0, c: testMethod })
    ),
    {
      a: Data(0),
      b: Data(1),
      c: Method(testMethod) }
  )
};

exports['test:composition with identical required props'] =
function(test) {
  assertSametrait(test,
    compose(
      trait({ a: required, b: 1 }),
      trait({ a: required, c: testMethod })
    ),
    {
      a: Required(),
      b: Data(1),
      c: Method(testMethod)
    }
  );
};

exports['test:composition satisfying a required prop'] = function (test) {
  assertSametrait(test,
    compose(
      trait({ a: required, b: 1 }),
      trait({ a: testMethod })
    ),
    {
      a: Method(testMethod),
      b: Data(1)
    }
  );
};

exports['test:compose is neutral wrt conflicts'] = function (test) {
  assertSametrait(test,
    compose(
      compose(
        trait({ a: 1 }),
        trait({ a: 2 })
      ),
      trait({ b: 0 })
    ),
    {
      a: Conflict('a'),
      b: Data(0)
    }
  );
};

exports['test:conflicting prop overrides required prop'] = function (test) {
  assertSametrait(test,
    compose(
      compose(
        trait({ a: 1 }),
        trait({ a: 2 })
      ),
      trait({ a: required })
    ),
    {
      a: Conflict('a')
    }
  );
};

exports['test:compose is commutative'] = function (test) {
  assertSametrait(test,
    compose(
      trait({ a: 0, b: 1 }),
      trait({ c: 2, d: testMethod })
    ),
    compose(
      trait({ c: 2, d: testMethod }),
      trait({ a: 0, b: 1 })
    )
  );
};

exports['test:compose is commutative, also for required/conflicting props'] =
function (test) {
  assertSametrait(test,
    compose(
      trait({ a: 0, b: 1, c: 3, e: required }),
      trait({ c: 2, d: testMethod })
    ),
    compose(
      trait({ c: 2, d: testMethod }),
      trait({ a: 0, b: 1, c: 3, e: required })
    )
  );
};
exports['test:compose is associative'] = function (test) {
  assertSametrait(test,
    compose(
      trait({ a: 0, b: 1, c: 3, d: required }),
      compose(
        trait({ c: 3, d: required }),
        trait({ c: 2, d: testMethod, e: 'foo' })
      )
    ),
    compose(
      compose(
        trait({ a: 0, b: 1, c: 3, d: required }),
        trait({ c: 3, d: required })
      ),
      trait({ c: 2, d: testMethod, e: 'foo' })
    )
  );
};

exports['test:diamond import of same prop does not generate conflict'] =
function (test) {
  assertSametrait(test,
    compose(
      compose(
        trait({ b: 2 }),
        trait({ a: 1 })
      ),
      compose(
        trait({ c: 3 }),
        trait({ a: 1 })
      ),
      trait({ d: 4 })
    ),
    {
      a: Data(1),
      b: Data(2),
      c: Data(3),
      d: Data(4)
    }
  );
};

exports['test:resolve with empty resolutions has no effect'] =
function (test) {
  assertSametrait(test, resolve({}, trait({
    a: 1,
    b: required,
    c: testMethod
  })), {
    a: Data(1),
    b: Required(),
    c: Method(testMethod)
  });
};

exports['test:resolve: renaming'] = function (test) {
  assertSametrait(test,
    resolve(
      { a: 'A', c: 'C' },
      trait({ a: 1, b: required, c: testMethod })
    ),
    {
      A: Data(1),
      b: Required(),
      C: Method(testMethod),
      a: Required(),
      c: Required()
    }
  );
};

exports['test:resolve: renaming to conflicting name causes conflict, order 1']
= function (test) {
  assertSametrait(test,
    resolve(
      { a: 'b'},
      trait({ a: 1, b: 2 })
    ),
    {
      b: Conflict('b'),
      a: Required()
    }
  );
};

exports['test:resolve: renaming to conflicting name causes conflict, order 2']
= function (test) {
  assertSametrait(test,
    resolve(
      { a: 'b' },
      trait({ b: 2, a: 1 })
    ),
    {
      b: Conflict('b'),
      a: Required()
    }
  );
};

exports['test:resolve: simple exclusion'] = function (test) {
  assertSametrait(test,
    resolve(
      { a: undefined },
      trait({ a: 1, b: 2 })
    ),
    {
      a: Required(),
      b: Data(2)
    }
  );
};

exports['test:resolve: exclusion to "empty" trait'] = function (test) {
  assertSametrait(test,
    resolve(
      { a: undefined, b: undefined },
      trait({ a: 1, b: 2 })
    ),
    {
      a: Required(),
      b: Required()
    }
  );
};

exports['test:resolve: exclusion and renaming of disjoint props'] =
function (test) {
  assertSametrait(test,
    resolve(
      { a: undefined, b: 'c' },
      trait({ a: 1, b: 2 })
    ),
    {
      a: Required(),
      c: Data(2),
      b: Required()
    }
  );
};

exports['test:resolve: exclusion and renaming of overlapping props'] =
function (test) {
  assertSametrait(test,
    resolve(
      { a: undefined, b: 'a' },
      trait({ a: 1, b: 2 })
    ),
    {
      a: Data(2),
      b: Required()
    }
  );
};

exports['test:resolve: renaming to a common alias causes conflict'] =
function (test) {
  assertSametrait(test,
    resolve(
      { a: 'c', b: 'c' },
      trait({ a: 1, b: 2 })
    ),
    {
      c: Conflict('c'),
      a: Required(),
      b: Required()
    }
  );
};

exports['test:resolve: renaming overrides required target'] =
function (test) {
  assertSametrait(test,
    resolve(
      { b: 'a' },
      trait({ a: required, b: 2 })
    ),
    {
      a: Data(2),
      b: Required()
    }
  );
};

exports['test:resolve: renaming required properties has no effect'] =
function (test) {
  assertSametrait(test,
    resolve(
      { b: 'a' },
      trait({ a: 2, b: required })
    ),
    {
      a: Data(2),
      b: Required()
    }
  );
};

exports['test:resolve: renaming of non-existent props has no effect'] =
function (test) {
  assertSametrait(test,
    resolve(
      { a: 'c', d: 'c' },
      trait({ a: 1, b: 2 })
    ),
    {
      c: Data(1),
      b: Data(2),
      a: Required()
    }
  );
};

exports['test:resolve: exclusion of non-existent props has no effect'] =
function (test) {
  assertSametrait(test,
    resolve(
      { b: undefined },
      trait({ a: 1 })
    ),
    {
      a: Data(1)
    }
  );
};

exports['test:resolve is neutral w.r.t. required properties'] =
function (test) {
  assertSametrait(test,
    resolve(
      { a: 'c', b: undefined },
      trait({ a: required, b: required, c: 'foo', d: 1 })
    ),
    {
      a: Required(),
      b: Required(),
      c: Data('foo'),
      d: Data(1)
    }
  );
};

exports['test:resolve supports swapping of property names, ordering 1'] =
function (test) {
  assertSametrait(test,
    resolve(
      { a: 'b', b: 'a' },
      trait({ a: 1, b: 2 })
    ),
    {
      a: Data(2),
      b: Data(1)
    }
  );
};

exports['test:resolve supports swapping of property names, ordering 2'] =
function (test) {
  assertSametrait(test,
    resolve(
      { b: 'a', a: 'b' },
      trait({ a: 1, b: 2 })
    ),
    {
      a: Data(2),
      b: Data(1)
    }
  );
};

exports['test:resolve supports swapping of property names, ordering 3'] =
function (test) {
  assertSametrait(test,
    resolve(
      { b: 'a', a: 'b' },
      trait({ b: 2, a: 1 })
    ),
    {
      a: Data(2),
      b: Data(1)
    }
  );
};

exports['test:resolve supports swapping of property names, ordering 4'] =
function (test) {
  assertSametrait(test,
    resolve(
      { a: 'b', b: 'a' },
      trait({ b: 2, a: 1 })
    ),
    {
      a: Data(2),
      b: Data(1)
    }
  );
};

exports['test:override of mutually exclusive traits'] = function (test) {
  assertSametrait(test,
    override(
      trait({ a: 1, b: 2 }),
      trait({ c: 3, d: testMethod })
    ),
    {
      a: Data(1),
      b: Data(2),
      c: Data(3),
      d: Method(testMethod)
    }
  );
};

exports['test:override of mutually exclusive traits is compose'] =
function (test) {
  assertSametrait(test,
    override(
      trait({ a: 1, b: 2 }),
      trait({ c: 3, d: testMethod })
    ),
    compose(
      trait({ d: testMethod, c: 3 }),
      trait({ b: 2, a: 1 })
    )
  );
};

exports['test:override of overlapping traits'] = function (test) {
  assertSametrait(test,
    override(
      trait({ a: 1, b: 2 }),
      trait({ a: 3, c: testMethod })
    ),
    {
      a: Data(1),
      b: Data(2),
      c: Method(testMethod)
    }
  );
};

exports['test:three-way override of overlapping traits'] = function (test) {
  assertSametrait(test,
    override(
      trait({ a: 1, b: 2 }),
      trait({ b: 4, c: 3 }),
      trait({ a: 3, c: testMethod, d: 5 })
    ),
    {
      a: Data(1),
      b: Data(2),
      c: Data(3),
      d: Data(5)
    }
  );
};

exports['test:override replaces required properties'] = function (test) {
  assertSametrait(test,
    override(
      trait({ a: required, b: 2 }),
      trait({ a: 1, c: testMethod })
    ),
    {
      a: Data(1),
      b: Data(2),
      c: Method(testMethod)
    }
  );
};

exports['test:override is not commutative'] = function (test) {
  assertSametrait(test,
    override(
      trait({ a: 1, b: 2 }),
      trait({ a: 3, c: 4 })
    ),
    {
      a: Data(1),
      b: Data(2),
      c: Data(4)
    }
  );

  assertSametrait(test,
    override(
      trait({ a: 3, c: 4 }),
      trait({ a: 1, b: 2 })
    ),
    {
      a: Data(3),
      b: Data(2),
      c: Data(4)
    }
  );
};

exports['test:override is associative'] = function (test) {
  assertSametrait(test,
    override(
      override(
        trait({ a: 1, b: 2 }),
        trait({ a: 3, c: 4, d: 5 })
      ),
      trait({ a: 6, c: 7, e: 8 })
    ),
    override(
      trait({ a: 1, b: 2 }),
      override(
        trait({ a: 3, c: 4, d: 5 }),
        trait({ a: 6, c: 7, e: 8 })
      )
    )
  );
};

exports['test:create simple'] = function(test) {
  let o1 = create(
    Object.prototype,
    trait({ a: 1, b: function() { return this.a; } })
  );

  test.assertEqual(
    Object.prototype,
    Object.getPrototypeOf(o1),
    'o1 prototype'
  );
  test.assertEqual(1, o1.a, 'o1.a');
  test.assertEqual(1, o1.b(), 'o1.b()');
  test.assertEqual(
    2,
    Object.getOwnPropertyNames(o1).length,
    'Object.keys(o1).length === 2'
  );
};

exports['test:create with Array.prototype'] = function(test) {
  let o2 = create(Array.prototype, trait({}));
  test.assertEqual(
    Array.prototype,
    Object.getPrototypeOf(o2),
    "o2 prototype"
  );
};

exports['test:exception for incomplete required properties'] =
function(test) {
  try {
    create(Object.prototype, trait({ foo: required }));
    test.fail('expected create to complain about missing required props');
  } catch(e) {
    test.assertEqual(
      'Error: Missing required property: foo',
      e.toString(),
      'required prop error'
    );
  }
};

exports['test:exception for unresolved conflicts'] = function(test) {
  try {
    create({}, compose(trait({ a: 0 }), trait({ a: 1 })));
    test.fail('expected create to complain about unresolved conflicts');
  } catch(e) {
    test.assertEqual(
      'Error: Remaining conflicting property: a',
      e.toString(),
      'conflicting prop error'
    );
  }
};

exports['test:verify that required properties are present but undefined'] =
function(test) {
  try {
    let o4 = Object.create(Object.prototype, trait({ foo: required }));
    test.assertEqual(true, 'foo' in o4, 'required property present');
    try {
      let foo = o4.foo;
      test.fail('access to required property must throw');
    } catch(e) {
      test.assertEqual(
        'Error: Missing required property: foo',
        e.toString(),
        'required prop error'
      )
    }
  } catch(e) {
    test.fail('did not expect create to complain about required props');
  }
};

exports['test:verify that conflicting properties are present'] =
function(test) {
  try {
    let o5 = Object.create(
      Object.prototype,
      compose(trait({ a: 0 }), trait({ a: 1 }))
    );
    test.assertEqual(true, 'a' in o5, 'conflicting property present');
    try {
      let a = o5.a; // accessors or data prop
      test.fail('expected conflicting prop to cause exception');
    } catch (e) {
      test.assertEqual(
        'Error: Remaining conflicting property: a',
        e.toString(),
        'conflicting prop access error'
      );
    }
  } catch(e) {
    test.fail('did not expect create to complain about conflicting props');
  }
};

exports['test diamond with conflicts'] = function(test) {
  function makeT1(x) trait({ m: function() { return x; } })
  function makeT2(x) compose(trait({ t2: 'foo' }), makeT1(x))
  function makeT3(x) compose(trait({ t3: 'bar' }), makeT1(x))

  let T4 = compose(makeT2(5), makeT3(5));
  try {
    let o = create(Object.prototype, T4);
    test.fail('expected diamond prop to cause exception');
  } catch(e) {
    test.assertEqual(
      'Error: Remaining conflicting property: m',
      e.toString(),
      'diamond prop conflict'
    );
  }
};


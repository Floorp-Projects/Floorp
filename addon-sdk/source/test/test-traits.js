/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Trait } = require('sdk/deprecated/traits');

exports['test:simple compose'] = function(test) {
  let List = Trait.compose({
    _list: null,
    constructor: function List() {
      this._list = [];
    },
    list: function list() this._list.slice(0),
    add: function add(item) this._list.push(item),
    remove: function remove(item) {
      let list = this._list;
      let index = list.indexOf(item);
      if (0 <= index) list.slice(index, 1);
    }
  });

  test.assertNotEqual(undefined, List, 'should not be undefined');
  test.assertEqual('function', typeof List, 'type should be function');
  test.assertEqual(
    Trait.compose,
    List.compose,
    'should inherit static compose'
  );
  test.assertEqual(
    Trait.override,
    List.override,
    'should inherit static override'
  );
  test.assertEqual(
    Trait.required,
    List.required,
    'should inherit static required'
  );
  test.assertEqual(
    Trait.resolve,
    List.resolve,
    'should inherit static resolve'
  );

  test.assert(
    !('_list' in List.prototype),
    'should not expose private API'
  );
}
exports['test: compose trait instance and create instance'] = function(test) {
  let List = Trait.compose({
    constructor: function List(options) {
      this._list = [];
      this._public.publicMember = options.publicMember;
    },
    _privateMember: true,
    get privateMember() this._privateMember,
    get list() this._list.slice(0),
    add: function add(item) this._list.push(item),
    remove: function remove(item) {
      let list = this._list
      let index = list.indexOf(item)
      if (0 <= index) list.slice(index, 1)
    }
  });
  let list = List({ publicMember: true });

  test.assertEqual('object', typeof list, 'should return an object')
  test.assertEqual(
    true,
    list instanceof List,
    'should be instance of a List'
  );

  test.assertEqual(
    undefined,
    list._privateMember,
    'instance should not expose private API'
  );

  test.assertEqual(
    true,
    list.privateMember,
    'privates are accessible by  public API'
  );

  list._privateMember = false;

  test.assertEqual(
    true,
    list.privateMember,
    'property changes on instance must not affect privates'
  );

  test.assert(
    !('_list' in list),
    'instance should not expose private members'
  );

  test.assertEqual(
    true,
    list.publicMember,
    'public members are exposed'
  )
  test.assertEqual(
    'function',
    typeof list.add,
    'should be function'
  )
  test.assertEqual(
    'function',
    typeof list.remove,
    'should be function'
  );

  list.add(1);
  test.assertEqual(
    1,
    list.list[0],
    'exposed public API should be able of modifying privates'
  )
};


exports['test:instances must not be hackable'] = function(test) {
  let SECRET = 'There is no secret!',
      secret = null;

  let Class = Trait.compose({
    _secret: null,
    protect: function(data) this._secret = data
  });

  let i1 = Class();
  i1.protect(SECRET);

  test.assertEqual(
    undefined,
    (function() this._secret).call(i1),
    'call / apply can\'t access private state'
  );

  let proto = Object.getPrototypeOf(i1);
  try {
    proto.reveal = function() this._secret;
    secret = i1.reveal();
  } catch(e) {}
  test.assertNotEqual(
    SECRET,
    secret,
    'public __proto__ changes should not affect privates'
  );
  secret = null;

  let Class2 = Trait.compose({
    _secret: null,
    protect: function(data) this._secret = data
  });
  let i2 = Class2();
  i2.protect(SECRET);
  try {
    Object.prototype.reveal = function() this._secret;
    secret = i2.reveal();
  } catch(e) {}
  test.assertNotEqual(
    SECRET,
    secret,
    'Object.prototype changes must not affect instances'
  );
}

exports['test:instanceof'] = function(test) {
  const List = Trait.compose({
    // private API:
    _list: null,
    // public API
    constructor: function List() {
      this._list = []
    },
    get length() this._list.length,
    add: function add(item) this._list.push(item),
    remove: function remove(item) {
      let list = this._list;
      let index = list.indexOf(item);
      if (0 <= index) list.slice(index, 1);
    }
  });

  test.assert(List() instanceof List, 'Must be instance of List');
  test.assert(new List() instanceof List, 'Must be instance of List');
};

exports['test:privates are unaccessible'] = function(test) {
  const List = Trait.compose({
    // private API:
    _list: null,
    // public API
    constructor: function List() {
      this._list = [];
    },
    get length() this._list.length,
    add: function add(item) this._list.push(item),
    remove: function remove(item) {
      let list = this._list;
      let index = list.indexOf(item);
      if (0 <= index) list.slice(index, 1);
    }
  });

  let list = List();
  test.assert(!('_list' in list), 'no privates on instance');
  test.assert(
    !('_list' in List.prototype),
    'no privates on prototype'
  );
};

exports['test:public API can access private API'] = function(test) {
  const List = Trait.compose({
    // private API:
    _list: null,
    // public API
    constructor: function List() {
      this._list = [];
    },
    get length() this._list.length,
    add: function add(item) this._list.push(item),
    remove: function remove(item) {
      let list = this._list;
      let index = list.indexOf(item);
      if (0 <= index) list.slice(index, 1);
    }
  });
  let list = List();

  list.add('test');

  test.assertEqual(
    1,
    list.length,
    'should be able to add element and access it from public getter'
  );
};

exports['test:required'] = function(test) {
  const Enumerable = Trait.compose({
    list: Trait.required,
    forEach: function forEach(consumer) {
      return this.list.forEach(consumer);
    }
  });

  try {
    let i = Enumerable();
    test.fail('should throw when creating instance with required properties');
  } catch(e) {
    test.assertEqual(
      'Error: Missing required property: list',
      e.toString(),
      'required prop error'
    );
  }
};

exports['test:compose with required'] = function(test) {
  const List = Trait.compose({
    // private API:
    _list: null,
    // public API
    constructor: function List() {
      this._list = [];
    },
    get length() this._list.length,
    add: function add(item) this._list.push(item),
    remove: function remove(item) {
      let list = this._list;
      let index = list.indexOf(item);
      if (0 <= index) list.slice(index, 1);
    }
  });

  const Enumerable = Trait.compose({
    list: Trait.required,
    forEach: function forEach(consumer) {
      return this.list.forEach(consumer);
    }
  });

  const EnumerableList = Enumerable.compose({
    get list() this._list.slice(0)
  }, List);

  let array = [1,2, 'ab']
  let l = EnumerableList(array);
  array.forEach(function(element) l.add(element));
  let number = 0;
  l.forEach(function(element, index) {
    number ++;
    test.assertEqual(array[index], element, 'should mach array element')
  });
  test.assertEqual(
    array.length,
    number,
    'should perform as many asserts as elements in array'
  );
};

exports['test:resolve'] = function(test) {
  const List = Trait.compose({
    // private API:
    _list: null,
    // public API
    constructor: function List() {
      this._list = [];
    },
    get length() this._list.length,
    add: function add(item) this._list.push(item),
    remove: function remove(item) {
      let list = this._list;
      let index = list.indexOf(item);
      if (0 <= index) list.slice(index, 1);
    }
  });

  const Range = List.resolve({
    constructor: null,
    add: '_add',
  }).compose({
    min: null,
    max: null,
    get list() this._list.slice(0),
    constructor: function Range(min, max) {
      this.min = min;
      this.max = max;
      this._list = [];
    },
    add: function(item) {
      if (item <= this.max && item >= this.min)
        this._add(item)
    }
  });

  let r = Range(0, 10);

  test.assertEqual(
    0,
    r.min,
    'constructor must have set min'
  );
  test.assertEqual(
    10,
    r.max,
    'constructor must have set max'
  );

  test.assertEqual(
    0,
    r.length,
    'should not contain any elements'
  );

  r.add(5);

  test.assertEqual(
    1,
    r.length,
    'should add `5` to list'
  );

  r.add(12);

  test.assertEqual(
    1,
    r.length,
    'should not add `12` to list'
  );
};

exports['test:custom iterator'] = function(test) {
  let Sub = Trait.compose({
    foo: "foo",
    bar: "bar",
    baz: "baz",
    __iterator__: function() {
      yield 1;
      yield 2;
      yield 3;
    }
  });

  let (i = 0, sub = Sub()) {
    for (let item in sub)
    test.assertEqual(++i, item, "iterated item has the right value");
  };
};


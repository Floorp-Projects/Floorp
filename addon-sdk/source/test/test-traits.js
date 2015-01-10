/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Trait } = require('sdk/deprecated/traits');

exports['test:simple compose'] = function(assert) {
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

  assert.notEqual(undefined, List, 'should not be undefined');
  assert.equal('function', typeof List, 'type should be function');
  assert.equal(
    Trait.compose,
    List.compose,
    'should inherit static compose'
  );
  assert.equal(
    Trait.override,
    List.override,
    'should inherit static override'
  );
  assert.equal(
    Trait.required,
    List.required,
    'should inherit static required'
  );
  assert.equal(
    Trait.resolve,
    List.resolve,
    'should inherit static resolve'
  );

  assert.ok(
    !('_list' in List.prototype),
    'should not expose private API'
  );
}
exports['test: compose trait instance and create instance'] = function(assert) {
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

  assert.equal('object', typeof list, 'should return an object')
  assert.equal(
    true,
    list instanceof List,
    'should be instance of a List'
  );

  assert.equal(
    undefined,
    list._privateMember,
    'instance should not expose private API'
  );

  assert.equal(
    true,
    list.privateMember,
    'privates are accessible by  public API'
  );

  list._privateMember = false;

  assert.equal(
    true,
    list.privateMember,
    'property changes on instance must not affect privates'
  );

  assert.ok(
    !('_list' in list),
    'instance should not expose private members'
  );

  assert.equal(
    true,
    list.publicMember,
    'public members are exposed'
  )
  assert.equal(
    'function',
    typeof list.add,
    'should be function'
  )
  assert.equal(
    'function',
    typeof list.remove,
    'should be function'
  );

  list.add(1);
  assert.equal(
    1,
    list.list[0],
    'exposed public API should be able of modifying privates'
  )
};


exports['test:instances must not be hackable'] = function(assert) {
  let SECRET = 'There is no secret!',
      secret = null;

  let Class = Trait.compose({
    _secret: null,
    protect: function(data) this._secret = data
  });

  let i1 = Class();
  i1.protect(SECRET);

  assert.equal(
    undefined,
    (function() this._secret).call(i1),
    'call / apply can\'t access private state'
  );

  let proto = Object.getPrototypeOf(i1);
  try {
    proto.reveal = function() this._secret;
    secret = i1.reveal();
  } catch(e) {}
  assert.notEqual(
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
  assert.notEqual(
    SECRET,
    secret,
    'Object.prototype changes must not affect instances'
  );
}

exports['test:instanceof'] = function(assert) {
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

  assert.ok(List() instanceof List, 'Must be instance of List');
  assert.ok(new List() instanceof List, 'Must be instance of List');
};

exports['test:privates are unaccessible'] = function(assert) {
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
  assert.ok(!('_list' in list), 'no privates on instance');
  assert.ok(
    !('_list' in List.prototype),
    'no privates on prototype'
  );
};

exports['test:public API can access private API'] = function(assert) {
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

  assert.equal(
    1,
    list.length,
    'should be able to add element and access it from public getter'
  );
};

exports['test:required'] = function(assert) {
  const Enumerable = Trait.compose({
    list: Trait.required,
    forEach: function forEach(consumer) {
      return this.list.forEach(consumer);
    }
  });

  try {
    let i = Enumerable();
    assert.fail('should throw when creating instance with required properties');
  } catch(e) {
    assert.equal(
      'Error: Missing required property: list',
      e.toString(),
      'required prop error'
    );
  }
};

exports['test:compose with required'] = function(assert) {
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
    assert.equal(array[index], element, 'should mach array element')
  });
  assert.equal(
    array.length,
    number,
    'should perform as many asserts as elements in array'
  );
};

exports['test:resolve'] = function(assert) {
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

  assert.equal(
    0,
    r.min,
    'constructor must have set min'
  );
  assert.equal(
    10,
    r.max,
    'constructor must have set max'
  );

  assert.equal(
    0,
    r.length,
    'should not contain any elements'
  );

  r.add(5);

  assert.equal(
    1,
    r.length,
    'should add `5` to list'
  );

  r.add(12);

  assert.equal(
    1,
    r.length,
    'should not add `12` to list'
  );
};

exports['test:custom iterator'] = function(assert) {
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

  let i = 0, sub = Sub();
  for (let item in sub) {
    assert.equal(++i, item, "iterated item has the right value");
  }
};

require('sdk/test').run(exports);

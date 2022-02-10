// |reftest| 

// Returns the argument in the constructor to allow stamping private fields into
// arbitrary objects.
class OverrideBase {
  constructor(o) {
    return o;
  }
};

class A extends OverrideBase {
  #a = 1;
  g() {
    return this.#a
  }

  static gs(o) {
    return o.#a;
  }
  static inca(obj) {
    obj.#a++;
  }
}

var obj = {};
Object.seal(obj);
new A(obj);  // Add #a to obj, but not g.
assertEq('g' in obj, false);
assertEq(A.gs(obj), 1);
A.inca(obj);
assertEq(A.gs(obj), 2);

// Ensure that the object remains non-extensible
obj.h = 'hi'
assertEq('h' in obj, false);


Object.freeze(obj);
A.inca(obj);  // Despite being frozen, private names are modifiable.
assertEq(A.gs(obj), 3);
assertEq(Object.isFrozen(obj), true);

var proxy = new Proxy({}, {});
assertEq(Object.isFrozen(proxy), false);

new A(proxy);
assertEq(A.gs(proxy), 1);

// Note: this doesn't exercise the non-native object
// path in TestIntegrityLevel like you might expect.
//
// For that see below.
Object.freeze(proxy);
assertEq(Object.isFrozen(proxy), true);

A.inca(proxy);
assertEq(A.gs(proxy), 2)

var target = { a: 10 };
Object.freeze(target);
new A(target);
assertEq(Object.isFrozen(target), true)

var getOwnKeys = [];
var proxy = new Proxy(target, {
  getOwnPropertyDescriptor: function (target, key) {
    getOwnKeys.push(key);
    return Reflect.getOwnPropertyDescriptor(target, key);
  },
});

Object.isFrozen(proxy);
assertEq(getOwnKeys.length, 1);

if (typeof reportCompare === 'function') reportCompare(0, 0);

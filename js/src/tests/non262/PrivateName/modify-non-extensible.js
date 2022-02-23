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

if (typeof reportCompare === 'function') reportCompare(0, 0);

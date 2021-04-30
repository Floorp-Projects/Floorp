// |reftest|
//
// Ensure PrivateNames aren't iterable.

class O {
  #x = 123;
  gx() {
    return this.#x;
  }
}
var o = new O;

assertEq(o.gx(), 123);

assertEq(Object.keys(o).length, 0);
assertEq(Object.getOwnPropertyNames(o).length, 0);
assertEq(Object.getOwnPropertySymbols(o).length, 0);
assertEq(Reflect.ownKeys(o).length, 0);

var forIn = [];
for (var pk in o) {
  forIn.push(pk);
}
assertEq(forIn.length, 0);

// Proxy case
var proxy = new Proxy(o, {});
assertEq(Object.keys(proxy).length, 0);
assertEq(Object.getOwnPropertyNames(proxy).length, 0);
assertEq(Object.getOwnPropertySymbols(proxy).length, 0);
assertEq(Reflect.ownKeys(proxy).length, 0);

for (var pk in proxy) {
  forIn.push(pk);
}
assertEq(forIn.length, 0);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);

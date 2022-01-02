// |reftest| 

class A {
  #x = 10;
  g() {
    return this.#x;
  }
};

var p = new Proxy(new A, {});
var completed = false;
try {
  p.g();
  completed = true;
} catch (e) {
  assertEq(e instanceof TypeError, true);
}
assertEq(completed, false);


if (typeof reportCompare === 'function') reportCompare(0, 0);

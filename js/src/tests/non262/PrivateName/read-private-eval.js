// |reftest| skip-if(!xulRuntime.shell) shell-option(--enable-private-fields)

class A {
  #x = 14;
  g() {
    return eval('this.#x');
  }
}

a = new A;
assertEq(a.g(), 14);

if (typeof reportCompare === 'function') reportCompare(0, 0);
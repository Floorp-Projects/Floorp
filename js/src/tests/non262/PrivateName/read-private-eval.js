// |reftest| shell-option(--enable-private-fields) skip-if(!xulRuntime.shell) -- requires shell-option

class A {
  #x = 14;
  g() {
    return eval('this.#x');
  }
}

a = new A;
assertEq(a.g(), 14);

if (typeof reportCompare === 'function') reportCompare(0, 0);
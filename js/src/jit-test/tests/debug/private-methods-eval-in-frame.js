// |jit-test| --enable-private-fields; --enable-private-methods;
load(libdir + 'asserts.js');
load(libdir + 'evalInFrame.js');

class B {
  #priv() {
    return 12;
  }

  #val = '';
  set #x(x) {
    this.#val = x + ' haha';
  }
  get #x() {
    return this.#val;
  }

  ef(str) {
    return evalInFrame(0, str);
  }
}

var b = new B();
assertEq(b.ef(`this.#priv();`), 12);
assertEq(b.ef(`this.#x = 'Hi'; this.#x`), 'Hi haha');

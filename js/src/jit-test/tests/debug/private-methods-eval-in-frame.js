// |jit-test| --enable-private-fields; --enable-private-methods;
load(libdir + 'asserts.js');
load(libdir + 'evalInFrame.js');

class B {
  #priv() {
    return 12;
  }

  #privF = () => { return 12; }

  callPriv() {
    return this.#priv();
  }

  callPrivF() {
    return this.#privF();
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

  callFunc(f) {
    f();
  }

}

var b = new B();

assertEq(b.ef("this.callPriv()"), 12);
assertEq(b.ef("this.callPrivF()"), 12);
assertThrowsInstanceOf(() => b.ef(`this.callFunc(() => { this.#privF(); })`), Error);

// See EmitterScope::lookupPrivate for the reasoning here.
assertThrowsInstanceOf(() => b.ef(`this.#privF();`), Error);
assertThrowsInstanceOf(() => b.ef(`var x = () => { return this.#privF(); } x();`), Error);
assertThrowsInstanceOf(() => b.ef(`this.#priv();`), Error);
assertThrowsInstanceOf(() => b.ef(`var x = () => { return this.#priv(); } x();`), Error);
assertThrowsInstanceOf(() => b.ef(`this.#x = 'Hi'; this.#x`), Error);
assertThrowsInstanceOf(() => b.ef(`var x = () => { this.#x = 'Hi'; return this.#x} x();`), Error);

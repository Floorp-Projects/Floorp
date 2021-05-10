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

  #val = 'constructed';
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
    return f();
  }

}

var b = new B();

assertEq(b.ef("this.callPriv()"), 12);
assertEq(b.ef("this.callPrivF()"), 12);

// Private fields don't need brand checking and should succeed.
assertEq(b.ef("this.#val"), 'constructed')

// PrivF is a field, not a method, and so isn't brand checked like a method.
assertEq(b.ef(`this.callFunc(() => { return this.#privF() })`), 12);
assertEq(b.ef(`this.#privF()`), 12);

// Accesors are stored like fields, and so successfully execute, as they don't
// need brand checking.
assertEq(b.ef(`this.#x = 'Bye'; this.#x`), 'Bye haha');
assertEq(b.ef(`var x = () => { this.#x = 'Hi'; return this.#x}; x()`), 'Hi haha');

// These require brand checking, and some new development to support.
// See EmitterScope::lookupPrivate for the reasoning here.
assertThrowsInstanceOf(() => b.ef(`this.#priv()`), Error);
assertThrowsInstanceOf(() => b.ef(`var x = () => { return this.#priv(); } x();`), Error);
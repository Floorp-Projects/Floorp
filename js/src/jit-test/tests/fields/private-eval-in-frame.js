load(libdir + 'asserts.js');
load(libdir + 'evalInFrame.js');

class B {
  #x = 12;
  x = 'his';
  ef(str) {
    return evalInFrame(0, str);
  }
}

var b = new B();
assertEq(b.ef(`this.x`), 'his');
assertThrowsInstanceOf(() => b.ef(`this.#x`), Error);
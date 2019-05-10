// |jit-test| --enable-experimental-fields

load(libdir + "asserts.js");

new class foo extends Array {
  e = function() {}
}

source = `new class bar extends Promise { e = function() {} }`;
// Calling the Promise constructor with empty args fails with TypeError.
assertThrowsInstanceOf(() => eval(source), TypeError);

class Base {
  constructor() {
    return new Proxy({}, {});
  }
}

new class prox extends Base {
  e = function () {}
}

// |jit-test| error:ReferenceError
test();
function test() {
  (arguments);
  F.prototype = new F();
}

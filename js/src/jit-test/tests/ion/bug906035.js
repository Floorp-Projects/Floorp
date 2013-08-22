// |jit-test| ion-eager
function y() { return "foo,bar"; }
function x() {
  var z = y().split(',');
  for (var i = 0; i < z.length; i++) {}
}
gczeal(2);
Object.prototype.length = function () {};
x();


function s(f) { this._m = f; }
function C() {
    Object.defineProperty(this, "m", {set: s});
    this.m = function () {};
}
var last = {};
for (var i = 0; i < 20; i++) {
  var a = new C;
  assertEq(a._m === last._m, false);
  last = a;
}

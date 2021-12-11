// Binary: cache/js-dbg-32-33f1ad45ccb8-linux
// Flags: -m -n
//
function f() {
  var N = 624;
  this.init_genrand = function(s) {
    for (z = 1; z < N; z++) {}
  };
}(function() {
  new f;
}());
function g(o) {
  var props = Object.getOwnPropertyNames(o);
  var prop = props[props.length - 1] + "p"
  o[prop] = Number.prototype.__proto__;
}
g(Number.prototype.__proto__);

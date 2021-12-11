// Binary: cache/js-dbg-64-1210706b4576-linux
// Flags:
//
load(libdir + "immutable-prototype.js");

if (globalPrototypeChainIsMutable()) {
  this.__proto__ = null;
  Object.prototype.__proto__ = this;
}

function exploreProperties(obj) {
  var props = [];
  for (var o = obj; o; o = Object.getPrototypeOf(o)) {
    props = props.concat(Object.getOwnPropertyNames(o));
  }
  for (var i = 0; i < props.length; ++i) {
    var p = props[i];
    var v = obj[p];
  }
}
var c = [{}];
exploreProperties(c);

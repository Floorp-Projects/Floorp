// throw, don't crash

var expect = "TypeError: property descriptor's setter field is neither undefined nor a function";
var actual = "";

try {

(x = Proxy.createFunction((function() {
  return {
    defineProperty: function(name, desc) {
      Object.defineProperty(x, name, desc)
    },
  }
})(), (eval)));
Object.defineProperty(x, "", ({
  get: function() {}
}))

} catch (e) {
    actual = '' + e;
}

assertEq(expect, actual);

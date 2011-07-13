// throw, don't crash

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

assertEq(actual, "InternalError: too much recursion");

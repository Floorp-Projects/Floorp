o14 = [undefined].__proto__
function f18() {
  try {
    [] = o[p]
  } catch (e) {}
}
for (var i; i < 20; i++) {
  ({
    x: function() {
      return eval("o14")
    }
  }.x().__proto__ = null);
  f18()
}

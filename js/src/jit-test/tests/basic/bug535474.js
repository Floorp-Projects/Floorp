function f() {
  var _76 = {};
  for (var i = 0; i < arguments.length; i++) {
    var typ = arguments[i];
    _76[typ] = typ;
  }
  return function () {
    for (var i = 0; i < arguments.length; i++) {
      if (!(typeof (arguments[i]) in _76)) {
        return false;
      }
    }
    return true;
  }
}

g = f("number", "boolean", "object");

g("a", "b", "c", "d", "e", "f", 2);
g(2, "a", "b", "c", "d", "e", "f", 2);

/*
 * Don't assert --
 * Assertion failed: frame entry -4 wasn't freed
 * : _activation.entry[i] == 0 (../nanojit/Assembler.cpp:786)
 */


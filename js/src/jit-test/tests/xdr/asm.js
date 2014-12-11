load(libdir + 'bytecode-cache.js');

var test = (function () {
  function f() {
    var x = function inner() {
	"use asm";
	function g() {}
	return g;
    };
  };
  return f.toSource();
})();

try {
  evalWithCache(test, {});
} catch (x) {
  assertEq(x.message.contains("AsmJS"), true);
  assertEq(x.message.contains("XDR"), true);
}

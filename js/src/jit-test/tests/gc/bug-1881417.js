for (let x = 0; x < 2; (function() { x++; })()) {};
function f() {
  var y = new (function () {})();
  (function () {
    Reflect.apply(y.toString, [], [0]);
  })();
}
f();
var z = [];
z.keepFailing = [];
oomTest(f, z);
dumpHeap();

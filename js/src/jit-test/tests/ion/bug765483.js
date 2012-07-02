// |jit-test| error: function can be called only in debug mode
var obj1 = {}, obj2 = {};
obj2['b'+i] = 0;
for (var k in obj2) {
  (function g() { evalInFrame(1, "assertStackIs(['eval-code', f, 'bound(f)', 'global-code'])", true); })();
}
for (var i = 0; i != array.length; ++i)
  array[i]();

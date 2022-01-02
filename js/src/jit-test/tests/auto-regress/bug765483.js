// |jit-test| error:ReferenceError;

// Binary: cache/js-dbg-64-de23a9fc29db-linux
// Flags: --ion-eager
//

load(libdir + "evalInFrame.js");

var obj1 = {}, obj2 = {};
obj2['b'+i] = 0;
for (var k in obj2) {
  (function g() { evalInFrame(1, "assertStackIs(['eval-code', f, 'bound(f)', 'global-code'])", true); })();
}
for (var i = 0; i != array.length; ++i)
  array[i]();

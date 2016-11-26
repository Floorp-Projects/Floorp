// Binary: cache/js-dbg-64-c9212eb6175b-linux
// Flags:
//
function f(a) {
   function g() {
       yield function () { return a; };
   }
   return g();
}
var x;
f(7).next()();

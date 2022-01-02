// |jit-test| error:TypeError

(function() {
   let r
   g = function(x) {
     ((-0x80000000 + (x >>> 0)) != 0) ? 0 : x()
   }
})()

g(NaN)
g(0x80000000);

setJitCompilerOption('baseline.usecount.trigger', 1);
let r;
(function() {
     function f() {
         return (1 + -1 / 0) << null;
     }
     assertEq(f(), 0);
     assertEq(f(), 0);

     function g(x,y) {
         var a = x|0;
         var b = y|0;
         return (a / b + a / b) | 0;
     }
     assertEq(g(3,4), 1);
     assertEq(g(3,4), 1);
})();

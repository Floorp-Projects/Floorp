var iters = 250;

// Generate a deeply nested version of:
//   function outer() {
//     var top_level_var = 42;
//     let x3 = 0;
//     function f2() {
//       let x2 = x3;
//       function f1() {
//         let x1 = x2;
//         function f0() {
//           let x0 = x1;
//           return top_level_var + x0;
//         }
//         return f0();
//       }
//       return f1();
//     }
//     return f2();
//   }

var src = "return top_level_var + x0; "

for (var i = 0; i < iters; i++) {
  var def = "let x" + i + " = x" + (i+1) + "; ";
  src = "function f" + i + "() { " + def + src + "} return f" + i + "();"
}
src = "let x" + iters + " = 0;" + src;
src = "var top_level_var = 42; " + src;

var outer = Function(src);
for (var i = 0; i < 2; i++) {
  assertEq(outer(), 42);
}

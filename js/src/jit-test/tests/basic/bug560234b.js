function f(a) {
   function* g() {
       yield function () { return a; };
   }
   if (a == 8)
       return g();
   a = 3;
}
var x;
for (var i = 0; i < 9; i++)
   x = f(i);
x.next().value();  // ReferenceError: a is not defined.

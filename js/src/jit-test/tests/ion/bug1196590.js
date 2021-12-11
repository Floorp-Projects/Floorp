
function bar(x, i) {
  if (i == 50)
    x.length = 0;
}
function foo(x, j, n) {
  for (var i = 0; i < n; i++) {
    bar(x, i);
  }
}
var a = foo([1,2,3,4], 3, 100);

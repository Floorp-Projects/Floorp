
function foo(x,n) {
  for (var i = -5; i < n; i++) {
    x[i] = 10;
  }
}
foo([1,2,3,4,5],5);

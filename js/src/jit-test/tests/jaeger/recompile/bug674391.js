a = [];
for (var i = 0; i < 1000; i++) {
  a[i] = i;
}
function foo(x) {
  for (var i in x) {
  }
}
schedulegc(100);
foo(a);

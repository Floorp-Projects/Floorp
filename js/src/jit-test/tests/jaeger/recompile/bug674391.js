a = [];
for (var i = 0; i < 1000; i++) {
  a[i] = i;
}
function foo(x) {
  for (var i in x) {
  }
}
if (typeof schedulegc != "undefined")
  schedulegc(100);
foo(a);

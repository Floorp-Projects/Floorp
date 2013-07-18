
var arr = new Int8Array(100);
function f(a) {
  for(var i=0; i<30; i++) {
    x = a[85.3];
  }
}
f(arr);

var buf = serialize(new Date(NaN));
var n =  -(8.64e15 + 1);
var nbuf = serialize(n);
for (var j = 0; j < 8; j++)
  buf[j + (0.00000000123)] = nbuf[j];

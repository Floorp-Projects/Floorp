var a = Boolean();
false.__proto__;
var c = ({}).__proto__;
var d = {};
gc()
c[0] = d;
for (var x in a) {
  a[x];
}

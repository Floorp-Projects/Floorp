// Binary: cache/js-dbg-32-c5b90ea7e475-linux
// Flags: -m -n -a
//

var save__proto__ = __proto__;
__proto__ = save__proto__;
function bar(x, y) {
  return x + y;
}
function foo(x, y) {
  var a = 0;
  for (var i = 0; i < 1000; i++) {
    a += (this.toString);
    a += bar(x, y);
    a = bar(x, (a));
    a += bar(x, y);
  }
  return a;
}
var q = foo(0, 1);
print(q.toSource());

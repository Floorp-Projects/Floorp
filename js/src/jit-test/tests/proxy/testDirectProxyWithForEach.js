// |jit-test| need-for-each

var proxy = new Proxy(['a', 'b', 'c'], {});
var a = [];
for each (x in proxy) {
  a.push(x);
}
assertEq(a.toString(), 'a,b,c');

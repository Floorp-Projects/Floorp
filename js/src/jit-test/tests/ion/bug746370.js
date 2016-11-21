// |jit-test| need-for-each

var a = ['p', 'q', 'r', 's', 't'];
var o = {p:1, q:2, r:(1), s:4, t:5};
for (var i in o) {
    delete o.p;
}
for each (var i in a)
  assertEq(o.hasOwnProperty(i), i != 'p');

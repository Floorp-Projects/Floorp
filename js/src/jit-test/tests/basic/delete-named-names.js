// |jit-test| need-for-each

var a = ['p', 'q', 'r', 's', 't'];
var o = {p:1, q:2, r:3, s:4, t:5};
for (var i in o) {
    delete o.p;
    delete o.q;
    delete o.r;
    delete o.s;
    delete o.t;
}
for each (var i in a)
    assertEq(o.hasOwnProperty(i), false);


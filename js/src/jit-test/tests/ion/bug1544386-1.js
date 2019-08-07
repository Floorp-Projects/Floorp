const arr = [{a: 0}, {a: 1}, {a: 2}, {a: 3}, {a: 4}];
function f() {
    if (arr.length == 0) {
        arr[3] = {a: 5};
    }
    var v = arr.pop();
    v.a;
    for (var i = 0; i < 3000; i++) {}
}
var p = {};
p.__proto__ = [{a: 0}, {a: 1}, {a: 2}];
p[0] = -1.8629373288622089e-06;
arr.__proto__ = p;
for (var i = 0; i < 10; i++) {
    f();
}

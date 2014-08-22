
function f() {}
function g() {
    f.apply(this, arguments);
}

var arr = [];
for (var j = 0; j < 128 /* 127 */; j++)
    arr.push(0);

for (var j = 0; j < 10000; j++)
    g.apply(null, arr);

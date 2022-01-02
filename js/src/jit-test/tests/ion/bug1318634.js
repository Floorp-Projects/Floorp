var o1 = {get x() { return 1; }};
var o2 = {get x() { return 2; }};
var o3 = Object.create(o1);

function f(o) {
    return o.x;
}

var res = 0;
for (var i=0; i<15; i++) {
    res += f(o3);
    res += f(o2);
}

o1.y = 1;

for (var i=0; i<110; i++)
    res += f(o2);

assertEq(res, 265);

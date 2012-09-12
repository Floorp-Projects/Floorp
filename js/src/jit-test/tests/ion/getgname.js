var x = 13;

function ReturnArray() {
    return Array;
}
function ReturnObject() {
    return Object;
}
function ReturnX() {
    return x;
}

y = null;
function ReturnY() {
    return y;
}
z = "3";
z = null;
function ReturnZ() {
    return z;
}

for (var i = 0; i < 100; i++)
    ReturnArray();
for (var i = 0; i < 100; i++)
    ReturnX();
for (var i = 0; i < 100; i++)
    ReturnZ();

gc();

assertEq(ReturnArray(), Array);
assertEq(ReturnObject(), Object);
assertEq(ReturnX(), 13);
assertEq(ReturnY(), null);
assertEq(ReturnZ(), null);


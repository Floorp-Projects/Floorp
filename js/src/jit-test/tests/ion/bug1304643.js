
var x = Object.create(this);
var y = '1';
for (var i = 0; i < 3; ++i) {
        y += x.y;
}
assertEq(y, "11111111");

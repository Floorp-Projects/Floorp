var g = newGlobal({sameCompartmentAs: this});

var o1 = Array(1, 2);
var o2 = g.Array(1, 2);
Array.prototype.x = 10;

function test(o, v) {
    for (var i = 0; i < 15; i++) {
        assertEq(o.x, v);
    }
}
test(o1, 10);
test(o2, undefined);

function test1() {
    var o1 = {x: 1, y: 2, z: 3};
    var o2 = Object.create(o1);
    var o3 = Object.create(o2);
    o2.x = 2; // Ensure teleporting is invalidated for o1.
    for (var i = 0; i < 30; i++) {
        assertEq(o3.y, i > 20 ? -1 : 2);
        if (i === 20) {
            // Add a (second) shadowing property to o2. The property access
            // above must detect this properly.
            o2.y = -1;
        }
    }
}
test1();

function test2() {
    var o1 = {x: 1, y: 2, z: 3};
    var o2 = Object.create(o1);
    var o3 = Object.create(o2);
    var o4 = Object.create(o3);
    o2.x = 1;
    o2.a = 2;
    o3.a = 2;
    for (var i = 0; i < 30; i++) {
        assertEq(o4.y, i > 20 ? undefined : 2);
        if (i === 20) {
            o2.__proto__ = null;
        }
    }
}
test2();

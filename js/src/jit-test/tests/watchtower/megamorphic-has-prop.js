function hasOwnProp(o, p) {
    return Object.prototype.hasOwnProperty.call(o, p);
}
function testHasProp() {
    var C = Object.create(null);
    C.protox = 0;
    var B = Object.create(C);
    var objs = [];
    for (var i = 0; i < 50; i++) {
        var A = Object.create(B);
        A["x" + i] = 0;
        A.ownx = 3;
        objs.push(A);
    }
    for (var i = 0; i < 10; i++) {
        for (var j = 0; j < objs.length; j++) {
            var obj = objs[j];
            assertEq(hasOwnProp(obj, "protox"), false);
            assertEq("protox" in obj, true);
            assertEq(obj.protox, 0);

            assertEq(hasOwnProp(obj, "ownx"), true);
            assertEq("ownx" in obj, true);
            assertEq(obj.ownx, 3);

            assertEq("missing" in obj, false);
            assertEq(hasOwnProp(obj, "missing"), false);
            assertEq(obj.missing, undefined);
        }
    }
}
testHasProp();


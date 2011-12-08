// for-in loops on Maps and Sets enumerate properties.

var test = function test(obj) {
    assertEq(Object.keys(obj).length, 0);

    var i = 0, v;
    for (v in obj)
        i++;
    assertEq(i, 0);

    obj.ownProp = 1;
    assertEq(Object.keys(obj).join(), "ownProp");

    for (v in obj)
        i++;
    assertEq(i, 1);
    assertEq(v, "ownProp");

    delete obj.ownProp;
};

test(Map.prototype);
test(new Map);
test(Set.prototype);
test(new Set);

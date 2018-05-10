function noElement() {
    for (var i = 0; i < 1e4; i++) {
        var obj = {length: 0};
        assertEq(obj[0], undefined);
    }
}

function noElementCheckPrototype() {
    for (var i = 0; i < 1e4; i++) {
        var obj = {length: 0};
        assertEq(obj[0], i <= 1e3 ? undefined : 1);
        if (i == 1e3) {
            Object.prototype[0] = 1;
        }
    }
    delete Object.prototype[0];
}

function elementOnPrototype() {
    Object.prototype[0] = 3;
    for (var i = 0; i < 1e4; i++) {
        var obj = {length: 0};
        assertEq(obj[0], 3);
    }
    delete Object.prototype[0];
}

function checkExpando() {
    for (var i = 0; i < 1e4; i++) {
        var obj = {length: 0};
        if (i >= 1e3) {
            obj[0] = 2;
        }
        assertEq(obj[0], i < 1e3 ? undefined : 2);
    }
}

noElement();
noElementCheckPrototype();
elementOnPrototype();
checkExpando();

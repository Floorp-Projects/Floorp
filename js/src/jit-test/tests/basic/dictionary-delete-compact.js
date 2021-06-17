// Stress test dictionary object/map property deletion/addition/compaction.

const numProps = 1000;

// Delete a range of properties and check correctness.
function deleteRange(deleteStart, deleteEnd) {
    for (var i = 0; i < numProps; i++) {
        o["x" + i] = i;
    }
    for (var i = deleteStart; i < deleteEnd; i++) {
        delete o["x" + i];
    }
    assertEq(Object.getOwnPropertyNames(o).length,
             numProps - (deleteEnd - deleteStart));
    for (var i = 0; i < numProps; i++) {
        if (deleteStart <= i && i < deleteEnd) {
            assertEq(("x" + i) in o, false);
        } else {
            assertEq(o["x" + i], i);
        }
    }
}

// For every "stride" properties, delete all of them except one.
function deleteMany(stride) {
    for (var i = 0; i < numProps; i++) {
        o["x" + i] = i;
    }
    var propsNotDeleted = 0;
    for (var i = 0; i < numProps; i++) {
        if ((i % stride) === 1) {
            propsNotDeleted++;
            continue;
        }
        delete o["x" + i];
    }
    assertEq(Object.getOwnPropertyNames(o).length, propsNotDeleted);
    for (var i = 0; i < numProps; i++) {
        if ((i % stride) !== 1) {
            assertEq(("x" + i) in o, false);
        } else {
            assertEq(o["x" + i], i);
        }
    }
}

var o = {};

function test(useFreshObject) {
    function testOne(f) {
        if (useFreshObject) {
            o = {};
        }
        f();
    }

    for (var i = 6; i < 12; i++) {
        testOne(_ => deleteRange(i, 1000));
    }
    testOne(_ => deleteRange(100, 1000));
    testOne(_ => deleteRange(0, 1000));
    testOne(_ => deleteRange(1, 1000));
    testOne(_ => deleteRange(8, 990));

    testOne(_ => deleteMany(3));
    testOne(_ => deleteMany(7));
    testOne(_ => deleteMany(8));
    testOne(_ => deleteMany(15));
    testOne(_ => deleteMany(111));
}

test(true);
o = {};
test(false);

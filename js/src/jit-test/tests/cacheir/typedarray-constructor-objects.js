function testArrayBufferThenOther() {
    var buf = new ArrayBuffer(4);
    var other = [1, 2];
    for (var i = 0; i < 150; i++) {
        var arg = i < 100 ? buf : other;
        var ta = new Int32Array(arg);
        assertEq(ta.length, arg === buf ? 1 : 2);
    }
}
testArrayBufferThenOther();

function testSharedArrayBufferThenOther() {
    if (!this.SharedArrayBuffer) {
        return;
    }
    var buf = new SharedArrayBuffer(4);
    var other = [1, 2];
    for (var i = 0; i < 150; i++) {
        var arg = i < 100 ? buf : other;
        var ta = new Int32Array(arg);
        assertEq(ta.length, arg === buf ? 1 : 2);
    }
}
testSharedArrayBufferThenOther();

function testArrayThenArrayBuffer() {
    var arr = [1, 2, 3];
    var buf = new ArrayBuffer(5);
    for (var i = 0; i < 150; i++) {
        var arg = i < 100 ? arr : buf;
        var ta = new Int8Array(arg);
        assertEq(ta.length, arg === arr ? 3 : 5);
    }
}
testArrayThenArrayBuffer();

function testArrayThenSharedArrayBuffer() {
    if (!this.SharedArrayBuffer) {
        return;
    }
    var arr = [1, 2, 3];
    var buf = new SharedArrayBuffer(5);
    for (var i = 0; i < 150; i++) {
        var arg = i < 100 ? arr : buf;
        var ta = new Int8Array(arg);
        assertEq(ta.length, arg === arr ? 3 : 5);
    }
}
testArrayThenSharedArrayBuffer();

function testArrayThenWrapper() {
    var arr = [1, 2, 3];
    var wrapper = newGlobal({newCompartment: true}).evaluate("[1, 2]");
    for (var i = 0; i < 150; i++) {
        var arg = i < 100 ? arr : wrapper;
        var ta = new Int8Array(arg);
        assertEq(ta.length, arg === arr ? 3 : 2);
    }
}
testArrayThenWrapper();

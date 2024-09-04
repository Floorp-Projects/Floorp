/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// The tests expect the clone buffer to be collected at a certain point.
// I don't think collecting it sooner would break anything, but just in case:
gczeal(0);

function testBasic() {
    const desc = "basic canTransfer, writeTransfer, freeTransfer";
    const BASE = 100;
    const obj = makeSerializable(BASE + 1);
    let s = serialize(obj, [obj]);
    assertEq("" + obj.log, "" + [101, "?", 101, "W"], "serialize " + desc);
    s = null;
    gc();
    print(`serialize ${arguments.callee.name}: ${obj.log}`);
    assertEq("" + obj.log, "" + [
        // canTransfer(obj) then writeTransfer(obj), then obj is read with
        // a backreference.
        101, "?", 101, "W",
        // Then discard the serialized data, triggering freeTransfer(obj).
        101, "F"
    ], "serialize " + desc);
    obj.log = null;
}

function testErrorDuringWrite() {
    const desc = "canTransfer, write=>error";
    const BASE = 200;
    const obj = makeSerializable(BASE + 1);
    const ab = new ArrayBuffer(100);
    detachArrayBuffer(ab);
    try {
        serialize([obj, ab], [obj]);
    } catch (e) {
    }
    gc();
    print(`${arguments.callee.name}: ${obj.log}`);
    assertEq("" + obj.log, "" + [201, "?"], desc);
    obj.log = null;
}

function testErrorDuringTransfer() {
    const desc = "canTransfer, write(ab), writeTransfer(obj), writeTransfer(ab)=>error, freeTransfer";
    const BASE = 300;
    const obj = makeSerializable(BASE + 1);
    const ab = new ArrayBuffer(100);
    detachArrayBuffer(ab);
    try {
        serialize([obj, ab], [obj, ab]);
    } catch (e) {
    }
    gc();
    print(`${arguments.callee.name}: ${obj.log}`);
    assertEq("" + obj.log, "" + [
        // canTransfer(obj) then writeTransfer(obj)
        301, "?", 301, "W",
        // error reading ab, freeTransfer(obj)
        301, "F"
    ], desc);
    obj.log = null;
}

function testMultiOkHelper(g, BASE, desc) {
    const obj = makeSerializable(BASE + 1);
    const obj2 = makeSerializable(BASE + 2);
    const obj3 = makeSerializable(BASE + 3);
    serialize([obj, obj2, obj3], [obj, obj3]);
    gc();
    print(`${arguments.callee.name}(${BASE}): ${obj.log}`);
    assertEq("" + obj.log, "" + [
        // canTransfer(obj then obj3).
        BASE + 1, "?", BASE + 3, "?",
        // write(obj2), which happens before transferring.
        BASE + 2, "w",
        // writeTransfer(obj then obj3).
        BASE + 1, "W", BASE + 3, "W",
        // discard the clone buffer without deserializing, so freeTransfer(obj1+obj3).
        BASE + 1, "F", BASE + 3, "F"
    ], desc);
    obj.log = null;
}

function testMultiOk() {
    const desc = "write 3 objects, transfer obj1 and obj3 only, write obj2";
    testMultiOkHelper(globalThis, 400, desc);
}

function testMultiOkCrossRealm() {
    const desc = "write 3 objects, transfer obj1 and obj2 only, cross-realm";
    testMultiOkHelper(newGlobal({ newCompartment: true }), 500, desc);
}

function printTrace(callee, global, base, log, phase = undefined) {
    phase = phase ? `${phase} ` : "";
    const test = callee.replace("Helper", "") + (global === globalThis ? "" : "CrossRealm");
    print(`${phase}${test}(${base}): ${log}`);
}

function testMultiOkWithDeserializeHelper(g, BASE, desc) {
    const obj = makeSerializable(BASE + 1);
    const obj2 = makeSerializable(BASE + 2);
    const obj3 = makeSerializable(BASE + 3);
    let s = serialize([obj, obj2, obj3], [obj, obj3]);
    gc();
    printTrace(arguments.callee.name, g, BASE, obj.log, "serialize");
    assertEq("" + obj.log, "" + [
        // canTransfer(obj+obj3).
        BASE + 1, "?", BASE + 3, "?",
        // write(obj2).
        BASE + 2, "w",
        // writeTransfer(obj1+obj3). All good.
        BASE + 1, "W", BASE + 3, "W",
        // Do not collect the clone buffer. It owns obj1+obj3 now.
    ], "serialize " + desc);
    obj.log = null;

    let clone = deserialize(s);
    s = null;
    gc();
    printTrace(arguments.callee.name, g, BASE, obj.log, "deserialize");
    assertEq("" + obj.log, "" + [
        // readTransfer(obj1+obj3).
        BASE + 1, "R", BASE + 3, "R",
        // read(obj2). The full clone is successful.
        BASE + 2, "r",
    ], "deserialize " + desc);
    obj.log = null;
}

function testMultiOkWithDeserialize() {
    const desc = "write 3 objects, transfer obj1 and obj3 only, write obj2, deserialize";
    testMultiOkWithDeserializeHelper(globalThis, 600, desc);
}

function testMultiOkWithDeserializeCrossRealm() {
    const desc = "write 3 objects, transfer obj1 and obj2 only, deserialize, cross-realm";
    testMultiOkWithDeserializeHelper(newGlobal({ newCompartment: true }), 700, desc);
}

function testMultiWithDeserializeReadTransferErrorHelper(g, BASE, desc) {
    const obj = makeSerializable(BASE + 1, 0);
    const obj2 = makeSerializable(BASE + 2, 1);
    const obj3 = makeSerializable(BASE + 3, 1);
    let s = serialize([obj, obj2, obj3], [obj, obj3]);
    gc();
    printTrace(arguments.callee.name, g, BASE, obj.log, "serialize");
    assertEq("" + obj.log, "" + [
        // Successful serialization.
        BASE + 1, "?", BASE + 3, "?",
        BASE + 2, "w",
        BASE + 1, "W", BASE + 3, "W",
    ], "serialize " + desc);
    obj.log = null;

    try {
        let clone = deserialize(s);
    } catch (e) {
        assertEq(e.message.includes("invalid transferable"), true);
    }

    try {
        // This fails without logging anything, since the re-transfer will be caught
        // by looking at its header before calling any callbacks.
        let clone = deserialize(s);
    } catch (e) {
        assertEq(e.message.includes("cannot transfer twice"), true);
    }

    s = null;
    gc();
    printTrace(arguments.callee.name, g, BASE, obj.log, "deserialize");
    assertEq("" + obj.log, "" + [
        // readTransfer(obj) then readTransfer(obj3) which fails.
        BASE + 1, "R", BASE + 3, "R",
        // obj2 has not been read at all because we errored out during readTransferMap(),
        // which comes before the main reading. obj transfer data is now owned by its
        // clone. obj3 transfer data was not successfully handed over to a new object,
        // so it is still owned by the clone buffer and must be discarded with freeTransfer.
        // 'F' means the data is freed.
        BASE + 3, "F",
    ], "deserialize " + desc);
    obj.log = null;
}

function testMultiWithDeserializeReadTransferError() {
    const desc = "write 3 objects, transfer obj1 and obj3 only, fail during readTransfer(obj3)";
    testMultiWithDeserializeReadTransferErrorHelper(globalThis, 800, desc);
}

function testMultiWithDeserializeReadTransferErrorCrossRealm() {
    const desc = "write 3 objects, transfer obj1 and obj2 only, fail during readTransfer(obj3), cross-realm";
    testMultiWithDeserializeReadTransferErrorHelper(newGlobal({ newCompartment: true }), 900, desc);
}

function testMultiWithDeserializeReadErrorHelper(g, BASE, desc) {
    const obj = makeSerializable(BASE + 1, 2);
    const obj2 = makeSerializable(BASE + 2, 2);
    const obj3 = makeSerializable(BASE + 3, 2);
    let s = serialize([obj, obj2, obj3], [obj, obj3]);
    gc();
    printTrace(arguments.callee.name, g, BASE, obj.log, "serialize");
    assertEq("" + obj.log, "" + [
        // Same as above. Everything is fine.
        BASE + 1, "?", BASE + 3, "?",
        BASE + 2, "w",
        BASE + 1, "W", BASE + 3, "W",
    ], "serialize " + desc);
    obj.log = null;

    try {
        let clone = deserialize(s);
    } catch (e) {
        assertEq(e.message.includes("Failed as requested"), true);
    }
    s = null;
    gc();
    printTrace(arguments.callee.name, g, BASE, obj.log, "deserialize");
    assertEq("" + obj.log, "" + [
        // The transferred objects will get restored via the readTransfer() hook
        // and thus will own their transferred data. They will get discarded during
        // the GC, but that doesn't call any hooks. When the GC collects `s`,
        // it will look for any transferrable data to release, but will
        // find that it no longer owns any such data and will do nothing.
        BASE + 1, "R", BASE + 3, "R",
        BASE + 2, "r",
    ], "deserialize " + desc);
    obj.log = null;
}

function testMultiWithDeserializeReadError() {
    const desc = "write 3 objects, transfer obj1 and obj3 only, fail during read(obj2)";
    testMultiWithDeserializeReadErrorHelper(globalThis, 1000, desc);
}

function testMultiWithDeserializeReadErrorCrossRealm() {
    const desc = "write 3 objects, transfer obj1 and obj2 only, fail during read(obj2), cross-realm";
    testMultiWithDeserializeReadErrorHelper(newGlobal({ newCompartment: true }), 1100, desc);
}

testBasic();
testErrorDuringWrite();
testErrorDuringTransfer();
testMultiOk();
testMultiOkCrossRealm();
testMultiOkWithDeserialize();
testMultiOkWithDeserializeCrossRealm();
testMultiWithDeserializeReadTransferError();
testMultiWithDeserializeReadTransferErrorCrossRealm();
testMultiWithDeserializeReadError();
testMultiWithDeserializeReadErrorCrossRealm();

let invalidTypedArrays = [
    // Uint8Array with invalid length.
    [3,0,0,0,0,0,241,255,0,0,0,0,32,0,255,255,3,0,0,0,0,0,0,255,0,0,0,0,31,0,255,255,3,0,0,0,0,0,0,0,1,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0],
    // Uint8Array with invalid byteOffset.
    [3,0,0,0,0,0,241,255,0,0,0,0,32,0,255,255,3,0,0,0,0,0,0,0,0,0,0,0,31,0,255,255,3,0,0,0,0,0,0,0,1,2,3,0,0,0,0,0,0,0,0,0,0,0,0,1],
];
let invalidDataViews = [
    // DataView with invalid length.
    [3,0,0,0,0,0,241,255,0,0,0,0,33,0,255,255,3,0,0,0,0,0,0,255,0,0,0,0,31,0,255,255,3,0,0,0,0,0,0,0,1,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0],
    // DataView with invalid byteOffset.
    [3,0,0,0,0,0,241,255,0,0,0,0,33,0,255,255,3,0,0,0,0,0,0,0,0,0,0,0,31,0,255,255,3,0,0,0,0,0,0,0,1,2,3,0,0,0,0,0,0,0,0,0,0,0,0,1],
];
function checkError(data, message) {
    let clonebuffer = serialize("dummy");
    let buf = new Uint8Array(data);
    clonebuffer.clonebuffer = buf.buffer;
    let ex = null;
    try {
        deserialize(clonebuffer);
    } catch (e) {
        ex = e;
    }
    assertEq(ex.toString().includes(message), true);
}
for (let data of invalidTypedArrays) {
    checkError(data, "invalid typed array length or offset");
}
for (let data of invalidDataViews) {
    checkError(data, "invalid DataView length or offset");
}

const gb = 1 * 1024 * 1024 * 1024;

const bufferSmall = new ArrayBuffer(8);
const bufferLarge = new ArrayBuffer(6 * gb);

const ArrayBufferByteLength = getSelfHostedValue("ArrayBufferByteLength");

function testBufferByteLengthInt32() {
    var arr = [bufferLarge, bufferSmall];
    for (var i = 0; i < 2000; i++) {
        var idx = +(i < 1900); // First 1 then 0.
        assertEq(ArrayBufferByteLength(arr[idx]), idx === 0 ? 6 * gb : 8);
    }
}
testBufferByteLengthInt32();

const gb = 1 * 1024 * 1024 * 1024;

const bufferSmall = new ArrayBuffer(8);
const bufferLarge = new ArrayBuffer(6 * gb);

const taSmall = new Uint8Array(bufferSmall);
const taLargeOffset = new Uint8Array(bufferLarge, 5 * gb);
const taLargeLength = new Uint8Array(bufferLarge);

const dvSmall = new DataView(bufferSmall);
const dvLargeOffset = new DataView(bufferLarge, 5 * gb);
const dvLargeLength = new DataView(bufferLarge);

const ArrayBufferByteLength = getSelfHostedValue("ArrayBufferByteLength");
const TypedArrayByteOffset = getSelfHostedValue("TypedArrayByteOffset");
const TypedArrayLength = getSelfHostedValue("TypedArrayLength");

function testBufferByteLengthInt32() {
    var arr = [bufferLarge, bufferSmall];
    for (var i = 0; i < 2000; i++) {
        var idx = +(i < 1900); // First 1 then 0.
        assertEq(ArrayBufferByteLength(arr[idx]), idx === 0 ? 6 * gb : 8);
        assertEq(arr[idx].byteLength, idx === 0 ? 6 * gb : 8);
    }
}
testBufferByteLengthInt32();

function testTypedArrayByteOffsetInt32() {
    var arr = [taLargeOffset, taSmall];
    for (var i = 0; i < 2000; i++) {
        var idx = +(i < 1900); // First 1 then 0.
        assertEq(TypedArrayByteOffset(arr[idx]), idx === 0 ? 5 * gb : 0);
        assertEq(arr[idx].byteOffset, idx === 0 ? 5 * gb : 0);
    }
}
testTypedArrayByteOffsetInt32();

function testTypedArrayLengthInt32() {
    var arr = [taLargeLength, taSmall];
    for (var i = 0; i < 2000; i++) {
        var idx = +(i < 1900); // First 1 then 0.
        assertEq(TypedArrayLength(arr[idx]), idx === 0 ? 6 * gb : 8);
        assertEq(arr[idx].length, idx === 0 ? 6 * gb : 8);
    }
}
testTypedArrayLengthInt32();

function testTypedArrayByteLengthInt32() {
    var arr = [taLargeLength, taSmall];
    for (var i = 0; i < 2000; i++) {
        var idx = +(i < 1900); // First 1 then 0.
        assertEq(arr[idx].byteLength, idx === 0 ? 6 * gb : 8);
    }
}
testTypedArrayByteLengthInt32();

function testDataViewByteOffsetInt32() {
    var arr = [dvLargeOffset, dvSmall];
    for (var i = 0; i < 2000; i++) {
        var idx = +(i < 1900); // First 1 then 0.
        assertEq(arr[idx].byteOffset, idx === 0 ? 5 * gb : 0);
    }
}
testDataViewByteOffsetInt32();

function testDataViewByteLengthInt32() {
    var arr = [dvLargeLength, dvSmall];
    for (var i = 0; i < 2000; i++) {
        var idx = +(i < 1900); // First 1 then 0.
        assertEq(arr[idx].byteLength, idx === 0 ? 6 * gb : 8);
    }
}
testDataViewByteLengthInt32();

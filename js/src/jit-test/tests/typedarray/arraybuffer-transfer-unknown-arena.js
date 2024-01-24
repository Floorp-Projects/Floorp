const headerSize = 8;
const undefinedSize = 8;

let source = serialize(undefined).arraybuffer
assertEq(source.detached, false);
assertEq(source.byteLength, headerSize + undefinedSize);

let target = source.transfer(128);
assertEq(source.detached, true);
assertEq(source.byteLength, 0);
assertEq(target.detached, false);
assertEq(target.byteLength, 128);

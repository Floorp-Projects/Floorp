assertThrowsInstanceOf(() => ArrayBuffer(), TypeError);
assertThrowsInstanceOf(() => ArrayBuffer(1), TypeError);
assertThrowsInstanceOf(() => ArrayBuffer.call(null), TypeError);
assertThrowsInstanceOf(() => ArrayBuffer.apply(null, []), TypeError);
assertThrowsInstanceOf(() => Reflect.apply(ArrayBuffer, null, []), TypeError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");

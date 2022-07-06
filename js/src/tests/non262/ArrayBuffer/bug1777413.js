// |reftest| skip-if(!xulRuntime.shell) -- needs shell functions

var b = createExternalArrayBuffer(0);
assertEq(b.byteLength, 0);

if (typeof reportCompare === 'function')
  reportCompare(true, true);

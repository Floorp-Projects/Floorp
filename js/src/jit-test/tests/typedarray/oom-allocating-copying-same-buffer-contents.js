// |jit-test| skip-if: !('oomTest' in this)

var buffer = new ArrayBuffer(16);
var i8 = new Int8Array(buffer);
var i16 = new Int16Array(buffer);

oomTest(function() {
  i8.set(i16);
});

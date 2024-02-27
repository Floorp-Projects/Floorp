// |jit-test| skip-if: !('oomTest' in this)

x = [];
x.keepFailing = [];
oomTest(function () {
  y = { z: [] };
  makeSerializable().log;
}, x);

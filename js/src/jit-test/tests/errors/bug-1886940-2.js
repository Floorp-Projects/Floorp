oomTest(function () {
  (function () {
    var x = [disassemble, new Int8Array(2 ** 8 + 1)];
    x.shift().apply([], x);
  })();
});

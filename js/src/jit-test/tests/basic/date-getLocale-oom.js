// |jit-test| skip-if: !('oomTest' in this)

oomTest(function () {
  new Date(NaN).toString();
}, {keepFailing: true});

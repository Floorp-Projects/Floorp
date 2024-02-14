// |jit-test| skip-if: !('oomTest' in this)

let x = 0;
oomTest(function () {
  let y = x++;
  [](y.toString(y));
});

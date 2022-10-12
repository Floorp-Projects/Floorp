// |jit-test| skip-if: !('oomTest' in this)

for (let i = 0; i < 2; i++) {
  oomTest(function () {
    RegExp("(?<name" + i + ">)").exec();
  })
}

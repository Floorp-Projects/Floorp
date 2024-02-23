for (let i = 0; i < 2; i++) {
  oomTest(function () {
    RegExp("(?<name" + i + ">)").exec();
  })
}

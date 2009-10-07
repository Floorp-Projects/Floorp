function test() {
    for (let z = 0; z < 4; ++z) {
      uneval({ x: 9 });
      gc()
    }
    return "pass";
}
assertEq(test(), "pass");
checkStats({
  recorderStarted: 1,
  recorderAborted: 0
});

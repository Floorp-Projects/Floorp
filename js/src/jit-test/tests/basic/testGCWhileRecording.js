function test() {
    for (let z = 0; z < 4; ++z) {
      JSON.stringify({ x: 9 });
      gc()
    }
    return "pass";
}
assertEq(test(), "pass");

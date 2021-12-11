// |jit-test| error:ReferenceError
function e() {
    try {} catch (e) {
    return (actual = "FAIL");
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
      a.x + a.x + a.x + a.x + a.x + a.x + a.x + a.x
    }
    while (t) continue;
}
e();

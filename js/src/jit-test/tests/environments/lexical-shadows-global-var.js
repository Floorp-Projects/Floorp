this.x = "OK";

for (var i = 0; i < 50; i++) {
  assertEq(x, "OK");
  if (i === 40) {
    this.x = "FAIL";
    evaluate("let x = 'OK';");
  }
}

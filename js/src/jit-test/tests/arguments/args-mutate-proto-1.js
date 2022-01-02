function outOfBounds() {
  var proto = ["pass"];

  var N = 100;
  for (var i = 0; i <= N; ++i) {
    if (i === N) {
      Object.setPrototypeOf(arguments, proto);
    }

    var arg = arguments[0];

    assertEq(arg, i !== N ? undefined : "pass");
  }
}
outOfBounds();

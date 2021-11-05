function outOfBounds() {
  var proto = [];
  Object.setPrototypeOf(arguments, proto);

  var N = 100;
  for (var i = 0; i <= N; ++i) {
    if (i === N) {
      proto[0] = "pass";
    }

    var arg = arguments[0];

    assertEq(arg, i !== N ? undefined : "pass");
  }
}
outOfBounds();

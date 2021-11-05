function outOfBounds() {
  Object.setPrototypeOf(arguments, Array.prototype);

  var N = 100;
  for (var i = 0; i <= N; ++i) {
    if (i === N) {
      Array.prototype[0] = "pass";
    }

    var arg = arguments[0];

    assertEq(arg, i !== N ? undefined : "pass");
  }
}
outOfBounds();

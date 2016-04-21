if (typeof assertShallowArray === 'undefined') {
  var assertShallowArray = function assertShallowArray(actual, expected) {
    var len = actual.length;
    assertEq(len, expected.length, "mismatching array lengths");

    for (var i = 0; i < len; i++)
      assertEq(actual[i], expected[i], "mismatch at element " + i);
  }
}

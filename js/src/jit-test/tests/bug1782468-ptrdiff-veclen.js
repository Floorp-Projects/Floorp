try {
  a = {};
  for (b = 0; b < 24; b++)
    a += a;
  Function(a, a, a);
  assertEq(true, false, "allocation overflow expected");
} catch (e) {
  if (getBuildConfiguration()['pointer-byte-size'] == 4) {
    assertEq((e + "").includes("InternalError: allocation size overflow"), true);
  } // else on 64-bit, it will be a SyntaxError for invalid code.
}

try {
    decodeURIComponent('%ED%A0%80');
    assertEq(true, false, "expected an URIError");
} catch (e) {
  assertEq(e instanceof URIError, true);
  reportCompare(true,true);
}

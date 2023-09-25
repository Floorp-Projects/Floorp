load(libdir + "syntax.js");

if (!getBuildConfiguration("decorators")) {
  function check_syntax_error_at(e, code, name) {
    assertEq(e instanceof SyntaxError, true, name + ": " + code);
    assertEq(e.message, "illegal character U+0040", name + ": " + code);
  }
  test_syntax(["@"], check_syntax_error_at, false);
}

function check_syntax_error_ellipsis(e, code, name) {
  assertEq(e instanceof SyntaxError, true, name + ": " + code);
  assertEq(e.message, "illegal character U+2026", name + ": " + code);
}
test_syntax(["…"], check_syntax_error_ellipsis, false);

function check_syntax_error_clown(e, code, name) {
  assertEq(e instanceof SyntaxError, true, name + ": " + code);
  assertEq(e.message, "illegal character U+1F921", name + ": " + code);
}
test_syntax(["🤡"], check_syntax_error_clown, false);


load(libdir + "syntax.js");

var repl_expr_flags = [
  "/bar/g; @",
  "\n/bar/g; @",
];

function check_syntax_error(e, code) {
  assertEq(e instanceof SyntaxError, true);
}

test_syntax(repl_expr_flags, check_syntax_error, true);

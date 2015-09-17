load(libdir + "syntax.js");

var JSMSG_ILLEGAL_CHARACTER = "illegal character";

var repl_expr_flags = [
  "@",
];

function check_syntax_error(e, code, name) {
  assertEq(e instanceof SyntaxError, true, name + ": " + code);
  assertEq(e.message, JSMSG_ILLEGAL_CHARACTER, name + ": " + code);
}

test_syntax(repl_expr_flags, check_syntax_error, false);

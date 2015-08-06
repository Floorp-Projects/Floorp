load(libdir + "syntax.js");

var repl_expr_flags = [
  "yield) @",
  "yield} @",
  "yield] @",
];

function check_syntax_error(e, code) {
  // No need to check exception type
}

test_syntax(repl_expr_flags, check_syntax_error, true);

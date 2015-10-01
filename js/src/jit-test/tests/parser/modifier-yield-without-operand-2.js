load(libdir + "syntax.js");

var postfixes = [
  "yield) @",
  "yield} @",
  "yield] @",
];

function check_syntax_error(e, code) {
  // No need to check exception type
}

test_syntax(postfixes, check_syntax_error, true);

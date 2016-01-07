load(libdir + "asserts.js");

var valid_strict_funs = [
  // directive ends on next line
  function () {
    "use strict"
    ;
  },
  function () {
    "use strict"
  },
  // directive ends on same line
  function () { "use strict"; },
  function () { "use strict" },
];

for (var f of valid_strict_funs) {
  assertThrowsInstanceOf(function() { f.caller }, TypeError);
}


var binary_ops = [
  "||", "&&",
  "|", "^", "&",
  "==", "!=", "===", "!==",
  "<", "<=", ">", ">=", "in", "instanceof",
  "<<", ">>", ">>>",
  "+", "-",
  "*", "/", "%",
];

var invalid_strict_funs = [
  function () {
    "use strict"
    , "not";
  },
  function () {
    "use strict"
    ? 1 : 0;
  },
  function () {
    "use strict"
    .length;
  },
  function () {
    "use strict"
    [0];
  },
  function () {
    "use strict"
    ();
  },
  ...([]),
  ...(binary_ops.map(op => Function("'use strict'\n " + op + " 'not'"))),
];

for (var f of invalid_strict_funs) {
  f.caller;
}


var assignment_ops = [
   "=", "+=", "-=",
  "|=", "^=", "&=",
  "<<=", ">>=", ">>>=",
  "*=", "/=", "%=",
];

var invalid_strict_funs_referror = assignment_ops.map(op => ("'use strict'\n " + op + " 'not'"));

// assignment with string literal as LHS is an early error, therefore we
// can only test for ReferenceError
for (var f of invalid_strict_funs_referror) {
  assertThrowsInstanceOf(function() { Function(f) }, ReferenceError);
}

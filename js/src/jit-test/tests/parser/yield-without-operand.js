// yield without an operand causes a warning. See bug 885463.

load(libdir + "asserts.js");

assertWarning(() => Function("yield"), SyntaxError,
             "yield followed by EOF should cause a warning");
assertWarning(() => Function("yield;"), SyntaxError,
             "yield followed by semicolon should cause a warning");
assertWarning(() => Function("yield\n  print('ok');"), SyntaxError,
             "yield followed by newline should cause a warning");

assertWarning(() => eval("(function () { yield; })"), SyntaxError,
             "yield followed by semicolon in eval code should cause a warning");
assertWarning(() => eval("(function () { yield })"), SyntaxError,
             "yield followed by } in eval code should cause a warning");

assertNoWarning(() => Function("yield 0;"),
                "yield with an operand should be fine");
assertNoWarning(() => Function("yield 0"),
                "yield with an operand should be fine, even without a semicolon");

print("\npassed - all those warnings are normal and there's no real way to suppress them");

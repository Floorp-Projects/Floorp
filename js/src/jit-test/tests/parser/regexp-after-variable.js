// RegExp after variable name get parsed or throws error correctly.

load(libdir + "asserts.js");

assertNoWarning(() => Function("var foo \n /bar/g"), SyntaxError,
                "RegExp in next line should be parsed");
assertThrowsInstanceOf(() => Function("var foo /bar/g"), SyntaxError,
                       "RegExp in same line should be error");

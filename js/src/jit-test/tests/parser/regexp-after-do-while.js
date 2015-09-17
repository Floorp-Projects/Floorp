// RegExp after do-while get parsed.

load(libdir + "asserts.js");

assertNoWarning(() => Function("do {} while (true) \n /bar/g"), SyntaxError,
                "RegExp in next line should be parsed");
assertNoWarning(() => Function("do {} while (true) /bar/g"), SyntaxError,
                "RegExp in same line should be parsed");

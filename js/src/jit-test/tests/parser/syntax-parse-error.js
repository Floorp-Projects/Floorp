load(libdir + "asserts.js");

assertThrowsInstanceOf(() => syntaxParse(">"), SyntaxError);


// "let" is not allowed as an identifier.

assertThrowsInstanceOf(function () { eval('[for (let of y) foo]') }, SyntaxError);
assertThrowsInstanceOf(function () { eval('(for (let of y) foo)') }, SyntaxError);

reportCompare(null, null, "test");

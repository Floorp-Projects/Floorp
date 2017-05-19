assertEq(evaluate("var f = x=>class { }; f()", { columnNumber: 1729 }).toString(), "class { }");

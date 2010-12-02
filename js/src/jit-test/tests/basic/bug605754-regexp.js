var result = "foobarbaz".replace(/foo(bar)?bar/, "$1");
assertEq(result, "baz");

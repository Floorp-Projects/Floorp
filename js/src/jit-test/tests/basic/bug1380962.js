// Source string has balanced parentheses even when the source code was discarded.

setDiscardSource(true);
eval("var f = function() { return 0; };");
assertEq(f.toSource(), "(function() {\n    [sourceless code]\n})");

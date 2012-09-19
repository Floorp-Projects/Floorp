// Cleaned-up version of bug 728079 comment 0.

version(170);
eval("(function f() { return [[b, a] for ([a, b] of c.items())]; })");

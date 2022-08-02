function bytecode(f) {
    if (typeof disassemble !== "function")
        return "unavailable";
    var d = disassemble(f);
    // Remove line numbers.
    d = d.replace(/(\n\d{5}:).*\d+/g, "$1");
    return d.slice(d.indexOf("main:"), d.indexOf("\n\n"));
}
assertEq(bytecode(() => "hello" + "world"),
         bytecode(() => "helloworld"));
assertEq(bytecode(() => 2 + "2" + "2"),
         bytecode(() => "222"));
assertEq(bytecode(() => "x" + "3"),
         bytecode(() => "x3"));

var s = "hoge";
assertEq(bytecode(() => "fo" + "o" + s + "ba" + "r"),
         bytecode(() => "foo" + s + "bar"));
assertEq(bytecode(() => "fo" + "o" + 1 + s + 1 + "ba" + "r"),
         bytecode(() => "foo1" + s + "1bar"));
assertEq(bytecode(() => 1 + (2 * 2) + "x"),
         bytecode(() => 5 + "x"));
assertEq(s + 1 + 2, "hoge12");
assertEq((() => s + 1 + 2)(), "hoge12");

// SpiderMonkey knows that 1 + 1 == "11".
assertEq(bytecode(() => "x" + s + 1 + 1),
         bytecode(() => "x" + s + "11"));

var n = 5;
assertEq(1 + n + 1 + "ba" + "r", "7bar");
assertEq(1 + 2 + {valueOf: () => 3, toString: () => 'x'} + 4 + 5,15);
assertEq(1+2+n,8);
assertEq(bytecode(() => 1 + 2 + n + 1 + 2),
         bytecode(() => 3 + n + 1 + 2));
assertEq(1 + 2 + n + 1 + 2, 11);
assertEq(bytecode(() => 1 + 2 + s + 1 + 2),
         bytecode(() => 3 + s + 1 + 2));
assertEq(1 + 2 + s + 1 + 2, "3hoge12");

assertEq(bytecode(() => ["a" + "b" + n]),
         bytecode(() => ["ab" + n]));
var a = ["a" + "b" + n];
assertEq(a[0], "ab5");


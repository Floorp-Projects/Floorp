function bytecode(f) {
    if (typeof disassemble !== "function")
        return "unavailable";
    var d = disassemble(f);
    return d.slice(d.indexOf("main:"), d.indexOf("\n\n"));
}

function hasGname(f, v) {
    // Do a try-catch that prints the full stack, so we can tell
    // _which_ part of this test failed.
    try {
        var b = bytecode(f);
        if (b != "unavailable") {
            assertEq(b.includes(`GetGName "${v}"`), true);
            assertEq(b.includes(`GetName "${v}"`), false);
        }
    } catch (e) {
        print(e.stack);
        throw e;
    }
}

var x = "outer";

var f1 = new Function("assertEq(x, 'outer')");
f1();
hasGname(f1, 'x');

{
    let x = "inner";
    var f3 = new Function("assertEq(x, 'outer')");
    f3();
    hasGname(f3, 'x');
}

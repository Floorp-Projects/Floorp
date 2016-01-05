var lfcode = new Array();
lfcode.push("gczeal(2, 1);");
lfcode.push(`
function test(stdlib, foreign) {
    "use asm"
    function f(y) {}
    return f
}`);
for (var i = 0; i < 2; ++i)
    evaluate(lfcode.shift());

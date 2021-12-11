load(libdir + "asm.js");

var code = `
    "use asm";
    var ff = foreign.ff;
    function f(x) {
        x = +x
        return ~~x + (ff(3 ^ 9 / 7), 1) & 1;
    }
    return f;
`;

assertEq(asmLink(asmCompile("b", "foreign", code), 0, { ff: decodeURIComponent })(Infinity), 1);

load(libdir + "asserts.js");

assertThrowsInstanceOf(function () {
    eval("function f()((l()))++2s");
}, SyntaxError);

// arrow functions are implicitly strict-mode code

load(libdir + "asserts.js");

assertThrowsInstanceOf(
    function () { Function("a => { with (a) f(); }"); },
    SyntaxError);

assertThrowsInstanceOf(
    function () { Function("a => function () { with (a) f(); }"); },
    SyntaxError);



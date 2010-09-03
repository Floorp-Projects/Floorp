var actual;
var expect = "function f() {\n    ff();\n}";
function fun() {
    (new Function ("function ff () { actual = '' + ff. caller; } function f () { ff (); } f ();")) ();
}
fun();
reportCompare(expect, actual, "");

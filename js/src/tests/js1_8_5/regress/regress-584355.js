var actual;
var expect = "function f() { ff (); }";
function fun() {
    (new Function ("function ff () { actual = '' + ff. caller; } function f () { ff (); } f ();")) ();
}
fun();
reportCompare(expect, actual, "");

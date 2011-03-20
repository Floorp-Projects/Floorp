function fun() {
    (new Function ("function ff () { actual = '' + ff. caller; } function f () { ff (); } f ();")) ('function pf' + fun + '() {}');
}
fun();

var re = /a(b)c/;

for (var i = 0; i < 10; i++) {
    // These two are of a form where we can convert exec() to test().
    if (!re.exec("abc")) print("huh?");
    re.exec("abc");
}

RegExp.prototype.test = 1;

for (var i = 0; i < 10; i++) {
    // These two are the same form, but we've replaced test(), so we must
    // not convert.
    if (!re.exec("abc")) print("huh?");     // don't crash/assert
    re.exec("abc");                         // don't crash/assert
}

var x = 0;
function f() {
    var s = "abcdef(((((((a|b)a|b)a|b)a|b)a|b)a|b)a|b)" + x;
    res = "abcdefa".match(new RegExp(s));
    x++;
}
f();
stackTest(f, true);

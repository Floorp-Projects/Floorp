function g() {
    z = newGlobal('');
    return function(code) {
        evalcx(code, z)
    }
}
f = g();
f("\
    options('strict_mode');\
    for (var x = 0; x < 1; ++x) {\
        a = x;\
    }\
    options('strict_mode');\
");
f("a in eval");


function f(foo) {
    "use strict";
    foo.bar;
}

var actual;
try {
    f();
    actual = "no error";
} catch (x) {
    actual = (x instanceof TypeError) ? "type error" : "some other error";
    actual += (/use strict/.test(x)) ? " with directive" : " without directive";
}

reportCompare("type error without directive", actual,
              "decompiled expressions in error messages should not include directive prologues");

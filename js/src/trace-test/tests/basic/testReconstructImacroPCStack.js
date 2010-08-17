var actual = "";
var expect = "TypeError: x is not a function";

x = Proxy.create((function () {
    return {
        get: function () {}
    }
}()), Object.e)

try {
    Function("\
      for(var a = 0; a < 2; ++a) {\
        if (a == 0) {}\
        else {\
          x > x\
        }\
      }\
    ")()
} catch (e) {
    actual = "" + e;
}

assertEq(actual, expect);

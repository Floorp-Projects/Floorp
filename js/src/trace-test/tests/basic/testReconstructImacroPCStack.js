x = Proxy.create((function () {
    return {
        get: function () {}
    }
}()), Object.e)

var hit = false;

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
    hit = true;

    var str = String(e);
    var match = (str == "TypeError: x is not a function" ||
                 str == "TypeError: can't convert x to number");

    assertEq(match, true);
}

assertEq(hit, true);

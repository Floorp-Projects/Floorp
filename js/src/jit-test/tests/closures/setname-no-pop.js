actual = '';
expected = '';

(function () {
    var y;
    (eval("(function () {\
               for (var x = 0; x < 3; ++x) {\
               ''.replace(/a/, (y = 3))\
               }\
           });\
     "))()
})()



assertEq(actual, expected)

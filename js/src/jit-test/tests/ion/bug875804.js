
eval('(function () {\
function range(n) {\
  for (var i = 0; i < 100000; i++)\
    yield i;\
}\
var r = range(10);\
var i = 0;\
for (var x in r)\
  assertEq(x,i++);\
' + '})();');
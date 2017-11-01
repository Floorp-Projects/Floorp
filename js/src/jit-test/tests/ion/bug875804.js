
eval('(function () {\
function* range(n) {\
  for (var i = 0; i < 5000; i++)\
    yield i;\
}\
var r = range(10);\
var i = 0;\
for (var x of r)\
  assertEq(x,i++);\
' + '})();');

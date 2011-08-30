
function foo() {
  var x = 0;
  for (var i = arguments.length - 1; i >= 0; i--)
    x += arguments[i];
  return x;
}

function bar() {
  var x = 0;
  for (var i = 0; i < arguments.length; i++)
    x += arguments[i];
  return x;
}

function baz(a,b,c,d,e) {
  var x = 0;
  for (var i = 0; i < arguments.length; i++)
    x += arguments[i];
  return x;
}

for (var i = 0; i < 10; i++) {
  assertEq(foo(1,2,3,4,5), 15);
  assertEq(bar(1,2.5,true,{valueOf:function() { return 10}},"five"), "14.5five");
  assertEq(baz(1,2,3,4,5), 15);
}

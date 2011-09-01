
/* Non-reentrant call on an inner and outer closure. */

function foo() {
  var x = 0;
  function bar() {
    var y = 0;
    function baz() {
      return ++x + ++y;
    }
    return baz;
  }
  return bar();
}

var a = foo();
var b = foo();
assertEq(a() + a() + b() + b(), 12);

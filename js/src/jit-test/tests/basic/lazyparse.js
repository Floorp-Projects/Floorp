
function outer() {
    var xyz = 0;
    function foo() {
	function bar() { xyz++; }
	bar();
	let x = 3;
    }
    foo();
    assertEq(xyz, 1);
}
outer();

function mapfloor(a) {
  var b = a.map(function(v) {
        "use strict";
        try {
            eval("delete String;");
        } catch (e) {
            return e instanceof res;
        }
    });
  var res = "";
}
try {
    mapfloor([1,2]);
} catch (e) {}

test();
function test() {
  try  { 
    eval('let(z) { with({}) let y = 3; }');
  } catch(ex) {
    (function(x) { return !(x) })(0/0)
  }
}

testCatch(15);
function testCatch(y) {
  try {
      throw 5;
  } catch(ex) {
      (function(x) { assertEq(x + y + ex, 25); })(5)
  }
}

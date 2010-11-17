
/* Handle recompilation on undefined variables. */

function local()
{
  var x;
  x++;
  assertEq(x, NaN);
  x = 0;
}
local();

function name(v)
{
  var x;
  with (v) {
    x++;
    assertEq(x, NaN);
  }
  assertEq(x, NaN);
  x = 0;
}
name({});

function letname(v)
{
  if (v) {
    let x;
    with (v) {
      x = "twelve";
    }
    assertEq(x, "twelve");
  }
}
letname({});

function upvar()
{
  var x;
  function inner() {
    x++;
    assertEq(x, NaN);
  }
  inner();
}
upvar();

var x;
var y;

function global()
{
  x++;
  assertEq(x, NaN);
  var z = 2 + y;
  assertEq(z, NaN);
}
global();

x = 0;
y = 0;

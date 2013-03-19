
function foo(str) {
  var x = eval(str); yield x[0];
}
for (var i = 0; i < 5; i++)
  assertEq(foo("[4,5,6]").next(), 4);
for (var i = 0; i < 5; i++)
  assertEq(foo("arguments").next(), "arguments");

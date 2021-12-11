
function test1() {
  eval("with (arguments) var arguments = 0;");
}
test1();

function test2() {
  eval("eval('with (arguments) var arguments = 0;')");
}
test2();


function foo() {
  with(foo) this["00"]=function(){}
}
new foo;

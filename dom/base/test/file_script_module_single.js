function baz() {}
function bar() {}
function foo() {
  bar();
}
foo();

window.dispatchEvent(new Event("test_evaluated"));

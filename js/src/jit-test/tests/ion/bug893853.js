
function f() {}
new EvalTest();
function EvalTest() {
  with (this) {
    f(EvalTest)
  }
}
evaluate("var obj = new f(1, 'x');");

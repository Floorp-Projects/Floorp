
function f(code) {
    g = eval("(function(){" + code + "})");
    g()
}
f();
f();
f();
f();
f();
f();
f();
f();
f();
f();
f();
f();
f();
f();
f();
f();
try { f("function x(){}(x())"); } catch (e) {}

function f2() {
    a = {
        x
    } = x, (x._)
    function
    x(){ return ({}); }
}
try { f2(); } catch (e) {}

function f3() {
  var x = 0;
  with ({}) { x = 'three'; }
  return x;
}
f3();

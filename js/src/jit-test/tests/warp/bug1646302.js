function f(x) {
  function fnc() {}
  fnc.prototype = 3;
  new fnc;
  if (x < 50) {
    new new.target(x + 1);
  }
}
new f(0);

var i = 0;
try { test(); } catch (e) { }
function test() {
  var jstop  = 900;
  var code = '';
  code+=createCode(i);
  eval();
}
function createCode(i) {
  jstop+= +  +  + i + " string.';";
}


function setterFunction(v) { called = true; }
function getterFunction(v) { return "getter"; }
Object.defineProperty(Array.prototype, 1,{ 
  get: getterFunction, 
  set: setterFunction 
});
gczeal(4);
var N = 350;
var source = "".concat(
  repeat_str("try { f(); } finally {\n", N),
  repeat_str("}", N));
function repeat_str(str, repeat_count) {
  var arr = new Array(--repeat_count);
  while (repeat_count != 0)
    arr[--repeat_count] = str;
  return str.concat.apply(str, arr);
}

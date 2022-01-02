// |jit-test| error:RangeError

function getterFunction(v) { return "getter"; }
Object.defineProperty(Array.prototype, 1,{ 
  get: getterFunction, 
});
var N = (10000);
repeat_str("try { f(); } finally {\n", N),
repeat_str("}", ("" ));
function repeat_str(str, repeat_count) {
  var arr = new Array(--repeat_count);
  while (repeat_count != 0)
    arr[--repeat_count] = str;
  return str.concat.apply(str, arr);
}

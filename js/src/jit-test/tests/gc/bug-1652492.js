function f1() {
  arr = f2;
  var p = arr[(gczeal(9))|0];
}
f2 = f1;
f2();
try {
  function allocObj() { return {}; }
  {
    let obj = allocObj();
    wr = new WeakRef(obj);
  }
  clearKeptObjects();
(new obj);
} catch(exc) {}
let obj = allocObj();
wr = new WeakRef(obj);

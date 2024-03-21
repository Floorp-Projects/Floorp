let g = newGlobal();
function f() {
  var o = {};
  o["prop" + Date.now()] = 1;
  gc();
  schedulezone("atoms");
  schedulezone(g);
  gc("zone");
  let [x] = [0];
}
f();
oomTest(f);

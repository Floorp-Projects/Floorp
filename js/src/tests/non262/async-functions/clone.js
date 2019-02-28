// |reftest| skip-if(!xulRuntime.shell) -- needs clone, cloneAndExecuteScript, drainJobQueue

// Async functions can be cloned.
let f = clone(async function f() {
  var a = await 1;
  var b = await 2;
  var c = await 3;
  return a + b + c;
});

var V;
f().then(v => V = v);
drainJobQueue();
assertEq(V, 6);

// Async function source code scripts can be cloned.
let g = newGlobal();
cloneAndExecuteScript(`
async function f() {
  var a = await 1;
  var b = await 2;
  var c = await 3;
  return a + b + c;
}
var V;
f().then(v => V = v);
drainJobQueue();
`, g);
assertEq(g.V, 6);

if (typeof reportCompare === "function")
    reportCompare(true, true);

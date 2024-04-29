// |jit-test| --baseline-eager

var src = "";
for (var i = 0; i < 100; i++) {
  src += "function foo" + i + "() { foo" + (i+1) + "(); }"
}
eval(src);

function foo100() {
  let g = newGlobal({newCompartment: true});
  let d = new g.Debugger(this);

  // When we set this debugger hook, we will trigger 100
  // baseline recompilations. We want an OOM to occur
  // during one of those recompilations. We allocate
  // about 400 times before starting recompilation, and
  // about 30000 times total. This number is picked to
  // leave a healthy margin on either side.
  oomAtAllocation(5000);
  d.onEnterFrame = () => {}
}
try {
  foo0();
} catch {}

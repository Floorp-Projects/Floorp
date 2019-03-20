const { Module, Instance, Global, instantiate, instantiateStreaming } = WebAssembly;

const g = new Global({value: "i32", mutable:true}, 0);

const code = wasmTextToBinary(`(module
    (global $g (import "" "g") (mut i32))
    (func $start (set_global $g (i32.add (get_global $g) (i32.const 1))))
    (start $start)
)`);
const module = new Module(code);

const importObj = { '': { get g() { g.value++; return g } } };

g.value = 0;
new Instance(module, importObj);
assertEq(g.value, 2);

g.value = 0;
instantiate(module, importObj).then(i => {
  assertEq(i instanceof Instance, true);
  assertEq(g.value, 2);
  g.value++;
});
assertEq(g.value, 1);
drainJobQueue();
assertEq(g.value, 3);

g.value = 0;
instantiate(code, importObj).then(({module,instance}) => {
    assertEq(module instanceof Module, true);
    assertEq(instance instanceof Instance, true);
    assertEq(g.value, 2); g.value++; }
);
drainJobQueue();
assertEq(g.value, 3);

if (wasmStreamingIsSupported()) {
  g.value = 0;
  instantiateStreaming(code, importObj).then(({module,instance}) => {
      assertEq(module instanceof Module, true);
      assertEq(instance instanceof Instance, true);
      assertEq(g.value, 2);
      g.value++;
  });
  drainJobQueue();
  assertEq(g.value, 3);
}

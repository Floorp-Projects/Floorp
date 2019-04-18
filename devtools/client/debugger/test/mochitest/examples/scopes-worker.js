function f() {
  var var_array = ["mango","pamplemousse","pineapple"];
  var var_tarray = new Uint8Array([42,43,44]);
  var var_set = new Set(["papaya","banana"]);
  var var_map = new Map([[1,"one"],[2,"two"]]);
  var var_weakmap = new WeakMap();
  var key = { foo: "foo" }, value = { bar: "bar" };
  var_weakmap.set(key, value);
  var var_weakset = new WeakSet();
  var_weakset.add(key);
  return 0;
}
f();

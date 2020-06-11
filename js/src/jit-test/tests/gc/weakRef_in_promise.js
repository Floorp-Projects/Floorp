// https://github.com/tc39/proposal-weakrefs/issues/39
// Weakref should keep the target until the end of current Job, that includes
// microtask(Promise).
let wr;
{
  let obj = {};
  wr = new WeakRef(obj);
  obj = null;
}
// obj is out of block scope now, should be GCed.

gc();

assertEq(undefined == wr.deref(), false);
Promise.resolve().then(() => {
  assertEq(undefined == wr.deref(), false);
});


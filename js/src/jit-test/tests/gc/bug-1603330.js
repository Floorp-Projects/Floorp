// Allocate the object in the function to prevent marked as a singleton so the
// object won't be kept alive by IC stub.
function allocObj() { return {}; }

let wr;
{
  let obj = allocObj();
  wr = new WeakRef(obj);
}

assertEq(wr.deref() !== undefined, true);

clearKeptObjects();
gc();

assertEq(wr.deref(), undefined);

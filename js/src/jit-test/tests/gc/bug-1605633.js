newGlobal();
nukeAllCCWs();
var g28 = newGlobal({
    newCompartment: true
});
try {
  let wr6 = new g28.WeakRef(newGlobal());
  new WeakRef(wr6);
} catch (e) {
  assertEq(e.message == "can't access dead object", true);
}


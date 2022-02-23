let count = 0;
setWatchtowerCallback(function(kind, object, extra) {
    assertEq(kind, "object-swap");
    assertEq(object, source);
    assertEq(extra instanceof FakeDOMObject, true);
    assertEq(extra !== source, true);
    count++;
});
let source = new FakeDOMObject();
addWatchtowerTarget(source);
let {transplant} = transplantableObject({object: source});
transplant(this);
assertEq(count, 1);

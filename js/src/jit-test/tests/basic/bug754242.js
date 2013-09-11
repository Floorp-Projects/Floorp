var g1 = newGlobal();
schedulegc(g1);
gcslice(0);
function testEq(b) {
    var a = deserialize(serialize(b));
}
testEq(Array(1024).join(Array(1024).join((false))));

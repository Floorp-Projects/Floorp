var g1 = newGlobal();
schedulezone(g1);
gcslice(1);
function testEq(b) {
    var a = deserialize(serialize(b));
}
testEq(Array(1024).join(Array(1024).join((false))));

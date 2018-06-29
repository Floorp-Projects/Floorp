if (!this.hasOwnProperty("TypedObject"))
    quit();
function test() {
    var g = newGlobal({sameCompartmentAs: this});
    var S = g.evaluate("new TypedObject.StructType({x: TypedObject.uint32});");
    for (var i = 0; i < 30; i++) {
        var o = new S();
        assertEq(objectGlobal(o), g);
    }
}
test();

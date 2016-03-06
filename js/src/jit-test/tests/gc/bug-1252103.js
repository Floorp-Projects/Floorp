// Bug 1252103: Inline typed array objects need delayed metadata collection.
// Shouldn't crash.

if (!this.hasOwnProperty("TypedObject"))
  quit();

function foo() {
    enableTrackAllocations();
    gczeal(2, 10);
    TO = TypedObject;
    PointType = new TO.StructType({
        y: TO.float64,
        name: TO.string
    })
    LineType = new TO.StructType({
        PointType
    })
    function testBasic() new LineType;
    testBasic();
}
evaluate("foo()");


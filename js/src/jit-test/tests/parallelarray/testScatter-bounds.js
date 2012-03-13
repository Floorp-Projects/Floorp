// |jit-test| error: Error;

function testScatterBounds() {
    var p = new ParallelArray([1,2,3,4,5]);
    var r = p.scatter([0,2,11]);
}

testScatterBounds();


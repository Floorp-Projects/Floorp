// Binary: cache/js-dbg-64-b01eb1ba58ce-linux
// Flags:
//

expect = 'Test skipped.';
function addPointImagedata(pointArray, n, col, row) {
    pointArray[expect + 2] = 2;
}
function createMandelSet() {
    points = new Array;
    for (var idx = 0; idx < 1440000 ; ++idx) {
    	points[idx] = 0
    }
    addPointImagedata(points, ({}, {}), 0,0)
}
createMandelSet();

// Test that we don't attach ICs to idempotent caches that are incompatible
// with the cache result type.

var missingObjs = [{a:1},Object.create({a:2}),{}];
function testMissing(limit)
{
    var res = 0;
    for (var i = 0; i < 1000; i++) {
	for (var j = 0; j < missingObjs.length; j++) {
	    var obj = missingObjs[j];
	    if (j < limit)
		res += obj.a;
	}
    }
    return res;
}
assertEq(testMissing(2), 3000);
assertEq(testMissing(3), NaN);

var lengthObjs = [{length:{a:1}},Object.create({length:{a:2}}),[0,1]];
function testArrayLength(limit)
{
    var res = 0;
    for (var i = 0; i < 1000; i++) {
	for (var j = 0; j < lengthObjs.length; j++) {
	    var obj = lengthObjs[j];
	    if (j < limit)
		res += obj.length.a;
	}
    }
    return res;
}
assertEq(testArrayLength(2), 3000);
assertEq(testArrayLength(3), NaN);

function testArrayInWithIndexedProto()
{
    Array.prototype[0] = "Got me";
    var zeroPresent, zeroPresent2;
    // Need to go to 18 because in the failure mode this is
    // testing we have various side-exits in there due to interp and
    // tracer not agreeing that confuse the issue and cause us to not
    // hit the bad case within 9 iterations.
    for (var j = 0; j < 18; ++j) {
	zeroPresent = 0 in [];
    }

    var arr = [1, 2];
    delete arr[0];
    for (var j = 0; j < 18; ++j) {
	zeroPresent2 = 0 in arr;
    }
    return [zeroPresent, zeroPresent2];
}

var [ret, ret2] = testArrayInWithIndexedProto();
assertEq(ret, true);
assertEq(ret2, true);


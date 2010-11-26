// Give us enough room to work with some holes too
const array_size = RUNLOOP + 20;

function testArrayIn()
{
    var arr = new Array(array_size);
    var i;
    for (i = 0; i < array_size; ++i) {
	arr[i] = i;
    }

    // Sanity check here
    checkStats({
	traceCompleted: 1,
	traceTriggered: 1,
	sideExitIntoInterpreter: 1
    });
    
    delete arr[RUNLOOP + 5];
    delete arr[RUNLOOP + 10];

    var ret = [];
    for (i = 0; i < array_size; ++i) {
	ret.push(i in arr);
    }

    checkStats({
	traceCompleted: 2,
	traceTriggered: 2,
	sideExitIntoInterpreter: 2
    });

    
    var ret2;
    for (i = 0; i < RUNLOOP; ++i) {
	ret2 = array_size in arr;
    }

    checkStats({
	traceCompleted: 3,
	traceTriggered: 3,
	sideExitIntoInterpreter: 3
    });

    return [ret, ret2];
}

var [ret, ret2] = testArrayIn();

assertEq(ret2, false);

for (var i = 0; i < array_size; ++i) {
    assertEq(ret[i], i != RUNLOOP + 5 && i != RUNLOOP + 10);
}

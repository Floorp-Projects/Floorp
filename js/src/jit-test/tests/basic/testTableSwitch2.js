function testTableSwitch2() {
    var arr = [2, 2, 2, 2, 2, 5, 2, 2, 5, 5, 5, 5, 5, 5, 5, 5];
    var s = '';
    for (var i = 0; i < arr.length; i++) {
        switch (arr[i]) {
        case 0: case 1: case 3: case 4:
            throw "FAIL";
        case 2:
            s += '2';
            break;
        case 5:
            s += '5';
        }
    }
    assertEq(s, arr.join(""));
}

testTableSwitch2();

if (HAVE_TM && jitstats.archIsIA32) {
    checkStats({
	recorderStarted: 1,
	sideExitIntoInterpreter: 4,
	recorderAborted: 0,
	traceCompleted: 3
    });
}

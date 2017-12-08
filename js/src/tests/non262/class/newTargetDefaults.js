// Check that new.target works properly in defaults.

function check(expected, actual = new.target) { assertEq(actual, expected); }
new check(check);
check(undefined);

let evaldCheck = eval("(" + check.toString() + ")");
new evaldCheck(evaldCheck);
evaldCheck(undefined);

function testInFunction() {
    function checkInFunction(expected, actual = new.target) { assertEq(actual, expected); }
    new checkInFunction(checkInFunction);
    checkInFunction(undefined);

    let evaldCheckInFunction = eval("(" + checkInFunction.toString() + ")");
    new evaldCheckInFunction(evaldCheckInFunction);
    evaldCheckInFunction(undefined);
}

testInFunction();
new testInFunction();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");

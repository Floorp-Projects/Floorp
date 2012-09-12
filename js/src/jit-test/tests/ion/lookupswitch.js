/////////////////////////////////////////
// This is a generated file!
// See jit-tests/etc/generate-lookupswitch-tests.js for the code
// that generated this code!
/////////////////////////////////////////

/////////////////////////////////////////
// PRELUDE                             //
/////////////////////////////////////////

// Avoid eager compilation of the global-scope.
try{} catch (x) {};

function ASSERT(cond, msg) {
    assertEq(cond, true, msg);
}
function IsNull(x) {
    return typeof x == "object" && x == null;
}
function IsNum(x) {
    return typeof x == "number";
}
function ArraysEqual(arr1, arr2) {
    ASSERT(arr1.length == arr2.length, "Lengths not equal");
    for (var i = 0; i < arr1.length; i++) {
        ASSERT(typeof arr1[i] == typeof arr2[i], "Types not equal for position " + i);
        ASSERT(arr1[i] == arr2[i], "Values not equal for position " + i);
    }
}
function InterpretSwitch(spec, input, outputArray) {
    var foundMatch = undefined, foundDefault = undefined;
    for (var i = 0; i < spec.length; i++) {
        var caseSpec = spec[i], match = caseSpec.match;
        if (IsNull(match)) {
            foundDefault = i;
            continue;
        } else if (match === input) {
            foundMatch = i;
            break;
        }
    }
    var matchI = IsNum(foundMatch) ? foundMatch : foundDefault;
    if (IsNum(matchI)) {
        for (var i = matchI; i < spec.length; i++) {
            var caseSpec = spec[i], match = caseSpec.match, body = caseSpec.body, fallthrough = caseSpec.fallthrough;
            if (!IsNull(body)) {
                outputArray.push(body);
            }
            if (!fallthrough) {
                break;
            }
        }
    }
}
function RunTest(test) {
    var inputs = test.INPUTS;
    inputs.push("UNMATCHED_CASE");
    var spec = test.SPEC;
    var results1 = [];
    for (var i = 0; i < 80; i++) {
        for (var j = 0; j < inputs.length; j++) {
            test(inputs[j], results1);
        }
    }
    var results2 = [];
    for (var i = 0; i < 80; i++) {
        for (var j = 0; j < inputs.length; j++) {
            InterpretSwitch(spec, inputs[j], results2);
        }
    }
    ArraysEqual(results1, results2);
}

/////////////////////////////////////////
// TEST CASES                          //
/////////////////////////////////////////

var TESTS = [];
function test_1(x, arr) {
    switch(x) {
    default:
    case 'foo':
        arr.push(777087170);
        break;
    case 'bar':
        arr.push(641037838);
        break;
    case 'zing':
        arr.push(1652156613);
        break;
    }
}
test_1.INPUTS = ['foo', 'bar', 'zing'];
test_1.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":777087170,"fallthrough":false},{"match":"bar","body":641037838,"fallthrough":false},{"match":"zing","body":1652156613,"fallthrough":false}];
TESTS.push(test_1);

function test_2(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(777087170);
        break;
    default:
    case 'bar':
        arr.push(641037838);
        break;
    case 'zing':
        arr.push(1652156613);
        break;
    }
}
test_2.INPUTS = ['foo', 'bar', 'zing'];
test_2.SPEC = [{"match":"foo","body":777087170,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":641037838,"fallthrough":false},{"match":"zing","body":1652156613,"fallthrough":false}];
TESTS.push(test_2);

function test_3(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(777087170);
        break;
    case 'bar':
        arr.push(641037838);
        break;
    case 'zing':
        arr.push(1652156613);
        break;
    default:
    }
}
test_3.INPUTS = ['foo', 'bar', 'zing'];
test_3.SPEC = [{"match":"foo","body":777087170,"fallthrough":false},{"match":"bar","body":641037838,"fallthrough":false},{"match":"zing","body":1652156613,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_3);

function test_4(x, arr) {
    switch(x) {
    default:
        arr.push(633415567);
    case 'foo':
        arr.push(777087170);
        break;
    case 'bar':
        arr.push(641037838);
        break;
    case 'zing':
        arr.push(1652156613);
        break;
    }
}
test_4.INPUTS = ['foo', 'bar', 'zing'];
test_4.SPEC = [{"match":null,"body":633415567,"fallthrough":true},{"match":"foo","body":777087170,"fallthrough":false},{"match":"bar","body":641037838,"fallthrough":false},{"match":"zing","body":1652156613,"fallthrough":false}];
TESTS.push(test_4);

function test_5(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(777087170);
        break;
    default:
        arr.push(633415567);
    case 'bar':
        arr.push(641037838);
        break;
    case 'zing':
        arr.push(1652156613);
        break;
    }
}
test_5.INPUTS = ['foo', 'bar', 'zing'];
test_5.SPEC = [{"match":"foo","body":777087170,"fallthrough":false},{"match":null,"body":633415567,"fallthrough":true},{"match":"bar","body":641037838,"fallthrough":false},{"match":"zing","body":1652156613,"fallthrough":false}];
TESTS.push(test_5);

function test_6(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(777087170);
        break;
    case 'bar':
        arr.push(641037838);
        break;
    case 'zing':
        arr.push(1652156613);
        break;
    default:
        arr.push(633415567);
    }
}
test_6.INPUTS = ['foo', 'bar', 'zing'];
test_6.SPEC = [{"match":"foo","body":777087170,"fallthrough":false},{"match":"bar","body":641037838,"fallthrough":false},{"match":"zing","body":1652156613,"fallthrough":false},{"match":null,"body":633415567,"fallthrough":true}];
TESTS.push(test_6);

function test_7(x, arr) {
    switch(x) {
    default:
        arr.push('5zO^Qj');
        break;
    case 'foo':
        arr.push(777087170);
        break;
    case 'bar':
        arr.push(641037838);
        break;
    case 'zing':
        arr.push(1652156613);
        break;
    }
}
test_7.INPUTS = ['foo', 'bar', 'zing'];
test_7.SPEC = [{"match":null,"body":"5zO^Qj","fallthrough":false},{"match":"foo","body":777087170,"fallthrough":false},{"match":"bar","body":641037838,"fallthrough":false},{"match":"zing","body":1652156613,"fallthrough":false}];
TESTS.push(test_7);

function test_8(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(777087170);
        break;
    default:
        arr.push('5zO^Qj');
        break;
    case 'bar':
        arr.push(641037838);
        break;
    case 'zing':
        arr.push(1652156613);
        break;
    }
}
test_8.INPUTS = ['foo', 'bar', 'zing'];
test_8.SPEC = [{"match":"foo","body":777087170,"fallthrough":false},{"match":null,"body":"5zO^Qj","fallthrough":false},{"match":"bar","body":641037838,"fallthrough":false},{"match":"zing","body":1652156613,"fallthrough":false}];
TESTS.push(test_8);

function test_9(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(777087170);
        break;
    case 'bar':
        arr.push(641037838);
        break;
    case 'zing':
        arr.push(1652156613);
        break;
    default:
        arr.push('5zO^Qj');
        break;
    }
}
test_9.INPUTS = ['foo', 'bar', 'zing'];
test_9.SPEC = [{"match":"foo","body":777087170,"fallthrough":false},{"match":"bar","body":641037838,"fallthrough":false},{"match":"zing","body":1652156613,"fallthrough":false},{"match":null,"body":"5zO^Qj","fallthrough":false}];
TESTS.push(test_9);

function test_10(x, arr) {
    switch(x) {
    default:
    case 'foo':
        break;
    case 'bar':
        arr.push('c');
        break;
    case 'zing':
        arr.push(2008006064);
        break;
    }
}
test_10.INPUTS = ['foo', 'bar', 'zing'];
test_10.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"c","fallthrough":false},{"match":"zing","body":2008006064,"fallthrough":false}];
TESTS.push(test_10);

function test_11(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
    case 'bar':
        arr.push('c');
        break;
    case 'zing':
        arr.push(2008006064);
        break;
    }
}
test_11.INPUTS = ['foo', 'bar', 'zing'];
test_11.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":"c","fallthrough":false},{"match":"zing","body":2008006064,"fallthrough":false}];
TESTS.push(test_11);

function test_12(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        arr.push('c');
        break;
    case 'zing':
        arr.push(2008006064);
        break;
    default:
    }
}
test_12.INPUTS = ['foo', 'bar', 'zing'];
test_12.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"c","fallthrough":false},{"match":"zing","body":2008006064,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_12);

function test_13(x, arr) {
    switch(x) {
    default:
        arr.push('M');
    case 'foo':
        break;
    case 'bar':
        arr.push('c');
        break;
    case 'zing':
        arr.push(2008006064);
        break;
    }
}
test_13.INPUTS = ['foo', 'bar', 'zing'];
test_13.SPEC = [{"match":null,"body":"M","fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"c","fallthrough":false},{"match":"zing","body":2008006064,"fallthrough":false}];
TESTS.push(test_13);

function test_14(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('M');
    case 'bar':
        arr.push('c');
        break;
    case 'zing':
        arr.push(2008006064);
        break;
    }
}
test_14.INPUTS = ['foo', 'bar', 'zing'];
test_14.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"M","fallthrough":true},{"match":"bar","body":"c","fallthrough":false},{"match":"zing","body":2008006064,"fallthrough":false}];
TESTS.push(test_14);

function test_15(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        arr.push('c');
        break;
    case 'zing':
        arr.push(2008006064);
        break;
    default:
        arr.push('M');
    }
}
test_15.INPUTS = ['foo', 'bar', 'zing'];
test_15.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"c","fallthrough":false},{"match":"zing","body":2008006064,"fallthrough":false},{"match":null,"body":"M","fallthrough":true}];
TESTS.push(test_15);

function test_16(x, arr) {
    switch(x) {
    default:
        arr.push(1424069880);
        break;
    case 'foo':
        break;
    case 'bar':
        arr.push('c');
        break;
    case 'zing':
        arr.push(2008006064);
        break;
    }
}
test_16.INPUTS = ['foo', 'bar', 'zing'];
test_16.SPEC = [{"match":null,"body":1424069880,"fallthrough":false},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"c","fallthrough":false},{"match":"zing","body":2008006064,"fallthrough":false}];
TESTS.push(test_16);

function test_17(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push(1424069880);
        break;
    case 'bar':
        arr.push('c');
        break;
    case 'zing':
        arr.push(2008006064);
        break;
    }
}
test_17.INPUTS = ['foo', 'bar', 'zing'];
test_17.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":1424069880,"fallthrough":false},{"match":"bar","body":"c","fallthrough":false},{"match":"zing","body":2008006064,"fallthrough":false}];
TESTS.push(test_17);

function test_18(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        arr.push('c');
        break;
    case 'zing':
        arr.push(2008006064);
        break;
    default:
        arr.push(1424069880);
        break;
    }
}
test_18.INPUTS = ['foo', 'bar', 'zing'];
test_18.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"c","fallthrough":false},{"match":"zing","body":2008006064,"fallthrough":false},{"match":null,"body":1424069880,"fallthrough":false}];
TESTS.push(test_18);

function test_19(x, arr) {
    switch(x) {
    default:
    case 'foo':
    case 'bar':
        arr.push(1915689729);
        break;
    case 'zing':
        arr.push(973913896);
        break;
    }
}
test_19.INPUTS = ['foo', 'bar', 'zing'];
test_19.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":1915689729,"fallthrough":false},{"match":"zing","body":973913896,"fallthrough":false}];
TESTS.push(test_19);

function test_20(x, arr) {
    switch(x) {
    case 'foo':
    default:
    case 'bar':
        arr.push(1915689729);
        break;
    case 'zing':
        arr.push(973913896);
        break;
    }
}
test_20.INPUTS = ['foo', 'bar', 'zing'];
test_20.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":1915689729,"fallthrough":false},{"match":"zing","body":973913896,"fallthrough":false}];
TESTS.push(test_20);

function test_21(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        arr.push(1915689729);
        break;
    case 'zing':
        arr.push(973913896);
        break;
    default:
    }
}
test_21.INPUTS = ['foo', 'bar', 'zing'];
test_21.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":1915689729,"fallthrough":false},{"match":"zing","body":973913896,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_21);

function test_22(x, arr) {
    switch(x) {
    default:
        arr.push(104770589);
    case 'foo':
    case 'bar':
        arr.push(1915689729);
        break;
    case 'zing':
        arr.push(973913896);
        break;
    }
}
test_22.INPUTS = ['foo', 'bar', 'zing'];
test_22.SPEC = [{"match":null,"body":104770589,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":1915689729,"fallthrough":false},{"match":"zing","body":973913896,"fallthrough":false}];
TESTS.push(test_22);

function test_23(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push(104770589);
    case 'bar':
        arr.push(1915689729);
        break;
    case 'zing':
        arr.push(973913896);
        break;
    }
}
test_23.INPUTS = ['foo', 'bar', 'zing'];
test_23.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":104770589,"fallthrough":true},{"match":"bar","body":1915689729,"fallthrough":false},{"match":"zing","body":973913896,"fallthrough":false}];
TESTS.push(test_23);

function test_24(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        arr.push(1915689729);
        break;
    case 'zing':
        arr.push(973913896);
        break;
    default:
        arr.push(104770589);
    }
}
test_24.INPUTS = ['foo', 'bar', 'zing'];
test_24.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":1915689729,"fallthrough":false},{"match":"zing","body":973913896,"fallthrough":false},{"match":null,"body":104770589,"fallthrough":true}];
TESTS.push(test_24);

function test_25(x, arr) {
    switch(x) {
    default:
        arr.push(304532507);
        break;
    case 'foo':
    case 'bar':
        arr.push(1915689729);
        break;
    case 'zing':
        arr.push(973913896);
        break;
    }
}
test_25.INPUTS = ['foo', 'bar', 'zing'];
test_25.SPEC = [{"match":null,"body":304532507,"fallthrough":false},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":1915689729,"fallthrough":false},{"match":"zing","body":973913896,"fallthrough":false}];
TESTS.push(test_25);

function test_26(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push(304532507);
        break;
    case 'bar':
        arr.push(1915689729);
        break;
    case 'zing':
        arr.push(973913896);
        break;
    }
}
test_26.INPUTS = ['foo', 'bar', 'zing'];
test_26.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":304532507,"fallthrough":false},{"match":"bar","body":1915689729,"fallthrough":false},{"match":"zing","body":973913896,"fallthrough":false}];
TESTS.push(test_26);

function test_27(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        arr.push(1915689729);
        break;
    case 'zing':
        arr.push(973913896);
        break;
    default:
        arr.push(304532507);
        break;
    }
}
test_27.INPUTS = ['foo', 'bar', 'zing'];
test_27.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":1915689729,"fallthrough":false},{"match":"zing","body":973913896,"fallthrough":false},{"match":null,"body":304532507,"fallthrough":false}];
TESTS.push(test_27);

function test_28(x, arr) {
    switch(x) {
    default:
    case 'foo':
        arr.push(2116660419);
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('FvxWZ');
        break;
    }
}
test_28.INPUTS = ['foo', 'bar', 'zing'];
test_28.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":2116660419,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"FvxWZ","fallthrough":false}];
TESTS.push(test_28);

function test_29(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(2116660419);
        break;
    default:
    case 'bar':
        break;
    case 'zing':
        arr.push('FvxWZ');
        break;
    }
}
test_29.INPUTS = ['foo', 'bar', 'zing'];
test_29.SPEC = [{"match":"foo","body":2116660419,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"FvxWZ","fallthrough":false}];
TESTS.push(test_29);

function test_30(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(2116660419);
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('FvxWZ');
        break;
    default:
    }
}
test_30.INPUTS = ['foo', 'bar', 'zing'];
test_30.SPEC = [{"match":"foo","body":2116660419,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"FvxWZ","fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_30);

function test_31(x, arr) {
    switch(x) {
    default:
        arr.push(121730727);
    case 'foo':
        arr.push(2116660419);
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('FvxWZ');
        break;
    }
}
test_31.INPUTS = ['foo', 'bar', 'zing'];
test_31.SPEC = [{"match":null,"body":121730727,"fallthrough":true},{"match":"foo","body":2116660419,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"FvxWZ","fallthrough":false}];
TESTS.push(test_31);

function test_32(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(2116660419);
        break;
    default:
        arr.push(121730727);
    case 'bar':
        break;
    case 'zing':
        arr.push('FvxWZ');
        break;
    }
}
test_32.INPUTS = ['foo', 'bar', 'zing'];
test_32.SPEC = [{"match":"foo","body":2116660419,"fallthrough":false},{"match":null,"body":121730727,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"FvxWZ","fallthrough":false}];
TESTS.push(test_32);

function test_33(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(2116660419);
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('FvxWZ');
        break;
    default:
        arr.push(121730727);
    }
}
test_33.INPUTS = ['foo', 'bar', 'zing'];
test_33.SPEC = [{"match":"foo","body":2116660419,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"FvxWZ","fallthrough":false},{"match":null,"body":121730727,"fallthrough":true}];
TESTS.push(test_33);

function test_34(x, arr) {
    switch(x) {
    default:
        arr.push(1614107154);
        break;
    case 'foo':
        arr.push(2116660419);
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('FvxWZ');
        break;
    }
}
test_34.INPUTS = ['foo', 'bar', 'zing'];
test_34.SPEC = [{"match":null,"body":1614107154,"fallthrough":false},{"match":"foo","body":2116660419,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"FvxWZ","fallthrough":false}];
TESTS.push(test_34);

function test_35(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(2116660419);
        break;
    default:
        arr.push(1614107154);
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('FvxWZ');
        break;
    }
}
test_35.INPUTS = ['foo', 'bar', 'zing'];
test_35.SPEC = [{"match":"foo","body":2116660419,"fallthrough":false},{"match":null,"body":1614107154,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"FvxWZ","fallthrough":false}];
TESTS.push(test_35);

function test_36(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(2116660419);
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('FvxWZ');
        break;
    default:
        arr.push(1614107154);
        break;
    }
}
test_36.INPUTS = ['foo', 'bar', 'zing'];
test_36.SPEC = [{"match":"foo","body":2116660419,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"FvxWZ","fallthrough":false},{"match":null,"body":1614107154,"fallthrough":false}];
TESTS.push(test_36);

function test_37(x, arr) {
    switch(x) {
    default:
    case 'foo':
        arr.push('-=Z');
        break;
    case 'bar':
    case 'zing':
        arr.push('R8f');
        break;
    }
}
test_37.INPUTS = ['foo', 'bar', 'zing'];
test_37.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":"-=Z","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"R8f","fallthrough":false}];
TESTS.push(test_37);

function test_38(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('-=Z');
        break;
    default:
    case 'bar':
    case 'zing':
        arr.push('R8f');
        break;
    }
}
test_38.INPUTS = ['foo', 'bar', 'zing'];
test_38.SPEC = [{"match":"foo","body":"-=Z","fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"R8f","fallthrough":false}];
TESTS.push(test_38);

function test_39(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('-=Z');
        break;
    case 'bar':
    case 'zing':
        arr.push('R8f');
        break;
    default:
    }
}
test_39.INPUTS = ['foo', 'bar', 'zing'];
test_39.SPEC = [{"match":"foo","body":"-=Z","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"R8f","fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_39);

function test_40(x, arr) {
    switch(x) {
    default:
        arr.push('XfrKO0');
    case 'foo':
        arr.push('-=Z');
        break;
    case 'bar':
    case 'zing':
        arr.push('R8f');
        break;
    }
}
test_40.INPUTS = ['foo', 'bar', 'zing'];
test_40.SPEC = [{"match":null,"body":"XfrKO0","fallthrough":true},{"match":"foo","body":"-=Z","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"R8f","fallthrough":false}];
TESTS.push(test_40);

function test_41(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('-=Z');
        break;
    default:
        arr.push('XfrKO0');
    case 'bar':
    case 'zing':
        arr.push('R8f');
        break;
    }
}
test_41.INPUTS = ['foo', 'bar', 'zing'];
test_41.SPEC = [{"match":"foo","body":"-=Z","fallthrough":false},{"match":null,"body":"XfrKO0","fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"R8f","fallthrough":false}];
TESTS.push(test_41);

function test_42(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('-=Z');
        break;
    case 'bar':
    case 'zing':
        arr.push('R8f');
        break;
    default:
        arr.push('XfrKO0');
    }
}
test_42.INPUTS = ['foo', 'bar', 'zing'];
test_42.SPEC = [{"match":"foo","body":"-=Z","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"R8f","fallthrough":false},{"match":null,"body":"XfrKO0","fallthrough":true}];
TESTS.push(test_42);

function test_43(x, arr) {
    switch(x) {
    default:
        arr.push(465477587);
        break;
    case 'foo':
        arr.push('-=Z');
        break;
    case 'bar':
    case 'zing':
        arr.push('R8f');
        break;
    }
}
test_43.INPUTS = ['foo', 'bar', 'zing'];
test_43.SPEC = [{"match":null,"body":465477587,"fallthrough":false},{"match":"foo","body":"-=Z","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"R8f","fallthrough":false}];
TESTS.push(test_43);

function test_44(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('-=Z');
        break;
    default:
        arr.push(465477587);
        break;
    case 'bar':
    case 'zing':
        arr.push('R8f');
        break;
    }
}
test_44.INPUTS = ['foo', 'bar', 'zing'];
test_44.SPEC = [{"match":"foo","body":"-=Z","fallthrough":false},{"match":null,"body":465477587,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"R8f","fallthrough":false}];
TESTS.push(test_44);

function test_45(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('-=Z');
        break;
    case 'bar':
    case 'zing':
        arr.push('R8f');
        break;
    default:
        arr.push(465477587);
        break;
    }
}
test_45.INPUTS = ['foo', 'bar', 'zing'];
test_45.SPEC = [{"match":"foo","body":"-=Z","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"R8f","fallthrough":false},{"match":null,"body":465477587,"fallthrough":false}];
TESTS.push(test_45);

function test_46(x, arr) {
    switch(x) {
    default:
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('(0');
        break;
    }
}
test_46.INPUTS = ['foo', 'bar', 'zing'];
test_46.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"(0","fallthrough":false}];
TESTS.push(test_46);

function test_47(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
    case 'bar':
        break;
    case 'zing':
        arr.push('(0');
        break;
    }
}
test_47.INPUTS = ['foo', 'bar', 'zing'];
test_47.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"(0","fallthrough":false}];
TESTS.push(test_47);

function test_48(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('(0');
        break;
    default:
    }
}
test_48.INPUTS = ['foo', 'bar', 'zing'];
test_48.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"(0","fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_48);

function test_49(x, arr) {
    switch(x) {
    default:
        arr.push('{5J~&%)kV');
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('(0');
        break;
    }
}
test_49.INPUTS = ['foo', 'bar', 'zing'];
test_49.SPEC = [{"match":null,"body":"{5J~&%)kV","fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"(0","fallthrough":false}];
TESTS.push(test_49);

function test_50(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('{5J~&%)kV');
    case 'bar':
        break;
    case 'zing':
        arr.push('(0');
        break;
    }
}
test_50.INPUTS = ['foo', 'bar', 'zing'];
test_50.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"{5J~&%)kV","fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"(0","fallthrough":false}];
TESTS.push(test_50);

function test_51(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('(0');
        break;
    default:
        arr.push('{5J~&%)kV');
    }
}
test_51.INPUTS = ['foo', 'bar', 'zing'];
test_51.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"(0","fallthrough":false},{"match":null,"body":"{5J~&%)kV","fallthrough":true}];
TESTS.push(test_51);

function test_52(x, arr) {
    switch(x) {
    default:
        arr.push('V^IbL');
        break;
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('(0');
        break;
    }
}
test_52.INPUTS = ['foo', 'bar', 'zing'];
test_52.SPEC = [{"match":null,"body":"V^IbL","fallthrough":false},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"(0","fallthrough":false}];
TESTS.push(test_52);

function test_53(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('V^IbL');
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('(0');
        break;
    }
}
test_53.INPUTS = ['foo', 'bar', 'zing'];
test_53.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"V^IbL","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"(0","fallthrough":false}];
TESTS.push(test_53);

function test_54(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('(0');
        break;
    default:
        arr.push('V^IbL');
        break;
    }
}
test_54.INPUTS = ['foo', 'bar', 'zing'];
test_54.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"(0","fallthrough":false},{"match":null,"body":"V^IbL","fallthrough":false}];
TESTS.push(test_54);

function test_55(x, arr) {
    switch(x) {
    default:
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        arr.push('4');
        break;
    }
}
test_55.INPUTS = ['foo', 'bar', 'zing'];
test_55.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"4","fallthrough":false}];
TESTS.push(test_55);

function test_56(x, arr) {
    switch(x) {
    case 'foo':
    default:
    case 'bar':
        break;
    case 'zing':
        arr.push('4');
        break;
    }
}
test_56.INPUTS = ['foo', 'bar', 'zing'];
test_56.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"4","fallthrough":false}];
TESTS.push(test_56);

function test_57(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        arr.push('4');
        break;
    default:
    }
}
test_57.INPUTS = ['foo', 'bar', 'zing'];
test_57.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"4","fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_57);

function test_58(x, arr) {
    switch(x) {
    default:
        arr.push('K');
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        arr.push('4');
        break;
    }
}
test_58.INPUTS = ['foo', 'bar', 'zing'];
test_58.SPEC = [{"match":null,"body":"K","fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"4","fallthrough":false}];
TESTS.push(test_58);

function test_59(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push('K');
    case 'bar':
        break;
    case 'zing':
        arr.push('4');
        break;
    }
}
test_59.INPUTS = ['foo', 'bar', 'zing'];
test_59.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":"K","fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"4","fallthrough":false}];
TESTS.push(test_59);

function test_60(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        arr.push('4');
        break;
    default:
        arr.push('K');
    }
}
test_60.INPUTS = ['foo', 'bar', 'zing'];
test_60.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"4","fallthrough":false},{"match":null,"body":"K","fallthrough":true}];
TESTS.push(test_60);

function test_61(x, arr) {
    switch(x) {
    default:
        arr.push(129591787);
        break;
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        arr.push('4');
        break;
    }
}
test_61.INPUTS = ['foo', 'bar', 'zing'];
test_61.SPEC = [{"match":null,"body":129591787,"fallthrough":false},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"4","fallthrough":false}];
TESTS.push(test_61);

function test_62(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push(129591787);
        break;
    case 'bar':
        break;
    case 'zing':
        arr.push('4');
        break;
    }
}
test_62.INPUTS = ['foo', 'bar', 'zing'];
test_62.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":129591787,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"4","fallthrough":false}];
TESTS.push(test_62);

function test_63(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        arr.push('4');
        break;
    default:
        arr.push(129591787);
        break;
    }
}
test_63.INPUTS = ['foo', 'bar', 'zing'];
test_63.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":"4","fallthrough":false},{"match":null,"body":129591787,"fallthrough":false}];
TESTS.push(test_63);

function test_64(x, arr) {
    switch(x) {
    default:
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        arr.push(60518010);
        break;
    }
}
test_64.INPUTS = ['foo', 'bar', 'zing'];
test_64.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":60518010,"fallthrough":false}];
TESTS.push(test_64);

function test_65(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
    case 'bar':
    case 'zing':
        arr.push(60518010);
        break;
    }
}
test_65.INPUTS = ['foo', 'bar', 'zing'];
test_65.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":60518010,"fallthrough":false}];
TESTS.push(test_65);

function test_66(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        arr.push(60518010);
        break;
    default:
    }
}
test_66.INPUTS = ['foo', 'bar', 'zing'];
test_66.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":60518010,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_66);

function test_67(x, arr) {
    switch(x) {
    default:
        arr.push('0]YO]}');
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        arr.push(60518010);
        break;
    }
}
test_67.INPUTS = ['foo', 'bar', 'zing'];
test_67.SPEC = [{"match":null,"body":"0]YO]}","fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":60518010,"fallthrough":false}];
TESTS.push(test_67);

function test_68(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('0]YO]}');
    case 'bar':
    case 'zing':
        arr.push(60518010);
        break;
    }
}
test_68.INPUTS = ['foo', 'bar', 'zing'];
test_68.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"0]YO]}","fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":60518010,"fallthrough":false}];
TESTS.push(test_68);

function test_69(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        arr.push(60518010);
        break;
    default:
        arr.push('0]YO]}');
    }
}
test_69.INPUTS = ['foo', 'bar', 'zing'];
test_69.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":60518010,"fallthrough":false},{"match":null,"body":"0]YO]}","fallthrough":true}];
TESTS.push(test_69);

function test_70(x, arr) {
    switch(x) {
    default:
        arr.push(1222888797);
        break;
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        arr.push(60518010);
        break;
    }
}
test_70.INPUTS = ['foo', 'bar', 'zing'];
test_70.SPEC = [{"match":null,"body":1222888797,"fallthrough":false},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":60518010,"fallthrough":false}];
TESTS.push(test_70);

function test_71(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push(1222888797);
        break;
    case 'bar':
    case 'zing':
        arr.push(60518010);
        break;
    }
}
test_71.INPUTS = ['foo', 'bar', 'zing'];
test_71.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":1222888797,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":60518010,"fallthrough":false}];
TESTS.push(test_71);

function test_72(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        arr.push(60518010);
        break;
    default:
        arr.push(1222888797);
        break;
    }
}
test_72.INPUTS = ['foo', 'bar', 'zing'];
test_72.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":60518010,"fallthrough":false},{"match":null,"body":1222888797,"fallthrough":false}];
TESTS.push(test_72);

function test_73(x, arr) {
    switch(x) {
    default:
    case 'foo':
    case 'bar':
    case 'zing':
        arr.push('ku]^x');
        break;
    }
}
test_73.INPUTS = ['foo', 'bar', 'zing'];
test_73.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"ku]^x","fallthrough":false}];
TESTS.push(test_73);

function test_74(x, arr) {
    switch(x) {
    case 'foo':
    default:
    case 'bar':
    case 'zing':
        arr.push('ku]^x');
        break;
    }
}
test_74.INPUTS = ['foo', 'bar', 'zing'];
test_74.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"ku]^x","fallthrough":false}];
TESTS.push(test_74);

function test_75(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
    case 'zing':
        arr.push('ku]^x');
        break;
    default:
    }
}
test_75.INPUTS = ['foo', 'bar', 'zing'];
test_75.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"ku]^x","fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_75);

function test_76(x, arr) {
    switch(x) {
    default:
        arr.push(1697959342);
    case 'foo':
    case 'bar':
    case 'zing':
        arr.push('ku]^x');
        break;
    }
}
test_76.INPUTS = ['foo', 'bar', 'zing'];
test_76.SPEC = [{"match":null,"body":1697959342,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"ku]^x","fallthrough":false}];
TESTS.push(test_76);

function test_77(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push(1697959342);
    case 'bar':
    case 'zing':
        arr.push('ku]^x');
        break;
    }
}
test_77.INPUTS = ['foo', 'bar', 'zing'];
test_77.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":1697959342,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"ku]^x","fallthrough":false}];
TESTS.push(test_77);

function test_78(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
    case 'zing':
        arr.push('ku]^x');
        break;
    default:
        arr.push(1697959342);
    }
}
test_78.INPUTS = ['foo', 'bar', 'zing'];
test_78.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"ku]^x","fallthrough":false},{"match":null,"body":1697959342,"fallthrough":true}];
TESTS.push(test_78);

function test_79(x, arr) {
    switch(x) {
    default:
        arr.push(2023306409);
        break;
    case 'foo':
    case 'bar':
    case 'zing':
        arr.push('ku]^x');
        break;
    }
}
test_79.INPUTS = ['foo', 'bar', 'zing'];
test_79.SPEC = [{"match":null,"body":2023306409,"fallthrough":false},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"ku]^x","fallthrough":false}];
TESTS.push(test_79);

function test_80(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push(2023306409);
        break;
    case 'bar':
    case 'zing':
        arr.push('ku]^x');
        break;
    }
}
test_80.INPUTS = ['foo', 'bar', 'zing'];
test_80.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":2023306409,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"ku]^x","fallthrough":false}];
TESTS.push(test_80);

function test_81(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
    case 'zing':
        arr.push('ku]^x');
        break;
    default:
        arr.push(2023306409);
        break;
    }
}
test_81.INPUTS = ['foo', 'bar', 'zing'];
test_81.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":"ku]^x","fallthrough":false},{"match":null,"body":2023306409,"fallthrough":false}];
TESTS.push(test_81);

function test_82(x, arr) {
    switch(x) {
    default:
    case 'foo':
        arr.push(588167318);
        break;
    case 'bar':
        arr.push(663884613);
        break;
    case 'zing':
        break;
    }
}
test_82.INPUTS = ['foo', 'bar', 'zing'];
test_82.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":588167318,"fallthrough":false},{"match":"bar","body":663884613,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_82);

function test_83(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(588167318);
        break;
    default:
    case 'bar':
        arr.push(663884613);
        break;
    case 'zing':
        break;
    }
}
test_83.INPUTS = ['foo', 'bar', 'zing'];
test_83.SPEC = [{"match":"foo","body":588167318,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":663884613,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_83);

function test_84(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(588167318);
        break;
    case 'bar':
        arr.push(663884613);
        break;
    case 'zing':
        break;
    default:
    }
}
test_84.INPUTS = ['foo', 'bar', 'zing'];
test_84.SPEC = [{"match":"foo","body":588167318,"fallthrough":false},{"match":"bar","body":663884613,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_84);

function test_85(x, arr) {
    switch(x) {
    default:
        arr.push(1238869146);
    case 'foo':
        arr.push(588167318);
        break;
    case 'bar':
        arr.push(663884613);
        break;
    case 'zing':
        break;
    }
}
test_85.INPUTS = ['foo', 'bar', 'zing'];
test_85.SPEC = [{"match":null,"body":1238869146,"fallthrough":true},{"match":"foo","body":588167318,"fallthrough":false},{"match":"bar","body":663884613,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_85);

function test_86(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(588167318);
        break;
    default:
        arr.push(1238869146);
    case 'bar':
        arr.push(663884613);
        break;
    case 'zing':
        break;
    }
}
test_86.INPUTS = ['foo', 'bar', 'zing'];
test_86.SPEC = [{"match":"foo","body":588167318,"fallthrough":false},{"match":null,"body":1238869146,"fallthrough":true},{"match":"bar","body":663884613,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_86);

function test_87(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(588167318);
        break;
    case 'bar':
        arr.push(663884613);
        break;
    case 'zing':
        break;
    default:
        arr.push(1238869146);
    }
}
test_87.INPUTS = ['foo', 'bar', 'zing'];
test_87.SPEC = [{"match":"foo","body":588167318,"fallthrough":false},{"match":"bar","body":663884613,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":1238869146,"fallthrough":true}];
TESTS.push(test_87);

function test_88(x, arr) {
    switch(x) {
    default:
        arr.push('pOh#');
        break;
    case 'foo':
        arr.push(588167318);
        break;
    case 'bar':
        arr.push(663884613);
        break;
    case 'zing':
        break;
    }
}
test_88.INPUTS = ['foo', 'bar', 'zing'];
test_88.SPEC = [{"match":null,"body":"pOh#","fallthrough":false},{"match":"foo","body":588167318,"fallthrough":false},{"match":"bar","body":663884613,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_88);

function test_89(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(588167318);
        break;
    default:
        arr.push('pOh#');
        break;
    case 'bar':
        arr.push(663884613);
        break;
    case 'zing':
        break;
    }
}
test_89.INPUTS = ['foo', 'bar', 'zing'];
test_89.SPEC = [{"match":"foo","body":588167318,"fallthrough":false},{"match":null,"body":"pOh#","fallthrough":false},{"match":"bar","body":663884613,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_89);

function test_90(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(588167318);
        break;
    case 'bar':
        arr.push(663884613);
        break;
    case 'zing':
        break;
    default:
        arr.push('pOh#');
        break;
    }
}
test_90.INPUTS = ['foo', 'bar', 'zing'];
test_90.SPEC = [{"match":"foo","body":588167318,"fallthrough":false},{"match":"bar","body":663884613,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":"pOh#","fallthrough":false}];
TESTS.push(test_90);

function test_91(x, arr) {
    switch(x) {
    default:
    case 'foo':
        arr.push('Z!I#t');
        break;
    case 'bar':
        arr.push('D');
        break;
    case 'zing':
    }
}
test_91.INPUTS = ['foo', 'bar', 'zing'];
test_91.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":"Z!I#t","fallthrough":false},{"match":"bar","body":"D","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_91);

function test_92(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('Z!I#t');
        break;
    default:
    case 'bar':
        arr.push('D');
        break;
    case 'zing':
    }
}
test_92.INPUTS = ['foo', 'bar', 'zing'];
test_92.SPEC = [{"match":"foo","body":"Z!I#t","fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":"D","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_92);

function test_93(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('Z!I#t');
        break;
    case 'bar':
        arr.push('D');
        break;
    case 'zing':
    default:
    }
}
test_93.INPUTS = ['foo', 'bar', 'zing'];
test_93.SPEC = [{"match":"foo","body":"Z!I#t","fallthrough":false},{"match":"bar","body":"D","fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_93);

function test_94(x, arr) {
    switch(x) {
    default:
        arr.push(63474909);
    case 'foo':
        arr.push('Z!I#t');
        break;
    case 'bar':
        arr.push('D');
        break;
    case 'zing':
    }
}
test_94.INPUTS = ['foo', 'bar', 'zing'];
test_94.SPEC = [{"match":null,"body":63474909,"fallthrough":true},{"match":"foo","body":"Z!I#t","fallthrough":false},{"match":"bar","body":"D","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_94);

function test_95(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('Z!I#t');
        break;
    default:
        arr.push(63474909);
    case 'bar':
        arr.push('D');
        break;
    case 'zing':
    }
}
test_95.INPUTS = ['foo', 'bar', 'zing'];
test_95.SPEC = [{"match":"foo","body":"Z!I#t","fallthrough":false},{"match":null,"body":63474909,"fallthrough":true},{"match":"bar","body":"D","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_95);

function test_96(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('Z!I#t');
        break;
    case 'bar':
        arr.push('D');
        break;
    case 'zing':
    default:
        arr.push(63474909);
    }
}
test_96.INPUTS = ['foo', 'bar', 'zing'];
test_96.SPEC = [{"match":"foo","body":"Z!I#t","fallthrough":false},{"match":"bar","body":"D","fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":63474909,"fallthrough":true}];
TESTS.push(test_96);

function test_97(x, arr) {
    switch(x) {
    default:
        arr.push(1165220694);
        break;
    case 'foo':
        arr.push('Z!I#t');
        break;
    case 'bar':
        arr.push('D');
        break;
    case 'zing':
    }
}
test_97.INPUTS = ['foo', 'bar', 'zing'];
test_97.SPEC = [{"match":null,"body":1165220694,"fallthrough":false},{"match":"foo","body":"Z!I#t","fallthrough":false},{"match":"bar","body":"D","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_97);

function test_98(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('Z!I#t');
        break;
    default:
        arr.push(1165220694);
        break;
    case 'bar':
        arr.push('D');
        break;
    case 'zing':
    }
}
test_98.INPUTS = ['foo', 'bar', 'zing'];
test_98.SPEC = [{"match":"foo","body":"Z!I#t","fallthrough":false},{"match":null,"body":1165220694,"fallthrough":false},{"match":"bar","body":"D","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_98);

function test_99(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('Z!I#t');
        break;
    case 'bar':
        arr.push('D');
        break;
    case 'zing':
    default:
        arr.push(1165220694);
        break;
    }
}
test_99.INPUTS = ['foo', 'bar', 'zing'];
test_99.SPEC = [{"match":"foo","body":"Z!I#t","fallthrough":false},{"match":"bar","body":"D","fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":1165220694,"fallthrough":false}];
TESTS.push(test_99);

function test_100(x, arr) {
    switch(x) {
    default:
    case 'foo':
        break;
    case 'bar':
        arr.push(1994756408);
        break;
    case 'zing':
        break;
    }
}
test_100.INPUTS = ['foo', 'bar', 'zing'];
test_100.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":1994756408,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_100);

function test_101(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
    case 'bar':
        arr.push(1994756408);
        break;
    case 'zing':
        break;
    }
}
test_101.INPUTS = ['foo', 'bar', 'zing'];
test_101.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":1994756408,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_101);

function test_102(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        arr.push(1994756408);
        break;
    case 'zing':
        break;
    default:
    }
}
test_102.INPUTS = ['foo', 'bar', 'zing'];
test_102.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":1994756408,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_102);

function test_103(x, arr) {
    switch(x) {
    default:
        arr.push('*8ZYmVI($X');
    case 'foo':
        break;
    case 'bar':
        arr.push(1994756408);
        break;
    case 'zing':
        break;
    }
}
test_103.INPUTS = ['foo', 'bar', 'zing'];
test_103.SPEC = [{"match":null,"body":"*8ZYmVI($X","fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":1994756408,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_103);

function test_104(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('*8ZYmVI($X');
    case 'bar':
        arr.push(1994756408);
        break;
    case 'zing':
        break;
    }
}
test_104.INPUTS = ['foo', 'bar', 'zing'];
test_104.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"*8ZYmVI($X","fallthrough":true},{"match":"bar","body":1994756408,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_104);

function test_105(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        arr.push(1994756408);
        break;
    case 'zing':
        break;
    default:
        arr.push('*8ZYmVI($X');
    }
}
test_105.INPUTS = ['foo', 'bar', 'zing'];
test_105.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":1994756408,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":"*8ZYmVI($X","fallthrough":true}];
TESTS.push(test_105);

function test_106(x, arr) {
    switch(x) {
    default:
        arr.push(207183901);
        break;
    case 'foo':
        break;
    case 'bar':
        arr.push(1994756408);
        break;
    case 'zing':
        break;
    }
}
test_106.INPUTS = ['foo', 'bar', 'zing'];
test_106.SPEC = [{"match":null,"body":207183901,"fallthrough":false},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":1994756408,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_106);

function test_107(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push(207183901);
        break;
    case 'bar':
        arr.push(1994756408);
        break;
    case 'zing':
        break;
    }
}
test_107.INPUTS = ['foo', 'bar', 'zing'];
test_107.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":207183901,"fallthrough":false},{"match":"bar","body":1994756408,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_107);

function test_108(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        arr.push(1994756408);
        break;
    case 'zing':
        break;
    default:
        arr.push(207183901);
        break;
    }
}
test_108.INPUTS = ['foo', 'bar', 'zing'];
test_108.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":1994756408,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":207183901,"fallthrough":false}];
TESTS.push(test_108);

function test_109(x, arr) {
    switch(x) {
    default:
    case 'foo':
    case 'bar':
        arr.push('YJQk');
        break;
    case 'zing':
        break;
    }
}
test_109.INPUTS = ['foo', 'bar', 'zing'];
test_109.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"YJQk","fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_109);

function test_110(x, arr) {
    switch(x) {
    case 'foo':
    default:
    case 'bar':
        arr.push('YJQk');
        break;
    case 'zing':
        break;
    }
}
test_110.INPUTS = ['foo', 'bar', 'zing'];
test_110.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":"YJQk","fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_110);

function test_111(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        arr.push('YJQk');
        break;
    case 'zing':
        break;
    default:
    }
}
test_111.INPUTS = ['foo', 'bar', 'zing'];
test_111.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"YJQk","fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_111);

function test_112(x, arr) {
    switch(x) {
    default:
        arr.push('04mJy');
    case 'foo':
    case 'bar':
        arr.push('YJQk');
        break;
    case 'zing':
        break;
    }
}
test_112.INPUTS = ['foo', 'bar', 'zing'];
test_112.SPEC = [{"match":null,"body":"04mJy","fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"YJQk","fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_112);

function test_113(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push('04mJy');
    case 'bar':
        arr.push('YJQk');
        break;
    case 'zing':
        break;
    }
}
test_113.INPUTS = ['foo', 'bar', 'zing'];
test_113.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":"04mJy","fallthrough":true},{"match":"bar","body":"YJQk","fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_113);

function test_114(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        arr.push('YJQk');
        break;
    case 'zing':
        break;
    default:
        arr.push('04mJy');
    }
}
test_114.INPUTS = ['foo', 'bar', 'zing'];
test_114.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"YJQk","fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":"04mJy","fallthrough":true}];
TESTS.push(test_114);

function test_115(x, arr) {
    switch(x) {
    default:
        arr.push('0NgLbYKr~c');
        break;
    case 'foo':
    case 'bar':
        arr.push('YJQk');
        break;
    case 'zing':
        break;
    }
}
test_115.INPUTS = ['foo', 'bar', 'zing'];
test_115.SPEC = [{"match":null,"body":"0NgLbYKr~c","fallthrough":false},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"YJQk","fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_115);

function test_116(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push('0NgLbYKr~c');
        break;
    case 'bar':
        arr.push('YJQk');
        break;
    case 'zing':
        break;
    }
}
test_116.INPUTS = ['foo', 'bar', 'zing'];
test_116.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":"0NgLbYKr~c","fallthrough":false},{"match":"bar","body":"YJQk","fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_116);

function test_117(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        arr.push('YJQk');
        break;
    case 'zing':
        break;
    default:
        arr.push('0NgLbYKr~c');
        break;
    }
}
test_117.INPUTS = ['foo', 'bar', 'zing'];
test_117.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"YJQk","fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":"0NgLbYKr~c","fallthrough":false}];
TESTS.push(test_117);

function test_118(x, arr) {
    switch(x) {
    default:
    case 'foo':
        break;
    case 'bar':
        arr.push('[^U}J^z');
        break;
    case 'zing':
    }
}
test_118.INPUTS = ['foo', 'bar', 'zing'];
test_118.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"[^U}J^z","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_118);

function test_119(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
    case 'bar':
        arr.push('[^U}J^z');
        break;
    case 'zing':
    }
}
test_119.INPUTS = ['foo', 'bar', 'zing'];
test_119.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":"[^U}J^z","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_119);

function test_120(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        arr.push('[^U}J^z');
        break;
    case 'zing':
    default:
    }
}
test_120.INPUTS = ['foo', 'bar', 'zing'];
test_120.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"[^U}J^z","fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_120);

function test_121(x, arr) {
    switch(x) {
    default:
        arr.push('Y');
    case 'foo':
        break;
    case 'bar':
        arr.push('[^U}J^z');
        break;
    case 'zing':
    }
}
test_121.INPUTS = ['foo', 'bar', 'zing'];
test_121.SPEC = [{"match":null,"body":"Y","fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"[^U}J^z","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_121);

function test_122(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('Y');
    case 'bar':
        arr.push('[^U}J^z');
        break;
    case 'zing':
    }
}
test_122.INPUTS = ['foo', 'bar', 'zing'];
test_122.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"Y","fallthrough":true},{"match":"bar","body":"[^U}J^z","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_122);

function test_123(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        arr.push('[^U}J^z');
        break;
    case 'zing':
    default:
        arr.push('Y');
    }
}
test_123.INPUTS = ['foo', 'bar', 'zing'];
test_123.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"[^U}J^z","fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":"Y","fallthrough":true}];
TESTS.push(test_123);

function test_124(x, arr) {
    switch(x) {
    default:
        arr.push(279382281);
        break;
    case 'foo':
        break;
    case 'bar':
        arr.push('[^U}J^z');
        break;
    case 'zing':
    }
}
test_124.INPUTS = ['foo', 'bar', 'zing'];
test_124.SPEC = [{"match":null,"body":279382281,"fallthrough":false},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"[^U}J^z","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_124);

function test_125(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push(279382281);
        break;
    case 'bar':
        arr.push('[^U}J^z');
        break;
    case 'zing':
    }
}
test_125.INPUTS = ['foo', 'bar', 'zing'];
test_125.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":279382281,"fallthrough":false},{"match":"bar","body":"[^U}J^z","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_125);

function test_126(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        arr.push('[^U}J^z');
        break;
    case 'zing':
    default:
        arr.push(279382281);
        break;
    }
}
test_126.INPUTS = ['foo', 'bar', 'zing'];
test_126.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":"[^U}J^z","fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":279382281,"fallthrough":false}];
TESTS.push(test_126);

function test_127(x, arr) {
    switch(x) {
    default:
    case 'foo':
    case 'bar':
        arr.push('7+leA1');
        break;
    case 'zing':
    }
}
test_127.INPUTS = ['foo', 'bar', 'zing'];
test_127.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"7+leA1","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_127);

function test_128(x, arr) {
    switch(x) {
    case 'foo':
    default:
    case 'bar':
        arr.push('7+leA1');
        break;
    case 'zing':
    }
}
test_128.INPUTS = ['foo', 'bar', 'zing'];
test_128.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":"7+leA1","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_128);

function test_129(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        arr.push('7+leA1');
        break;
    case 'zing':
    default:
    }
}
test_129.INPUTS = ['foo', 'bar', 'zing'];
test_129.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"7+leA1","fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_129);

function test_130(x, arr) {
    switch(x) {
    default:
        arr.push(282691036);
    case 'foo':
    case 'bar':
        arr.push('7+leA1');
        break;
    case 'zing':
    }
}
test_130.INPUTS = ['foo', 'bar', 'zing'];
test_130.SPEC = [{"match":null,"body":282691036,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"7+leA1","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_130);

function test_131(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push(282691036);
    case 'bar':
        arr.push('7+leA1');
        break;
    case 'zing':
    }
}
test_131.INPUTS = ['foo', 'bar', 'zing'];
test_131.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":282691036,"fallthrough":true},{"match":"bar","body":"7+leA1","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_131);

function test_132(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        arr.push('7+leA1');
        break;
    case 'zing':
    default:
        arr.push(282691036);
    }
}
test_132.INPUTS = ['foo', 'bar', 'zing'];
test_132.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"7+leA1","fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":282691036,"fallthrough":true}];
TESTS.push(test_132);

function test_133(x, arr) {
    switch(x) {
    default:
        arr.push('C^kPR');
        break;
    case 'foo':
    case 'bar':
        arr.push('7+leA1');
        break;
    case 'zing':
    }
}
test_133.INPUTS = ['foo', 'bar', 'zing'];
test_133.SPEC = [{"match":null,"body":"C^kPR","fallthrough":false},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"7+leA1","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_133);

function test_134(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push('C^kPR');
        break;
    case 'bar':
        arr.push('7+leA1');
        break;
    case 'zing':
    }
}
test_134.INPUTS = ['foo', 'bar', 'zing'];
test_134.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":"C^kPR","fallthrough":false},{"match":"bar","body":"7+leA1","fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_134);

function test_135(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        arr.push('7+leA1');
        break;
    case 'zing':
    default:
        arr.push('C^kPR');
        break;
    }
}
test_135.INPUTS = ['foo', 'bar', 'zing'];
test_135.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":"7+leA1","fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":"C^kPR","fallthrough":false}];
TESTS.push(test_135);

function test_136(x, arr) {
    switch(x) {
    default:
    case 'foo':
        arr.push(1580091060);
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_136.INPUTS = ['foo', 'bar', 'zing'];
test_136.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":1580091060,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_136);

function test_137(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1580091060);
        break;
    default:
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_137.INPUTS = ['foo', 'bar', 'zing'];
test_137.SPEC = [{"match":"foo","body":1580091060,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_137);

function test_138(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1580091060);
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    default:
    }
}
test_138.INPUTS = ['foo', 'bar', 'zing'];
test_138.SPEC = [{"match":"foo","body":1580091060,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_138);

function test_139(x, arr) {
    switch(x) {
    default:
        arr.push(1822221944);
    case 'foo':
        arr.push(1580091060);
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_139.INPUTS = ['foo', 'bar', 'zing'];
test_139.SPEC = [{"match":null,"body":1822221944,"fallthrough":true},{"match":"foo","body":1580091060,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_139);

function test_140(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1580091060);
        break;
    default:
        arr.push(1822221944);
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_140.INPUTS = ['foo', 'bar', 'zing'];
test_140.SPEC = [{"match":"foo","body":1580091060,"fallthrough":false},{"match":null,"body":1822221944,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_140);

function test_141(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1580091060);
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    default:
        arr.push(1822221944);
    }
}
test_141.INPUTS = ['foo', 'bar', 'zing'];
test_141.SPEC = [{"match":"foo","body":1580091060,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":1822221944,"fallthrough":true}];
TESTS.push(test_141);

function test_142(x, arr) {
    switch(x) {
    default:
        arr.push(1855786158);
        break;
    case 'foo':
        arr.push(1580091060);
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_142.INPUTS = ['foo', 'bar', 'zing'];
test_142.SPEC = [{"match":null,"body":1855786158,"fallthrough":false},{"match":"foo","body":1580091060,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_142);

function test_143(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1580091060);
        break;
    default:
        arr.push(1855786158);
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_143.INPUTS = ['foo', 'bar', 'zing'];
test_143.SPEC = [{"match":"foo","body":1580091060,"fallthrough":false},{"match":null,"body":1855786158,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_143);

function test_144(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1580091060);
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    default:
        arr.push(1855786158);
        break;
    }
}
test_144.INPUTS = ['foo', 'bar', 'zing'];
test_144.SPEC = [{"match":"foo","body":1580091060,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":1855786158,"fallthrough":false}];
TESTS.push(test_144);

function test_145(x, arr) {
    switch(x) {
    default:
    case 'foo':
        arr.push('XO');
        break;
    case 'bar':
    case 'zing':
        break;
    }
}
test_145.INPUTS = ['foo', 'bar', 'zing'];
test_145.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":"XO","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_145);

function test_146(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('XO');
        break;
    default:
    case 'bar':
    case 'zing':
        break;
    }
}
test_146.INPUTS = ['foo', 'bar', 'zing'];
test_146.SPEC = [{"match":"foo","body":"XO","fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_146);

function test_147(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('XO');
        break;
    case 'bar':
    case 'zing':
        break;
    default:
    }
}
test_147.INPUTS = ['foo', 'bar', 'zing'];
test_147.SPEC = [{"match":"foo","body":"XO","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_147);

function test_148(x, arr) {
    switch(x) {
    default:
        arr.push('L');
    case 'foo':
        arr.push('XO');
        break;
    case 'bar':
    case 'zing':
        break;
    }
}
test_148.INPUTS = ['foo', 'bar', 'zing'];
test_148.SPEC = [{"match":null,"body":"L","fallthrough":true},{"match":"foo","body":"XO","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_148);

function test_149(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('XO');
        break;
    default:
        arr.push('L');
    case 'bar':
    case 'zing':
        break;
    }
}
test_149.INPUTS = ['foo', 'bar', 'zing'];
test_149.SPEC = [{"match":"foo","body":"XO","fallthrough":false},{"match":null,"body":"L","fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_149);

function test_150(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('XO');
        break;
    case 'bar':
    case 'zing':
        break;
    default:
        arr.push('L');
    }
}
test_150.INPUTS = ['foo', 'bar', 'zing'];
test_150.SPEC = [{"match":"foo","body":"XO","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":"L","fallthrough":true}];
TESTS.push(test_150);

function test_151(x, arr) {
    switch(x) {
    default:
        arr.push(1118900933);
        break;
    case 'foo':
        arr.push('XO');
        break;
    case 'bar':
    case 'zing':
        break;
    }
}
test_151.INPUTS = ['foo', 'bar', 'zing'];
test_151.SPEC = [{"match":null,"body":1118900933,"fallthrough":false},{"match":"foo","body":"XO","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_151);

function test_152(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('XO');
        break;
    default:
        arr.push(1118900933);
        break;
    case 'bar':
    case 'zing':
        break;
    }
}
test_152.INPUTS = ['foo', 'bar', 'zing'];
test_152.SPEC = [{"match":"foo","body":"XO","fallthrough":false},{"match":null,"body":1118900933,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_152);

function test_153(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('XO');
        break;
    case 'bar':
    case 'zing':
        break;
    default:
        arr.push(1118900933);
        break;
    }
}
test_153.INPUTS = ['foo', 'bar', 'zing'];
test_153.SPEC = [{"match":"foo","body":"XO","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":1118900933,"fallthrough":false}];
TESTS.push(test_153);

function test_154(x, arr) {
    switch(x) {
    default:
    case 'foo':
        arr.push('H@');
        break;
    case 'bar':
        break;
    case 'zing':
    }
}
test_154.INPUTS = ['foo', 'bar', 'zing'];
test_154.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":"H@","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_154);

function test_155(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('H@');
        break;
    default:
    case 'bar':
        break;
    case 'zing':
    }
}
test_155.INPUTS = ['foo', 'bar', 'zing'];
test_155.SPEC = [{"match":"foo","body":"H@","fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_155);

function test_156(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('H@');
        break;
    case 'bar':
        break;
    case 'zing':
    default:
    }
}
test_156.INPUTS = ['foo', 'bar', 'zing'];
test_156.SPEC = [{"match":"foo","body":"H@","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_156);

function test_157(x, arr) {
    switch(x) {
    default:
        arr.push('f8n');
    case 'foo':
        arr.push('H@');
        break;
    case 'bar':
        break;
    case 'zing':
    }
}
test_157.INPUTS = ['foo', 'bar', 'zing'];
test_157.SPEC = [{"match":null,"body":"f8n","fallthrough":true},{"match":"foo","body":"H@","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_157);

function test_158(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('H@');
        break;
    default:
        arr.push('f8n');
    case 'bar':
        break;
    case 'zing':
    }
}
test_158.INPUTS = ['foo', 'bar', 'zing'];
test_158.SPEC = [{"match":"foo","body":"H@","fallthrough":false},{"match":null,"body":"f8n","fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_158);

function test_159(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('H@');
        break;
    case 'bar':
        break;
    case 'zing':
    default:
        arr.push('f8n');
    }
}
test_159.INPUTS = ['foo', 'bar', 'zing'];
test_159.SPEC = [{"match":"foo","body":"H@","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":"f8n","fallthrough":true}];
TESTS.push(test_159);

function test_160(x, arr) {
    switch(x) {
    default:
        arr.push('4rg');
        break;
    case 'foo':
        arr.push('H@');
        break;
    case 'bar':
        break;
    case 'zing':
    }
}
test_160.INPUTS = ['foo', 'bar', 'zing'];
test_160.SPEC = [{"match":null,"body":"4rg","fallthrough":false},{"match":"foo","body":"H@","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_160);

function test_161(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('H@');
        break;
    default:
        arr.push('4rg');
        break;
    case 'bar':
        break;
    case 'zing':
    }
}
test_161.INPUTS = ['foo', 'bar', 'zing'];
test_161.SPEC = [{"match":"foo","body":"H@","fallthrough":false},{"match":null,"body":"4rg","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_161);

function test_162(x, arr) {
    switch(x) {
    case 'foo':
        arr.push('H@');
        break;
    case 'bar':
        break;
    case 'zing':
    default:
        arr.push('4rg');
        break;
    }
}
test_162.INPUTS = ['foo', 'bar', 'zing'];
test_162.SPEC = [{"match":"foo","body":"H@","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":"4rg","fallthrough":false}];
TESTS.push(test_162);

function test_163(x, arr) {
    switch(x) {
    default:
    case 'foo':
        arr.push(1921603085);
        break;
    case 'bar':
    case 'zing':
    }
}
test_163.INPUTS = ['foo', 'bar', 'zing'];
test_163.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":1921603085,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_163);

function test_164(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1921603085);
        break;
    default:
    case 'bar':
    case 'zing':
    }
}
test_164.INPUTS = ['foo', 'bar', 'zing'];
test_164.SPEC = [{"match":"foo","body":1921603085,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_164);

function test_165(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1921603085);
        break;
    case 'bar':
    case 'zing':
    default:
    }
}
test_165.INPUTS = ['foo', 'bar', 'zing'];
test_165.SPEC = [{"match":"foo","body":1921603085,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_165);

function test_166(x, arr) {
    switch(x) {
    default:
        arr.push(2201436);
    case 'foo':
        arr.push(1921603085);
        break;
    case 'bar':
    case 'zing':
    }
}
test_166.INPUTS = ['foo', 'bar', 'zing'];
test_166.SPEC = [{"match":null,"body":2201436,"fallthrough":true},{"match":"foo","body":1921603085,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_166);

function test_167(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1921603085);
        break;
    default:
        arr.push(2201436);
    case 'bar':
    case 'zing':
    }
}
test_167.INPUTS = ['foo', 'bar', 'zing'];
test_167.SPEC = [{"match":"foo","body":1921603085,"fallthrough":false},{"match":null,"body":2201436,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_167);

function test_168(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1921603085);
        break;
    case 'bar':
    case 'zing':
    default:
        arr.push(2201436);
    }
}
test_168.INPUTS = ['foo', 'bar', 'zing'];
test_168.SPEC = [{"match":"foo","body":1921603085,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":2201436,"fallthrough":true}];
TESTS.push(test_168);

function test_169(x, arr) {
    switch(x) {
    default:
        arr.push('(vPssM{');
        break;
    case 'foo':
        arr.push(1921603085);
        break;
    case 'bar':
    case 'zing':
    }
}
test_169.INPUTS = ['foo', 'bar', 'zing'];
test_169.SPEC = [{"match":null,"body":"(vPssM{","fallthrough":false},{"match":"foo","body":1921603085,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_169);

function test_170(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1921603085);
        break;
    default:
        arr.push('(vPssM{');
        break;
    case 'bar':
    case 'zing':
    }
}
test_170.INPUTS = ['foo', 'bar', 'zing'];
test_170.SPEC = [{"match":"foo","body":1921603085,"fallthrough":false},{"match":null,"body":"(vPssM{","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_170);

function test_171(x, arr) {
    switch(x) {
    case 'foo':
        arr.push(1921603085);
        break;
    case 'bar':
    case 'zing':
    default:
        arr.push('(vPssM{');
        break;
    }
}
test_171.INPUTS = ['foo', 'bar', 'zing'];
test_171.SPEC = [{"match":"foo","body":1921603085,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":"(vPssM{","fallthrough":false}];
TESTS.push(test_171);

function test_172(x, arr) {
    switch(x) {
    default:
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_172.INPUTS = ['foo', 'bar', 'zing'];
test_172.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_172);

function test_173(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_173.INPUTS = ['foo', 'bar', 'zing'];
test_173.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_173);

function test_174(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    default:
    }
}
test_174.INPUTS = ['foo', 'bar', 'zing'];
test_174.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_174);

function test_175(x, arr) {
    switch(x) {
    default:
        arr.push('y');
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_175.INPUTS = ['foo', 'bar', 'zing'];
test_175.SPEC = [{"match":null,"body":"y","fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_175);

function test_176(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('y');
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_176.INPUTS = ['foo', 'bar', 'zing'];
test_176.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"y","fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_176);

function test_177(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    default:
        arr.push('y');
    }
}
test_177.INPUTS = ['foo', 'bar', 'zing'];
test_177.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":"y","fallthrough":true}];
TESTS.push(test_177);

function test_178(x, arr) {
    switch(x) {
    default:
        arr.push('H');
        break;
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_178.INPUTS = ['foo', 'bar', 'zing'];
test_178.SPEC = [{"match":null,"body":"H","fallthrough":false},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_178);

function test_179(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('H');
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_179.INPUTS = ['foo', 'bar', 'zing'];
test_179.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"H","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_179);

function test_180(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    default:
        arr.push('H');
        break;
    }
}
test_180.INPUTS = ['foo', 'bar', 'zing'];
test_180.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":"H","fallthrough":false}];
TESTS.push(test_180);

function test_181(x, arr) {
    switch(x) {
    default:
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_181.INPUTS = ['foo', 'bar', 'zing'];
test_181.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_181);

function test_182(x, arr) {
    switch(x) {
    case 'foo':
    default:
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_182.INPUTS = ['foo', 'bar', 'zing'];
test_182.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_182);

function test_183(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        break;
    default:
    }
}
test_183.INPUTS = ['foo', 'bar', 'zing'];
test_183.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_183);

function test_184(x, arr) {
    switch(x) {
    default:
        arr.push('0vM}');
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_184.INPUTS = ['foo', 'bar', 'zing'];
test_184.SPEC = [{"match":null,"body":"0vM}","fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_184);

function test_185(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push('0vM}');
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_185.INPUTS = ['foo', 'bar', 'zing'];
test_185.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":"0vM}","fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_185);

function test_186(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        break;
    default:
        arr.push('0vM}');
    }
}
test_186.INPUTS = ['foo', 'bar', 'zing'];
test_186.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":"0vM}","fallthrough":true}];
TESTS.push(test_186);

function test_187(x, arr) {
    switch(x) {
    default:
        arr.push('jn~d(x');
        break;
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_187.INPUTS = ['foo', 'bar', 'zing'];
test_187.SPEC = [{"match":null,"body":"jn~d(x","fallthrough":false},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_187);

function test_188(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push('jn~d(x');
        break;
    case 'bar':
        break;
    case 'zing':
        break;
    }
}
test_188.INPUTS = ['foo', 'bar', 'zing'];
test_188.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":"jn~d(x","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_188);

function test_189(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        break;
    case 'zing':
        break;
    default:
        arr.push('jn~d(x');
        break;
    }
}
test_189.INPUTS = ['foo', 'bar', 'zing'];
test_189.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":"jn~d(x","fallthrough":false}];
TESTS.push(test_189);

function test_190(x, arr) {
    switch(x) {
    default:
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        break;
    }
}
test_190.INPUTS = ['foo', 'bar', 'zing'];
test_190.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_190);

function test_191(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
    case 'bar':
    case 'zing':
        break;
    }
}
test_191.INPUTS = ['foo', 'bar', 'zing'];
test_191.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_191);

function test_192(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        break;
    default:
    }
}
test_192.INPUTS = ['foo', 'bar', 'zing'];
test_192.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_192);

function test_193(x, arr) {
    switch(x) {
    default:
        arr.push('[');
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        break;
    }
}
test_193.INPUTS = ['foo', 'bar', 'zing'];
test_193.SPEC = [{"match":null,"body":"[","fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_193);

function test_194(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('[');
    case 'bar':
    case 'zing':
        break;
    }
}
test_194.INPUTS = ['foo', 'bar', 'zing'];
test_194.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"[","fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_194);

function test_195(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        break;
    default:
        arr.push('[');
    }
}
test_195.INPUTS = ['foo', 'bar', 'zing'];
test_195.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":"[","fallthrough":true}];
TESTS.push(test_195);

function test_196(x, arr) {
    switch(x) {
    default:
        arr.push('3DbGY');
        break;
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        break;
    }
}
test_196.INPUTS = ['foo', 'bar', 'zing'];
test_196.SPEC = [{"match":null,"body":"3DbGY","fallthrough":false},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_196);

function test_197(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('3DbGY');
        break;
    case 'bar':
    case 'zing':
        break;
    }
}
test_197.INPUTS = ['foo', 'bar', 'zing'];
test_197.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"3DbGY","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_197);

function test_198(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
    case 'zing':
        break;
    default:
        arr.push('3DbGY');
        break;
    }
}
test_198.INPUTS = ['foo', 'bar', 'zing'];
test_198.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":"3DbGY","fallthrough":false}];
TESTS.push(test_198);

function test_199(x, arr) {
    switch(x) {
    default:
    case 'foo':
    case 'bar':
    case 'zing':
        break;
    }
}
test_199.INPUTS = ['foo', 'bar', 'zing'];
test_199.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_199);

function test_200(x, arr) {
    switch(x) {
    case 'foo':
    default:
    case 'bar':
    case 'zing':
        break;
    }
}
test_200.INPUTS = ['foo', 'bar', 'zing'];
test_200.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_200);

function test_201(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
    case 'zing':
        break;
    default:
    }
}
test_201.INPUTS = ['foo', 'bar', 'zing'];
test_201.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_201);

function test_202(x, arr) {
    switch(x) {
    default:
        arr.push(1320190826);
    case 'foo':
    case 'bar':
    case 'zing':
        break;
    }
}
test_202.INPUTS = ['foo', 'bar', 'zing'];
test_202.SPEC = [{"match":null,"body":1320190826,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_202);

function test_203(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push(1320190826);
    case 'bar':
    case 'zing':
        break;
    }
}
test_203.INPUTS = ['foo', 'bar', 'zing'];
test_203.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":1320190826,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_203);

function test_204(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
    case 'zing':
        break;
    default:
        arr.push(1320190826);
    }
}
test_204.INPUTS = ['foo', 'bar', 'zing'];
test_204.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":1320190826,"fallthrough":true}];
TESTS.push(test_204);

function test_205(x, arr) {
    switch(x) {
    default:
        arr.push(1211439111);
        break;
    case 'foo':
    case 'bar':
    case 'zing':
        break;
    }
}
test_205.INPUTS = ['foo', 'bar', 'zing'];
test_205.SPEC = [{"match":null,"body":1211439111,"fallthrough":false},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_205);

function test_206(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push(1211439111);
        break;
    case 'bar':
    case 'zing':
        break;
    }
}
test_206.INPUTS = ['foo', 'bar', 'zing'];
test_206.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":1211439111,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false}];
TESTS.push(test_206);

function test_207(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
    case 'zing':
        break;
    default:
        arr.push(1211439111);
        break;
    }
}
test_207.INPUTS = ['foo', 'bar', 'zing'];
test_207.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":false},{"match":null,"body":1211439111,"fallthrough":false}];
TESTS.push(test_207);

function test_208(x, arr) {
    switch(x) {
    default:
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
    }
}
test_208.INPUTS = ['foo', 'bar', 'zing'];
test_208.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_208);

function test_209(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
    case 'bar':
        break;
    case 'zing':
    }
}
test_209.INPUTS = ['foo', 'bar', 'zing'];
test_209.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_209);

function test_210(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
    default:
    }
}
test_210.INPUTS = ['foo', 'bar', 'zing'];
test_210.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_210);

function test_211(x, arr) {
    switch(x) {
    default:
        arr.push(1547874695);
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
    }
}
test_211.INPUTS = ['foo', 'bar', 'zing'];
test_211.SPEC = [{"match":null,"body":1547874695,"fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_211);

function test_212(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push(1547874695);
    case 'bar':
        break;
    case 'zing':
    }
}
test_212.INPUTS = ['foo', 'bar', 'zing'];
test_212.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":1547874695,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_212);

function test_213(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
    default:
        arr.push(1547874695);
    }
}
test_213.INPUTS = ['foo', 'bar', 'zing'];
test_213.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":1547874695,"fallthrough":true}];
TESTS.push(test_213);

function test_214(x, arr) {
    switch(x) {
    default:
        arr.push('@_2GFlnK=t');
        break;
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
    }
}
test_214.INPUTS = ['foo', 'bar', 'zing'];
test_214.SPEC = [{"match":null,"body":"@_2GFlnK=t","fallthrough":false},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_214);

function test_215(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('@_2GFlnK=t');
        break;
    case 'bar':
        break;
    case 'zing':
    }
}
test_215.INPUTS = ['foo', 'bar', 'zing'];
test_215.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"@_2GFlnK=t","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_215);

function test_216(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
        break;
    case 'zing':
    default:
        arr.push('@_2GFlnK=t');
        break;
    }
}
test_216.INPUTS = ['foo', 'bar', 'zing'];
test_216.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":"@_2GFlnK=t","fallthrough":false}];
TESTS.push(test_216);

function test_217(x, arr) {
    switch(x) {
    default:
    case 'foo':
    case 'bar':
        break;
    case 'zing':
    }
}
test_217.INPUTS = ['foo', 'bar', 'zing'];
test_217.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_217);

function test_218(x, arr) {
    switch(x) {
    case 'foo':
    default:
    case 'bar':
        break;
    case 'zing':
    }
}
test_218.INPUTS = ['foo', 'bar', 'zing'];
test_218.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_218);

function test_219(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        break;
    case 'zing':
    default:
    }
}
test_219.INPUTS = ['foo', 'bar', 'zing'];
test_219.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_219);

function test_220(x, arr) {
    switch(x) {
    default:
        arr.push('~C$');
    case 'foo':
    case 'bar':
        break;
    case 'zing':
    }
}
test_220.INPUTS = ['foo', 'bar', 'zing'];
test_220.SPEC = [{"match":null,"body":"~C$","fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_220);

function test_221(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push('~C$');
    case 'bar':
        break;
    case 'zing':
    }
}
test_221.INPUTS = ['foo', 'bar', 'zing'];
test_221.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":"~C$","fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_221);

function test_222(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        break;
    case 'zing':
    default:
        arr.push('~C$');
    }
}
test_222.INPUTS = ['foo', 'bar', 'zing'];
test_222.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":"~C$","fallthrough":true}];
TESTS.push(test_222);

function test_223(x, arr) {
    switch(x) {
    default:
        arr.push('2sfo%');
        break;
    case 'foo':
    case 'bar':
        break;
    case 'zing':
    }
}
test_223.INPUTS = ['foo', 'bar', 'zing'];
test_223.SPEC = [{"match":null,"body":"2sfo%","fallthrough":false},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_223);

function test_224(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push('2sfo%');
        break;
    case 'bar':
        break;
    case 'zing':
    }
}
test_224.INPUTS = ['foo', 'bar', 'zing'];
test_224.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":"2sfo%","fallthrough":false},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_224);

function test_225(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
        break;
    case 'zing':
    default:
        arr.push('2sfo%');
        break;
    }
}
test_225.INPUTS = ['foo', 'bar', 'zing'];
test_225.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":false},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":"2sfo%","fallthrough":false}];
TESTS.push(test_225);

function test_226(x, arr) {
    switch(x) {
    default:
    case 'foo':
        break;
    case 'bar':
    case 'zing':
    }
}
test_226.INPUTS = ['foo', 'bar', 'zing'];
test_226.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_226);

function test_227(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
    case 'bar':
    case 'zing':
    }
}
test_227.INPUTS = ['foo', 'bar', 'zing'];
test_227.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_227);

function test_228(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
    case 'zing':
    default:
    }
}
test_228.INPUTS = ['foo', 'bar', 'zing'];
test_228.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_228);

function test_229(x, arr) {
    switch(x) {
    default:
        arr.push(1637942279);
    case 'foo':
        break;
    case 'bar':
    case 'zing':
    }
}
test_229.INPUTS = ['foo', 'bar', 'zing'];
test_229.SPEC = [{"match":null,"body":1637942279,"fallthrough":true},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_229);

function test_230(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push(1637942279);
    case 'bar':
    case 'zing':
    }
}
test_230.INPUTS = ['foo', 'bar', 'zing'];
test_230.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":1637942279,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_230);

function test_231(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
    case 'zing':
    default:
        arr.push(1637942279);
    }
}
test_231.INPUTS = ['foo', 'bar', 'zing'];
test_231.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":1637942279,"fallthrough":true}];
TESTS.push(test_231);

function test_232(x, arr) {
    switch(x) {
    default:
        arr.push('4E!jR');
        break;
    case 'foo':
        break;
    case 'bar':
    case 'zing':
    }
}
test_232.INPUTS = ['foo', 'bar', 'zing'];
test_232.SPEC = [{"match":null,"body":"4E!jR","fallthrough":false},{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_232);

function test_233(x, arr) {
    switch(x) {
    case 'foo':
        break;
    default:
        arr.push('4E!jR');
        break;
    case 'bar':
    case 'zing':
    }
}
test_233.INPUTS = ['foo', 'bar', 'zing'];
test_233.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":null,"body":"4E!jR","fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_233);

function test_234(x, arr) {
    switch(x) {
    case 'foo':
        break;
    case 'bar':
    case 'zing':
    default:
        arr.push('4E!jR');
        break;
    }
}
test_234.INPUTS = ['foo', 'bar', 'zing'];
test_234.SPEC = [{"match":"foo","body":null,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":"4E!jR","fallthrough":false}];
TESTS.push(test_234);

function test_235(x, arr) {
    switch(x) {
    default:
    case 'foo':
    case 'bar':
    case 'zing':
    }
}
test_235.INPUTS = ['foo', 'bar', 'zing'];
test_235.SPEC = [{"match":null,"body":null,"fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_235);

function test_236(x, arr) {
    switch(x) {
    case 'foo':
    default:
    case 'bar':
    case 'zing':
    }
}
test_236.INPUTS = ['foo', 'bar', 'zing'];
test_236.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_236);

function test_237(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
    case 'zing':
    default:
    }
}
test_237.INPUTS = ['foo', 'bar', 'zing'];
test_237.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":null,"fallthrough":true}];
TESTS.push(test_237);

function test_238(x, arr) {
    switch(x) {
    default:
        arr.push(')fSNzp06');
    case 'foo':
    case 'bar':
    case 'zing':
    }
}
test_238.INPUTS = ['foo', 'bar', 'zing'];
test_238.SPEC = [{"match":null,"body":")fSNzp06","fallthrough":true},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_238);

function test_239(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push(')fSNzp06');
    case 'bar':
    case 'zing':
    }
}
test_239.INPUTS = ['foo', 'bar', 'zing'];
test_239.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":")fSNzp06","fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_239);

function test_240(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
    case 'zing':
    default:
        arr.push(')fSNzp06');
    }
}
test_240.INPUTS = ['foo', 'bar', 'zing'];
test_240.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":")fSNzp06","fallthrough":true}];
TESTS.push(test_240);

function test_241(x, arr) {
    switch(x) {
    default:
        arr.push(974910083);
        break;
    case 'foo':
    case 'bar':
    case 'zing':
    }
}
test_241.INPUTS = ['foo', 'bar', 'zing'];
test_241.SPEC = [{"match":null,"body":974910083,"fallthrough":false},{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_241);

function test_242(x, arr) {
    switch(x) {
    case 'foo':
    default:
        arr.push(974910083);
        break;
    case 'bar':
    case 'zing':
    }
}
test_242.INPUTS = ['foo', 'bar', 'zing'];
test_242.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":null,"body":974910083,"fallthrough":false},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true}];
TESTS.push(test_242);

function test_243(x, arr) {
    switch(x) {
    case 'foo':
    case 'bar':
    case 'zing':
    default:
        arr.push(974910083);
        break;
    }
}
test_243.INPUTS = ['foo', 'bar', 'zing'];
test_243.SPEC = [{"match":"foo","body":null,"fallthrough":true},{"match":"bar","body":null,"fallthrough":true},{"match":"zing","body":null,"fallthrough":true},{"match":null,"body":974910083,"fallthrough":false}];
TESTS.push(test_243);


/////////////////////////////////////////
// RUNNER                              //
/////////////////////////////////////////

for(var i = 0; i < TESTS.length; i++) {
  RunTest(TESTS[i]);
}

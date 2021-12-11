
/**
 * A test case spec is an array of objects of the following kind:
 *  { 'match': Num|Str|Null,
 *    'body': Num|Str|Null,
 *    'fallthrough': Boolean }
 *
 * If the 'match' is null, then it represents a 'default:'
 * If the 'match' is not null, it represents a 'case X:' where X is the value.
 * If the 'body' is null, then it means that the case body is empty.  Otherwise,
 * it means that the case body is a single 'arr.push(V);' where "arr" is an input
 * array to the function containing the switch statement, and V is the value.
 * If 'fallthrough' is false, then the body is terminated with a break, otherwise
 * it is not.
 *
 * So a spec: [{'match':3, 'body':null, 'fallthrough':false}, {'match':null, 'body':"foo", 'fallthrough':true}]
 * Represents a switch function:
 *  function(x, arr) {
 *      switch(x) {
 *      case 3:
 *          break;
 *      default:
 *          arr.push('foo');
 *      }
 *  }
 *
 * GenerateSpecPermutes generates a bunch of relevant specs, using the given case match-values,
 * and appends them to result the array passed into it.
 *
 * InterpretSwitch takes a spec, a value, and a result array, and behaves as if the switch specified
 * by the spec had been called on the value and the result array.
 *
 * VerifySwitchSpec is there but not used in the code.  I was using it while testing the test case
 * generator.  It verifies that a switch spec is sane.
 *
 * RunSpec uses eval to run the test directly.  It's not used currently.
 *
 * GenerateSwitchCode generates a string of the form "function NAME(x, arg) { .... }" which
 * contains the switch modeled by its input spec.
 *
 * RunTest is there to be used from within the generated script.  Its code is dumped out
 * to the generated script text, and invoked there.
 *
 * Hope all of this makes some sort of sense.
 *  -kannan
 */

/** HELPERS **/

function ASSERT(cond, msg) { assertEq(cond, true, msg); }

function IsUndef(x) { return typeof(x) == 'undefined'; }
function IsNull(x) { return typeof(x) == 'object' && x == null; }
function IsNum(x) { return typeof(x) == 'number'; }
function IsStr(x) { return typeof(x) == 'string'; }
function IsBool(x) { return typeof(x) == 'boolean'; }

function Repr(x) {
    ASSERT(IsNum(x) || IsStr(x), "Repr");
    if(IsNum(x)) { return ""+x; }
    else { return "'"+x+"'"; }
}

function RandBool() { return Math.random() >= 0.5; }
function RandInt(max) {
    if(IsUndef(max)) { max = 0x7fffffff; }
    return (Math.random() * max)|0;
}

var CHARS = "abcdefghijklmnopqrstuvywxyzABCDEFGHIJKLMNOPQRSTUVYWXYZ0123456789~!@#$%^&*()-_=+{}[]";
function RandStr() {
    var arr = [];
    var len = Math.floor(Math.random() * 10) + 1;
    for(var i = 0; i < len; i++) {
        var c = Math.floor(Math.random() * CHARS.length);
        arr.push(CHARS[c]);
    }
    return arr.join('');
}

function RandVal() { return RandBool() ? RandInt() : RandStr(); }

/**
 * Compare two arrays and ensure they are equal.
 */
function ArraysEqual(arr1, arr2) {
    ASSERT(arr1.length == arr2.length, "Lengths not equal");
    for(var i = 0; i < arr1.length; i++) {
        ASSERT(typeof(arr1[i]) == typeof(arr2[i]), "Types not equal for position " + i);
        ASSERT(arr1[i] == arr2[i], "Values not equal for position " + i);
    }
}

function VerifySwitchSpec(spec) {
    var foundDefault = undefined;
    for(var i = 0; i < spec.length; i++) {
        var caseSpec = spec[i],
            match = caseSpec.match,
            body = caseSpec.body,
            fallthrough = caseSpec.fallthrough;
        ASSERT(IsNum(match) || IsStr(match) || IsNull(match), "Invalid case match for " + i);
        ASSERT(IsNum(body) || IsStr(body) || IsNull(body), "Invalid case body for " + i);
        ASSERT(IsBool(fallthrough), "Invalid fallthrough for " + i);

        if(IsNull(match)) {
            ASSERT(IsUndef(foundDefault), "Duplicate default found at " + i);
            foundDefault = i;
        }
    }
}

/**
 * Do a manual interpretation of a particular spec, given an input
 * and outputting to an output array.
 */
function InterpretSwitch(spec, input, outputArray) {
    var foundMatch = undefined, foundDefault = undefined;
    // Go through cases, trying to find a matching clause.
    for(var i = 0; i < spec.length; i++) {
        var caseSpec = spec[i], match = caseSpec.match;

        if(IsNull(match)) {
            foundDefault = i;
            continue;
        } else if(match === input) {
            foundMatch = i;
            break;
        }
    }
    // Select either matching clause or default.
    var matchI = IsNum(foundMatch) ? foundMatch : foundDefault;

    // If match or default was found, interpret body from that point on.
    if(IsNum(matchI)) {
        for(var i = matchI; i < spec.length; i++) {
            var caseSpec = spec[i],
                match = caseSpec.match,
                body = caseSpec.body,
                fallthrough = caseSpec.fallthrough;
            if(!IsNull(body)) { outputArray.push(body); }
            if(!fallthrough) { break; }
        }
    }
}

/**
 * Generate the code string for a javascript function containing the
 * switch specified by the spec, in pure JS syntax.
 */
function GenerateSwitchCode(spec, name) {
    var lines = [];
    if(!name) { name = ""; }

    lines.push("function "+name+"(x, arr) {");
    lines.push("    switch(x) {");
    for(var i = 0; i < spec.length; i++) {
        var caseSpec = spec[i],
            match = caseSpec.match,
            body = caseSpec.body,
            fallthrough = caseSpec.fallthrough;

        if(IsNull(match))   { lines.push("    default:"); }
        else                { lines.push("    case "+Repr(match)+":"); }

        if(!IsNull(body))   { lines.push("        arr.push("+Repr(body)+");"); }
        if(!fallthrough)    { lines.push("        break;"); }
    }
    lines.push("    }");
    lines.push("}");
    return lines.join("\n");
}

/**
 * Generates all possible specs for a given set of case match values.
 */
function GenerateSpecPermutes(matchVals, resultArray) {
    ASSERT((0 < matchVals.length) && (matchVals.length <= 5), "Invalid matchVals");
    var maxPermuteBody = (1 << matchVals.length) - 1;
    for(var bod_pm = 0; bod_pm <= maxPermuteBody; bod_pm++) {
        var maxPermuteFallthrough = (1 << matchVals.length) - 1;

        for(var ft_pm = 0; ft_pm <= maxPermuteFallthrough; ft_pm++) {
            // use bod_m and ft_pm to decide the placement of null vs value bodies,
            // and the placement of fallthroughs vs breaks.
            // Empty bodies always fall through, so fallthrough bitmask 1s must be
            // a subset of the body bitmask 1s.
            if((bod_pm | ft_pm) != bod_pm) {
                continue;
            }

            var spec = [];
            for(var k = 0; k < matchVals.length; k++) {
                var match = matchVals[k];
                var body = ((bod_pm & (1 << k)) > 0) ? null : RandVal();
                var fallthrough = ((ft_pm & (1 << k)) > 0) ? true : false;
                var caseSpec = {'match':match, 'body':body, 'fallthrough':fallthrough};
                spec.push(caseSpec);
            }

            // Variant specs for following cases:

            // Default with empty body, fallthrough
            GenerateDefaultPermutes(spec, null, true, resultArray);
            // Default with nonempty body, fallthrough
            GenerateDefaultPermutes(spec, RandVal(), true, resultArray);
            // Default with nonempty body, break
            GenerateDefaultPermutes(spec, RandVal(), false, resultArray);
        }
    }
}
function GenerateDefaultPermutes(spec, body, fallthrough, resultArray) {
    if(spec.length <= 2) {
        for(var i = 0; i <= spec.length; i++) {
            var copy = CopySpec(spec);
            if(IsNull(body)) {
                copy.splice(i,0,{'match':null, 'body':null, 'fallthrough':true});
            } else {
                copy.splice(i,0,{'match':null, 'body':body, 'fallthrough':fallthrough});
            }
            resultArray.push(copy);
        }
    } else {
        var posns = [0, Math.floor(spec.length / 2), spec.length];
        posns.forEach(function (i) {
            var copy = CopySpec(spec);
            if(IsNull(body)) {
                copy.splice(i,0,{'match':null, 'body':null, 'fallthrough':true});
            } else {
                copy.splice(i,0,{'match':null, 'body':body, 'fallthrough':fallthrough});
            }
            resultArray.push(copy);
        });
    }
}
function CopySpec(spec) {
    var newSpec = [];
    for(var i = 0; i < spec.length; i++) {
        var caseSpec = spec[i];
        newSpec.push({'match':caseSpec.match,
                      'body':caseSpec.body,
                      'fallthrough':caseSpec.fallthrough});
    }
    return newSpec;
}


function RunSpec(spec, matchVals) {
    var code = GenerateSwitchCode(spec);

    // Generate roughly 200 inputs for the test case spec, exercising
    // every match value, as well as 3 randomly generated values for every
    // iteration of the match values.
    var inputs = [];
    while(inputs.length < 500) {
        for(var i = 0; i < matchVals.length; i++) { inputs.push(matchVals[i]); }
        for(var i = 0; i < 3; i++) { inputs.push(RandVal()); }
    }

    // Interpret the lookupswitch with the inputs.
    var interpResults = [];
    for(var i = 0; i < inputs.length; i++) {
        InterpretSwitch(spec, inputs[i], interpResults);
    }

    // Run compiled lookupswitch with the inputs.
    var fn = eval("_ = " + code);
    print("Running spec: " + code);
    var compileResults = RunCompiled(fn, inputs);
    print("Done Running spec");

    // Verify that they produce the same output.
    ASSERT(interpResults.length == compileResults.length, "Length mismatch");
    for(var i = 0; i < interpResults.length; i++) {
        ASSERT(interpResults[i] == compileResults[i], "Value mismatch");
    }
}
function RunCompiled(fn, inputs) {
    var results = [];
    var len = inputs.length;
    for(var i = 0; i < len; i++) { fn(inputs[i], results); }
    return results;
}

function PrintSpec(spec, inputs, fname) {
    var code = GenerateSwitchCode(spec, fname);
    var input_s = fname + ".INPUTS = [" + inputs.map(Repr).join(', ') + "];";
    var spec_s = fname + ".SPEC = " + JSON.stringify(spec) + ";";
    print(code + "\n" + input_s + "\n" + spec_s);
}

function RunTest(test) {
    // Exercise every test case as well as one case which won't match with any of the
    // ("But what if it randomly generates a string case match whose value is
    //   UNMATCHED_CASE?!", you ask incredulously.  Well, RandStr limits itself to 11 chars.
    //   So there.)
    var inputs = test.INPUTS;
    inputs.push("UNMATCHED_CASE");
    var spec = test.SPEC;

    var results1 = [];
    for(var i = 0; i < 80; i++) {
        for(var j = 0; j < inputs.length; j++) {
            test(inputs[j], results1);
        }
    }

    var results2 = [];
    for(var i = 0; i < 80; i++) {
        for(var j = 0; j < inputs.length; j++) {
            InterpretSwitch(spec, inputs[j], results2);
        }
    }
    ArraysEqual(results1, results2);
}

// NOTES:
//  * RunTest is used within the generated test script.
//  * InterpretSwitch is printed out into the generated test script.

print("/////////////////////////////////////////");
print("// This is a generated file!");
print("// See jit-tests/etc/generate-lookupswitch-tests.js for the code");
print("// that generated this code!");
print("/////////////////////////////////////////");
print("");
print("/////////////////////////////////////////");
print("// PRELUDE                             //");
print("/////////////////////////////////////////");
print("");
print("// Avoid eager compilation of the global-scope.");
print("try{} catch (x) {};");
print("");
print(ASSERT);
print(IsNull);
print(IsNum);
print(ArraysEqual);
print(InterpretSwitch);
print(RunTest);
print("");
print("/////////////////////////////////////////");
print("// TEST CASES                          //");
print("/////////////////////////////////////////");
print("");
print("var TESTS = [];");
var MATCH_SETS = [["foo", "bar", "zing"]];
var count = 0;
for(var i = 0; i < MATCH_SETS.length; i++) {
    var matchSet = MATCH_SETS[i];
    var specs = [];
    GenerateSpecPermutes(matchSet, specs);
    for(var j = 0; j < specs.length; j++) {
        count++;
        PrintSpec(specs[j], matchSet.slice(), 'test_'+count);
        print("TESTS.push(test_"+count+");\n");
    }
}

print("");
print("/////////////////////////////////////////");
print("// RUNNER                              //");
print("/////////////////////////////////////////");
print("");
print("for(var i = 0; i < TESTS.length; i++) {");
print("  RunTest(TESTS[i]);");
print("}");

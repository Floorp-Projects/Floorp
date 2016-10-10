gczeal(0);

var values = {
    input1: null,
    input2: undefined,
    input3: {},
    input4: [],
    input5: ""
};

var original = function (x) {
    var res = { start: inIon(), end: false };
    for (var i in x.input) {
        throw "Iterator is not empty";
    }
    res.end = inIon();
    return res;
};

for (var i = 1; i < 6; i++) {
    // Reset type inference.
    var res = false;
    var test = eval(original.toSource().replace(".input", ".input" + i));

    // Run until the end is running within Ion, or skip if we are unable to run
    // in Ion.
    while (!res.start)
        res = test(values);
    assertEq(!res.start || !res.end, false);
}

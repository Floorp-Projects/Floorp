setJitCompilerOption("baseline.usecount.trigger", 10);
setJitCompilerOption("ion.usecount.trigger", 20);

function join_check() {
    var lengthWasCalled = false;
    var obj = {"0": "", "1": ""};
    Object.defineProperty(obj, "length", {
        get : function(){ lengthWasCalled = true; return 2; },
        enumerable : true,
        configurable : true
    });

    var res = Array.prototype.join.call(obj, { toString: function () {
        if (lengthWasCalled)
            return "good";
        else
            return "bad";
    }})

    assertEq(res, "good");
}
function split(i) {
    var x = (i + "->" + i).split("->");
    assertEq(x[0], "" + i);
    return i;
}

function join(i) {
    var x = [i, i].join("->");
    assertEq(x, i + "->" + i);
    return i;
}

function split_join(i) {
    var x = (i + "-" + i).split("-").join("->");
    assertEq(x, i + "->" + i);
    return i;
}

function split_join_2(i) {
    var x = (i + "-" + i).split("-");
    x.push("" + i);
    var res = x.join("->");
    assertEq(res, i + "->" + i + "->" + i);
    return i;
}

function resumeHere() { bailout(); }

function split_join_3(i) {
    var x = (i + "-" + i).split("-");
    resumeHere();
    var res = x.join("->");
    assertEq(res, i + "->" + i);
    return i;
}

function trip(i) {
    if (i == 99)
        assertEq(myjoin.arguments[1][0], "" + i)
}

function myjoin(i, x) {
    trip(i);
    return x.join("->");
}

function split_join_4(i) {
    var x = (i + "-" + i).split("-");
    var res = myjoin(i, x);
    assertEq(res, i + "->" + i);
    return i;
}

for (var i = 0; i < 100; ++i) {
    join_check(i);
    split(i);
    join(i);
    split_join(i);
    split_join_2(i);
    split_join_3(i);
    split_join_4(i);
}

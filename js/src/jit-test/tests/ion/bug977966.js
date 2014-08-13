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

for (var i = 0; i < 100; ++i) {
    join_check(i);
    split(i);
    join(i);
}

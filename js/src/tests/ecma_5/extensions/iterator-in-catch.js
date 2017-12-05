//Bug 350712

function iterator () {
    for (var i in []);
}

try {
    try {
        throw 5;
    }
    catch(error if iterator()) {
        assertEq(false, true);
    }
}
catch(error) {
  assertEq(error, 5);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);

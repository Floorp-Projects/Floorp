load(libdir + "asserts.js");

assertThrowsInstanceOf(function () {
    var [] = [x, ...d];
}, ReferenceError);

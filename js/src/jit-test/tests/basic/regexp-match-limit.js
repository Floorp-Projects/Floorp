// See bug 953013

load(libdir + "asserts.js");

function test() {
    var input = Array(999999+1).join('a');
    var result = /^([\w])+$/.test(input);
}
assertThrowsInstanceOf(test, InternalError);

// |jit-test| allow-oom; allow-overrecursed

gcparam("maxBytes", gcparam("gcBytes") + 4*1024);
function f() {
    var upvar = "";
    function g() { upvar += ""; }
    var inner4 = f("get"),
	x1,x2,x3,x4,x5,x11,x12,x13,x14,x15,x16,x17,x18,
        otherGlobalSameCompartment = newGlobal("same-compartment");
    eval('');
}
assertEq("aaa".replace(/a/g, f()), "poniesponiesponies");

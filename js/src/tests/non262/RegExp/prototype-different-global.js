function test(otherGlobal) {
    var otherRegExp = otherGlobal.RegExp;

    for (let name of ["global", "ignoreCase", "multiline", "sticky", "unicode", "source"]) {
        let getter = Object.getOwnPropertyDescriptor(RegExp.prototype, name).get;
        assertEq(typeof getter, "function");

        // Note: TypeError gets reported from wrong global if cross-compartment,
        // so we test both cases.
        let ex;
        try {
            getter.call(otherRegExp.prototype);
        } catch (e) {
            ex = e;
        }
        assertEq(ex instanceof TypeError || ex instanceof otherGlobal.TypeError, true);
    }

    let flagsGetter = Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get;
    assertEq(flagsGetter.call(otherRegExp.prototype), "");

    assertEq(RegExp.prototype.toString.call(otherRegExp.prototype), "/(?:)/");
}
test(newGlobal());
test(newGlobal({newCompartment: true}));

if (typeof reportCompare === "function")
    reportCompare(0, 0);

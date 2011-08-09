// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Jim Blandy

if (typeof findReferences == "function") {

    var global = newGlobal('new-compartment');
    var o = ({});
    global.o = o;

    // Don't trip a cross-compartment reference assertion.
    findReferences(o);

    reportCompare(true, true);

} else {
    reportCompare(true, true, "test skipped: findReferences is not a function");
}

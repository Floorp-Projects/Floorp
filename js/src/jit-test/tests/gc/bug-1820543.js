gczeal(0);

const v1 = ("DEB1").startsWith("DEB1");
function f2(a3, a4, a5, a6) {
    return ({"constructor":this,"b":a3,"__proto__":this}).newGlobal(f2);
}
f2.newCompartment = v1;
with (f2()) {
    function f11(a12, a13) {
        return "DEB1";
    }
    const v15 = new FinalizationRegistry(f11);
    v15.register(f2);
}
this.reportLargeAllocationFailure();
gc()

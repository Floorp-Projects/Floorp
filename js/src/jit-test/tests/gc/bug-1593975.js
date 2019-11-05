function runtest(func) {
    func();
}
const g1 = newGlobal({
    newCompartment: true
});
enqueueMark("enter-weak-marking-mode");
function transplantMarking() {
    const vals = {};
    vals.map = new WeakMap();
    enqueueMark(vals.map);
    enqueueMark("yield");
    enqueueMark("enter-weak-marking-mode");
}
runtest(transplantMarking);
egc = 60;
gcslice(egc * 100);
try {
x();
} catch(e) {}

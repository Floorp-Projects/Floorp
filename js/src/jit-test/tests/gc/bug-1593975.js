// |jit-test| error: ReferenceError
function runtest(func) {
    func();
}
const g1 = newGlobal({
    newCompartment: true
});
function transplantMarking() {
    const vals = {};
    vals.map = new WeakMap();
    enqueueMark(vals.map);
    enqueueMark("yield");
    enqueueMark("enter-weak-marking-mode");
}
if (this.enqueueMark) {
  enqueueMark("enter-weak-marking-mode");
  runtest(transplantMarking);
  egc = 60;
  gcslice(egc * 100);
}
x();

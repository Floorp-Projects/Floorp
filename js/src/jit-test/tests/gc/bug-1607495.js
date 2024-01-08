// |jit-test| slow;

gczeal(14, 2);
var g32 = newGlobal();
let wr6 = new g32.WeakRef(newGlobal({
    newCompartment: true
}));
let wr7 = new WeakRef(wr6);

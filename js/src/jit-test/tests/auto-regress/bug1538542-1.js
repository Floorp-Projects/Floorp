var lfOffThreadGlobal = newGlobal();
nukeAllCCWs();
const thisGlobal = this;
const otherGlobalNewCompartment = newGlobal({
    newCompartment: true
});
let { transplant } = transplantableObject();

// Just don't crash.
try {
    transplant(otherGlobalNewCompartment);
    transplant(thisGlobal);
} catch {}

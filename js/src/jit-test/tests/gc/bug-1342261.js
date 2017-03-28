// With --baseline-eager, create a cross-compartment wrapper IC referring to a
// target in an otherwise-doomed compartment.

fullcompartmentchecks(true);
newGlobal({
    sameZoneAs: []
}).z;

// |jit-test| slow
gczeal(9, 1)
for (var a = 0; a < 1; a++) {
    newGlobal({
        sameZoneAs: {}
    })
}

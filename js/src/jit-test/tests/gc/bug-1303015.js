// |jit-test| slow

var x = ``.split();
oomTest(function() {
    var lfGlobal = newGlobal({sameZoneAs: this});
    for (lfLocal in this) {
        if (!(lfLocal in lfGlobal)) {
                lfGlobal[lfLocal] = this[lfLocal];
        }
    }
});


if (!('oomTest' in this))
    quit();

var x = ``.split();
oomTest(function() {
    var lfGlobal = newGlobal();
    for (lfLocal in this) {
        if (!(lfLocal in lfGlobal)) {
                lfGlobal[lfLocal] = this[lfLocal];
        }
    }
});


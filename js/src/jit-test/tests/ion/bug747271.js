// vim: set ts=8 sts=4 et sw=4 tw=99:
function randomFloat () {
    // note that in fuzz-testing, this can used as the size of a buffer to allocate.
    // so it shouldn't return astronomic values. The maximum value 10000000 is already quite big.
    var fac = 1.0;
    var r = Math.random();
    if (r < 0.25)
        fac = 10;
    else if (r < 0.7)
        fac = 10000000;
    else if (r < 0.8)
        fac = NaN;
    return -0.5*fac + Math.random() * fac;
}

for (var i = 0; i < 100000; i++)
    randomFloat();


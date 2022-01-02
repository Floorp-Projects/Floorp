
let a, b;
for (i=0; i < 300000; i++) {
    let c = { a: a, b: b };
    a = b;
    b = c;
}

gc();

// GCRuntime::setLargeHeapSizeMinBytes will change the low value to be one
// byte lower than the high value (if necessary).  But this blew up later
// when the values were mistakingly cast to float then compared, rather than
// kept as size_t.
gcparam('largeHeapSizeMin', 99);



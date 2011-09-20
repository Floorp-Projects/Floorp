function f0(p0,p1) {
    var v0;
    var v1;
    while (p0) {
        if (p0) {
           v1 = v0 + p0;
           v0 = v1;
        }
        v0 = p1;
        if (v1) {
            while (v1);
            break;
        }
    }
}
f0();
/* Don't assert. */

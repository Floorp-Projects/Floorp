
function throwsRangeError(t) {
    try {
        t: for (t[t++] in object) {
            t++
            break t;
        }
        date(t)
    } catch (err) {}
}
throwsRangeError(Infinity);

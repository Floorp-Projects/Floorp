StrictEquality(true, new Boolean(true), false, 0);
function StrictEquality(x, y, expect, i) {
    result = ( x === y );
    if (i < 100)
        StrictEquality(y.environment !== Set.environment, true, false, i + 1);
}
